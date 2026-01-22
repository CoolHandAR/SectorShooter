#include "g_common.h"

#include <stdio.h>
#include <stdlib.h>

#include "utility.h"
#include "game_info.h"
#include "u_math.h"

#define SAVE_VERSION 1
#define SAVE_MAGIC 0xF0Cef0

#define LINE_LUMP 0
#define SECTOR_LUMP 1
#define OBJECT_LUMP 2
#define SECTOR_OBJECT_LUMP 3
#define PLAYER_LUMP 4
#define MAX_LUMPS 5

typedef struct
{
	int offset, size;
} SaveLump;

typedef struct
{
	int special;
	int flags;
} LineLump;

typedef struct
{
	int ceil;
	int floor;
	int object;
} SectorLump;

typedef struct
{
	int x, y, z;
	int dir_x, dir_y;
	int type;
	int sub_type;
	int hp;
	int state;
	int unique_id;
	int target_id;
	int sector_index;
	float stop_timer;
} ObjectLump;

typedef struct
{
	int x, y, z;
	int angle;
	int hp;
	int buck_ammo;
	int bullet_ammo;
	int rocket_ammo;
	int held_gun;
	bool gun_check[GUN__MAX];
} PlayerLump;

typedef struct
{
	int version;
	int magic;
	char name[SAVE_GAME_NAME_MAX_CHARS];
	char map_name[MAX_MAP_NAME];
	SaveLump lumps[MAX_LUMPS];
} SaveHeader;

static void GetSaveDirectory(WCHAR dest[MAX_PATH])
{
	//query working directory
	TCHAR dir_buffer[MAX_PATH];
	memset(dir_buffer, 0, sizeof(dir_buffer));

	DWORD d = GetCurrentDirectory(MAX_PATH, dir_buffer);
	if (d == 0)
	{
		return false;
	}

	for (int i = 0; i < sizeof(SAVEFOLDER) / sizeof(SAVEFOLDER[0]); i++)
	{
		if (d >= MAX_PATH)
		{
			break;
		}

		dir_buffer[d] = SAVEFOLDER[i];
		d++;
	}

	for (int i = 0; i < d; i++)
	{
		dest[i] = (WCHAR)dir_buffer[i];
	}
}
static void SetupSaveDirectory()
{
	//makes sure save folder is creating
	WCHAR save_dir[MAX_PATH];
	memset(&save_dir, 0, sizeof(save_dir));

	GetSaveDirectory(save_dir);

	CreateDirectory(save_dir, NULL);
}

static void* MallocLump(FILE* file, SaveHeader* header, int lump_num, int size, int* r_size)
{
	if (lump_num < 0 || lump_num >= MAX_LUMPS)
	{
		return NULL;
	}

	SaveLump* lump = &header->lumps[lump_num];

	int lump_size = Num_LittleLong(lump->size);
	int lump_pos = Num_LittleLong(lump->offset);

	if (lump_size <= 0 || lump_size < size)
	{
		return NULL;
	}

	void* data = malloc(lump_size);

	if (!data)
	{
		return NULL;
	}

	fseek(file, lump_pos, SEEK_SET);
	fread(data, lump_size, 1, file);

	if (r_size)
		*r_size = lump_size / size;

	return data;
}

static bool Save_Lump(FILE* file, SaveHeader* header, int lump_num, const void* data, int size)
{
	SaveLump* lump = &header->lumps[lump_num];

	lump->offset = Num_LittleLong(ftell(file));
	lump->size = Num_LittleLong(size);

	return File_Write(file, data, (size + 3) & ~3);
}

static bool Save_WorldState(FILE* file, SaveHeader* header, Map* map)
{
	LineLump* line_lump = calloc(map->num_linedefs, sizeof(LineLump));

	if (!line_lump)
	{
		return false;
	}

	for (int i = 0; i < map->num_linedefs; i++)
	{
		Linedef* line = &map->linedefs[i];
		LineLump* lump = &line_lump[i];

		lump->special = Num_LittleLong(line->special);
		lump->flags = Num_LittleLong(line->flags);
	}

	if (!Save_Lump(file, header, LINE_LUMP, line_lump, sizeof(LineLump) * map->num_linedefs))
	{
		free(line_lump);
		return false;
	}

	free(line_lump);

	SectorLump* sector_lump = calloc(map->num_sectors, sizeof(SectorLump));

	if (!sector_lump)
	{
		return false;
	}

	for (int i = 0; i < map->num_sectors; i++) 
	{
		Sector* sector = &map->sectors[i];
		SectorLump* lump = &sector_lump[i];
		
		lump->ceil = Num_LittleLong(sector->ceil);
		lump->floor = Num_LittleLong(sector->floor);
		lump->object = Num_LittleLong(sector->sector_object);
	}

	if (!Save_Lump(file, header, SECTOR_LUMP, sector_lump, sizeof(SectorLump) * map->num_sectors))
	{
		free(sector_lump);
		return false;
	}

	free(sector_lump);

	return true;
}
static bool Save_ObjState(FILE* file, SaveHeader* header, Map* map)
{
	ObjectLump* obj_lump = calloc(MAX_OBJECTS, sizeof(ObjectLump));

	if (!obj_lump)
	{
		return false;
	}

	int num = 0;
	for (int i = 0; i < MAX_OBJECTS; i++)
	{
		Object* obj = Map_GetObject(i);

		if (obj->type == OT__MONSTER || obj->type == OT__PICKUP)
		{
			ObjectLump* lump = &obj_lump[num++];

			lump->x = Num_LittleLong(obj->x);
			lump->y = Num_LittleLong(obj->y);
			lump->z = Num_LittleLong(obj->z);
			lump->dir_x = Num_LittleLong(obj->dir_x);
			lump->dir_y = Num_LittleLong(obj->dir_y);
			lump->state = Num_LittleLong(obj->state);
			lump->hp = Num_LittleLong(obj->hp);
			lump->type = Num_LittleLong(obj->type);
			lump->sub_type = Num_LittleLong(obj->sub_type);
			lump->unique_id = Num_LittleLong(obj->unique_id);

			Object* target = obj->target;

			if (target && target->hp > 0)
			{
				lump->target_id = Num_LittleLong(target->unique_id);
			}
			else
			{
				lump->target_id = Num_LittleLong(-1);
			}
		}
	}

	if (!Save_Lump(file, header, OBJECT_LUMP, obj_lump, sizeof(ObjectLump) * num))
	{
		free(obj_lump);
		return false;
	}

	free(obj_lump);

	return true;
}
static bool Save_SectorObjects(FILE* file, SaveHeader* header, Map* map)
{
	ObjectLump* obj_lump = calloc(MAX_OBJECTS, sizeof(ObjectLump));

	if (!obj_lump)
	{
		return false;
	}

	int num = 0;

	for (int i = 0; i < map->num_sectors; i++)
	{
		Sector* sector = &map->sectors[i];

		if (sector->sector_object < 0)
		{
			continue;
		}

		Object* obj = Map_GetObject(sector->sector_object);

		if (!obj)
		{
			continue;
		}

		if (obj->sector_index != sector->index)
		{
			continue;
		}
		
		ObjectLump* lump = &obj_lump[num++];
		
		lump->stop_timer = Num_LittleFloat(obj->stop_timer);
		lump->state = Num_LittleLong(obj->state);
		lump->type = Num_LittleLong(obj->type);
		lump->sub_type = Num_LittleLong(obj->sub_type);
		lump->sector_index = Num_LittleLong(obj->sector_index);
	}

	if (!Save_Lump(file, header, SECTOR_OBJECT_LUMP, obj_lump, sizeof(ObjectLump) * num))
	{
		free(obj_lump);
		return false;
	}

	free(obj_lump);

	return true;
}

static bool Save_PlayerState(FILE* file, SaveHeader* header, Map* map)
{
	PlayerLump* player_lump = calloc(1, sizeof(PlayerLump));

	if (!player_lump)
	{
		return false;
	}

	const PlayerData* pdata = Player_GetPlayerData();
	
	if (!pdata->obj)
	{
		return false;
	}

	player_lump->hp = Num_LittleLong(pdata->obj->hp);
	player_lump->x = Num_LittleLong(pdata->obj->x);
	player_lump->y = Num_LittleLong(pdata->obj->y);
	player_lump->z = Num_LittleLong(pdata->obj->z);
	player_lump->buck_ammo = Num_LittleLong(pdata->buck_ammo);
	player_lump->bullet_ammo = Num_LittleLong(pdata->bullet_ammo);
	player_lump->rocket_ammo = Num_LittleLong(pdata->rocket_ammo);
	player_lump->held_gun = Num_LittleLong(pdata->gun);

	for (int k = 0; k < GUN__MAX; k++)
	{
		player_lump->gun_check[k] = Num_LittleLong(pdata->gun_check[k]);
	}

	player_lump->angle = Num_LittleLong(Math_RadToDeg(pdata->angle));

	if (!Save_Lump(file, header, PLAYER_LUMP, player_lump, sizeof(PlayerLump) * 1))
	{
		free(player_lump);
		return false;
	}

	free(player_lump);

	return true;
}
bool Save_Game(const char* filename, const char name[SAVE_GAME_NAME_MAX_CHARS])
{
	SetupSaveDirectory();

	Map* map = Map_GetMap();

	SaveHeader header;
	memset(&header, 0, sizeof(header));

	header.version = SAVE_VERSION;
	header.magic = SAVE_MAGIC;
	strncpy(header.name, name, sizeof(header.name));
	strncpy(header.map_name, map->name, sizeof(header.map_name));

	FILE* file = fopen(filename, "wb");

	if (!file)
	{
		printf("Failed to save file \n");
		return false;
	}

	if (!File_Write(file, &header, sizeof(header)))
	{
		fclose(file);
		return false;
	}

	if (!Save_WorldState(file, &header, map))
	{
		fclose(file);
		return false;
	}
	if (!Save_SectorObjects(file, &header, map))
	{
		fclose(file);
		return false;
	}
	if (!Save_ObjState(file, &header, map))
	{
		fclose(file);
		return false;
	}
	if (!Save_PlayerState(file, &header, map))
	{
		fclose(file);
		return false;
	}

	fseek(file, 0, SEEK_SET);

	if (!File_Write(file, &header, sizeof(header)))
	{
		fclose(file);
		return false;
	}

	return fclose(file) == 0;
}

static void Load_ParseWorld(SaveHeader* header, FILE* file, Map* map)
{
	int num_lines = 0;
	LineLump* line_lump = MallocLump(file, header, LINE_LUMP, sizeof(LineLump), &num_lines);

	//probably should not happen, but clamp so it won't crash
	if (num_lines > map->num_linedefs)
	{
		num_lines = map->num_linedefs;
	}

	for (int i = 0; i < num_lines; i++)
	{
		Linedef* line = &map->linedefs[i];
		LineLump* lump = &line_lump[i];

		line->special = Num_LittleLong(lump->special);
		line->flags = Num_LittleLong(lump->flags);
	}

	if (line_lump) free(line_lump);
	
	int num_sectors = 0;
	SectorLump* sector_lump = MallocLump(file, header, SECTOR_LUMP, sizeof(SectorLump), &num_sectors);

	//probably should not happen, but clamp so it won't crash
	if (num_sectors > map->num_sectors)
	{
		num_sectors = map->num_sectors;
	}

	for (int i = 0; i < num_sectors; i++)
	{
		Sector* sector = &map->sectors[i];
		SectorLump* lump = &sector_lump[i];

		sector->ceil = Num_LittleLong(lump->ceil);
		sector->floor = Num_LittleLong(lump->floor);

		sector->r_floor = sector->floor;
		sector->r_ceil = sector->ceil;
	}

	if (sector_lump) free(sector_lump);
}

static void Load_ParseSectorObjects(SaveHeader* header, FILE* file, Map* map)
{
	int num_objects = 0;
	ObjectLump* obj_lump = MallocLump(file, header, SECTOR_OBJECT_LUMP, sizeof(ObjectLump), &num_objects);

	//probably should not happen, but clamp so it won't crash
	if (num_objects > MAX_OBJECTS)
	{
		num_objects = MAX_OBJECTS;
	}
	for (int i = 0; i < num_objects; i++)
	{
		ObjectLump* lump = &obj_lump[i];

		int sector_index = Num_LittleLong(lump->sector_index);
		int state = Num_LittleLong(lump->state);
		int type = Num_LittleLong(lump->type);
		int sub_type = Num_LittleLong(lump->sub_type);
		float stop_timer = Num_LittleFloat(lump->stop_timer);
		
		Event_CreateExistingSectorObject(type, sub_type, state, stop_timer, sector_index);
	}

	if (obj_lump) free(obj_lump);
}
static void Load_ParseObj(SaveHeader* header, FILE* file, Map* map)
{
	int num_objects = 0;
	ObjectLump* obj_lump = MallocLump(file, header, OBJECT_LUMP, sizeof(ObjectLump), &num_objects);

	//probably should not happen, but clamp so it won't crash
	if (num_objects > MAX_OBJECTS)
	{
		num_objects = MAX_OBJECTS;
	}

	for (int i = 0; i < num_objects; i++)
	{
		ObjectLump* lump = &obj_lump[i];
		
		int obj_unique_id = Num_LittleLong(lump->unique_id);

		Object* obj = Map_FindObjectByUniqueID(obj_unique_id);

		if (!obj)
		{
			continue;
		}

		//double check everything
		if (obj->type != Num_LittleLong(lump->type))
		{
			continue;
		}
		if (obj->sub_type != Num_LittleLong(lump->sub_type))
		{
			continue;
		}
		
		//it is fine, so copy everything
		obj->hp = Num_LittleLong(lump->hp);
		obj->state = Num_LittleLong(lump->state);
		obj->x = Num_LittleLong(lump->x);
		obj->y = Num_LittleLong(lump->y);
		obj->z = Num_LittleLong(lump->z);
		
		Sprite_ResetAnimState(&obj->sprite);

		//hack for dead objects
		if (obj->hp <= 0)
		{
			//just so that we won't see monsters dying after loading the map
			if (obj->type == OT__MONSTER)
			{
				obj->sprite.frame = 0x7ff;
			}
			else
			{
				Object_UnlinkSector(obj);
				obj->sprite.img = NULL;
			}
		}
		

		//setup target, if we have one
		int target_id = Num_LittleLong(lump->target_id);

		if (target_id >= 0)
		{
			Object* target = Map_FindObjectByUniqueID(target_id);

			if (target && target->hp > 0)
			{
				obj->target = target;
			}
		}

		//set position
		Move_SetPosition(obj, obj->x, obj->y, obj->z, obj->size);
	}

	if (obj_lump) free(obj_lump);
}

static void Load_ParsePlayer(SaveHeader* header, FILE* file, Map* map)
{
	int num_players = 0;
	PlayerLump* player_lump = MallocLump(file, header, PLAYER_LUMP, sizeof(PlayerLump), &num_players);
	if (!player_lump)
	{
		return;
	}

	PlayerData* pdata = Player_GetPlayerData();

	pdata->obj->x = Num_LittleLong(player_lump->x);
	pdata->obj->y = Num_LittleLong(player_lump->y);
	pdata->obj->z = Num_LittleLong(player_lump->z);
	pdata->obj->hp = Num_LittleLong(player_lump->hp);
	pdata->buck_ammo = Num_LittleLong(player_lump->buck_ammo);
	pdata->bullet_ammo = Num_LittleLong(player_lump->bullet_ammo);
	pdata->rocket_ammo = Num_LittleLong(player_lump->rocket_ammo);
	pdata->gun = Num_LittleLong(player_lump->held_gun);

	for (int k = 0; k < GUN__MAX; k++)
	{
		pdata->gun_check[k] = Num_LittleLong(player_lump->gun_check[k]);
	}
	
	pdata->view_x = pdata->obj->x;
	pdata->view_y = pdata->obj->y;
	pdata->view_z = pdata->obj->z;

	Player_Rotate(Math_DegToRad(Num_LittleLong(player_lump->angle)));
	Move_SetPosition(pdata->obj, pdata->obj->x, pdata->obj->y, pdata->obj->size);

	if (player_lump) free(player_lump);
}

bool Load_Game(const char* filename)
{
	Map* map = Map_GetMap();

	FILE* file = fopen(filename, "rb");

	if (!file)
	{
		return false;
	}

	SaveHeader header;
	memset(&header, 0, sizeof(header));

	if (!File_Read(file, &header, sizeof(header)))
	{
		fclose(file) == 0;
		return false;
	}

	if (header.version != SAVE_VERSION)
	{
		printf("Invalid Save File Version \n");

		fclose(file) == 0;
		return false;
	}
	if (header.magic != SAVE_MAGIC)
	{
		printf("Invalid Save File \n");

		fclose(file) == 0;
		return false;
	}

	int level_index = -1;
	for (int i = 0; i < sizeof(LEVELS) / sizeof(LEVELS[0]); i++)
	{
		if (!strcmp(LEVELS[i], header.map_name))
		{
			level_index = i;
			break;
		}
	}

	if (level_index == -1)
	{
		printf("Invalid map\n");
		fclose(file) == 0;
		return false;
	}

	//load the actual map
	Game_ChangeLevel(level_index, false);

	Load_ParseWorld(&header, file, map);
	Load_ParseObj(&header, file, map);
	Load_ParsePlayer(&header, file, map);
	Load_ParseSectorObjects(&header, file, map);

	return fclose(file) == 0;
}

bool Scan_SaveGames(SaveSlot slots[MAX_SAVE_SLOTS])
{
	SaveHeader header;
	memset(&header, 0, sizeof(header));

	char buffer[256];
	memset(&buffer, 0, sizeof(buffer));

	for (int i = 0; i < MAX_SAVE_SLOTS; i++)
	{
		sprintf(buffer, "saves/SAVE%i.sg", i);

		FILE* file = fopen(buffer, "rb");

		if (!file)
		{
			continue;
		}
		if (File_Read(file, &header, sizeof(header)))
		{
			if (header.version == SAVE_VERSION && header.magic == SAVE_MAGIC)
			{
				slots[i].is_valid = true;
				strncpy(slots[i].name, header.name, sizeof(slots[i].name));
			}
		}

		fclose(file);
	}


	return true;
}
