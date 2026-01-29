#include "g_common.h"

#include <stdio.h>

#include "game_info.h"
#include "main.h"
#include "sound.h"

#define START_LEVEL 2

static Game game;
static GameAssets assets;

Game* Game_GetGame()
{
	return &game;
}

bool Game_Init()
{
	memset(&game, 0, sizeof(game));

	if (!Game_LoadAssets())
	{
		return false;
	}
	
	Game_SetState(GS__MENU);

	game.level_index = -1;
	VisualMap_Init();
	Menu_Init();

	//Sound_SetAsMusic(SOUND__MUSIC1);

	return true;
}

void Game_Exit()
{
	Map_Destruct();

	Game_DestructAssets();
}

bool Game_LoadAssets()
{
	memset(&assets, 0, sizeof(assets));

	if (!Load_DoomIWAD("assets/OURWAD.wad"))
	{
		return false;
	}

	if (!Image_CreateFromPath(&assets.object_textures, "assets/textures/object_sheet.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.shotgun_texture, "assets/textures/shaker.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.machinegun_texture, "assets/textures/machine.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.devastator_texture, "assets/textures/devastator.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.pistol_texture, "assets/textures/pistol.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.imp_texture, "assets/textures/blood_imp.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.missile_textures, "assets/textures/missile_sheet.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.pinky_texture, "assets/textures/pinky_sheet.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.particle_textures, "assets/textures/particle_sheet.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.decal_textures, "assets/textures/decal_sheet.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.menu_texture, "assets/textures/menu.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.bruiser_texture, "assets/textures/bruiser.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.templar_texture, "assets/textures/templar.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.missing_texture.img, "assets/textures/missing_texture.png"))
	{
		return false;
	}

	assets.missing_texture.width_mask = assets.missing_texture.img.width - 1;
	assets.missing_texture.height_mask = assets.missing_texture.img.height - 1;
	strcpy(assets.missing_texture.name, "MISS");

	assets.object_textures.h_frames = 4;
	assets.object_textures.v_frames = 22;

	assets.shotgun_texture.h_frames = 18;
	assets.shotgun_texture.v_frames = 1;

	assets.machinegun_texture.h_frames = 3;
	assets.machinegun_texture.v_frames = 1;

	assets.pistol_texture.h_frames = 5;
	assets.pistol_texture.v_frames = 1;

	assets.devastator_texture.h_frames = 7;
	assets.devastator_texture.v_frames = 2;

	assets.missile_textures.h_frames = 5;
	assets.missile_textures.v_frames = 4;

	assets.imp_texture.h_frames = 9;
	assets.imp_texture.v_frames = 9;

	assets.pinky_texture.h_frames = 8;
	assets.pinky_texture.v_frames = 6;

	assets.bruiser_texture.h_frames = 12;
	assets.bruiser_texture.v_frames = 7;

	assets.templar_texture.h_frames = 10;
	assets.templar_texture.v_frames = 7;

	assets.particle_textures.h_frames = 4;
	assets.particle_textures.v_frames = 7;

	assets.decal_textures.h_frames = 5;
	assets.decal_textures.v_frames = 3;

	Image_GenerateFrameInfo(&assets.object_textures);
	Image_GenerateFrameInfo(&assets.shotgun_texture);
	Image_GenerateFrameInfo(&assets.imp_texture);
	Image_GenerateFrameInfo(&assets.missile_textures);
	Image_GenerateFrameInfo(&assets.pinky_texture);
	Image_GenerateFrameInfo(&assets.bruiser_texture);
	Image_GenerateFrameInfo(&assets.templar_texture);
	Image_GenerateFrameInfo(&assets.particle_textures);
	Image_GenerateFrameInfo(&assets.decal_textures);
	Image_GenerateFrameInfo(&assets.machinegun_texture);
	Image_GenerateFrameInfo(&assets.pistol_texture);
	Image_GenerateFrameInfo(&assets.devastator_texture);

	return true;
}

void Game_DestructAssets()
{
	Image_Destruct(&assets.object_textures);
	Image_Destruct(&assets.shotgun_texture);
	Image_Destruct(&assets.imp_texture);
	Image_Destruct(&assets.missile_textures);
	Image_Destruct(&assets.pinky_texture);
	Image_Destruct(&assets.bruiser_texture);
	Image_Destruct(&assets.templar_texture);
	Image_Destruct(&assets.particle_textures);
	Image_Destruct(&assets.decal_textures);
	Image_Destruct(&assets.menu_texture);
	Image_Destruct(&assets.machinegun_texture);
	Image_Destruct(&assets.pistol_texture);
	Image_Destruct(&assets.devastator_texture);
	Image_Destruct(&assets.missing_texture);

	for (int i = 0; i < assets.num_flat_textures; i++)
	{
		Texture* t = &assets.flat_textures[i];

		Image_Destruct(&t->img);
	}

	if (assets.flat_textures) free(assets.flat_textures);

	for (int i = 0; i < assets.num_patchy_textures; i++)
	{
		Texture* t = &assets.patchy_textures[i];

		Image_Destruct(&t->img);
	}
	if (assets.patchy_textures) free(assets.patchy_textures);

	for (int i = 0; i < assets.num_png_texures; i++)
	{
		Texture* t = &assets.png_textures[i];

		Image_Destruct(&t->img);
	}
	if (assets.png_textures) free(assets.png_textures);
}

void Game_NewTick()
{
	game.tick++;
}

int Game_GetTick()
{
	return game.tick;
}

void Game_LogicUpdate(double delta)
{
	if (game.status_msg_timer > 0)
	{
		game.status_msg_timer -= delta;
	}

	GLFWwindow* window = Engine_GetWindow();

	switch (game.state)
	{
	case GS__MENU:
	{
		break;
	}
	case GS__LEVEL:
	{
		//no map loaded? load the first map
		if (game.level_index < 0)
		{
			Game_ChangeLevel(START_LEVEL, false);
			break;
		}

		Player_Update(window, delta);
		Map_UpdateObjects(delta);

		break;
	}
	case GS__LEVEL_END:
	{
		break;
	}
	case GS__FINALE:
	{
		break;
	}
	default:
		break;
	}

}

void Game_SmoothUpdate(double lerp, double delta)
{
	GLFWwindow* window = Engine_GetWindow();

	switch (game.state)
	{
	case GS__MENU:
	{
		Menu_Update(delta);
		break;
	}
	case GS__LEVEL:
	{
		//no map loaded? load the first map
		if (game.level_index < 0)
		{
			Game_ChangeLevel(START_LEVEL, false);
			break;
		}

		Render_LockObjectMutex(true);

		Player_LerpUpdate(lerp, delta);
		Map_SmoothUpdate(lerp, delta);
		VisualMap_Update(window, delta);

		Render_UnlockObjectMutex(true);
		break;
	}
	case GS__LEVEL_END:
	{
		Menu_LevelEnd_Update(delta, game.prev_secrets_found, game.prev_total_secrets, game.prev_monsters_killed, game.prev_total_monsters);
		break;
	}
	case GS__FINALE:
	{
		Menu_Finale_Update(delta);
		break;
	}
	default:
		break;
	}
}

void Game_Draw(Image* image, FontData* fd)
{
	switch (game.state)
	{
	case GS__MENU:
	{
		Menu_Draw(image, fd);
		break;
	}
	case GS__LEVEL:
	{
		if (game.level_index < 0)
		{
			break;
		}
		//level rendering and sprite rendering is handled by the renderer
		VisualMap_Draw(image, fd);

		break;
	}
	case GS__LEVEL_END:
	{
		Menu_LevelEnd_Draw(image, fd);
		break;
	}
	case GS__FINALE:
	{
		Menu_Finale_Draw(image, fd);
		break;
	}
	default:
		break;
	}
}

void Game_DrawHud(Image* image, FontData* fd, int start_x, int end_x)
{
	switch (game.state)
	{
	case GS__LEVEL:
	{
		if (game.level_index < 0)
		{
			break;
		}

		//draw player stuff (gun and hud) and some shader stuff
		Player_DrawHud(image, fd, start_x, end_x);

		break;
	}
	default:
		break;
	}

	if (game.status_msg_timer > 0)
	{
		Text_DrawColor(image, fd, 0.05, 0.1, 0.5, 0.5, start_x, end_x, 232, 0, 0, 255, game.status_msg);
	}
}

void Game_SetState(GameState state)
{
	Render_FinishAndStall();

	game.state = state;

	Render_Resume();
}

GameState Game_GetState()
{
	return game.state;
}

GameAssets* Game_GetAssets()
{
	return &assets;
}

bool Game_ChangeLevel(int level_index, bool player_keep_stuff)
{
	//stall render threads
	Render_FinishAndStall();

	//clear screen
	Render_Clear(0);

	game.prev_total_monsters = game.total_monsters;
	game.prev_total_secrets = game.total_secrets;
	game.prev_secrets_found = game.secrets_found;
	game.prev_monsters_killed = game.monsters_killed;

	game.total_secrets = 0;
	game.total_monsters = 0;
	game.secrets_found = 0;
	game.monsters_killed = 0;

	int arr_size = sizeof(LEVELS) / sizeof(LEVELS[0]);

	//finale
	if (level_index >= arr_size)
	{
		Game_SetState(GS__FINALE);
	}
	else
	{
		//flush sound system
		Sound_FlushAll();

		const char* level = LEVELS[level_index];
		const char* sky = SKIES[level_index];
		LightCompilerInfo* lci = Info_GetLightCompilerInfo(level_index);

		printf("Setting Level to %s\n", level);

		if (!Map_Load(level, sky, lci))
		{
			Render_Resume();
			return false;
		}

		Map* map = Map_GetMap();
		printf("Num Linedefs: %i, Num Sectors %i, Num Objects %i \n", map->num_linedefs, map->num_sectors, map->num_objects);

		Player_Init(player_keep_stuff);

	}

	game.level_index = level_index;

	//resume
	Render_Resume();

	return true;
}

void Game_NextLevel()
{
	Game_ChangeLevel(game.level_index + 1, true);
	Game_SetState(GS__LEVEL_END);
}

void Game_Save(int slot, char desc[32])
{
	if (slot < 0 || slot >= MAX_SAVE_SLOTS || game.level_index < 0)
	{
		return false;
	}

	//slot name
	char slot_name[32];
	memset(slot_name, 0, sizeof(slot_name));

	sprintf(slot_name, "saves/SAVE%i.sg", slot);

	if (!Save_Game(slot_name, desc))
	{
		return false;
	}

	printf("Save Game Saved\n");
	Game_SetStatusMessage("GAME SAVED");

	return true;
}

bool Game_Load(int slot)
{
	if (slot < 0 || slot >= MAX_SAVE_SLOTS)
	{
		return false;
	}

	//slot name
	char slot_name[32];
	memset(slot_name, 0, sizeof(slot_name));

	sprintf(slot_name, "saves/SAVE%i.sg", slot);

	if (!Load_Game(slot_name))
	{
		return false;
	}

	printf("Save Game Loaded\n");
	Game_SetStatusMessage("GAME LOADED");

	return true;
}

int Game_GetLevelIndex()
{
	return game.level_index;
}

void Game_SetStatusMessage(char msg[MAX_STATUS_MESSAGE])
{
	strncpy(game.status_msg, msg, sizeof(game.status_msg));

	game.status_msg_timer = STATUS_TIME;
}

void Game_Reset(bool to_start)
{
	game.total_secrets = 0;
	game.total_monsters = 0;
	game.secrets_found = 0;
	game.monsters_killed = 0;

	int index = game.level_index;

	if (to_start)
	{
		index = 0;
	}

	Game_ChangeLevel(index, false);
}

void Game_SecretFound()
{
	Sound_Emit(SOUND__SECRET_FOUND, 0.025);
	
	Game_SetStatusMessage("SECRET FOUND!");

	game.secrets_found++;
}

Texture* Game_FindTextureByName(const char p_name[8])
{
	//make sure the str is null terminated
	unsigned char name[10];
	memset(name, 0, sizeof(name));

	strncpy(name, p_name, 8);

	//no texture
	if (!strncmp(name, "-", 8))
	{
		return NULL;
	}

	//look for flat textures
	for (int i = 0; i < assets.num_flat_textures; i++)
	{
		Texture* texture = &assets.flat_textures[i];

		if (!_stricmp(texture->name, name))
		{
			return texture;
		}
	}

	//look for patchy textures
	for (int i = 0; i < assets.num_patchy_textures; i++)
	{
		Texture* texture = &assets.patchy_textures[i];

		if (!_stricmp(texture->name, name))
		{
			return texture;
		}
	}

	//look for png textures
	for (int i = 0; i < assets.num_png_texures; i++)
	{
		Texture* texture = &assets.png_textures[i];

		if (!_stricmp(texture->name, name))
		{
			return texture;
		}
	}

	//return pink and black stripe texture
	return &assets.missing_texture;
}

