#include "g_common.h"

#include "main.h"
#include "sound.h"

#include <stdio.h>

#define SUB_MENU_MAIN 0
#define SUB_MENU_OPTIONS 1
#define SUB_MENU_HELP 2
#define SUB_MENU_EXIT 3

#define INPUT_COOLDOWN 0.25

#define X_TEXT_START 0.25
#define Y_TEXT_START 0.15
#define Y_TEXT_STEP 0.25

#define X_HELPTEXT_START 0.05
#define Y_HELPTEXT_START 0.1
#define X_HELPTEXT_STEP 0.23
#define Y_HELPTEXT_STEP 0.15

#define RENDERSCALE_ID 0
#define SOUND_ID 1
#define SENSITIVITY_ID 2
#define BACK_ID 3

#define SENS_OPTION_STEP 0.5
#define SOUND_OPTION_STEP 0.1
#define LEVEL_END_COUNTER_SPEED 10

typedef struct
{
	//menu data
	float help_x;
	float help_y;
	float input_timer;
	int index;
	int sub_menu;
	int id;

	//level end data
	float secret_counter;
	float monster_counter;

	int secret_max;
	int monster_max;
} MenuCore;

static MenuCore menu_core;

static bool Menu_CheckInput(int key, int state)
{
	if (menu_core.input_timer > 0)
	{
		return false;
	}

	GLFWwindow* window = Engine_GetWindow();

	if (glfwGetKey(window, key) == state)
	{
		Render_RedrawSprites();
		menu_core.input_timer = INPUT_COOLDOWN;
		return true;
	}

	return false;
}
static bool Menu_CheckMouseInput(int key, int state)
{
	if (menu_core.input_timer > 0)
	{
		return false;
	}

	GLFWwindow* window = Engine_GetWindow();

	if (glfwGetMouseButton(window, key) == state)
	{
		Render_RedrawSprites();
		menu_core.input_timer = INPUT_COOLDOWN;
		return true;
	}

	return false;
}

static void Menu_Text(Image* image, FontData* fd, int id, const char* fmt, ...)
{
	char str[2048];
	memset(str, 0, sizeof(str));

	va_list args;
	va_start(args, fmt);

	vsprintf(str, fmt, args);

	va_end(args);

	bool is_selected = (id == menu_core.index);

	if (is_selected)
	{
		Text_DrawStr(image, fd, X_TEXT_START, Y_TEXT_START + (id * Y_TEXT_STEP), 1, 1, 255, 0, 0, 255, str);
	}
	else
	{
		Text_DrawStr(image, fd, X_TEXT_START, Y_TEXT_START + (id * Y_TEXT_STEP), 1, 1, 255, 255, 255, 255, str);
	}
	
	menu_core.id++;
}

static void Menu_HelpText(Image* image, FontData* fd, bool new_line, const char* str)
{
	if (new_line)
	{
		menu_core.help_y += Y_HELPTEXT_STEP;
		menu_core.help_x = 0;
	}

	Text_DrawStr(image, fd, X_HELPTEXT_START + menu_core.help_x, Y_HELPTEXT_START + menu_core.help_y, 0.5, 0.5, 255, 255, 255, 255, str);

	menu_core.help_x += X_HELPTEXT_STEP;
}

static void Menu_HandleOption(int step)
{
	switch (menu_core.index)
	{
	case RENDERSCALE_ID:
	{
		if(step != 0)
			Render_SetRenderScale(Render_GetRenderScale() + step);

		break;
	}
	case SOUND_ID:
	{
		if (step != 0)
			Sound_setMasterVolume(Sound_GetMasterVolume() + (SOUND_OPTION_STEP * step));

		break;
	}
	case SENSITIVITY_ID:
	{
		if(step != 0)
			Player_SetSensitivity(Player_GetSensitivity() + (SENS_OPTION_STEP * step));

		break;
	}
	case BACK_ID:
	{
		if (Menu_CheckInput(GLFW_KEY_ENTER, GLFW_PRESS))
			menu_core.sub_menu = SUB_MENU_MAIN;

		break;
	}
	default:
		break;
	}

	Render_RedrawSprites();
}

void Menu_Update(float delta)
{
	if (menu_core.sub_menu == SUB_MENU_EXIT)
	{
		Game_SetState(GS__EXIT);
		return;
	}

	Render_FinishAndStall();

	if (menu_core.input_timer > 0) menu_core.input_timer -= delta;
	
	if (menu_core.sub_menu == SUB_MENU_OPTIONS)
	{
		int step = 0;

		if (Menu_CheckInput(GLFW_KEY_LEFT, GLFW_PRESS)) step = -1;
		else if (Menu_CheckInput(GLFW_KEY_RIGHT, GLFW_PRESS)) step = 1;

		Menu_HandleOption(step);
	}

	if (Menu_CheckInput(GLFW_KEY_ENTER, GLFW_PRESS))
	{
		switch (menu_core.sub_menu)
		{
		case SUB_MENU_MAIN:
		{
			if (menu_core.index == 0)
			{
				Game_SetState(GS__LEVEL);
				break;
			}

			menu_core.sub_menu = menu_core.index;
			//reset the index
			menu_core.index = 0;
			break;
		}
		case SUB_MENU_OPTIONS:
		{
			break;
		}
		case SUB_MENU_HELP:
		{
			//go back to main screen
			menu_core.sub_menu = SUB_MENU_MAIN;
			break;
		}
		default:
			break;
		}

	}
	else if (Menu_CheckInput(GLFW_KEY_ESCAPE, GLFW_PRESS))
	{
		if (menu_core.sub_menu > 0) menu_core.sub_menu = 0;
		else if(menu_core.sub_menu == 0)
		{
			//Game_SetState(GS__LEVEL);
		}
	}
	
	if (menu_core.id > 0)
	{
		if (Menu_CheckInput(GLFW_KEY_UP, GLFW_PRESS)) menu_core.index--;
		else if (Menu_CheckInput(GLFW_KEY_DOWN, GLFW_PRESS)) menu_core.index++;

		if (menu_core.index < 0) menu_core.index = menu_core.id - 1;
		else if (menu_core.index > menu_core.id - 1) menu_core.index = 0;
	}

	Render_Resume();
}

void Menu_Draw(Image* image, FontData* fd)
{
	menu_core.id = 0;
	menu_core.help_x = 0;
	menu_core.help_y = 0;

	Video_DrawScreenTexture(image, &Game_GetAssets()->menu_texture, 0, 0, 2, 2);

	switch (menu_core.sub_menu)
	{
	case SUB_MENU_MAIN:
	{
		Menu_Text(image, fd, 0, "PLAY");
		Menu_Text(image, fd, 1, "OPTIONS");
		Menu_Text(image, fd, 2, "HELP");
		Menu_Text(image, fd, 3, "EXIT");
		break;
	}
	case SUB_MENU_OPTIONS:
	{
		Menu_Text(image, fd, RENDERSCALE_ID, "RENDER SCALE  < %i > ", Render_GetRenderScale());
		Menu_Text(image, fd, SOUND_ID, "VOLUME  < %.1f > ", Sound_GetMasterVolume());
		Menu_Text(image, fd, SENSITIVITY_ID, "SENSITIVITY  < %.1f > ", Player_GetSensitivity());

		Menu_Text(image, fd, BACK_ID, "BACK");
		break;
	}
	case SUB_MENU_HELP:
	{
		Menu_HelpText(image, fd, false, "FORWARD = W");
		Menu_HelpText(image, fd, false, "BACK = S");
		Menu_HelpText(image, fd, false, "LEFT = A");
		Menu_HelpText(image, fd, false, "RIGHT = D");

		Menu_HelpText(image, fd, true, "SHOOT = LEFT MOUSE");
		Menu_HelpText(image, fd, true, "INTERACT = E");
		Menu_HelpText(image, fd, true, "SLOW WALK = SHIFT");
		Menu_HelpText(image, fd, true, "MENU = ESC");

		Menu_HelpText(image, fd, true, "1 = PISTOL");
		Menu_HelpText(image, fd, false, "2 = SHOTGUN");
		Menu_HelpText(image, fd, false, "3 = MACHINE GUN");
		Menu_HelpText(image, fd, false, "4 = DEVASTATOR");
		break;
	}
	default:
		break;
	}
}

void Menu_LevelEnd_Update(float delta, int secret_goal, int secret_max, int monster_goal, int monster_max)
{
	if (menu_core.input_timer > 0) menu_core.input_timer -= delta;

	menu_core.secret_max = secret_max;
	menu_core.monster_max = monster_max;

	bool finished = true;

	if (menu_core.secret_counter < secret_goal)
	{
		menu_core.secret_counter += delta * LEVEL_END_COUNTER_SPEED;
		finished = false;
	}
	if (menu_core.monster_counter < monster_goal)
	{
		menu_core.monster_counter += delta * LEVEL_END_COUNTER_SPEED;
		finished = false;
	}

	if (Menu_CheckInput(GLFW_KEY_ENTER, GLFW_PRESS) || Menu_CheckMouseInput(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS))
	{
		if (finished)
		{
			//resume
			Game_SetState(GS__LEVEL);

			menu_core.secret_counter = 0;
			menu_core.monster_counter = 0;

			menu_core.secret_max = 0;
			menu_core.monster_max = 0;
		}
		else
		{
			//set to goal
			menu_core.secret_counter = secret_goal;
			menu_core.monster_counter = monster_goal;
		}
	}

	Render_RedrawSprites();

}
void Menu_LevelEnd_Draw(Image* image, FontData* fd)
{
	Video_DrawScreenTexture(image, &Game_GetAssets()->menu_texture, 0, 0, 2, 2);

	Text_DrawColor(image, fd, 0.25, 0.2, 1, 1, 255, 255, 255, 255, "SECRETS %i/%i", (int)menu_core.secret_counter, menu_core.secret_max);
	Text_DrawColor(image, fd, 0.25, 0.5, 1, 1, 255, 255, 255, 255, "KILLS %i/%i", (int)menu_core.monster_counter, menu_core.monster_max);
}

void Menu_Finale_Update(float delta)
{
	if (menu_core.input_timer > 0) menu_core.input_timer = 0;

	if (Menu_CheckInput(GLFW_KEY_ENTER, GLFW_PRESS))
	{
		Game_Reset(true);
		Game_SetState(GS__LEVEL);
	}
}

void Menu_Finale_Draw(Image* image, FontData* fd)
{
	Video_DrawScreenTexture(image, &Game_GetAssets()->menu_texture, 0, 0, 2, 2);

	Text_DrawColor(image, fd, 0.15, 0.2, 1, 1, 255, 255, 255, 255, "THE END \n");
	Text_DrawColor(image, fd, 0.15, 0.5, 1, 1, 255, 255, 255, 255, "THANKS FOR PLAYING \n");
	Text_DrawColor(image, fd, 0.15, 0.7, 1, 1, 255, 255, 255, 255, "PRESS ENTER TO CONTINUE \n");
}