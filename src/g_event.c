#include "g_common.h"

#include "u_math.h"
#include "game_info.h"

static void Event_ActivateCrusherObject(Sector* sector)
{
	if(sector->sector_object >= 0)
	{
		return;
	}

	float sector_center_x = 0;
	float sector_center_y = 0;
	Math_GetBoxCenter(sector->bbox, &sector_center_x, &sector_center_y);

	Object* crusher = Object_Spawn(OT__CRUSHER, 0, sector_center_x, sector_center_y, 0);

	crusher->sector_index = sector->index;
	crusher->speed = 50;
	crusher->hp = 0; //0 == means one cycle only, 1 == repeats
	crusher->dir_y = 1; //floor move
	crusher->dir_z = -1; //ceil move

	sector->sector_object = crusher->id;
}

static void Event_ActivateCrushersByTag(int tag)
{
	if (tag <= 0)
	{
		return;
	}

	int iter_index = 0;

	Sector* sector = NULL;

	while (sector = Map_GetNextSectorByTag(&iter_index, tag))
	{
		//already has object associated with it
		if (sector->sector_object >= 0)
		{
			continue;
		}

		Event_ActivateCrusherObject(sector);
	}
}
static void Event_ActivateDoorObject(Sector* sector, bool never_close)
{
	//already has object associated with it
	if (sector->sector_object >= 0)
	{
		Object* door = Map_GetObject(sector->sector_object);

		//flip the direction
		if (door->state == SECTOR_OPEN)
		{
			door->state = SECTOR_CLOSE;
		}
		else if (door->state == SECTOR_CLOSE)
		{
			door->state = SECTOR_OPEN;
		}
		return;
	}

	float sector_center_x = 0;
	float sector_center_y = 0;
	float sector_center_z = sector->ceil - sector->floor;
	Math_GetBoxCenter(sector->bbox, &sector_center_x, &sector_center_y);

	Object* door = Object_Spawn(OT__DOOR, 0, sector_center_x, sector_center_y, sector_center_z);

	door->sector_index = sector->index;
	door->speed = 100;
	door->hp = 0; //0 == means one cycle only, 1 == repeats
	door->state = SECTOR_OPEN;
	if (never_close)
	{
		door->flags |= OBJ_FLAG__DOOR_NEVER_CLOSE;
		sector->sector_tag = 0; //don't allow other lines to retrigger this
	}

	sector->sector_object = door->id;
}
static void Event_ActivateDoorsByTag(int tag, bool never_close)
{
	if (tag == 0)
	{
		return;
	}

	int iter_index = 0;

	Sector* sector = NULL;

	while (sector = Map_GetNextSectorByTag(&iter_index, tag))
	{
		Event_ActivateDoorObject(sector, never_close);
	}
}

static void Event_ActivateLiftObject(Sector* sector)
{
	//already has object associated with it
	if (sector->sector_object >= 0)
	{
		Object* lift = Map_GetObject(sector->sector_object);

		//flip the direction
		if (lift->state == SECTOR_OPEN)
		{
			lift->state = SECTOR_CLOSE;
		}
		else if (lift->state == SECTOR_CLOSE)
		{
			lift->state = SECTOR_OPEN;
		}
		return;
	}

	float sector_center_x = 0;
	float sector_center_y = 0;
	float sector_center_z = sector->ceil - sector->floor;
	Math_GetBoxCenter(sector->bbox, &sector_center_x, &sector_center_y);

	Object* lift = Object_Spawn(OT__LIFT, 0, sector_center_x, sector_center_y, sector_center_z);

	lift->sector_index = sector->index;
	lift->speed = 200;
	lift->hp = 0; //0 == means one cycle only, 1 == repeats
	lift->state = SECTOR_OPEN;

	sector->sector_object = lift->id;
}

static void Event_HandleSpecialUseLines(Linedef* line, int side)
{
	Sector* frontsector = Map_GetSector(line->front_sector);
	Sector* backsector = NULL;

	if (line->back_sector >= 0)
	{
		backsector = Map_GetSector(line->back_sector);
	}

	switch (line->special)
	{
	case SPECIAL__USE_DOOR:
	{
		if (backsector)
		{
			Event_ActivateDoorObject(backsector, false);
		}	
		break;
	}
	case SPECIAL__USE_DOOR_NEVER_CLOSE:
	{
		if (backsector)
		{
			Event_ActivateDoorObject(backsector, true);
			line->special = 0;
		}
		break;
	}
	case SPECIAL__USE_LIFT:
	{
		if (backsector)
		{
			Event_ActivateLiftObject(backsector);
		}
		break;
	}
	default:
	{
		break;
	}
	}
}
static void Event_HandleSpecialWalkoverLines(Linedef* line, int side)
{
	Sector* frontsector = Map_GetSector(line->front_sector);
	Sector* backsector = NULL;

	if (line->back_sector >= 0)
	{
		backsector = Map_GetSector(line->back_sector);
	}

	switch (line->special)
	{
	case SPECIAL__TRIGGER_DOOR_NEVER_CLOSE:
	{
		Event_ActivateDoorsByTag(line->sector_tag, true);
		line->special = 0;
		break;
	}
	case SPECIAL__TRIGGER_CRUSHER:
	{
		Event_ActivateCrushersByTag(line->sector_tag);
		break;
	}
	case SPECIAL__TRIGGER_EXIT:
	{
		Game_ChangeLevel(6);
		break;
	}
	default:
	{
		break;
	}
	}
}
void Event_TriggerSpecialLine(Object* obj, int side, Linedef* line, EventLineTriggerType trigger_type)
{
	if (trigger_type == EVENT_TRIGGER__LINE_USE)
	{
		Event_HandleSpecialUseLines(line, side);
	}
	else if (trigger_type == EVENT_TRIGGER__LINE_WALK_OVER)
	{
		Event_HandleSpecialWalkoverLines(line, side);
	}

	

}

void Event_CreateExistingSectorObject(int type, int sub_type, int state, float stop_timer, int sector_index)
{
	if (sector_index < 0)
	{
		return;
	}

	Sector* sector = Map_GetSector(sector_index);

	if (!sector || sector->sector_object >= 0)
	{
		return;
	}

	switch (type)
	{
	case OT__CRUSHER:
	{
		Event_ActivateCrusherObject(sector);
		break;
	}
	case OT__LIFT:
	{
		Event_ActivateLiftObject(sector);
		break;
	}
	case OT__DOOR:
	{
		Event_ActivateDoorObject(sector, false);
		break;
	}
	default:
		break;
	}

	if (sector->sector_object < 0)
	{
		return;
	}
	Object* obj = Map_GetObject(sector->sector_object);

	obj->state = state;
	obj->stuck_timer = stop_timer;
}
