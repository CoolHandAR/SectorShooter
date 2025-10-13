#include "g_common.h"

#include "game_info.h"
#include "main.h"
#include "sound.h"
#include <stdio.h>

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

	//load the first map
	if (!Map_LoadFromIndex(0))
	{
		printf("ERROR:: Failed to load map !\n");
		return false;
	}

	Player_Init(false);

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

	if (!Image_CreateFromPath(&assets.wall_textures, "assets/textures/walls.png"))
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
	if (!Image_CreateFromPath(&assets.menu_texture, "assets/textures/menu.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.bruiser_texture, "assets/textures/bruiser.png"))
	{
		return false;
	}
	if (!Image_CreateFromPath(&assets.missing_texture, "assets/textures/missing_texture.png"))
	{
		return false;
	}

	assets.wall_textures.h_frames = 7;
	assets.wall_textures.v_frames = 1;

	assets.object_textures.h_frames = 4;
	assets.object_textures.v_frames = 8;

	assets.shotgun_texture.h_frames = 18;
	assets.shotgun_texture.v_frames = 1;

	assets.machinegun_texture.h_frames = 3;
	assets.machinegun_texture.v_frames = 1;

	assets.pistol_texture.h_frames = 5;
	assets.pistol_texture.v_frames = 1;

	assets.devastator_texture.h_frames = 7;
	assets.devastator_texture.v_frames = 2;

	assets.imp_texture.h_frames = 9;
	assets.imp_texture.v_frames = 9;

	assets.missile_textures.h_frames = 5;
	assets.missile_textures.v_frames = 2;

	assets.pinky_texture.h_frames = 8;
	assets.pinky_texture.v_frames = 6;

	assets.bruiser_texture.h_frames = 12;
	assets.bruiser_texture.v_frames = 7;

	assets.particle_textures.h_frames = 4;
	assets.particle_textures.v_frames = 2;

	Image_GenerateFrameInfo(&assets.wall_textures);
	Image_GenerateFrameInfo(&assets.object_textures);
	Image_GenerateFrameInfo(&assets.shotgun_texture);
	Image_GenerateFrameInfo(&assets.imp_texture);
	Image_GenerateFrameInfo(&assets.missile_textures);
	Image_GenerateFrameInfo(&assets.pinky_texture);
	Image_GenerateFrameInfo(&assets.bruiser_texture);
	Image_GenerateFrameInfo(&assets.particle_textures);
	Image_GenerateFrameInfo(&assets.machinegun_texture);
	Image_GenerateFrameInfo(&assets.pistol_texture);
	Image_GenerateFrameInfo(&assets.devastator_texture);

	Image_GenerateMipmaps(&assets.wall_textures);

	return true;
}

void Game_DestructAssets()
{
	Image_Destruct(&assets.wall_textures);
	Image_Destruct(&assets.object_textures);
	Image_Destruct(&assets.shotgun_texture);
	Image_Destruct(&assets.imp_texture);
	Image_Destruct(&assets.missile_textures);
	Image_Destruct(&assets.pinky_texture);
	Image_Destruct(&assets.bruiser_texture);
	Image_Destruct(&assets.particle_textures);
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
}

void Game_Update(float delta)
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
		Player_Update(window, delta);
		Map_UpdateObjects(delta);

		if (game.secret_timer > 0) game.secret_timer -= delta;
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

void Game_Draw(Image* image, FontData* fd, float* depth_buffer, DrawSpan* draw_spans, float p_x, float p_y, float p_dirX, float p_dirY, float p_planeX, float p_planeY)
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
		//sort and draw map objects
		Map_DrawObjects(image, depth_buffer, draw_spans, p_x, p_y, p_dirX, p_dirY, p_planeX, p_planeY);

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

void Game_DrawHud(Image* image, FontData* fd)
{
	//only used for level state rn

	switch (game.state)
	{
	case GS__LEVEL:
	{
		//draw player stuff (gun and hud)
		Player_Draw(image, fd);

		if (game.secret_timer > 0)
		{
			Text_DrawColor(image, fd, 0.25, 0.2, 1, 1, 255, 255, 255, 255, "SECRET FOUND!");
		}

		break;
	}
	default:
		break;
	}
}

void Game_SetState(GameState state)
{
	Render_FinishAndStall();

	if (state != game.state)
	{
		Render_RedrawWalls();
		Render_RedrawSprites();
	}

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

void Game_ChangeLevel()
{
	//stall render threads
	Render_FinishAndStall();

	game.prev_total_monsters = game.total_monsters;
	game.prev_total_secrets = game.total_secrets;
	game.prev_secrets_found = game.secrets_found;
	game.prev_monsters_killed = game.monsters_killed;

	game.total_secrets = 0;
	game.total_monsters = 0;
	game.secrets_found = 0;
	game.monsters_killed = 0;

	Map* map = Map_GetMap();

	map->level_index++;

	int arr_size = sizeof(LEVELS) / sizeof(LEVELS[0]);

	//finale
	if (map->level_index >= arr_size)
	{
		Game_SetState(GS__FINALE);
	}
	else
	{
		const char* level = LEVELS[map->level_index];

		Map_Load(level);
		Player_Init(true);

		Game_SetState(GS__LEVEL_END);
	}

	//resume
	Render_Resume();
}

void Game_Reset(bool to_start)
{
	//stall render threads
	Render_FinishAndStall();

	game.total_secrets = 0;
	game.total_monsters = 0;
	game.secrets_found = 0;
	game.monsters_killed = 0;

	int index = 0;

	if (!to_start)
	{
		index = Map_GetLevelIndex();
	}

	const char* level = LEVELS[index];

	Map_Load(level);

	Player_Init(false);

	Render_RedrawWalls();
	Render_RedrawSprites();

	//resume
	Render_Resume();
}

void Game_SecretFound()
{
	Sound_Emit(SOUND__SECRET_FOUND, 0.25);

	game.secret_timer = 1;
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

	return NULL;
}

