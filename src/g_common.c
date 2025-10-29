#include "g_common.h"

#include <string.h>

#include "game_info.h"
#include "u_math.h"

#include "sound.h"
#include "main.h"


bool Check_CanObjectFitInSector(Object* obj, Sector* sector, Sector* backsector)
{
	if (!backsector)
	{
		return false;
	}


	float open_low = max(sector->floor, backsector->floor);
	float open_high = min(sector->ceil, backsector->ceil);
	float open_range = open_high - open_low;

	//can't fit at all
	if (open_range < obj->height)
	{
		return false;
	}
	//you will bonk your head
	if (open_high < obj->z + obj->height)
	{
		return false;
	}
	//you are too short for stepping over
	if (open_low - obj->z > (obj->step_height))
	{
		return false;
	}
	
	return true;
}

void Missile_Update(Object* obj, float delta)
{
	bool exploding = obj->flags & OBJ_FLAG__EXPLODING;

	const MissileInfo* missile_info = Info_GetMissileInfo(obj->sub_type);
	const AnimInfo* anim_info = &missile_info->anim_info[(int)exploding];
	
	obj->sprite.anim_speed_scale = 1.0;
	obj->sprite.frame_count = anim_info->frame_count;
	obj->sprite.frame_offset_x = anim_info->x_offset;
	obj->sprite.frame_offset_y = anim_info->y_offset;
	obj->sprite.looping = anim_info->looping;
	obj->sprite.playing = true;

	Sprite_UpdateAnimation(&obj->sprite, delta);

	//the explosion animation is still playing
	if (exploding)
	{
		obj->move_timer += delta;
		//the animation is finished or timer has passes, delete the object
		if (!obj->sprite.playing || obj->move_timer > 10)
		{
			Map_DeleteObject(obj);
		}
		return;
	}


	float speed = (missile_info->speed * 32) * delta;

	Move_ZMove(obj, obj->dir_z * delta);

	if (Move_SetPosition(obj, obj->x + (obj->dir_x * speed), obj->y + (obj->dir_y * speed), obj->size))
	{
		//we have moved fully
		return;
	}

	//missile exploded
	Missile_Explode(obj);
}

void Missile_DirectHit(Object* obj, Object* target)
{
	//already exploded
	if (obj->flags & OBJ_FLAG__EXPLODING)
	{
		return;
	}

	if (obj->owner != target)
	{
		const MissileInfo* missile_info = Info_GetMissileInfo(obj->sub_type);
		Object_Hurt(target, obj, missile_info->direct_damage);
	}
}

void Missile_Explode(Object* obj)
{
	//already exploded
	if (obj->flags & OBJ_FLAG__EXPLODING)
	{
		return;
	}

	const MissileInfo* missile_info = Info_GetMissileInfo(obj->sub_type);

	//cause damage if any 
	if (missile_info->explosion_damage > 0)
	{
		Object_AreaEffect(obj, missile_info->explosion_size);
	}

	//play sound
	Sound_EmitWorldTemp(SOUND__FIREBALL_EXPLODE, obj->x, obj->y, obj->z, 0, 0, 0);

	obj->flags |= OBJ_FLAG__EXPLODING;
}



DirEnum DirVectorToDirEnum(int x, int y)
{
	if (x > 0)
	{
		if (y > 0)
		{
			return DIR_NORTH_WEST;
		}
		else if (y < 0)
		{
			return DIR_SOUTH_WEST;
		}
		else
		{
			return DIR_WEST;
		}
	}
	else if (x < 0)
	{
		if (y > 0)
		{
			return DIR_NORTH_EAST;
		}
		else if (y < 0)
		{
			return DIR_SOUTH_EAST;
		}
		else
		{
			return DIR_EAST;
		}
	}
	else if (x == 0)
	{
		if (y > 0)
		{
			return DIR_NORTH;
		}
		else if (y < 0)
		{
			return DIR_SOUTH;
		}
	}

	return DIR_NONE;
}

DirEnum DirVectorToRoundedDirEnum(int x, int y)
{
	if (x > 0)
	{
		return DIR_WEST;
	}
	else if (x < 0)
	{
		return DIR_EAST;
	}
	else if (x == 0)
	{
		if (y > 0)
		{
			return DIR_NORTH;
		}
		else if (y < 0)
		{
			return DIR_SOUTH;
		}
	}

	return DIR_NONE;
}

void DirEnumToDirEnumVector(DirEnum dir, DirEnum* r_x, DirEnum* r_y)
{
	if (dir == DIR_WEST || dir == DIR_NORTH_WEST || dir == DIR_SOUTH_WEST)
	{
		*r_x = DIR_WEST;
	}
	else if (dir == DIR_EAST || dir == DIR_NORTH_EAST || dir == DIR_SOUTH_EAST)
	{
		*r_x = DIR_EAST;
	}
	if (dir == DIR_NORTH || dir == DIR_NORTH_WEST || dir ==  DIR_NORTH_EAST)
	{
		*r_y = DIR_NORTH;
	}
	else if (dir == DIR_SOUTH || dir == DIR_SOUTH_WEST || dir == DIR_SOUTH_EAST)
	{
		*r_y = DIR_SOUTH;
	}
}

void Particle_Update(Object* obj, float delta)
{
	if (obj->sprite.img && obj->sprite.frame_count > 0 && obj->sprite.anim_speed_scale > 0)
	{
		Sprite_UpdateAnimation(&obj->sprite, delta);
	}

	if (obj->sub_type == SUB__PARTICLE_BLOOD)
	{
		Move_ZMove(obj, -200 * delta);
	}
	else
	{
		Move_ZMove(obj, 200 * delta);
	}

	obj->move_timer -= delta;

	if (obj->move_timer < 0)
	{
		Map_DeleteObject(obj);
	}
}

int BSP_GetNodeSide(BSPNode* node, float p_x, float p_y)
{
	if (!node->line_dx)
	{
		if (p_x <= node->line_x)
		{
			return node->line_dy > 0;
		}

		return node->line_dy < 0;
	}
	if (!node->line_dy)
	{
		if (p_y <= node->line_y)
		{
			return node->line_dx < 0;
		}
		return node->line_dx > 0;
	}

float dx = p_x - node->line_x;
float dy = p_y - node->line_y;

float left = node->line_dy * dx;
float right = dy * node->line_dx;

if (right < left)
{
	return 0;
}

return 1;
}

int Line_PointSide(Line* line, float p_x, float p_y)
{
	return (p_y - line->y0) * line->dx + (line->x0 - p_x) * line->dy > MATH_EQUAL_EPSILON;
}

int Line_BoxOnPointSide(Line* line, float p_bbox[2][2])
{
	int p1 = 0;
	int p2 = 0;

	if (line->dx == 0)
	{
		p1 = p_bbox[1][0] < line->x1;
		p2 = p_bbox[0][0] < line->x1;

		if (line->dy < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
	}
	else if (line->dy == 0)
	{
		p1 = p_bbox[0][1] > line->y1;
		p2 = p_bbox[1][1] > line->y1;

		if (line->dx < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
	}
	else if (line->dx * line->dy >= 0)
	{
		p1 = Line_PointSide(line, p_bbox[0][0], p_bbox[1][1]);
		p2 = Line_PointSide(line, p_bbox[1][0], p_bbox[0][1]);
	}
	else
	{
		p1 = Line_PointSide(line, p_bbox[1][0], p_bbox[1][1]);
		p2 = Line_PointSide(line, p_bbox[0][0], p_bbox[0][1]);
	}

	return (p1 == p2) ? p1 : -1;
}

float Line_Intercept(float p_x, float p_y, float p_dx, float p_dy, Line* line)
{
	float den = p_dy * line->dx - p_dx * line->dy;

	if (den == 0)
		return 0;

	float num = (p_x - line->x0) * p_dy + (line->y0 - p_y) * p_dx;

	return num / den;
}

float Line_InterceptLine(Line* line1, Line* line2)
{
	return Line_Intercept(line1->x0, line1->y0, line1->dx, line1->dy, line2);
}

bool Line_SegmentInterceptSegmentLine(Line* line1, Line* line2, float* r_frac, float* r_interX, float* r_interY)
{
	if (line1->dot <= 0)
	{
		return false;
	}

	float c_dx = line2->x0 - line1->x0;
	float c_dy = line2->y0 - line1->y0;

	float d_dx = line2->x1 - line1->x0;
	float d_dy = line2->y1 - line1->y0;

	float l0n_x = line1->dx / line1->dot;
	float l0n_y = line1->dy / line1->dot;

	float c_x = c_dx * l0n_x + c_dy * l0n_y;
	float c_y = c_dy * l0n_x - c_dx * l0n_y;

	float d_x = d_dx * l0n_x + d_dy * l0n_y;
	float d_y = d_dy * l0n_x - d_dx * l0n_y;

	if (c_y < (float)-CMP_EPSILON && d_y < (float)-CMP_EPSILON)
	{
		return false;
	}
	if (c_y > (float)CMP_EPSILON && d_y > (float)CMP_EPSILON)
	{
		return false;
	}

	if (Math_IsEqualApprox(c_y, d_y))
	{
		return false;
	}

	float pos = d_x + (c_x - d_x) * d_y / (d_y - c_y);

	if (pos < 0 || pos > 1)
	{
		return false;
	}

	if (r_frac) *r_frac = pos;
	if (r_interX) *r_interX = line1->x0 + line1->dx * pos;
	if (r_interY) *r_interY = line1->y0 + line1->dy * pos;

	return true;
}
