#include "g_common.h"

#include "u_math.h"
#include "main.h"

#define MAX_BUMPS 5
#define MAX_CLIP_OBJECTS 4

static void Move_CreateClipLine(float p_x, float p_y, float p_moveX, float p_moveY, Line* dest)
{
	dest->x0 = p_x;
	dest->y0 = p_y;
	dest->x1 = p_x + p_moveX;
	dest->y1 = p_y + p_moveY;
	dest->dx = dest->x1 - dest->x0;
	dest->dy = dest->y1 - dest->y0;
}
static bool Move_ClipMove2(Object* obj, float p_moveX, float p_moveY)
{
	Map* map = Map_GetMap();

	if (p_moveX == 0 && p_moveY == 0)
	{
		return false;
	}

	Line* clip_hits[MAX_CLIP_OBJECTS];
	int num_clips = 0;

	bool moved = false;

	int bump_count = 0;

	float time_left = 1;

	Line start_vel_line;
	Move_CreateClipLine(obj->x, obj->y, p_moveX, p_moveY, &start_vel_line);

	//clip_hits[num_clips++] = &start_vel_line;

	for (bump_count = 0; bump_count < MAX_BUMPS; bump_count++)
	{
		if (p_moveX == 0 && p_moveY == 0)
		{
			break;
		}

		float best_frac = 1.001;
		
		float c_dx = p_moveX;
		float c_dy = p_moveY;

		float start_x = obj->x;
		float start_y = obj->y;

		float end_x = obj->x + c_dx;
		float end_y = obj->y + c_dy;

		Line vel_line;
		Move_CreateClipLine(start_x, start_y, c_dx, c_dy, &vel_line);
		int hit = Trace_FindSlideHit(obj, start_x, start_y, end_x, end_y, obj->size, &best_frac);


		float trace_end_x = start_x + best_frac * (end_x - start_x);
		float trace_end_y = start_y + best_frac * (end_y - start_y);

		if (best_frac > 0)
		{
			if(Move_SetPosition(obj, trace_end_x, trace_end_y, obj->size))
			{
				moved = true;
			}
			else
			{
				break;
			}
		}

		if (hit == TRACE_NO_HIT || best_frac >= 1.0)
		{
			break;
		}

		time_left -= time_left * best_frac;

		if (num_clips >= MAX_CLIP_OBJECTS)
		{
			break;
		}

		clip_hits[num_clips++] = &map->line_segs[-(hit + 1)];


		for (int i = 0; i < num_clips; i++)
		{
			Line* line0 = clip_hits[i];

			float clip_dx = p_moveX;
			float clip_dy = p_moveY;

			if (Move_GetLineDot(clip_dx, clip_dy, line0) >= 0.1)
			{
				continue;
			}

			Move_ClipVelocity(obj->x, obj->y, &clip_dx, &clip_dy, line0);

			for (int j = 0; j < num_clips; j++)
			{
				if (j == i)
				{
					continue;
				}
				Line* line1 = clip_hits[j];

				if (Move_GetLineDot(clip_dx, clip_dy, line1) >= 0.1)
				{
					continue;
				}

				Move_ClipVelocity(obj->x, obj->y, &clip_dx, &clip_dy, line1);

				if (Move_GetLineDot(clip_dx, clip_dy, line0) >= 0.1)
				{
					continue;
				}

				for (int k = 0; k < num_clips; k++)
				{
					if (k == i || k == j)
					{
						continue;
					}

					Line* line2 = clip_hits[k];

					if (Move_GetLineDot(clip_dx, clip_dy, line2) >= 0.1)
					{
						continue;
					}

					return false;
				}
			}

			p_moveX = clip_dx;
			p_moveY = clip_dy;
			break;
		}

	}


	return moved;
}
static bool Move_ClipMove(Object* obj, float p_moveX, float p_moveY)
{
	//return Move_ClipMove2(obj, p_moveX, p_moveY);

	Map* map = Map_GetMap();

	if (Move_SetPosition(obj, obj->x + p_moveX, obj->y + p_moveY, obj->size * 1.5))
	{
		return true;
	}

	if (p_moveX == 0 && p_moveY == 0)
	{
		return false;
	}
	int bump_count = 0;

	for (bump_count = 0; bump_count < 3; bump_count++)
	{	
		if (p_moveX == 0 && p_moveY == 0)
		{
			return false;
		}

		float lead_x = 0;
		float lead_y = 0;
		float trail_x = 0;
		float trail_y = 0;

		if (p_moveX > 0)
		{
			lead_x = obj->x + obj->size;
			trail_x = obj->x - obj->size;
		}
		else
		{
			lead_x = obj->x - obj->size;
			trail_x = obj->x + obj->size;
		}
		if (p_moveY > 0)
		{
			lead_y = obj->y + obj->size;
			trail_y = obj->y - obj->size;
		}
		else
		{
			lead_y = obj->y - obj->size;
			trail_y = obj->y + obj->size;
		}

		Line vel_line;

		float best_frac = 1.01;
		int hit = TRACE_NO_HIT;

		float frac0 = best_frac;
		int hit0 = Trace_FindSlideHit(obj, lead_x, lead_y, lead_x + p_moveX, lead_y + p_moveY, obj->size, &frac0);

		if (frac0 < best_frac && hit0 != TRACE_NO_HIT)
		{
			best_frac = frac0;
			hit = hit0;
		}

		if (hit0 != TRACE_NO_HIT)
		{
			float frac1 = best_frac;
			int hit1 = Trace_FindSlideHit(obj, trail_x, lead_y, trail_x + p_moveX, lead_y + p_moveY, obj->size, &frac1);
			if (frac1 < best_frac && hit1 != TRACE_NO_HIT)
			{
				best_frac = frac1;
				hit = hit1;
			}
			if (hit1 != TRACE_NO_HIT)
			{
				float frac2 = best_frac;
				int hit2 = Trace_FindSlideHit(obj, lead_x, trail_y, lead_x + p_moveX, trail_y + p_moveY, obj->size, &frac2);
				if (frac2 < best_frac && hit2 != TRACE_NO_HIT)
				{
					best_frac = frac2;
					hit = hit2;
				}
			}
		}

		if (hit == TRACE_NO_HIT || best_frac >= 1)
		{
			if (!Move_SetPosition(obj, obj->x + p_moveX, obj->y + p_moveY, obj->size))
			{	
				if (fabs(p_moveX) > fabs(p_moveY))
				{
					if (!Move_SetPosition(obj, obj->x + p_moveX, obj->y, obj->size))
					{
						if (Move_SetPosition(obj, obj->x, obj->y + p_moveY, obj->size))
						{
							continue;
						}
					}
				}
				else
				{
					if (!Move_SetPosition(obj, obj->x, obj->y + p_moveY, obj->size))
					{
						if (Move_SetPosition(obj, obj->x + p_moveX, obj->y, obj->size))
						{
							continue;
						}
					}
				}
			
			}

			return true;
		}


		int line_index = -(hit + 1);
		Line* line0 = &map->line_segs[line_index];

		float clip_dx = p_moveX;
		float clip_dy = p_moveY;

		Move_ClipVelocity(obj->x, obj->y, &clip_dx, &clip_dy, line0);

		p_moveX = clip_dx;
		p_moveY = clip_dy;

		if (Move_SetPosition(obj, obj->x + p_moveX, obj->y + p_moveY, obj->size))
		{
			return true;
		}
	}

	return true;
}

float Move_GetLineDot(float x, float y, Line* line)
{
	if (line->dot == 0)
	{
		return 0;
	}

	float den = (x * line->dx + line->dy * y);

	if (den == 0)
	{
		return 0;
	}

	den = den / line->dot;

	return den;
}

void Move_Accelerate(Object* obj, float p_moveX, float p_moveY, float p_moveZ)
{

}

void Move_ClipVelocity(float x, float y, float* r_dx, float* r_dy, Line* clip_line)
{
	float dx = *r_dx;
	float dy = *r_dy;

	if (clip_line->dx == 0)
	{
		*r_dx = 0;
		return;
	}
	if (clip_line->dy == 0)
	{
		*r_dy = 0;
		return;
	}

	if (dx == 0 && dy == 0)
	{
		*r_dx = 0;
		*r_dy = 0;
		return;
	}
	
	if (clip_line->dot == 0)
	{
		*r_dx = 0;
		*r_dy = 0;
		return;
	}

	float den = (dx * clip_line->dx + clip_line->dy * dy);

	if (den == 0)
	{
		*r_dx = 0;
		*r_dy = 0;
		return;
	}

	den = den / clip_line->dot;

	*r_dx = clip_line->dx * den;
	*r_dy = clip_line->dy * den;
}

bool Move_CheckPosition(Object* obj, float x, float y, float size, int* r_sectorIndex, float* r_floorZ, float* r_ceilZ)
{
	Sector* new_sector = Map_FindSector(x, y);

	float floor = new_sector->floor;
	float ceil = new_sector->ceil;
	float low_floor = new_sector->floor;
	
	bool on_ground = (obj->z <= floor);

	float old_z = obj->z;

	if (obj->z <= new_sector->floor)
	{
		//obj->z = new_sector->floor;
	}
	if (obj->z + obj->height > new_sector->ceil)
	{
		//obj->z = new_sector->ceil - obj->height;
	}

	if (!Trace_CheckBoxPosition(obj, x, y, size, &floor, &ceil, &low_floor))
	{
		obj->z = old_z;
		return false;
	}

	float range = ceil - floor;

	//can't fit in gap
	if (range < obj->height)
	{
		obj->z = old_z;
		return false;
	}
	//you will bonk your head
	if (ceil - obj->z < obj->height)
	{
		obj->z = old_z;
		return false;
	}
	if (obj->type == OT__MISSILE)
	{
		if (floor - obj->z > obj->height)
		{
			obj->z = old_z;
			return false;
		}
	}
	else
	{
		//you are too short for stepping over
		if (floor - obj->z > obj->step_height)
		{
			obj->z = old_z;
			return false;
		}
		//too big of a fall
		if (obj->type != OT__PLAYER)
		{
			if ((floor - low_floor) > obj->step_height)
			{
				obj->z = old_z;
				return false;
			}
		}
	}

	if (r_sectorIndex) *r_sectorIndex = new_sector->index;
	if (r_floorZ) *r_floorZ = floor;
	if (r_ceilZ) *r_ceilZ = ceil;

	obj->z = old_z;

	return true;
}
bool Move_SetPosition(Object* obj, float x, float y, float size)
{
	obj->prev_x = obj->x;
	obj->prev_y = obj->y;

	int new_sector_index = -1;

	float floor_z = 0;
	float ceil_z = 0;

	if (!(obj->flags & OBJ_FLAG__IGNORE_POSITION_CHECK))
	{
		//just spawned? try to corrent to proper position
		if (obj->sector_index < 0 && obj->type != OT__MISSILE)
		{
			Sector* new_sector = Map_FindSector(x, y);

			obj->z = new_sector->floor;
			floor_z = new_sector->floor;
			ceil_z = new_sector->ceil;
			new_sector_index = new_sector->index;
		}
		else
		{
			if (!Move_CheckPosition(obj, x, y, size, &new_sector_index, &floor_z, &ceil_z))
			{
				return false;
			}
		}
	}
	else
	{
		Sector* new_sector = Map_FindSector(x, y);

		new_sector_index = new_sector->index;
		floor_z = new_sector->floor;
		ceil_z = new_sector->ceil;
	}

	float old_pos_x = obj->x;
	float old_pos_y = obj->y;

	//we can move
	obj->x = x;
	obj->y = y;

	//update clamp
	obj->ceil_clamp = ceil_z;
	obj->floor_clamp = floor_z;

	//setup sector links
	if (new_sector_index != obj->sector_index)
	{
		Render_LockObjectMutex(true);

		Object_UnlinkSector(obj);

		obj->sector_index = new_sector_index;
		Object_LinkSector(obj);

		Render_UnlockObjectMutex(true);
	}
	
	//update light
	Map_CalcBlockLight(obj->x, obj->y, obj->z, &obj->sprite.light);

	//update bvh
	if (obj->spatial_id >= 0)
	{
		float box[2][2];
		Math_SizeToBbox(x, y, obj->size, box);

		Map* map = Map_GetMap();

		BVH_Tree_UpdateBounds(&map->spatial_tree, obj->spatial_id, box);
	}

	//trigger any special lines
	if (obj->type == OT__PLAYER)
	{
		Map* map = Map_GetMap();

		int num_special_lines = 0;
		int* line_indices = Trace_GetSpecialLines(&num_special_lines);

		for (int i = 0; i < num_special_lines; i++)
		{
			int index = line_indices[i];
			Line* line = &map->line_segs[index];

			//line might have hit twice and it's special could be changed
			if (line->special == 0)
			{
				continue;
			}
			
			int old_side = Line_PointSide(line, old_pos_x, old_pos_y);
			int new_side = Line_PointSide(line, obj->x, obj->y);

			if (old_side != new_side)
			{
				Event_TriggerSpecialLine(obj, old_side, line, EVENT_TRIGGER__LINE_WALK_OVER);
			}
		}
	}


	return true;
}

bool Move_CheckStep(Object* obj, float p_stepX, float p_stepY, float p_size)
{
	return Move_CheckPosition(obj, obj->x + p_stepX, obj->y + p_stepY, p_size, NULL, NULL, NULL);
}

bool Move_ZMove(Object* obj, float p_moveZ)
{
	float next_z = obj->z + p_moveZ;
	float z = next_z;

	obj->prev_z = obj->z;

	if (!(obj->flags & OBJ_FLAG__IGNORE_POSITION_CHECK))
	{
		if (next_z <= obj->floor_clamp)
		{
			if (obj->type == OT__MISSILE)
			{
				return false;
			}

			z = obj->floor_clamp;
		}
		if (next_z + obj->height > obj->ceil_clamp)
		{
			if (obj->type == OT__MISSILE)
			{
				return false;
			}

			z = obj->ceil_clamp - obj->height;
		}
	}

	obj->vel_z = p_moveZ;
	obj->z = z;

	return true;
}

bool Move_Object(Object* obj, float p_moveX, float p_moveY, float delta, bool p_slide)
{
	obj->prev_x = obj->x;
	obj->prev_y = obj->y;

	bool moved = false;

	float max_speed = obj->speed * delta;
	float friction = 1.0 - (4 * delta);
	float stop_speed = 4 * delta;
	//apply friction
	if (obj->type == OT__PLAYER)
	{
		//p_moveX *= 24;
		///p_moveY *= 24;

		obj->vel_x += p_moveX *delta;
		obj->vel_y += p_moveY *delta;

		obj->vel_x *= friction;
		obj->vel_y *= friction;

		
		obj->vel_x = p_moveX * max_speed;
		obj->vel_y = p_moveY * max_speed;
	}
	else
	{
		obj->vel_x = p_moveX * max_speed;
		obj->vel_y = p_moveY * max_speed;
	}

	obj->vel_x = Math_Clamp(obj->vel_x, -max_speed, max_speed);
	obj->vel_y = Math_Clamp(obj->vel_y, -max_speed, max_speed);

	//nothing to move
	if (Math_IsZeroApprox(obj->vel_x) && Math_IsZeroApprox(obj->vel_y))
	{
		return false;
	}

	if (p_slide)
	{
		//returns true if we moved the full ammount
		moved = Move_ClipMove(obj, obj->vel_x, obj->vel_y);
	}
	else
	{
		//return true if we moved
		moved = Move_SetPosition(obj, obj->x + obj->vel_x, obj->y + obj->vel_y, obj->size);
	}

	return moved;
}

bool Move_Teleport(Object* obj, float x, float y)
{


	return false;
}
bool Move_Unstuck(Object* obj)
{
	for (int i = 0; i < 8; i++)
	{
		float x_random = Math_randf();
		float y_random = Math_randf();

		int x_sign = (rand() % 100 > 50) ? 1 : -1;
		int y_sign = (rand() % 100 > 50) ? 1 : -1;

		if (Move_Object(obj, x_random * x_sign, y_random * y_sign, Engine_GetDeltaTime(), false))
		{
			return true;
		}
	}

	return false;
}

bool Move_Sector(Sector* sector, float p_moveFloor, float p_moveCeil, float p_minFloorClamp, float p_maxFloorFlamp, float p_minCeilClamp, float p_maxCeilClamp, bool p_crush)
{
	//try to clip the move, so that we get back to proper height
	if (sector->floor + p_moveFloor > sector->ceil)
	{
		float remainder = sector->ceil - sector->floor;
		
		p_moveFloor = remainder;
		p_moveCeil = 0;
	}
	else if (sector->ceil + p_moveCeil < sector->floor)
	{
		float remainder = sector->floor - sector->ceil;

		p_moveCeil = remainder;
		p_moveFloor = 0;
	}

	//no movement
	if (p_moveFloor == 0 && p_moveCeil == 0)
	{
		return false;
	}

	//get all objects
	int num_objs = Trace_SectorObjects(sector);
	int* indices = Trace_GetHitObjects();

	float next_floor = sector->floor + p_moveFloor;
	float next_ceil = sector->ceil + p_moveCeil;

	next_floor = Math_Clamp(next_floor, p_minFloorClamp, p_maxFloorFlamp);
	next_ceil = Math_Clamp(next_ceil, p_minCeilClamp, p_maxCeilClamp);

	if (next_floor == sector->floor && next_ceil == sector->ceil)
	{
		return true;
	}

	for (int i = 0; i < num_objs; i++)
	{
		int index = indices[i];
		if (index < 0 || sector->sector_object == index)
		{
			continue;
		}

		Object* obj = Map_GetObject(index);

		if (obj->sector_index != sector->index)
		{
			continue;
		}

		Move_SetPosition(obj, obj->x, obj->y, obj->size);

		if (obj->hp <= 0)
		{
			continue;
		}

		//obj will be not be crushed
		if (next_ceil - next_floor >= obj->height)
		{
			continue;
		}

		if (p_crush)
		{
			//can't crash this so don't move any further
			if (!Object_Crush(obj))
			{
				return false;
			}
		}
		else
		{
			//dont move' any further
			return false;
		}
	}

	sector->floor = next_floor;
	sector->ceil = next_ceil;


	return true;
}