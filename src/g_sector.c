#include "g_common.h"
#include "u_math.h"
#include "game_info.h"
#include "sound.h"

typedef enum
{
	CRUSHER__IDLE,
	CRUSHER__MOVING_TO_TARGET,
	CRUSHER__MOVING_TO_ORIGIN
} CrusherState;

static Sector* Sector_GetLineOtherSector(Linedef* line, int sector_index)
{
	if (line->back_sector < 0)
	{
		return NULL;
	}

	if (line->front_sector == sector_index)
	{
		return Map_GetSector(line->back_sector);
	}

	return Map_GetSector(line->front_sector);
}

void Sector_CreateLightStrober(Sector* sector, SubType light_type)
{
	//already has object associated with it
	if (sector->sector_object >= 0)
	{
		return;
	}

	float sector_center_x = 0;
	float sector_center_y = 0;
	float sector_center_z = sector->ceil - sector->floor;
	Math_GetBoxCenter(sector->bbox, &sector_center_x, &sector_center_y);

	Object* strober = Object_Spawn(OT__LIGHT_STROBER, light_type, sector_center_x, sector_center_y, sector_center_z);

	strober->sector_index = sector->index;
	strober->hp = sector->light_level; //store the base light level somewhere

	sector->sector_object = strober->id;
}

float Sector_FindHighestNeighbourCeilling(Sector* sector)
{
	int num_lines = Trace_SectorLines(sector, false);
	int* hits = Trace_GetHitObjects();

	float highest_ceil = 0;

	for (int i = 0; i < num_lines; i++)
	{
		Linedef* line = Map_GetLineDef(hits[i]);

		Sector* other_sector = Sector_GetLineOtherSector(line, sector);

		if (!other_sector)
		{
			continue;
		}

		if (other_sector->ceil > highest_ceil)
		{
			highest_ceil = other_sector->ceil;
		}
	}

	return highest_ceil;
}

float Sector_FindLowestNeighbourCeilling(Sector* sector)
{
	int num_lines = Trace_SectorLines(sector, false);
	int* hits = Trace_GetHitObjects();

	float lowest_ceil = 1e3 * 2.0;

	for (int i = 0; i < num_lines; i++)
	{
		Linedef* line = Map_GetLineDef(hits[i]);

		Sector* other_sector = Sector_GetLineOtherSector(line, sector);

		if (!other_sector)
		{
			continue;
		}

		if (sector == other_sector)
		{
			continue;
		}

		if (other_sector->ceil < lowest_ceil)
		{
			lowest_ceil = other_sector->ceil;
		}
	}

	return lowest_ceil;
}

float Sector_FindLowestNeighbourFloor(Sector* sector)
{
	int num_lines = Trace_SectorLines(sector, false);
	int* hits = Trace_GetHitObjects();

	float lowest_floor = sector->floor;

	for (int i = 0; i < num_lines; i++)
	{
		Linedef* line = Map_GetLineDef(hits[i]);

		Sector* other_sector = Sector_GetLineOtherSector(line, sector);

		if (!other_sector)
		{
			continue;
		}

		if (other_sector->floor < lowest_floor)
		{
			lowest_floor = other_sector->floor;
		}
	}

	return lowest_floor;
}

void Sector_RemoveSectorObject(Sector* sector)
{
	if (sector->sector_object < 0)
	{
		return;
	}

	Object* sector_object = Map_GetObject(sector->sector_object);

	Map_DeleteObject(sector_object);

	sector->sector_object = -1;
}

void Crusher_Update(Object* obj, float delta)
{
	if (obj->sector_index < 0)
	{
		return;
	}

	Sector* sector = Map_GetSector(obj->sector_index);
	
	obj->prev_x = sector->ceil;
	obj->prev_y = sector->floor;

	float speed = delta * obj->speed;
	float floor_move = obj->dir_y * speed;
	float ceil_move = obj->dir_z * speed;

	if (obj->move_timer <= 0)
	{
		obj->state = CRUSHER__MOVING_TO_TARGET;
	}

	if (obj->state == CRUSHER__MOVING_TO_TARGET)
	{
		if (!Move_Sector(sector, floor_move, ceil_move, sector->base_floor, sector->ceil, sector->floor, sector->base_ceil, true))
		{
			obj->state = CRUSHER__MOVING_TO_ORIGIN;
		}
	}
	else if (obj->state == CRUSHER__MOVING_TO_ORIGIN)
	{
		Move_Sector(sector, -floor_move, -ceil_move, sector->base_floor, sector->ceil, sector->floor, sector->base_ceil, false);

		if ((int)sector->floor == (int)sector->base_floor && (int)sector->ceil == (int)sector->base_ceil)
		{
			//cycle completed
			if (obj->hp == 0)
			{
				Sector_RemoveSectorObject(sector);
				return;
			}

			obj->move_timer = 0;
			return;
		}
	}

	obj->move_timer += delta;
}

void Door_Update(Object* obj, float delta)
{
	if (obj->sector_index < 0)
	{
		return;
	}

	Sector* sector = Map_GetSector(obj->sector_index);

	obj->prev_x = sector->ceil;
	obj->prev_y = sector->floor;

	float speed = delta * obj->speed;
	float ceil_clamp = sector->neighbour_sector_value;

	if (obj->state == SECTOR_CLOSE && obj->flags & OBJ_FLAG__DOOR_NEVER_CLOSE)
	{
		Sector_RemoveSectorObject(sector);
		return;
	}
	else if (obj->state == SECTOR_SLEEP)
	{
		//door is open
		if ((int)sector->ceil == (int)ceil_clamp && !(obj->flags & OBJ_FLAG__DOOR_NEVER_CLOSE))
		{
			obj->stop_timer += delta;

			//close the door
			if (obj->stop_timer >= SECTOR_AUTOCLOSE_TIME)
			{
				obj->state = SECTOR_CLOSE;
			}
			else
			{
				return;
			}
		}
		else if ((obj->flags & OBJ_FLAG__DOOR_NEVER_CLOSE))
		{
			Sector_RemoveSectorObject(sector);
			return;
		}
	}

	bool just_moved = false;

	//open the door
	if (obj->state == SECTOR_OPEN)
	{
		if (sector->ceil == sector->base_ceil)
		{
			just_moved = true;
		}

		Move_Sector(sector, 0, speed, -FLT_MAX, FLT_MAX, sector->base_ceil, ceil_clamp, false);

		//reached the top
		if (sector->ceil == ceil_clamp)
		{
			obj->stop_timer = 0;
			obj->state = SECTOR_SLEEP;
		}
	}
	//close the door
	else if (obj->state == SECTOR_CLOSE)
	{
		if (sector->ceil == ceil_clamp)
		{
			just_moved = true;
		}

		//blocked by something?
		if (!Move_Sector(sector, 0, -speed, -FLT_MAX, FLT_MAX, sector->base_ceil, ceil_clamp, false))
		{
			obj->state = SECTOR_OPEN;
			obj->stop_timer = 0;
		}

		if (sector->ceil == sector->base_ceil)
		{
			obj->state = SECTOR_SLEEP;
		}
	}

	//door was open and closed so it completed its cycle
	if (obj->state == SECTOR_SLEEP && (int)sector->ceil == (int)sector->base_ceil)
	{
		Sector_RemoveSectorObject(sector);
		return;
	}

	if (just_moved)
	{
		//play the sound
		Sound_EmitWorldTemp(SOUND__DOOR_ACTION, obj->x, obj->y, obj->z, 0, 0, 0, 1);
	}
}

void Lift_Update(Object* obj, float delta)
{
	if (obj->sector_index < 0)
	{
		return;
	}

	//kinda same as the door code above
	Sector* sector = Map_GetSector(obj->sector_index);

	obj->prev_x = sector->ceil;
	obj->prev_y = sector->floor;

	float speed = delta * obj->speed;
	float floor_clamp = sector->neighbour_sector_value;

	if (obj->state == SECTOR_SLEEP)
	{
		//lift is raised to target
		if ((int)sector->floor == (int)floor_clamp)
		{
			obj->stop_timer += delta;

			//close the door
			if (obj->stop_timer >= SECTOR_AUTOCLOSE_TIME)
			{
				obj->state = SECTOR_CLOSE;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	bool just_moved = false;

	//raise the lift
	if (obj->state == SECTOR_OPEN)
	{
		if (sector->floor == sector->base_floor)
		{
			just_moved = true;
		}

		Move_Sector(sector, -speed, 0, floor_clamp, sector->base_floor, -FLT_MAX, FLT_MAX, false);

		//reached the target
		if (sector->floor == floor_clamp)
		{
			obj->stop_timer = 0;
			obj->state = SECTOR_SLEEP;
		}
	}
	//return the lift to starting position
	else if (obj->state == SECTOR_CLOSE)
	{
		if (sector->floor == floor_clamp)
		{
			just_moved = true;
		}

		if (!Move_Sector(sector, speed, 0, floor_clamp, sector->base_floor, -FLT_MAX, FLT_MAX, false))
		{
			obj->state = SECTOR_OPEN;
			obj->stop_timer = 0;
		}

		if (sector->floor == sector->base_floor)
		{
			obj->state = SECTOR_SLEEP;
		}
	}

	//lift was moved and and returned so it completed its cycle
	if (obj->state == SECTOR_SLEEP && (int)sector->floor == (int)sector->base_floor)
	{
		Sector_RemoveSectorObject(sector);
		return;
	}

	if (just_moved)
	{
		//play the sound
		Sound_EmitWorldTemp(SOUND__DOOR_ACTION, obj->x, obj->y, obj->z, 0, 0, 0, 1);
	}
}

void LightStrober_Update(Object* obj, float delta)
{
	if (obj->sector_index < 0)
	{
		return;
	}

	Sector* sector = Map_GetSector(obj->sector_index);

	int base_light = obj->hp;

	switch (obj->sub_type)
	{
	case SUB__LIGHT_STROBER_GLOW:
	{
		if (obj->move_timer <= base_light / 2)
		{
			obj->state = true;
		}
		else if (obj->move_timer >= base_light)
		{
			obj->state = false;
		}

		if (obj->state)
		{
			obj->move_timer += delta * 200;
		}
		else
		{
			obj->move_timer -= delta * 200;
		}

		sector->light_level = obj->move_timer;

		break;
	}
	case SUB__LIGHT_STROBER_FLICKER_RANDOM:
	{
		if (obj->move_timer > 0)
		{
			obj->move_timer -= delta;
			break;
		}

		if (Math_rand() % 4 == 0)
		{
			sector->light_level = base_light / 2;
		}
		else
		{
			sector->light_level = base_light;
		}

		obj->move_timer = 0.25;

		break;
	}
	default:
		break;
	}

	//always make sure to clamp
	sector->light_level = Math_Clampl(sector->light_level, 0, 255);

}
