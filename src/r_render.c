#include "r_common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glad/glad.h>
#include "g_common.h"
#include <main.h>
#include "utility.h"
#include "u_math.h"

#define NUM_RENDER_THREADS 12
#define MAX_SCREENSPRITES 10
#define MAX_SCREENTEXTS 10


static const char* VERTEX_SHADER_SOURCE[] =
{
	"#version 330 core \n"

	"layout(location = 0) in vec3 a_Pos;\n"
	"layout(location = 1) in vec2 a_texCoords;\n"

	"out vec2 TexCoords;\n"

	"void main()\n"
	"{\n"
		"TexCoords = a_texCoords;\n"
		"gl_Position = vec4(a_Pos.x, -a_Pos.y, 1.0, 1.0);\n"
	"}\n"
};
static const char* FRAGMENT_SHADER_SOURCE[] =
{
	"#version 330 core \n"

	"out vec4 FragColor;\n"

	"in vec2 TexCoords;\n"

	"uniform sampler2D scene_texture;\n"

	"void main()\n"
	"{\n"
		"vec4 TexColor = texture(scene_texture, TexCoords);\n"

		"FragColor = TexColor;\n"
	"}\n"
};


typedef struct
{
	int render_ticks;
	//DrawCollumns draw_collumns;
	short* clip_y_top;
	short* clip_y_bottom;

	GLuint gl_texture, gl_vao, gl_vbo, gl_shader;

	Image framebuffer;

	float* depth_buffer;

	int w, h;
	int win_w, win_h;

	float hfov, vfov;

	float view_x, view_y, view_z,view_cos, view_sin;

	int scale;
	bool is_fullscreen;

	FontData font_data;

	RenderThread threads[NUM_RENDER_THREADS];

	Sprite* screen_sprites[MAX_SCREENSPRITES];
	int num_screen_sprites;

	RenderData render_data;

	int main_thread_draw_sprite_indices[MAX_DRAWSPRITES];
	int num_main_thread_draw_sprites;

	ShaderFun fullscreen_shader_fun;

	ReaderWriterLockMutex reader_writer_object_mutex;
	CRITICAL_SECTION main_thread_mutex;
	CONDITION_VARIABLE main_thread_cv;
	HANDLE main_thread_active_event;
	HANDLE main_thread_standby_event;
	HANDLE main_thread_handle;

	bool stall_main_thread;
	bool size_changed;
	bool main_thread_shutdown;
} RenderCore;

static RenderCore s_renderCore;

static bool Shader_checkCompileErrors(unsigned int p_object, const char* p_type)
{
	int success;
	char infoLog[1024];
	if (p_type != "PROGRAM")
	{
		glGetShaderiv(p_object, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(p_object, 1024, NULL, infoLog);
			printf("Failed to Compile %s Shader. Reason: %s", p_type, infoLog);

			return false;
		}
	}
	else
	{
		glGetProgramiv(p_object, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(p_object, 1024, NULL, infoLog);
			printf("Failed to Compile %s Shader. Reason: %s", p_type, infoLog);
			return false;
		}
	}
	return true;
}

static void Render_SizeChanged()
{
	EnterCriticalSection(&s_renderCore.main_thread_mutex);

	s_renderCore.size_changed = true;

	LeaveCriticalSection(&s_renderCore.main_thread_mutex);
}

static void Render_ThreadMutexLock(RenderThread* thread)
{
	EnterCriticalSection(&thread->mutex);
}
static void Render_ThreadMutexUnlock(RenderThread* thread)
{
	LeaveCriticalSection(&thread->mutex);
}

static void Render_Level(Map* map, RenderData* render_data, int start_x, int end_x)
{
	//setup render data
	RenderUtl_SetupRenderData(render_data, s_renderCore.w, start_x, end_x);

	DrawingArgs drawing_args;
	
	//setup drawing args
	drawing_args.view_x = s_renderCore.view_x;
	drawing_args.view_y = s_renderCore.view_y;
	drawing_args.view_z = s_renderCore.view_z;
	drawing_args.view_cos = s_renderCore.view_cos;
	drawing_args.view_sin = s_renderCore.view_sin;
	drawing_args.yclip_bottom = s_renderCore.clip_y_bottom;
	drawing_args.yclip_top = s_renderCore.clip_y_top;
	drawing_args.depth_buffer = s_renderCore.depth_buffer;
	drawing_args.h_fov = s_renderCore.hfov * (float)s_renderCore.h;
	drawing_args.v_fov = s_renderCore.vfov * (float)s_renderCore.h;
	drawing_args.start_x = start_x;
	drawing_args.end_x = end_x;
	drawing_args.render_data = render_data;

	//draw lines and add sprites to draw list
	int nodes_drawn = Scene_ProcessBSPNode(&s_renderCore.framebuffer, map, map->num_nodes - 1, &drawing_args);

	//draw draw collumns
	Scene_DrawDrawCollumns(&s_renderCore.framebuffer, &drawing_args.render_data->draw_collums, s_renderCore.depth_buffer);
	

	//draw sprites
	for (int i = 0; i < render_data->num_draw_sprites; i++)
	{
		DrawSprite* sprite = &render_data->draw_sprites[i];

		sprite->scale_x = 32;
		sprite->scale_y = 32;
		Video_DrawSprite3(&s_renderCore.framebuffer, &drawing_args, sprite);
	}
}

static void Render_MainThreadLoop()
{
	GLFWwindow* window = Engine_GetWindow();

	glfwMakeContextCurrent(window);

	float view_x = 0;
	float view_y = 0;
	float view_z = 0;
	float angle = 0;
	float angle_cos = 0;
	float angle_sin = 0;

	SetEvent(s_renderCore.main_thread_active_event);

	while (!glfwWindowShouldClose(window) || !s_renderCore.main_thread_shutdown)
	{
		float aspect = Render_GetWindowAspect();

		Player_GetView(&view_x, &view_y, &view_z, NULL, &angle);

		Render_View(view_x, view_y, view_z, cosf(angle), sinf(angle));

		glfwSwapBuffers(window);

		if (Game_GetState() == GS__EXIT)
		{
			break;
		}

		EnterCriticalSection(&s_renderCore.main_thread_mutex);
		if (s_renderCore.size_changed)
		{
			glViewport(0, 0, s_renderCore.win_w, s_renderCore.win_h);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, s_renderCore.w, s_renderCore.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

			s_renderCore.size_changed = false;
		}
		//check for stall request
		{
			if (s_renderCore.stall_main_thread)
			{
				ResetEvent(s_renderCore.main_thread_active_event);
				SetEvent(s_renderCore.main_thread_standby_event);

				while (s_renderCore.stall_main_thread)
				{
					SleepConditionVariableCS(&s_renderCore.main_thread_cv, &s_renderCore.main_thread_mutex, INFINITE);
				}

				//resume
				ResetEvent(s_renderCore.main_thread_standby_event);
				SetEvent(s_renderCore.main_thread_active_event);
			}
		}
		LeaveCriticalSection(&s_renderCore.main_thread_mutex);
	}
}

static void Render_ThreadLoop(RenderThread* thread)
{
	SetEvent(thread->finished_work_event);

	Map* map = Map_GetMap();

	while (!thread->shutdown)
	{
		//wait for work
		WaitForSingleObject(thread->start_work_event, INFINITE);

		//set active state
		ResetEvent(thread->finished_work_event);
		ResetEvent(thread->start_work_event);

		SetEvent(thread->active_event);
		thread->state = TS__WORKING;

		//do the work
		Render_ThreadMutexLock(thread);

		switch (thread->work_type)
		{
		case TWT__SHADER:
		{
			Video_Shade(&s_renderCore.framebuffer, s_renderCore.fullscreen_shader_fun, thread->x_start, 0, thread->x_end, s_renderCore.h);
			break;
		}
		case TWT__DRAW_LEVEL:
		{
			Render_Level(map, &thread->render_data, thread->x_start, thread->x_end);

			Game_DrawHud(&s_renderCore.framebuffer, &s_renderCore.font_data, thread->x_start, thread->x_end);
			break;
		}
		default:
			break;
		}

		Render_ThreadMutexUnlock(thread);

		//set inactive state
		ResetEvent(thread->active_event);
		SetEvent(thread->finished_work_event);

		thread->state = TS__SLEEPING;
	}
}

static void Render_WaitForThread(RenderThread* thread)
{
	WaitForSingleObject(thread->finished_work_event, INFINITE);
}

static void Render_WaitForAllThreads()
{
	for (int i = 0; i < NUM_RENDER_THREADS; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		Render_WaitForThread(thr);
	}
}

static void Render_SetWorkStateForAllThreads(ThreadWorkType work_type)
{
	//set start work event for all threads
	for (int i = 0; i < NUM_RENDER_THREADS; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		Render_ThreadMutexLock(thr);

		thr->work_type = work_type;

		SetEvent(thr->start_work_event);

		WaitForSingleObject(thr->active_event, INFINITE);

		Render_ThreadMutexUnlock(thr);
	}
}

static void Render_StallMainThread()
{
	s_renderCore.stall_main_thread = true;

	WaitForSingleObject(s_renderCore.main_thread_standby_event, INFINITE);
}

static void Render_ResumeMainThread()
{
	s_renderCore.stall_main_thread = false;
	WakeConditionVariable(&s_renderCore.main_thread_cv);

	WaitForSingleObject(s_renderCore.main_thread_active_event, INFINITE);
}

static void Render_SetThreadsStartAndEnd(int width)
{
	float num_threads = NUM_RENDER_THREADS;
	int slice = ceil((float)width / num_threads);

	int x_start = 0;
	int x_end = slice;

	for (int i = 0; i < NUM_RENDER_THREADS; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		thr->x_start = x_start;
		thr->x_end = x_end;

		x_start = x_end;
		x_end += slice;
	}
}

static bool Render_SetupGL(int width, int height)
{
	unsigned shader_id = 0;

	//setup shaders
	{
		GLuint vertex_id, frag_id;
		vertex_id = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex_id, 1, &VERTEX_SHADER_SOURCE, NULL);
		glCompileShader(vertex_id);

		if (!Shader_checkCompileErrors(vertex_id, "Vertex"))
		{
			return false;
		}
		frag_id = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(frag_id, 1, &FRAGMENT_SHADER_SOURCE, NULL);
		glCompileShader(frag_id);

		if (!Shader_checkCompileErrors(frag_id, "Fragment"))
		{
			return false;
		}

		shader_id = glCreateProgram();
		glAttachShader(shader_id, vertex_id);
		glAttachShader(shader_id, frag_id);

		glLinkProgram(shader_id);
		if (!Shader_checkCompileErrors(shader_id, "Program"))
			return false;

		glDeleteShader(vertex_id);
		glDeleteShader(frag_id);
	}

	//setup main screen texture
	GLuint texture = 0;
	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	//setup quad vao and vbo
	unsigned vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	unsigned vbo = 0;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	const float quad_vertices[16] =
	{ -1, -1, 0, 0,
			-1,  1, 0, 1,
			1,  1, 1, 1,
			1, -1, 1, 0,
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
	glEnableVertexAttribArray(1);

	//setup gl state
	glUseProgram(shader_id);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	glViewport(0, 0, width, height);

	glBindTexture(GL_TEXTURE_2D, texture);

	s_renderCore.gl_shader = shader_id;
	s_renderCore.gl_texture = texture;
	s_renderCore.gl_vao = vao;
	s_renderCore.gl_vbo = vbo;

	return true;
}

void Render_WindowCallback(GLFWwindow* window, int width, int height)
{
	s_renderCore.win_w = width;
	s_renderCore.win_h = height;

	Render_SizeChanged();
}

bool Render_Init(int width, int height)
{
	memset(&s_renderCore, 0, sizeof(RenderCore));

	Video_Setup();

	if (!Render_SetupGL(width, height))
	{
		return;
	}

	if (!Text_LoadFont("assets/font/font.json", "assets/font/font.png", &s_renderCore.font_data))
	{
		return false;
	}

	if (!Image_Create(&s_renderCore.framebuffer, width, height, 4))
	{
		return false;
	}

	glfwMakeContextCurrent(NULL);

	s_renderCore.win_w = width;
	s_renderCore.win_h = height;
	
	s_renderCore.hfov = 0.73;
	s_renderCore.vfov = 0.2;

	DWORD render_thread_id = 0;
	s_renderCore.main_thread_handle = CreateThread(NULL, 0, Render_MainThreadLoop, NULL, 0, &render_thread_id);

	ReaderWriterLockMutex_Init(&s_renderCore.reader_writer_object_mutex);
	InitializeCriticalSection(&s_renderCore.main_thread_mutex);
	InitializeConditionVariable(&s_renderCore.main_thread_cv);
	s_renderCore.main_thread_active_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	s_renderCore.main_thread_standby_event = CreateEvent(NULL, TRUE, FALSE, NULL);

	for (int i = 0; i < NUM_RENDER_THREADS; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		InitializeCriticalSection(&thr->mutex);
		thr->start_work_event = CreateEvent(NULL, TRUE, FALSE, NULL);
		thr->finished_work_event = CreateEvent(NULL, TRUE, FALSE, NULL);
		thr->active_event = CreateEvent(NULL, TRUE, FALSE, NULL);

		thr->thread_handle = CreateThread(NULL, 0, Render_ThreadLoop, thr, 0, &render_thread_id);
	}

	Render_ResizeWindow(width, height);

	Scene_Setup(width, height, (float)width * s_renderCore.hfov, (float)height * s_renderCore.vfov);

	return true;
}

void Render_ShutDown()
{
	//wait for main thread to exit
	s_renderCore.main_thread_shutdown = true;
	WaitForSingleObject(s_renderCore.main_thread_handle, INFINITE);
	CloseHandle(s_renderCore.main_thread_handle);

	//shut down render threads
	for (int i = 0; i < NUM_RENDER_THREADS; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		//this will exit the thread loop
		thr->shutdown = true;
		SetEvent(thr->start_work_event);

		WaitForSingleObject(thr->thread_handle, INFINITE);

		DeleteCriticalSection(&thr->mutex);
		CloseHandle(thr->thread_handle);
		CloseHandle(thr->active_event);
		CloseHandle(thr->finished_work_event);
		CloseHandle(thr->start_work_event);

		RenderUtl_DestroyRenderData(&thr->render_data);
	}

	ReaderWriterLockMutex_Destruct(&s_renderCore.reader_writer_object_mutex);
	DeleteCriticalSection(&s_renderCore.main_thread_mutex);
	CloseHandle(s_renderCore.main_thread_active_event);
	CloseHandle(s_renderCore.main_thread_standby_event);

	Image_Destruct(&s_renderCore.framebuffer);
	Image_Destruct(&s_renderCore.font_data.font_image);

	free(s_renderCore.depth_buffer);
	free(s_renderCore.clip_y_bottom);
	free(s_renderCore.clip_y_top);
}


void Render_LockThreadsMutex()
{
	for (int i = 0; i < NUM_RENDER_THREADS; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		Render_ThreadMutexLock(thr);
	}
}

void Render_UnlockThreadsMutex()
{
	for (int i = 0; i < NUM_RENDER_THREADS; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		Render_ThreadMutexUnlock(thr);
	}
}

void Render_LockObjectMutex(bool writer)
{
	if (writer)
	{
		ReaderWriterLockMutex_EnterWrite(&s_renderCore.reader_writer_object_mutex);
	}
	else
	{
		ReaderWriterLockMutex_EnterRead(&s_renderCore.reader_writer_object_mutex);
	}
}

void Render_UnlockObjectMutex(bool writer)
{
	if (writer)
	{
		ReaderWriterLockMutex_ExitWrite(&s_renderCore.reader_writer_object_mutex);
	}
	else
	{
		ReaderWriterLockMutex_ExitRead(&s_renderCore.reader_writer_object_mutex);
	}
}

void Render_FinishAndStall()
{
	Render_StallMainThread();

	Render_WaitForAllThreads();
}

void Render_Resume()
{
	//not stalled
	if (!s_renderCore.stall_main_thread)
	{
		return;
	}

	Render_ResumeMainThread();
}

void Render_AddScreenSpriteToQueue(Sprite* sprite)
{
	if (s_renderCore.num_screen_sprites >= MAX_SCREENSPRITES)
	{
		return;
	}

	s_renderCore.screen_sprites[s_renderCore.num_screen_sprites++] = sprite;
}

void Render_QueueFullscreenShader(ShaderFun shader_fun)
{
	s_renderCore.fullscreen_shader_fun = shader_fun;
}


void Render_ResizeWindow(int width, int height)
{
	Render_FinishAndStall();
	Render_SetThreadsStartAndEnd(width);

	Scene_Setup(width, height, s_renderCore.hfov, s_renderCore.vfov);

	Image_Resize(&s_renderCore.framebuffer, width, height);

	if (s_renderCore.depth_buffer)
	{
		free(s_renderCore.depth_buffer);
	}

	s_renderCore.depth_buffer = malloc(sizeof(float) * width * height);

	if (!s_renderCore.depth_buffer)
	{
		return;
	}

	memset(s_renderCore.depth_buffer, 1e3, sizeof(float) * width * height);

	if (s_renderCore.clip_y_bottom)
	{
		free(s_renderCore.clip_y_bottom);
	}

	s_renderCore.clip_y_bottom = calloc(width + 2, sizeof(short));
	
	if (s_renderCore.clip_y_top)
	{
		free(s_renderCore.clip_y_top);
	}

	s_renderCore.clip_y_top = calloc(width + 2, sizeof(short));

	//if (s_renderCore.draw_collumns.collumns)
	{
		//free(s_renderCore.draw_collumns.collumns);
	}

	//s_renderCore.draw_collumns.collumns = calloc(width * 2, sizeof(DrawCollumn));

	s_renderCore.w = width;
	s_renderCore.h = height;

	Render_SizeChanged();

	Render_Resume();
}

void Render_View(float x, float y, float z, float angleCos, float angleSin)
{
	GameState game_state = Game_GetState();

	//store view information
	s_renderCore.view_x = x;
	s_renderCore.view_y = y;
	s_renderCore.view_z = z;
	s_renderCore.view_cos = angleCos;
	s_renderCore.view_sin = angleSin;

	//clear image to black
	Image_Clear(&s_renderCore.framebuffer, 0);

	Map* map = Map_GetMap();

	DrawingArgs drawing_args;

	if (game_state == GS__LEVEL)
	{
		//setup top and bottom clips
		for (int i = 0; i < s_renderCore.w; i++)
		{
			s_renderCore.clip_y_top[i] = 0;
			s_renderCore.clip_y_bottom[i] = s_renderCore.h - 1;
		}

		//clear wall depth buffer
		memset(s_renderCore.depth_buffer, (int)DEPTH_CLEAR, sizeof(float) * s_renderCore.w * s_renderCore.h);

		//Render_Level(map, &s_renderCore.render_data, 0, s_renderCore.w);

		//set work state for all threads
		Render_SetWorkStateForAllThreads(TWT__DRAW_LEVEL);

		//wait for all threads to finish
		Render_WaitForAllThreads();

		Game_Draw(&s_renderCore.framebuffer, &s_renderCore.font_data);
	}
	else
	{
		Game_Draw(&s_renderCore.framebuffer, &s_renderCore.font_data);
	}
	if (s_renderCore.fullscreen_shader_fun)
	{
		Render_WaitForAllThreads();

		Render_SetWorkStateForAllThreads(TWT__SHADER);

		Render_WaitForAllThreads();
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s_renderCore.w, s_renderCore.h, GL_RGBA, GL_UNSIGNED_BYTE, s_renderCore.framebuffer.data);

	//render fullscreen quad
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	//reset stuff
	s_renderCore.num_screen_sprites = 0;
	s_renderCore.num_main_thread_draw_sprites = 0;
	s_renderCore.fullscreen_shader_fun = NULL;
	s_renderCore.render_ticks++;
}

void Render_GetWindowSize(int* r_width, int* r_height)
{
	if (r_width) *r_width = s_renderCore.win_w;
	if (r_height) *r_height = s_renderCore.win_h;
}

void Render_GetRenderSize(int* r_width, int* r_height)
{
	if (r_width) *r_width = s_renderCore.w;
	if (r_height) *r_height = s_renderCore.h;
}

int Render_GetRenderScale()
{
	return s_renderCore.scale;
}

void Render_SetRenderScale(int scale)
{
	if (scale < 1)
	{
		scale = 1;
	}
	else if (scale > MAX_RENDER_SCALE)
	{
		scale = MAX_RENDER_SCALE;
	}

	s_renderCore.scale = scale;

	Render_ResizeWindow(BASE_RENDER_WIDTH * scale, BASE_RENDER_HEIGHT * scale);
}
float Render_GetWindowAspect()
{
	if (s_renderCore.win_h <= 0)
	{
		return 1;
	}

	return (float)s_renderCore.win_w / (float)s_renderCore.win_h;
}

int Render_IsFullscreen()
{
	return s_renderCore.is_fullscreen;
}

void Render_ToggleFullscreen()
{
	s_renderCore.is_fullscreen = !s_renderCore.is_fullscreen;

	GLFWwindow* window = Engine_GetWindow();

	if (s_renderCore.is_fullscreen)
	{
		glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, s_renderCore.w, s_renderCore.h, GLFW_DONT_CARE);
	}
	else
	{
		glfwSetWindowMonitor(window, NULL, 0, 0, s_renderCore.w, s_renderCore.h, GLFW_DONT_CARE);
	}
}

int Render_GetTicks()
{
	return s_renderCore.render_ticks;
}
