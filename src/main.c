#define DYNAMIC_ARRAY_IMPLEMENTATION
#include "dynamic_array.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stb_image/stb_image.h>

#include "g_common.h"
#include "r_common.h"
#include "utility.h"
#include "main.h"
#include "sound.h"
#include "u_math.h"

#define WINDOW_SCALE 3
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 360
#define WINDOW_NAME "SECTOR_SHOOTER"

#define MAX_STEPS 8
#define TICKS_PER_SECOND 60.0

typedef struct
{
	double time_scale;
	double delta;
	double lerp_fraction;
	uint64_t ticks;
	GLFWwindow* window;
} EngineData;

static EngineData s_engine;
static const double MIN_DT = 1.0 / 1000000.0;
static const double TIME_STEP = 1.0 / TICKS_PER_SECOND;

extern void Render_WindowCallback(GLFWwindow* window, int width, int height);

static void LoadExeIcon(GLFWwindow* window)
{
	GLFWimage image[1];

	image[0].pixels = stbi_load("icon.png", &image[0].width, &image[0].height, 0, 4);

	if (!image[0].pixels)
	{
		printf("Failed to load exe icon\n");
		return;
	}

	glfwSetWindowIcon(window, 1, image);
	stbi_image_free(image[0].pixels);
}

static void MouseCallback(GLFWwindow* window, double x, double y)
{
	Player_MouseCallback(x, y);
}
static void MouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	
}

static bool Engine_SaveCfg(const char* filename)
{
	FILE* file = NULL;

	fopen_s(&file, filename, "w");
	if (!file)
	{
		printf("Failed to save cfg \n");
		return false;
	}

	fprintf(file, "render_scale %i \n", Render_GetRenderScale());
	fprintf(file, "mouse_sens %.1f \n", Player_GetSensitivity());
	fprintf(file, "volume %.1f \n", Sound_GetMasterVolume());

	return fclose(file) == 0;
}

static bool Engine_LoadCfg(const char* filename)
{
	FILE* file = NULL;

	fopen_s(&file, filename, "r");
	if (!file)
	{
		printf("Failed to load cfg \n");
		return false;
	}

	char buf[256];
	char value_buf[256];

	memset(buf, 0, sizeof(buf));
	memset(value_buf, 0, sizeof(value_buf));

	while (fscanf(file, "%s %s \n", buf, value_buf) == 2)
	{
		float value = strtod(value_buf, (char**)NULL);

		if (!strcmp(buf, "render_scale"))
		{
			Render_SetRenderScale((int)value);
		}
		else if (!strcmp(buf, "mouse_sens"))
		{
			Player_SetSensitivity(value);
		}
		else if (!strcmp(buf, "volume"))
		{
			Sound_setMasterVolume(value);
		}
	}

	return fclose(file) == 0;
}


static bool Engine_SetupWindow()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	s_engine.window = glfwCreateWindow(WINDOW_WIDTH * WINDOW_SCALE, WINDOW_HEIGHT * WINDOW_SCALE, WINDOW_NAME, NULL, NULL);

	if (!s_engine.window)
	{
		return false;
	}
	printf("Window created \n");

	glfwMakeContextCurrent(s_engine.window);
	glfwSetWindowSizeCallback(s_engine.window, Render_WindowCallback);
	glfwSetCursorPosCallback(s_engine.window, MouseCallback);
	glfwSetInputMode(s_engine.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	return true;
}
static bool Engine_SetupSubSystems()
{
	if (!Sound_Init())
	{
		printf("ERROR::Failed to init sound!\n");
		return false;
	}

	printf("Sound loaded \n");

	//load glfw
	if (!glfwInit())
	{
		printf("Failed to load glfw!");
		return false;
	}

	printf("Glfw loaded \n");

	if (!Engine_SetupWindow())
	{
		printf("Failed to create window!");
		glfwTerminate();
		return false;
	}

	//load glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		printf("Failed to init GLAD\n");
		return false;
	}

	printf("GLAD Loaded\n");
	printf("OpenGL loaded\n");
	printf("Vendor:   %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("Version:  %s\n", glGetString(GL_VERSION));

	glfwSwapInterval(0);

	if (!Render_Init(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_SCALE))
	{
		printf("Failed to load renderer!\n");
		return false;
	}

	printf("Renderer Loaded\n");

	if (!Game_Init())
	{
		printf("ERROR::Failed to load game assets!\n");
		return false;
	}

	printf("Game loaded \n");

	return true;
}

static void Engine_ExitSubsystems()
{
	printf("Exiting Renderer... \n");
	Render_ShutDown();
	printf("Renderer Exited \n");

	printf("Exiting Game... \n");
	Game_Exit();
	printf("Game Exited \n");

	printf("Exiting Sound... \n");
	Sound_Shutdown();
	printf("Sound Exited \n");

	glfwDestroyWindow(s_engine.window);
	glfwTerminate();
}

uint64_t Engine_GetTicks()
{
	return s_engine.ticks;
}
double Engine_GetDeltaTime()
{
	return s_engine.delta;
}

double Engine_GetLerpFraction()
{
	return s_engine.lerp_fraction;
}

void Engine_SetTimeScale(float scale)
{
	if (scale < 0)
	{
		scale = 0;
	}
	else if (scale > 1)
	{
		scale = 1;
	}

	s_engine.time_scale = scale;
}

GLFWwindow* Engine_GetWindow()
{
	return s_engine.window;
}

int main()
{
	memset(&s_engine, 0, sizeof(EngineData));

	srand(time(NULL));

	if (!Engine_SetupSubSystems())
	{
		return -1;
	}

	LoadExeIcon(s_engine.window);

	s_engine.time_scale = 1.0;

	//set defaults
	Player_SetSensitivity(1);
	Sound_setMasterVolume(3.0);
	//Render_SetRenderScale(WINDOW_SCALE);

	//attempt to load cfg
	Engine_LoadCfg("config.cfg");

	//Render_ToggleFullscreen();

	double lastTime = 0;
	double currentTime = 0;
	double accumulator = 0;

	//MAIN LOOP
	while (!glfwWindowShouldClose(s_engine.window) && Game_GetState() != GS__EXIT)
	{
		currentTime = glfwGetTime();
		float delta = currentTime - lastTime;
		lastTime = currentTime;

		delta *= s_engine.time_scale;

		s_engine.delta = TIME_STEP;

		accumulator += delta;
		int steps = 0;
		while (accumulator >= TIME_STEP && steps < MAX_STEPS)
		{	
			Game_LogicUpdate(TIME_STEP);
			accumulator -= TIME_STEP;
			steps++;
		}

		double lerp_time = Math_Clampd(accumulator / TIME_STEP, 0.0, 1.0);

		s_engine.delta = delta;
		s_engine.lerp_fraction = lerp_time;

		Game_SmoothUpdate(lerp_time, delta);

		glfwPollEvents();

		s_engine.ticks++;
	}

	printf("Saving config... \n");
	Engine_SaveCfg("config.cfg");

	printf("Shutting down...\n");

	Engine_ExitSubsystems();

	return 0;
}

