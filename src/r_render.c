#include "r_common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glad/glad.h>
#include "g_common.h"
#include <main.h>
#include "utility.h"
#include "u_math.h"

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

	short* clip_y_top;
	short* clip_y_bottom;

	short* floor_plane_ytop;
	short* floor_plane_ybottom;

	short* ceil_plane_ytop;
	short* ceil_plane_ybottom;

	short* span_end;

	float* yslopes;

	GLuint gl_texture, gl_vao, gl_vbo, gl_shader;

	Image framebuffer;

	float* depth_buffer;

	int w, h;
	int win_w, win_h;

	float hfov, vfov;

	float view_x, view_y, view_z, view_cos, view_sin, view_angle;

	int scale;
	bool is_fullscreen;

	FontData font_data;

	int threads_finished;
	int num_threads;
	RenderThread* threads;

	RenderData render_data;

	ShaderFun fullscreen_shader_fun;

	ThreadWorkType work_type;

	ReaderWriterLockMutex reader_writer_object_mutex;

	CONDITION_VARIABLE start_work_cv;
	CRITICAL_SECTION start_mutex;

	CONDITION_VARIABLE end_work_cv;
	CRITICAL_SECTION end_mutex;

	CRITICAL_SECTION main_thread_mutex;
	CONDITION_VARIABLE main_thread_cv;
	HANDLE main_thread_active_event;
	HANDLE main_thread_standby_event;
	HANDLE main_thread_handle;

	bool stall_main_thread;
	bool shutdown_threads;
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
	
	float tangent = tan(Math_DegToRad(VIEW_FOV) / 2.0);

	//setup drawing args
	drawing_args.view_x = s_renderCore.view_x;
	drawing_args.view_y = s_renderCore.view_y;
	drawing_args.view_z = s_renderCore.view_z;
	drawing_args.view_cos = s_renderCore.view_cos;
	drawing_args.view_sin = s_renderCore.view_sin;
	drawing_args.tan_cos = tangent * s_renderCore.view_cos;
	drawing_args.tan_sin = tangent * s_renderCore.view_sin;
	drawing_args.focal_length_x = ((float)s_renderCore.w / 2.0) / tangent;
	drawing_args.tangent = tangent;
	drawing_args.view_angle = s_renderCore.view_angle;
	drawing_args.yclip_bottom = s_renderCore.clip_y_bottom;
	drawing_args.yclip_top = s_renderCore.clip_y_top;
	drawing_args.floor_plane_ytop = s_renderCore.floor_plane_ytop;
	drawing_args.floor_plane_ybottom = s_renderCore.floor_plane_ybottom;
	drawing_args.ceil_plane_ytop = s_renderCore.ceil_plane_ytop;
	drawing_args.ceil_plane_ybottom = s_renderCore.ceil_plane_ybottom;
	drawing_args.yslope = s_renderCore.yslopes;
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
	
	//draw sprites and decals
	for (int i = 0; i < render_data->num_draw_sprites; i++)
	{
		DrawSprite* sprite = &render_data->draw_sprites[i];

		if (sprite->decal_line_index >= 0)
		{
			Video_DrawDecalSprite(&s_renderCore.framebuffer, &drawing_args, sprite);
		}
		else
		{
			Video_DrawSprite(&s_renderCore.framebuffer, &drawing_args, sprite);
		}	
	}
}

static void Render_DrawAllObjectBoxes()
{
	Map* map = Map_GetMap();

	float tangent = tan(Math_DegToRad(VIEW_FOV) / 2.0);
	float view_cos = tangent * s_renderCore.view_cos;
	float view_sin = tangent * s_renderCore.view_sin;

	for (int i = 0; i < map->num_objects; i++)
	{
		Object* obj = Map_GetObject(i);

		if (obj->type == OT__NONE || obj->type == OT__PLAYER)
		{
			continue;
		}

		float box[2][3];
		box[0][0] = obj->x - obj->size;
		box[0][1] = obj->y - obj->size;
		box[0][2] = obj->z;

		box[1][0] = obj->x + obj->size;
		box[1][1] = obj->y + obj->size;
		box[1][2] = obj->z + obj->height;

		Video_DrawBox(&s_renderCore.framebuffer, box, s_renderCore.view_x, s_renderCore.view_y, s_renderCore.view_z, s_renderCore.view_cos, s_renderCore.view_sin, view_sin, view_cos, s_renderCore.vfov * (float)s_renderCore.h,
			0, s_renderCore.framebuffer.width);
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

	SetEvent(s_renderCore.main_thread_active_event);

	while (!glfwWindowShouldClose(window) || !s_renderCore.main_thread_shutdown)
	{
		float aspect = Render_GetWindowAspect();

		Player_GetView(&view_x, &view_y, &view_z, NULL, &angle);

		Render_View(view_x, view_y, view_z, angle, cos(angle), sin(angle));

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
	Map* map = Map_GetMap();

	int start_tick = s_renderCore.render_ticks;
	int last_tick = start_tick;

	while (!s_renderCore.shutdown_threads)
	{
		//wait for work
		EnterCriticalSection(&s_renderCore.start_mutex);

		while (s_renderCore.render_ticks == last_tick)
		{
			SleepConditionVariableCS(&s_renderCore.start_work_cv, &s_renderCore.start_mutex, INFINITE);
		}
		
		ThreadWorkType work_type = s_renderCore.work_type;

		last_tick = s_renderCore.render_ticks;

		LeaveCriticalSection(&s_renderCore.start_mutex);

		//set active state
		SetEvent(thread->active_event);
		thread->state = TS__WORKING;

		//do the work
		Render_ThreadMutexLock(thread);

		switch (work_type)
		{
		case TWT__SHADER:
		{
			Video_Shade(&s_renderCore.framebuffer, s_renderCore.fullscreen_shader_fun, thread->x_start, 0, thread->x_end, s_renderCore.h);
			break;
		}
		case TWT__DRAW_LEVEL:
		{
			//setup top and bottom clips
			for (int x = thread->x_start; x < thread->x_end; x++)
			{
				s_renderCore.clip_y_top[x] = 0;
				s_renderCore.clip_y_bottom[x] = s_renderCore.h - 1;
			}

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

		EnterCriticalSection(&s_renderCore.end_mutex);
		s_renderCore.threads_finished++;
		LeaveCriticalSection(&s_renderCore.end_mutex);
		WakeAllConditionVariable(&s_renderCore.end_work_cv);

		thread->state = TS__SLEEPING;
	}
}


static void Render_WaitForAllThreads()
{
	EnterCriticalSection(&s_renderCore.end_mutex);

	while (s_renderCore.threads_finished < s_renderCore.num_threads)
	{
		SleepConditionVariableCS(&s_renderCore.end_work_cv, &s_renderCore.end_mutex, INFINITE);
	}

	LeaveCriticalSection(&s_renderCore.end_mutex);
	s_renderCore.threads_finished = 0;
}

static void Render_SetWorkStateForAllThreads(ThreadWorkType work_type)
{
	//set start work event for all threads
	EnterCriticalSection(&s_renderCore.start_mutex);
	if (work_type == TWT__EXIT) s_renderCore.shutdown_threads = true;
	s_renderCore.work_type = work_type;
	s_renderCore.render_ticks++;
	LeaveCriticalSection(&s_renderCore.start_mutex);

	WakeAllConditionVariable(&s_renderCore.start_work_cv);
}

static void Render_StallMainThread()
{
	EnterCriticalSection(&s_renderCore.main_thread_mutex);
	s_renderCore.stall_main_thread = true;
	LeaveCriticalSection(&s_renderCore.main_thread_mutex);

	WaitForSingleObject(s_renderCore.main_thread_standby_event, INFINITE);
}

static void Render_ResumeMainThread()
{
	EnterCriticalSection(&s_renderCore.main_thread_mutex);
	s_renderCore.stall_main_thread = false;
	LeaveCriticalSection(&s_renderCore.main_thread_mutex);
	WakeConditionVariable(&s_renderCore.main_thread_cv);

	WaitForSingleObject(s_renderCore.main_thread_active_event, INFINITE);
}

static void Render_SetThreadsStartAndEnd(int width, int height)
{
	float num_threads = s_renderCore.num_threads;
	int slice = ceil((float)width / num_threads);

	int x_start = 0;
	int x_end = slice;

	for (int i = 0; i < s_renderCore.num_threads; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		thr->x_start = x_start;
		thr->x_end = x_end;

		RenderUtl_Resize(&thr->render_data, width, height, thr->x_start, thr->x_end);

		x_start = x_end;
		x_end += slice;

		x_start = Math_Clampl(x_start, 0, width);
		x_end = Math_Clampl(x_end, 0, width);
	}

	printf("Render Thread Slice: %i\n", slice);
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

bool Render_Init(int width, int height, int scale)
{
	memset(&s_renderCore, 0, sizeof(RenderCore));

	Video_Setup();

	if (scale < 1)
	{
		scale = 1;
	}
	else if (scale > MAX_RENDER_SCALE)
	{
		scale = MAX_RENDER_SCALE;
	}

	s_renderCore.scale = scale;

	width *= scale;
	height *= scale;

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
	s_renderCore.vfov = 0.25;

	InitializeConditionVariable(&s_renderCore.start_work_cv);
	InitializeCriticalSection(&s_renderCore.start_mutex);

	InitializeConditionVariable(&s_renderCore.end_work_cv);
	InitializeCriticalSection(&s_renderCore.end_mutex);

	ReaderWriterLockMutex_Init(&s_renderCore.reader_writer_object_mutex);
	InitializeCriticalSection(&s_renderCore.main_thread_mutex);
	InitializeConditionVariable(&s_renderCore.main_thread_cv);
	s_renderCore.main_thread_active_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	s_renderCore.main_thread_standby_event = CreateEvent(NULL, TRUE, FALSE, NULL);

	DWORD render_thread_id = 0;
	s_renderCore.main_thread_handle = CreateThread(NULL, 0, Render_MainThreadLoop, NULL, 0, &render_thread_id);

	int num_threads = QueryNumLogicalProcessors();

	if (num_threads <= 0)
	{
		num_threads = 1;
	}
	s_renderCore.threads = calloc(num_threads, sizeof(RenderThread));

	if (!s_renderCore.threads)
	{
		return false;
	}

	for (int i = 0; i < num_threads; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		InitializeCriticalSection(&thr->mutex);
		thr->active_event = CreateEvent(NULL, TRUE, FALSE, NULL);

		thr->thread_handle = CreateThread(NULL, 0, Render_ThreadLoop, thr, 0, &render_thread_id);
	}

	s_renderCore.num_threads = num_threads;

	printf("Initialized Render Threads: %i \n", s_renderCore.num_threads);

	Render_ResizeWindow(width, height);

	return true;
}

void Render_ShutDown()
{
	//wait for main thread to exit
	s_renderCore.main_thread_shutdown = true;
	WaitForSingleObject(s_renderCore.main_thread_handle, INFINITE);
	CloseHandle(s_renderCore.main_thread_handle);

	//shut down render threads
	Render_SetWorkStateForAllThreads(TWT__EXIT);
	Render_WaitForAllThreads();

	for (int i = 0; i < s_renderCore.num_threads; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		WaitForSingleObject(thr->thread_handle, INFINITE);

		DeleteCriticalSection(&thr->mutex);
		CloseHandle(thr->thread_handle);
		CloseHandle(thr->active_event);

		RenderUtl_DestroyRenderData(&thr->render_data);
	}

	ReaderWriterLockMutex_Destruct(&s_renderCore.reader_writer_object_mutex);
	DeleteCriticalSection(&s_renderCore.main_thread_mutex);
	DeleteCriticalSection(&s_renderCore.start_mutex);
	DeleteCriticalSection(&s_renderCore.end_mutex);
	CloseHandle(s_renderCore.main_thread_active_event);
	CloseHandle(s_renderCore.main_thread_standby_event);

	Image_Destruct(&s_renderCore.framebuffer);
	Image_Destruct(&s_renderCore.font_data.font_image);

	if (s_renderCore.depth_buffer) free(s_renderCore.depth_buffer);
	if (s_renderCore.clip_y_bottom) free(s_renderCore.clip_y_bottom);
	if (s_renderCore.clip_y_top) free(s_renderCore.clip_y_top);
	if (s_renderCore.floor_plane_ytop) free(s_renderCore.floor_plane_ytop);
	if (s_renderCore.floor_plane_ybottom) free(s_renderCore.floor_plane_ybottom);
	if (s_renderCore.ceil_plane_ytop) free(s_renderCore.ceil_plane_ytop);
	if (s_renderCore.ceil_plane_ybottom) free(s_renderCore.ceil_plane_ybottom);
	if (s_renderCore.yslopes) free(s_renderCore.yslopes);
	if (s_renderCore.threads) free(s_renderCore.threads);
}


void Render_LockThreadsMutex()
{
	for (int i = 0; i < s_renderCore.num_threads; i++)
	{
		RenderThread* thr = &s_renderCore.threads[i];

		Render_ThreadMutexLock(thr);
	}
}

void Render_UnlockThreadsMutex()
{
	for (int i = 0; i < s_renderCore.num_threads; i++)
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
	//already stalled
	if (s_renderCore.stall_main_thread)
	{
		return;
	}

	Render_StallMainThread();

	Render_SetWorkStateForAllThreads(TWT__NONE);
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

void Render_QueueFullscreenShader(ShaderFun shader_fun)
{
	s_renderCore.fullscreen_shader_fun = shader_fun;
}


void Render_ResizeWindow(int width, int height)
{
	Render_FinishAndStall();
	Render_SetThreadsStartAndEnd(width, height);

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

	if (s_renderCore.floor_plane_ytop) free(s_renderCore.floor_plane_ytop);
	s_renderCore.floor_plane_ytop = calloc(width + 2, sizeof(short));

	if (s_renderCore.floor_plane_ybottom) free(s_renderCore.floor_plane_ybottom);
	s_renderCore.floor_plane_ybottom = calloc(width + 2, sizeof(short));

	if (s_renderCore.ceil_plane_ytop) free(s_renderCore.ceil_plane_ytop);
	s_renderCore.ceil_plane_ytop = calloc(width + 2, sizeof(short));

	if (s_renderCore.ceil_plane_ybottom) free(s_renderCore.ceil_plane_ybottom);
	s_renderCore.ceil_plane_ybottom = calloc(width + 2, sizeof(short));

	if (s_renderCore.span_end) free(s_renderCore.span_end);
	s_renderCore.span_end = calloc(width + 2, sizeof(short));

	if (s_renderCore.yslopes) free(s_renderCore.yslopes);
	s_renderCore.yslopes = calloc(height + 2, sizeof(float));

	for (int y = 0; y < height + 2; y++)
	{
		s_renderCore.yslopes[y] = ((height * s_renderCore.vfov) / ((height / 2.0 - ((float)y)) - 0.0 * height * s_renderCore.vfov));
	}

	s_renderCore.w = width;
	s_renderCore.h = height;

	printf("Window size: %i W, %i H \n", s_renderCore.w, s_renderCore.h);

	Render_SizeChanged();

	Render_Resume();
}

void Render_View(float x, float y, float z, float angle, float angleCos, float angleSin)
{
	GameState game_state = Game_GetState();

	//store view information
	s_renderCore.view_x = x;
	s_renderCore.view_y = y;
	s_renderCore.view_z = z;
	s_renderCore.view_cos = angleCos;
	s_renderCore.view_sin = angleSin;
	s_renderCore.view_angle = angle;

	if (game_state == GS__LEVEL && Game_GetLevelIndex() >= 0)
	{
		//set work state for all threads
		Render_SetWorkStateForAllThreads(TWT__DRAW_LEVEL);

		//wait for all threads to finish
		Render_WaitForAllThreads();

		//Render_DrawAllObjectBoxes();

		Game_Draw(&s_renderCore.framebuffer, &s_renderCore.font_data);
	}
	else
	{
		Game_Draw(&s_renderCore.framebuffer, &s_renderCore.font_data);
	}
	if (s_renderCore.fullscreen_shader_fun)
	{
		//Render_SetWorkStateForAllThreads(TWT__SHADER);

		//Render_WaitForAllThreads();
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s_renderCore.w, s_renderCore.h, GL_RGBA, GL_UNSIGNED_BYTE, s_renderCore.framebuffer.data);

	//render fullscreen quad
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	//reset stuff
	s_renderCore.fullscreen_shader_fun = NULL;
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

void Render_Clear(int c)
{
	Image_Clear(&s_renderCore.framebuffer, c);
}
