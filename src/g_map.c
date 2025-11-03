#include "g_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <cjson/cJSON.h>
#include "u_math.h"
#include "game_info.h"
#include "main.h"
#include "utility.h"

static Map s_map;

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
	obj->spatial_id = -1;
	obj->sector_index = -1;
	obj->sector_next = NULL;
	obj->sector_prev = NULL;
	obj->unique_id = s_map.unique_id_index++;

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


	return true;
}

bool Map_Load(const char* filename)
{
	Map_Destruct();

	Load_Doommap(filename, &s_map);

	return true;
}

Object* Map_GetObject(ObjectID id)
{
	assert((id >= 0 && id < MAX_OBJECTS) && "Invalid Object ID");

	if (id < 0 || id >= MAX_OBJECTS)
	{
		return NULL;
	}

	return &s_map.objects[id];
}

Object* Map_FindObjectByUniqueID(int unique_id)
{
	//lazy search but, probably will only be used for loading save file
	for (int i = 0; i < MAX_OBJECTS; i++)
	{
		Object* obj = Map_GetObject(i);

		if (obj->unique_id == unique_id)
		{
			return obj;
		}
	}

	return NULL;
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

void Map_UpdateObjects(float delta)
{
	for (int i = 0; i < s_map.num_sorted_objects; i++)
	{
		ObjectID id = s_map.sorted_list[i];
		if (id < 0)
		{
			continue;
		}

		Object* obj = &s_map.objects[id];

		if (obj->type == OT__NONE)
		{
			continue;
		}

		//always make sure to store prev position
		obj->prev_x = obj->x;
		obj->prev_y = obj->y;
		obj->prev_z = obj->z;

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
			Door_Update(obj, delta);
			break;
		}
		case OT__PARTICLE:
		{
			Particle_Update(obj, delta);
			break;
		}
		case OT__DECAL:
		{
			Decal_Update(obj, delta);
			break;
		}
		case OT__CRUSHER:
		{
			Crusher_Update(obj, delta);
			break;
		}
		case OT__LIGHT_STROBER:
		{
			LightStrober_Update(obj, delta);
			break;
		}
		case OT__LIFT:
		{
			Lift_Update(obj, delta);
			break;
		}
		default:
		{
			break;
		}
		}
		
	}
}

void Map_SmoothUpdate(double lerp, double delta)
{
	Render_LockObjectMutex(true);

	for (int i = 0; i < s_map.num_sorted_objects; i++)
	{
		ObjectID id = s_map.sorted_list[i];
		if (id < 0)
		{
			continue;
		}

		Object* obj = &s_map.objects[id];

		//ignore
		if (obj->type == OT__NONE || obj->type == OT__PLAYER || !obj->sprite.img)
		{
			continue;
		}

		//only lerp objects that are most likely to move
		if (obj->type == OT__MONSTER || obj->type == OT__MISSILE || obj->type == OT__PARTICLE)
		{
			obj->sprite.x = obj->x * lerp + obj->prev_x * (1.0 - lerp);
			obj->sprite.y = obj->y * lerp + obj->prev_y * (1.0 - lerp);
			obj->sprite.z = obj->z * lerp + obj->prev_z * (1.0 - lerp);
		}											   
		if (obj->type == OT__MONSTER)
		{
			Monster_UpdateSpriteAnimation(obj, delta);
		}
		if (obj->sprite.frame_count > 0 && obj->sprite.anim_speed_scale > 0)
		{
			Sprite_UpdateAnimation(&obj->sprite, delta);
		}
	}

	Render_UnlockObjectMutex(true);
}

void Map_DeleteObject(Object* obj)
{
	if (obj->spatial_id >= 0)
	{
		BVH_Tree_Remove(&s_map.spatial_tree, obj->spatial_id);
	}

	if (Object_IsSectorLinked(obj))
	{
		Render_LockObjectMutex(true);

		Object_UnlinkSector(obj);

		Render_UnlockObjectMutex(true);
	}

	ObjectID id = obj->id;

	assert(id < MAX_OBJECTS);

	//reset the object
	memset(obj, 0, sizeof(Object));

	//store the id in free list
	Map_FreeListStoreID(id);

	//update sorted list
	Map_UpdateSortedList();
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

Sector* Map_GetSector(int index)
{
	if (index < 0 || index >= s_map.num_sectors)
	{
		return NULL;
	}

	return &s_map.sectors[index];
}

Sector* Map_GetNextSectorByTag(int* r_iterIndex, int tag)
{
	while (*r_iterIndex < s_map.num_sectors)
	{
		Sector* sector = &s_map.sectors[(*r_iterIndex)++];

		if (sector->sector_tag == tag)
		{
			return sector;
		}
	}

	return NULL;
}

Line* Map_GetLine(int index)
{
	assert(index >= 0);

	if (index >= s_map.num_line_segs)
	{
		return NULL;
	}

	return &s_map.line_segs[index];
}

bool Map_CheckSectorReject(int s1, int s2)
{
	if (s_map.reject_size > 0 && s1 >= 0 && s2 >= 0)
	{
		int pnum = s1 * s_map.num_sectors + s2;
		return !(s_map.reject_matrix[pnum >> 3] & (1 << (pnum & 7)));
	}

	return true;
}

BVH_Tree* Map_GetSpatialTree()
{
	return &s_map.spatial_tree;
}

void Map_Destruct()
{
	if (s_map.line_segs) free(s_map.line_segs);
	if (s_map.bsp_nodes) free(s_map.bsp_nodes);
	if (s_map.sectors) free(s_map.sectors);
	if (s_map.sub_sectors) free(s_map.sub_sectors);
	if (s_map.sidedefs) free(s_map.sidedefs);
	if (s_map.reject_matrix) free(s_map.reject_matrix);

	BVH_Tree_Destruct(&s_map.spatial_tree);

	memset(&s_map, 0, sizeof(s_map));
}
