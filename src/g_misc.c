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