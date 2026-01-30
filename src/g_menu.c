#include "g_common.h"

#include <stdio.h>

#include "main.h"
#include "sound.h"
#include "game_info.h"

#define SUB_MENU_MAIN 0
#define SUB_MENU_LOAD 1
#define SUB_MENU_SAVE 2
#define SUB_MENU_OPTIONS 3
#define SUB_MENU_HELP 4
#define SUB_MENU_EXIT 5

#define INPUT_COOLDOWN 0.25

#define X_TEXT_START 0.25
#define Y_TEXT_START 0.15
#define Y_TEXT_STEP 0.25

#define X_HELPTEXT_START 0.05
#define Y_HELPTEXT_START 0.1
#define X_HELPTEXT_STEP 0.23
#define Y_HELPTEXT_STEP 0.12

#define RENDERSCALE_ID 0
#define SOUND_ID 1
#define SENSITIVITY_ID 2
#define BACK_ID 3

#define SENS_OPTION_STEP 0.5
#define SOUND_OPTION_STEP 0.1
#define LEVEL_END_COUNTER_SPEED 10
#define NUM_SPRITES 2

typedef struct
{
	//bg stuff
	Sprite sprites[NUM_SPRITES];

	//menu data
	float help_x;
	float help_y;
	float input_timer;
	int index;
	int sub_menu;
	int id;
	int max_index;

	//level end data
	float secret_counter;
	float monster_counter;

	int secret_max;
	int monster_max;

	SaveSlot save_slots[MAX_SAVE_SLOTS];
} MenuCore;

static MenuCore menu_core;

static void Menu_UpdateSprites(float delta)
{
	for (int i = 0; i < NUM_SPRITES; i++)
	{
		Sprite* sprite = &menu_core.sprites[i];

		Sprite_UpdateAnimation(sprite, delta);
	}
}

static bool Menu_CheckInput(int key, int state)
{
	if (menu_core.input_timer > 0)
	{
		return false;
	}

	GLFWwindow* window = Engine_GetWindow();

	if (glfwGetKey(window, key) == state)
	{
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
		menu_core.input_timer = INPUT_COOLDOWN;
		return true;
	}

	return false;
}

static void Menu_Text(Image* image, FontData* fd, int id, float y_step, const char* fmt, ...)
{
	char str[2048];
	memset(str, 0, sizeof(str));

	va_list args;
	va_start(args, fmt);

	vsprintf(str, fmt, args);

	va_end(args);

	bool is_selected = (id == menu_core.index);
	bool is_blocked = (id < 0);

	if (is_selected)
	{
		Text_DrawStr(image, fd, X_TEXT_START, Y_TEXT_START + (id * y_step),1, 1, 0, image->width, 255, 0, 0, 255, str);
	}
	else if (is_blocked)
	{
		if (-id == menu_core.index)
		{
			Text_DrawStr(image, fd, X_TEXT_START, Y_TEXT_START + (-id * y_step), 1, 1, 0, image->width, 56, 56, 56, 255, str);
		}
		else
		{
			Text_DrawStr(image, fd, X_TEXT_START, Y_TEXT_START + (-id * y_step), 1, 1, 0, image->width, 128, 128, 128, 255, str);
		}
	}
	else
	{
		Text_DrawStr(image, fd, X_TEXT_START, Y_TEXT_START + (id * y_step), 1, 1,0, image->width, 255, 255, 255, 255, str);
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

	Text_DrawStr(image, fd, X_HELPTEXT_START + menu_core.help_x, Y_HELPTEXT_START + menu_core.help_y, 0.5, 0.5, 0, image->width, 255, 255, 255, 255, str);

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
}

static void Menu_HandleLoadSave()
{
	if (Menu_CheckInput(GLFW_KEY_ENTER, GLFW_PRESS))
	{
		if (menu_core.sub_menu == SUB_MENU_LOAD)
		{
			Game_Load(menu_core.index);
			menu_core.sub_menu = SUB_MENU_MAIN;
		}
		else if (menu_core.sub_menu == SUB_MENU_SAVE)
		{
			SYSTEMTIME time;
			memset(&time, 0, sizeof(time));
			GetSystemTime(&time);

			char time_buf[32];
			memset(time_buf, 0, sizeof(time_buf));

			sprintf(time_buf, "%i-%i-%i-%i", time.wYear, time.wMonth, time.wDay, time.wHour);

			Game_Save(menu_core.index, time_buf);

			menu_core.sub_menu = SUB_MENU_MAIN;
		}
	}
}

void Menu_Update(float delta)
{
	if (menu_core.sub_menu == SUB_MENU_EXIT)
	{
		Game_SetState(GS__EXIT);
		return;
	}

	Menu_UpdateSprites(delta);

	if (menu_core.input_timer > 0) menu_core.input_timer -= delta;
	
	//handle sub menu
	if (menu_core.sub_menu == SUB_MENU_OPTIONS)
	{
		int step = 0;

		if (Menu_CheckInput(GLFW_KEY_LEFT, GLFW_PRESS)) step = -1;
		else if (Menu_CheckInput(GLFW_KEY_RIGHT, GLFW_PRESS)) step = 1;

		Menu_HandleOption(step);
	}
	else if (menu_core.sub_menu == SUB_MENU_LOAD || menu_core.sub_menu == SUB_MENU_SAVE)
	{
		Menu_HandleLoadSave();
	}

	//handle enter input when in submenu
	if (Menu_CheckInput(GLFW_KEY_ENTER, GLFW_PRESS))
	{
		switch (menu_core.sub_menu)
		{
		case SUB_MENU_MAIN:
		{
			if (menu_core.index == SUB_MENU_MAIN)
			{
				Game_SetState(GS__LEVEL);
				break;
			}
			else if (menu_core.index == SUB_MENU_LOAD || menu_core.index == SUB_MENU_SAVE)
			{
				//can't save when map isn't loaded
				if (menu_core.index == SUB_MENU_SAVE && Game_GetLevelIndex() < 0)
				{
					break;
				}

				Scan_SaveGames(menu_core.save_slots);
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
		{
			//go back to main screen
			menu_core.sub_menu = SUB_MENU_MAIN;
			break;
		}
			break;
		}

	}
	else if (Menu_CheckInput(GLFW_KEY_ESCAPE, GLFW_PRESS))
	{
		if (menu_core.sub_menu > 0) menu_core.sub_menu = SUB_MENU_MAIN;
		else if(menu_core.sub_menu == SUB_MENU_MAIN && Game_GetLevelIndex() >= 0)
		{
			//Game_SetState(GS__LEVEL);
		}
	}

	//get max index, we could do this by incrementing id in menu_text function
	//but it's called by render thread and that causes sync issues
	switch (menu_core.sub_menu)
	{
		case SUB_MENU_MAIN:
		{
			menu_core.max_index = SUB_MENU_EXIT;
			break;
		}
		case SUB_MENU_OPTIONS:
		{
			menu_core.max_index = BACK_ID;
			break;
		}
		case SUB_MENU_LOAD:
			//fallthrough
		case SUB_MENU_SAVE:
		{
			menu_core.max_index = MAX_SAVE_SLOTS - 1;
			break;
		}
		default:
		{
			menu_core.max_index = 0;
			break;
		}
	}

	
	if (Menu_CheckInput(GLFW_KEY_UP, GLFW_PRESS)) menu_core.index--;
	else if (Menu_CheckInput(GLFW_KEY_DOWN, GLFW_PRESS)) menu_core.index++;

	if (menu_core.index < 0) menu_core.index = menu_core.max_index;
	else if (menu_core.index > menu_core.max_index) menu_core.index = 0;

}

void Menu_Draw(Image* image, FontData* fd)
{
	Render_LockObjectMutex(false);

	int game_level = Game_GetLevelIndex();

	menu_core.id = 0;
	menu_core.help_x = 0;
	menu_core.help_y = 0;

	Menu_DrawBackGround(image);

	switch (menu_core.sub_menu)
	{
	case SUB_MENU_MAIN:
	{
		const float y_step = 0.1;

		if (game_level < 0) Menu_Text(image, fd, SUB_MENU_MAIN, y_step, "PLAY"); else Menu_Text(image, fd, SUB_MENU_MAIN, y_step, "RESUME");

		Menu_Text(image, fd, SUB_MENU_LOAD, y_step, "LOAD");

		if (game_level < 0) Menu_Text(image, fd, -SUB_MENU_SAVE, y_step, "SAVE"); else Menu_Text(image, fd, SUB_MENU_SAVE, y_step, "SAVE");
		Menu_Text(image, fd, SUB_MENU_OPTIONS, y_step, "OPTIONS");
		Menu_Text(image, fd, SUB_MENU_HELP, y_step, "HELP");
		Menu_Text(image, fd, SUB_MENU_EXIT, y_step, "EXIT");
		break;
	}
	case SUB_MENU_LOAD:
	{
		const float y_step = 0.1;
		for (int i = 0; i < MAX_SAVE_SLOTS; i++)
		{
			SaveSlot* slot = &menu_core.save_slots[i];

			if (slot->is_valid)
			{
				Menu_Text(image, fd, i, y_step, slot->name);
			}
			else
			{
				Menu_Text(image, fd, i, y_step, "EMPTY");
			}
		}
		break;
	}
	case SUB_MENU_SAVE:
	{
		const float y_step = 0.1;
		for (int i = 0; i < MAX_SAVE_SLOTS; i++)
		{
			SaveSlot* slot = &menu_core.save_slots[i];

			if (slot->is_valid)
			{
				Menu_Text(image, fd, i, y_step, slot->name);
			}
			else
			{
				Menu_Text(image, fd, i, y_step, "EMPTY");
			}
		}
		break;
	}
	case SUB_MENU_OPTIONS:
	{
		Menu_Text(image, fd, RENDERSCALE_ID, 0.25, "RENDER SCALE  < %i > ", Render_GetRenderScale());
		Menu_Text(image, fd, SOUND_ID, 0.25, "VOLUME  < %.1f > ", Sound_GetMasterVolume());
		Menu_Text(image, fd, SENSITIVITY_ID, 0.25, "SENSITIVITY  < %.1f > ", Player_GetSensitivity());

		Menu_Text(image, fd, BACK_ID, 0.25, "BACK");
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
		Menu_HelpText(image, fd, true, "TAB = VISUAL MAP");

		Menu_HelpText(image, fd, true, "1 = PISTOL");
		Menu_HelpText(image, fd, false, "2 = SHOTGUN");
		Menu_HelpText(image, fd, false, "3 = MACHINE GUN");
		Menu_HelpText(image, fd, false, "4 = DEVASTATOR");

		Menu_HelpText(image, fd, true, "F5 = QUICKSAVE");
		Menu_HelpText(image, fd, false, "F9 = QUICKLOAD");
		Menu_HelpText(image, fd, false, "F11 = FULLSCREEN ON/OFF");
		break;
	}
	default:
		break;
	}

	Render_UnlockObjectMutex(false);
}

void Menu_LevelEnd_Update(float delta, int secret_goal, int secret_max, int monster_goal, int monster_max)
{
	Menu_UpdateSprites(delta);

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

}
void Menu_LevelEnd_Draw(Image* image, FontData* fd)
{
	Render_LockObjectMutex(false);

	Menu_DrawBackGround(image);

	Text_Draw(image, fd, 0.25, 0.2, 1, 1, 0, image->width, "SECRETS %i/%i", (int)menu_core.secret_counter, menu_core.secret_max);
	Text_Draw(image, fd, 0.25, 0.5, 1, 1, 0, image->width, "KILLS %i/%i", (int)menu_core.monster_counter, menu_core.monster_max);
	Text_Draw(image, fd, 0.15, 0.7, 1, 1, 0, image->width, "PRESS ENTER TO CONTINUE");

	Render_UnlockObjectMutex(false);
}

void Menu_Finale_Update(float delta)
{
	Menu_UpdateSprites(delta);

	if (menu_core.input_timer > 0) menu_core.input_timer = 0;

	if (Menu_CheckInput(GLFW_KEY_ENTER, GLFW_PRESS))
	{
		Game_Reset(true);
		Game_SetState(GS__LEVEL);
	}
}

void Menu_Finale_Draw(Image* image, FontData* fd)
{
	Menu_DrawBackGround(image);
	
	Text_Draw(image, fd, 0.15, 0.2, 1, 1, 0, image->width, "THE END");
	Text_Draw(image, fd, 0.15, 0.5, 1, 1, 0, image->width, "THANKS FOR PLAYING");
	Text_Draw(image, fd, 0.15, 0.7, 1, 1, 0, image->width, "PRESS ENTER TO CONTINUE");
}

void Menu_DrawBackGround(Image* image)
{
	Video_DrawScreenTexture(image, &Game_GetAssets()->menu_texture, 0, 0, 1.5, 1.5);

	for (int i = 0; i < 2; i++)
	{
		Sprite* sprite = &menu_core.sprites[i];

		Video_DrawScreenSprite(image, sprite, 0, image->width);
	}
}

void Menu_Init()
{
	//setup sprites
	{
		GameAssets* assets = Game_GetAssets();
		ObjectInfo* obj_info = Info_GetObjectInfo(OT__THING, SUB__THING_SPINNING_PYRAMID);
		for (int i = 0; i < NUM_SPRITES; i++)
		{
			Sprite* sprite = &menu_core.sprites[i];
			AnimInfo* anim_info = &obj_info->anim_info;

			sprite->anim_speed_scale = obj_info->anim_speed;
			sprite->frame_count = anim_info->frame_count;
			sprite->looping = true;
			sprite->playing = true;
			sprite->img = &assets->object_textures;
			sprite->scale_x = 1;
			sprite->scale_y = 1;
			sprite->light.r = 255;
			sprite->light.g = 255;
			sprite->light.b = 255;
			sprite->frame_offset_x = anim_info->x_offset;
			sprite->frame_offset_y = anim_info->y_offset;
		}

		Sprite* sprite0 = &menu_core.sprites[0];
		Sprite* sprite1 = &menu_core.sprites[1];

		sprite0->x = 0.0;
		sprite0->y = 0.6;

		sprite1->x = 0.8;
		sprite1->y = 0.6;
	}
}