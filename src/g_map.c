#include "g_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <cjson/cJSON.h>
#include "u_math.h"
#include "game_info.h"

#include "utility.h"

static Map s_map;
static int CompareSpriteDistances(const void* a, const void* b)
{
	Sprite* arg1 = *(Sprite**)a;
	Sprite* arg2 = *(Sprite**)b;

	if (arg1->dist > arg2->dist) return -1;
	if (arg1->dist < arg2->dist) return 1;

	return 0;
}

static void Map_ClearTempLight()
{
	Render_LockThreadsMutex();

	

	Render_UnlockThreadsMutex();
	Render_RedrawWalls();
}
static void Map_UpdateSortedList()
{
	int index = 0;
	int max_index = 0;

	for (int i = 0; i < s_map.num_objects; i++)
	{
		Object* obj = &s_map.objects[i];

		if (obj->type == OT__NONE
			|| obj->type == OT__PLAYER)
		{
			continue;
		}

		s_map.sorted_list[index++] = i;

		if (i > max_index)
		{
			max_index = i;
		}
	}

	//s_map.num_objects = max_index + 1;
	s_map.num_sorted_objects = index;
}

static void Map_UpdateObjectTilemap()
{
	
}
static void Map_ConnectTriggersToTargets()
{
	for (int i = 0; i < s_map.num_objects; i++)
	{
		Object* obj = &s_map.objects[i];

		if (obj->type != OT__TRIGGER)
		{
			continue;
		}

		int target_id = (int)obj->target;
		obj->target = NULL;

		for (int k = 0; k < s_map.num_objects; k++)
		{
			Object* target_obj = &s_map.objects[k];

			if (target_obj->type == OT__TARGET || target_obj->type == OT__DOOR)
			{
				
			}
		}

		Object* t = obj->target;

		if (t && t->sub_type == SUB__TARGET_TELEPORT)
		{
			GameAssets* assets = Game_GetAssets();

			ObjectInfo* object_info = Info_GetObjectInfo(OT__TARGET, SUB__TARGET_TELEPORT);

			obj->sprite.img = &assets->object_textures;
			obj->sprite.frame_count = object_info->anim_info.frame_count;
			obj->sprite.anim_speed_scale = object_info->anim_speed;
			obj->sprite.playing = true;
			obj->sprite.looping = object_info->anim_info.looping;
			obj->sprite.offset_x = object_info->sprite_offset_x;
			obj->sprite.offset_y = object_info->sprite_offset_y;
			obj->sprite.frame_offset_y = object_info->anim_info.y_offset;
			obj->sprite.frame_offset_x = object_info->anim_info.x_offset;
			obj->sprite.light = 1;
		}
	}
}

static float calc_light_attenuation(float distance, float inv_range, float decay)
{
	float nd = distance * inv_range;
	nd *= nd;
	nd *= nd;
	nd = max(1.0 - nd, 0.0);
	nd *= nd;
	return nd * pow(max(distance, 0.0001), -decay);
}

static void Map_CalculateLight(int light_x, int light_y, int tile_x, int tile_y, float radius, float attenuation, float scale, bool temp_light)
{
	
}

static void SetLightTileCallback(int center_x, int center_y, int x, int y)
{
	Object* light_obj = Map_GetObjectAtTile(center_x, center_y);

	float radius = 6;
	float attenuation = 1;
	float scale = 1;

	if (light_obj && light_obj->type == OT__LIGHT)
	{
		LightInfo* light_info = Info_GetLightInfo(light_obj->sub_type);

		if (light_info)
		{
			radius = light_info->radius;
			attenuation = light_info->attenuation;
			scale = light_info->scale;
		}
	}

	Map_CalculateLight(x, y, center_x, center_y, radius, attenuation, scale, false);
}
static void SetTempLightTileCallback(int center_x, int center_y, int x, int y)
{
	Map_CalculateLight(x, y, center_x, center_y, 4.5, 1, 0.5, true);
}

static void Map_SetupLightTiles()
{
	
}

static void Map_FreeListStoreID(ObjectID id)
{
	if (s_map.num_free_list >= MAX_OBJECTS)
	{
		return;
	}

	s_map.free_list[s_map.num_free_list++] = id;
}

static ObjectID Map_GetNewObjectIndex()
{
	ObjectID id = -1;
	//get from free list
	if (s_map.num_free_list > 0)
	{
		id = s_map.free_list[s_map.num_free_list - 1];
		s_map.num_free_list--;
	}
	else
	{
		if (s_map.num_objects < MAX_OBJECTS)
		{
			id = s_map.num_objects++;
		}
	}

	return id;
}

void Map_SetDirtyTempLight()
{
	s_map.dirty_temp_light = true;
}

int Map_GetLevelIndex()
{
	return s_map.level_index;
}

Map* Map_GetMap()
{
	return &s_map;
}

Object* Map_NewObject(ObjectType type)
{
	ObjectID index = Map_GetNewObjectIndex();

	if (index < 0)
	{
		return NULL;
	}

	Object* obj = &s_map.objects[index];

	obj->sprite.scale_x = 1;
	obj->sprite.scale_y = 1;
	obj->id = index;
	obj->type = type;
	obj->hp = 1;
	obj->size = 0.5;

	Map_UpdateSortedList();

	return obj;
}

bool Map_LoadFromIndex(int index)
{
	int arr_size = sizeof(LEVELS) / sizeof(LEVELS[0]);
	if (index >= arr_size)
	{
		index = arr_size - 1;
	}
	else if (index < 0)
	{
		index = 0;
	}

	if (!Map_Load(LEVELS[index]))
	{
		return false;
	}

	s_map.level_index = index;

	return true;
}

bool Map_Load(const char* filename)
{
	Map_Destruct();

	//Load_BuildMap("newboard.map", &s_map);
	Load_DoomIWAD("DOOM.WAD", &s_map);
	Load_Doommap("test.wad", &s_map);	
	//LoadData();

	return true;
}

TileID Map_GetTile(int x, int y)
{
	

	return 0;
}

TileID Map_GetFloorTile(int x, int y)
{

	return 0;
}

TileID Map_GetCeilTile(int x, int y)
{


	return 0;
}

Object* Map_GetObject(ObjectID id)
{
	assert((id >= 0 && id < MAX_OBJECTS) && "Invalid Object ID");

	return &s_map.objects[id];
}

Object* Map_GetObjectAtTile(int x, int y)
{

	return 0;
}


void Map_SetTempLight(int x, int y, int size, int light)
{
	Trace_ShadowCast(x, y, size, SetTempLightTileCallback);
}

TileID Map_Raycast(float p_x, float p_y, float dir_x, float dir_y, float* r_hitX, float* r_hitY)
{
	

	return 0;
}


void Map_GetSize(int* r_width, int* r_height)
{
	if (r_width)
	{
		//*r_width = s_map.width;
	}
	if (r_height)
	{
		//*r_height = s_map.height;
	}
}

void Map_GetSpawnPoint(int* r_x, int* r_y, int* r_z, int* r_sector, float* r_rot)
{
	if (r_x)
	{
		*r_x = s_map.player_spawn_point_x;
	}
	if (r_y)
	{
		*r_y = s_map.player_spawn_point_y;
	}
	if (r_z)
	{
		*r_z = s_map.player_spawn_point_z;
	}
	if (r_sector)
	{
		*r_sector = s_map.player_spawn_sector;
	}
	if (r_rot)
	{
		*r_rot = s_map.player_spawn_rot;
	}

}
bool Map_UpdateObjectTile(Object* obj)
{
	return true;

	if (obj->x < 0 || obj->y < 0)
	{
		return false;
	}

	if (obj->type == OT__PARTICLE)
	{
		return true;
	}

	int total_tiles = 0;

	int tile_x = (int)obj->x;
	int tile_y = (int)obj->y;

	int index = tile_x + tile_y;

	if (index >= total_tiles || index < 0)
	{
		return false;
	}

	ObjectID id = s_map.object_tiles[index];

	if (id != NULL_INDEX)
	{
		Object* tile_obj = &s_map.objects[id];

		if (tile_obj != obj)
		{
			//if we are a missile don't take take anyone's place
			if (obj->type == OT__MISSILE)
			{
				return true;
			}

			if (tile_obj->type == OT__PLAYER || tile_obj == OT__MONSTER || tile_obj->type == OT__THING)
			{
				return false;
			}

			//return true, but don't take the spot
			if (tile_obj->type == OT__DOOR || tile_obj->type == OT__TRIGGER || tile_obj->type == OT__SPECIAL_TILE || tile_obj->type == OT__PICKUP)
			{
				return true;
			}
			
		}
	}


	s_map.object_tiles[index] = obj->id;

	return true;
}


void Map_DrawObjects(Image* image, float* depth_buffer, DrawSpan* draw_spans, float p_x, float p_y, float p_dirX, float p_dirY, float p_planeX, float p_planeY)
{
	for (int i = 0; i < s_map.num_sorted_objects; i++)
	{
		ObjectID id = s_map.sorted_list[i];
		Object* object = &s_map.objects[id];

		if (object->type == OT__NONE || !object->sprite.img || object->sprite.skip_draw == true)
		{
			continue;
		}

		if (object->type != OT__THING)
		{
			continue;
		}

		Sprite* sprite = &object->sprite;

		Render_AddSpriteToQueue(sprite);
	}
}

void Map_UpdateObjects(float delta)
{
	float view_x, view_y, dir_x, dir_y, dir_z, plane_x, plane_y;
	Player_GetView(&view_x, &view_y, &dir_x, &dir_y, &plane_x, &plane_y);

	float inv_det = 1.0 / (plane_x * dir_y - dir_x * plane_y);

	for (int i = 0; i < s_map.num_sorted_objects; i++)
	{
		ObjectID id = s_map.sorted_list[i];
		if (id < 0)
		{
			continue;
		}

		Object* obj = &s_map.objects[id];

		obj->sprite.skip_draw = false;

		if (obj->type == OT__NONE)
		{
			continue;
		}

		switch (obj->type)
		{
		case OT__MONSTER:
		{
			Monster_Update(obj, delta);
			break;
		}
		case OT__MISSILE:
		{
			Missile_Update(obj, delta);
			break;
		}
		case OT__DOOR:
		{
			Move_Door(obj, delta);
			break;
		}
		case OT__PARTICLE:
		{
			Particle_Update(obj, delta);
			break;
		}
		//fallthrough
		case OT__TRIGGER:
		case OT__PICKUP:
		case OT__THING:
		case OT__LIGHT:
		{
			if (obj->sprite.img && obj->sprite.frame_count > 0 && obj->sprite.anim_speed_scale > 0)
			{
				Sprite_UpdateAnimation(&obj->sprite, delta);
			}
			break;
		}
		default:
			break;
		}

		if (!obj->sprite.img)
		{
			continue;
		}

		obj->sprite.x = obj->x + obj->sprite.offset_x;
		obj->sprite.y = obj->y + obj->sprite.offset_y;
		obj->sprite.z = obj->z + 64;
		
	}
}

void Map_DeleteObject(Object* obj)
{
	if (obj->spatial_id >= 0)
	{
		BVH_Tree_Remove(&s_map.spatial_tree, obj->spatial_id);
	}

	Render_LockObjectMutex();

	Object_UnlinkSector(obj);

	ObjectID id = obj->id;

	assert(id < MAX_OBJECTS);

	//reset the object
	memset(obj, 0, sizeof(Object));

	//store the id in free list
	Map_FreeListStoreID(id);

	//update sorted list
	Map_UpdateSortedList();

	Render_UnlockObjectMutex();
}

Subsector* Map_FindSubsector(float p_x, float p_y)
{
	int nodenum = s_map.num_nodes - 1;

	while (!(nodenum & MF__NODE_SUBSECTOR))
	{
		BSPNode* node = &s_map.bsp_nodes[nodenum];
		int side = BSP_GetNodeSide(node, p_x, p_y);
		nodenum = node->children[side];
	}

	return &s_map.sub_sectors[nodenum & ~MF__NODE_SUBSECTOR];
}

Sector* Map_FindSector(float p_x, float p_y)
{
	return Map_FindSubsector(p_x, p_y)->sector;
}

void Map_Destruct()
{
	//keep old level index
	int old_level_index = s_map.level_index;

	if (s_map.object_tiles) free(s_map.object_tiles);

	if (s_map.line_segs) free(s_map.line_segs);
	if (s_map.bsp_nodes) free(s_map.bsp_nodes);
	if (s_map.sectors) free(s_map.sectors);
	if (s_map.sub_sectors) free(s_map.sub_sectors);
	if (s_map.sidedefs) free(s_map.sidedefs);
	
	BVH_Tree_Destruct(&s_map.spatial_tree);

	memset(&s_map, 0, sizeof(s_map));

	s_map.level_index = old_level_index;
}
