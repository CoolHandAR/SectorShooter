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

void Missile_Update(Object* obj, double delta)
{
	bool exploding = obj->flags & OBJ_FLAG__EXPLODING;

	const MissileInfo* missile_info = Info_GetMissileInfo(obj->sub_type);
	const AnimInfo* anim_info = &missile_info->anim_info[(int)exploding];

	obj->sprite.anim_speed_scale = 1.0;
	obj->sprite.frame_count = anim_info->frame_count;
	obj->sprite.frame_offset_x = anim_info->x_offset;
	obj->sprite.frame_offset_y = anim_info->y_offset;
	obj->sprite.looping = anim_info->looping;
	obj->sprite.scale_x = missile_info->sprite_scale;
	obj->sprite.scale_y = missile_info->sprite_scale;

	//the explosion animation is still playing
	if (exploding)
	{
		obj->move_timer += delta;
		//the animation is finished or timer has passes, delete the object
		if (!obj->sprite.playing || obj->move_timer > 20)
		{
			Map_DeleteObject(obj);
		}
		return;
	}

	float speed = (missile_info->speed * 32) * delta;

	obj->vel_x = obj->dir_x * speed;
	obj->vel_y = obj->dir_y * speed;
	obj->vel_z = obj->dir_z * speed;

	if (Move_SetPosition(obj, obj->x + obj->vel_x, obj->y + obj->vel_y, obj->size) && Move_ZMove(obj, obj->vel_z))
	{
		//we have moved fully

		//update sound position
		Sound_SetTransform(obj->sound_id, obj->x, obj->y, obj->z, obj->dir_x, obj->dir_y, obj->dir_z);

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
		Object_Hurt(target, obj, missile_info->direct_damage, true);
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
	Sound_EmitWorldTemp(SOUND__FIREBALL_EXPLODE, obj->x, obj->y, obj->z, 0, 0, 0, 1);

	//place scorch decal if we hit a line
	if (obj->collision_hit < 0)
	{
		int line_index = -(obj->collision_hit + 1);
		Object* decal = Object_Spawn(OT__DECAL, SUB__DECAL_SCORCH, obj->x, obj->y, obj->z);

		if (decal)
		{
			decal->sprite.decal_line_index = line_index;
		}
	}

	obj->flags |= OBJ_FLAG__EXPLODING;

	obj->sprite.playing = true;
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