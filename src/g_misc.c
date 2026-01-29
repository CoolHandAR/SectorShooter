#include "g_common.h"

#include "game_info.h"

void Particle_Spawn(int sub_type, float x, float y, float z)
{
	switch (sub_type)
	{
	default:
		break;
	}
}

void Particle_Update(Object* obj, float delta)
{
	switch (obj->sub_type)
	{
	case SUB__PARTICLE_BLOOD:
	{
		obj->vel_z -= 20 * delta;

		Move_ZMove(obj, obj->vel_z);
		break;
	}
	case SUB__PARTICLE_WALL_HIT:
	{
		obj->vel_z += 5 * delta;

		Move_ZMove(obj, obj->vel_z);
		break;
	}
	case SUB__PARTICLE_BLOOD_EXPLOSION:
	{
		break;
	}
	default:
		break;
	}

	obj->move_timer -= delta;

	if (obj->move_timer < 0)
	{
		Map_DeleteObject(obj);
	}
}
void Decal_BloodTrace(Object* obj, float x, float y, float z, float p_dir_x, float p_dir_y, float dir_z)
{
	p_dir_x *= BLOOD_SPLATTER_TRACE_DIST;
	p_dir_y *= BLOOD_SPLATTER_TRACE_DIST;

	float frac = 0;
	float inter_x = 0;
	float inter_y = 0;
	float inter_z = 0;

	int hit = Trace_AttackLine(obj, x, y, x + p_dir_x, y + p_dir_y, z, BLOOD_SPLATTER_TRACE_DIST, &inter_x, &inter_y, &inter_z, &frac);

	if (hit == TRACE_NO_HIT || hit >= 0)
	{
		return;
	}

	float dist = Math_XY_Distance(inter_x, inter_y, x, y);

	//way too far away
	if (dist >= BLOOD_SPLATTER_TRACE_DIST)
	{
		return;
	}

	//hit a wall
	//spawn blood decal
	Object* decal = Object_Spawn(OT__DECAL, SUB__DECAL_BLOOD_SPLATTER, inter_x, inter_y, inter_z);

	if (decal) decal->sprite.decal_line_index = -(hit + 1);
}
void Decal_Update(Object* obj, float delta)
{
	obj->move_timer -= delta;

	if (obj->move_timer < 0)
	{
		Map_DeleteObject(obj);
	}
}

void SFX_Update(Object* obj)
{
	Object* player = Player_GetObj();

	if (!player || obj->sound_id <= 0)
	{
		return;
	}

	if (Map_CheckSectorReject(obj->sector_index, player->sector_index))
	{
		Sound_Play(obj->sound_id);
	}
	else
	{
		Sound_Stop(obj->sound_id);
	}
}