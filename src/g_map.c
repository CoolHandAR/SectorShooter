#include "g_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "u_math.h"
#include "game_info.h"
#include "main.h"
#include "utility.h"

static Map s_map;

static void Map_UpdateSortedList()
{
	int index = 0;

	for (int i = 0; i < s_map.num_objects; i++)
	{
		Object* obj = &s_map.objects[i];

		if (obj->type == OT__NONE
			|| obj->type == OT__PLAYER)
		{
			continue;
		}

		s_map.sorted_list[index++] = i;
	}

	s_map.num_sorted_objects = index;
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
		printf("Can't create new Object\n");

		return NULL;
	}

	Object* obj = &s_map.objects[index];

	obj->sprite.decal_line_index = -1;
	obj->sprite.action_frame = -1;
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
	obj->sector_linked = false;
	obj->sound_id = -1;
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
	
	LightCompilerInfo* lci = Info_GetLightCompilerInfo(index);

	if (!Map_Load(LEVELS[index], SKIES[index], lci))
	{
		return false;
	}


	return true;
}

bool Map_Load(const char* filename, const char* skyname, LightCompilerInfo* light_compiler_info)
{
	Map_Destruct();

	return Load_Doommap(filename, skyname, light_compiler_info, &s_map);
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

Object* Map_GetNextObjectByType(int* r_iterIndex, int type)
{
	while (*r_iterIndex < s_map.num_objects)
	{
		Object* obj = Map_GetObject((*r_iterIndex)++);
		
		if (obj->type == type)
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
		case OT__LIGHT:
			//fallthrough
		case OT__SFX_EMITTER:
		{
			SFX_Update(obj);
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
	for (int i = 0; i < s_map.num_sorted_objects; i++)
	{
		ObjectID id = s_map.sorted_list[i];
		if (id < 0)
		{
			continue;
		}

		Object* obj = &s_map.objects[id];

		//ignore
		if (obj->type == OT__NONE || obj->type == OT__PLAYER)
		{
			continue;
		}

		if (obj->sprite.img)
		{
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
		else
		{
			if (obj->type == OT__DOOR || obj->type == OT__CRUSHER || obj->type == OT__LIFT)
			{
				if (obj->sector_index >= 0)
				{
					Sector* sector = Map_GetSector(obj->sector_index);

					sector->r_ceil = sector->ceil * lerp + obj->prev_x * (1.0 - lerp);
					sector->r_floor = sector->floor * lerp + obj->prev_y * (1.0 - lerp);
				}
			}
		}
	}
}

void Map_DeleteObject(Object* obj)
{
	if (obj->spatial_id >= 0)
	{
		BVH_Tree_Remove(&s_map.spatial_tree, obj->spatial_id);
	}
	if (obj->sound_id >= 0)
	{
		Sound_DeleteSound(obj->sound_id);
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
	if (s_map.num_nodes <= 0 || s_map.num_sub_sectors == 1)
	{
		return &s_map.sub_sectors[0];
	}

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

	if (index < 0 ||index >= s_map.num_line_segs)
	{
		return NULL;
	}

	return &s_map.line_segs[index];
}

Linedef* Map_GetLineDef(int index)
{
	assert(index >= 0);

	if (index < 0 || index >= s_map.num_line_segs)
	{
		return NULL;
	}

	return &s_map.linedefs[index];
}

Sidedef* Map_GetSideDef(int index)
{
	assert(index >= 0);

	if (index < 0 || index >= s_map.num_sidedefs)
	{
		return NULL;
	}

	return &s_map.sidedefs[index];
}

bool Map_CheckSectorReject(int s1, int s2)
{
	if (s_map.reject_size > 1 && s1 >= 0 && s2 >= 0)
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

void Map_SetupLightGrid(Lightblock* data)
{
	Lightgrid* lightgrid = &s_map.lightgrid;

	memset(lightgrid, 0, sizeof(Lightgrid));

	float world_bounds[2][3];
	world_bounds[0][0] = s_map.world_bounds[0][0] - (LIGHT_GRID_SIZE * LIGHT_GRID_WORLD_BIAS_MULTIPLIER);
	world_bounds[0][1] = s_map.world_bounds[0][1] - (LIGHT_GRID_SIZE * LIGHT_GRID_WORLD_BIAS_MULTIPLIER);
	world_bounds[0][2] = s_map.world_min_height - (LIGHT_GRID_Z_SIZE * LIGHT_GRID_WORLD_BIAS_MULTIPLIER);

	world_bounds[1][0] = s_map.world_bounds[1][0] + (LIGHT_GRID_SIZE * LIGHT_GRID_WORLD_BIAS_MULTIPLIER);
	world_bounds[1][1] = s_map.world_bounds[1][1] + (LIGHT_GRID_SIZE * LIGHT_GRID_WORLD_BIAS_MULTIPLIER);
	world_bounds[1][2] = s_map.world_max_height + (LIGHT_GRID_Z_SIZE * LIGHT_GRID_WORLD_BIAS_MULTIPLIER);

	float world_size[3];
	for (int i = 0; i < 3; i++)
	{
		world_size[i] = world_bounds[1][i] - world_bounds[0][i];
	}

	int x_blocks = ceil((world_size[0]) / LIGHT_GRID_SIZE) + 1;
	int y_blocks = ceil((world_size[1]) / LIGHT_GRID_SIZE) + 1;
	int z_blocks = ceil((world_size[2]) / LIGHT_GRID_Z_SIZE) + 1;

	if (data)
	{
		lightgrid->blocks = data;
	}
	else
	{
		lightgrid->blocks = calloc(x_blocks * y_blocks * z_blocks, sizeof(Lightblock));

		if (!lightgrid->blocks)
		{
			return;
		}
	}

	lightgrid->block_size[0] = x_blocks;
	lightgrid->block_size[1] = y_blocks;
	lightgrid->block_size[2] = z_blocks;

	lightgrid->size[0] = LIGHT_GRID_SIZE;
	lightgrid->size[1] = LIGHT_GRID_SIZE;
	lightgrid->size[2] = LIGHT_GRID_Z_SIZE;

	lightgrid->inv_size[0] = 1.0 / lightgrid->size[0];
	lightgrid->inv_size[1] = 1.0 / lightgrid->size[1];
	lightgrid->inv_size[2] = 1.0 / lightgrid->size[2];

	for (int i = 0; i < 3; i++)
	{
		lightgrid->origin[i] = lightgrid->size[i] * ceil((world_bounds[0][i]) / lightgrid->size[i]);

		float maxs = lightgrid->size[i] * floor((world_bounds[1][i]) / lightgrid->size[i]);

		lightgrid->bounds[i] = (maxs - lightgrid->origin[i]) / lightgrid->size[i] + 1;
	}
}

void Map_UpdateObjectsLight()
{
	for (int i = 0; i < s_map.num_objects; i++)
	{
		Object* obj = Map_GetObject(i);

		if (obj->type == OT__NONE || !obj->sprite.img)
		{
			continue;
		}

		Map_CalcBlockLight(obj->x, obj->y, obj->z, &obj->sprite.light);
	}
}

void Map_CalcBlockLight(float p_x, float p_y, float p_z, Vec3_u16* dest)
{
	Lightgrid* grid = &s_map.lightgrid;

	if (!grid->blocks)
	{
		return;
	}

	float local_pos[3] = { p_x - grid->origin[0], p_y - grid->origin[1], p_z - grid->origin[2] };

	int pos[3];
	float frac[3];
	for (int i = 0; i < 3; i++)
	{
		float v = local_pos[i] * grid->inv_size[i];

		pos[i] = floor(v);
		frac[i] = v - pos[i];

		if (pos[i] < 0)
		{
			pos[i] = 0;
		}
		else if (pos[i] >= grid->bounds[i] - 1)
		{
			pos[i] = grid->bounds[i] - 1;
		}
	}

	int grid_step[3];
	grid_step[0] = 1;
	grid_step[1] = grid->bounds[0];
	grid_step[2] = grid->bounds[0] * grid->bounds[1];

	Lightblock* block_data = grid->blocks + pos[0] * grid_step[0] + pos[1] * grid_step[1] + pos[2] * grid_step[2];

	float total_factor = 0;
	Vec4 total_light = Vec4_Zero();

	for (int i = 0; i < 8; i++)
	{
		Lightblock* data = block_data;
		float factor = 1.0;

		for (int j = 0; j < 3; j++)
		{
			if (i & (1 << j))
			{
				factor *= frac[j];
				data += grid_step[j];
			}
			else
			{
				factor *= (1.0 - frac[j]);
			}
		}
		if (!data)
		{
			continue;
		}

		total_factor += factor;

		total_light.r += (float)data->light.r * factor;
		total_light.g += (float)data->light.g * factor;
		total_light.b += (float)data->light.b * factor;
	}

	if (total_factor > 0 && total_factor < 0.99)
	{
		total_factor = 1.0 / total_factor;
		Vec4_ScaleScalar(&total_light, total_factor);
	}

	dest->r = Math_Clampl(total_light.r, 0, MAX_LIGHT_VALUE - 255);
	dest->g = Math_Clampl(total_light.g, 0, MAX_LIGHT_VALUE - 255);
	dest->b = Math_Clampl(total_light.b, 0, MAX_LIGHT_VALUE - 255);
}

void Map_Destruct()
{
	//delete lightmaps
	for (int i = 0; i < s_map.num_sectors; i++)
	{
		Sector* sector = &s_map.sectors[i];

		if (sector->floor_lightmap.data)
		{
			free(sector->floor_lightmap.data);
		}
		if (sector->ceil_lightmap.data)
		{
			free(sector->ceil_lightmap.data);
		}
	}
	for (int i = 0; i < s_map.num_linedefs; i++)
	{
		Linedef* linedef = &s_map.linedefs[i];

		if (linedef->lightmap.data)
		{
			free(linedef->lightmap.data);
		}
	}

	//delete map stuff
	if (s_map.linedefs) free(s_map.linedefs);
	if (s_map.line_segs) free(s_map.line_segs);
	if (s_map.bsp_nodes) free(s_map.bsp_nodes);
	if (s_map.sectors) free(s_map.sectors);
	if (s_map.sub_sectors) free(s_map.sub_sectors);
	if (s_map.sidedefs) free(s_map.sidedefs);
	if (s_map.reject_matrix) free(s_map.reject_matrix);
	if (s_map.lightgrid.blocks) free(s_map.lightgrid.blocks);

	BVH_Tree_Destruct(&s_map.spatial_tree);

	memset(&s_map, 0, sizeof(s_map));
}
