#include "g_common.h"

#include "game_info.h"

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

	//make it follow target
	if (obj->sub_type == SUB__MISSILE_STARSTRIKE && obj->target)
	{
		Object* target = obj->target;

		//make sure it's alive
		if (target->hp >= 0)
		{
			float dx = (target->x) - obj->x;
			float dy = (target->y) - obj->y;
			float dz = (target->z + target->height * 0.5) - (obj->z);

			Math_XYZ_Normalize(&dx, &dy, &dz);

			obj->dir_x = dx;
			obj->dir_y = dy;
			obj->dir_z = dz;
		}
	}

	float speed = (obj->speed) * delta;

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
	//spawn mini versions of the missile if we hit object
	else if (obj->collision_hit >= 0 && obj->sub_type == SUB__MISSILE_MEGASHOT && !(obj->flags & OBJ_FLAG__MINI_MISSILE))
	{
		Object* coll_object = Map_GetObject(obj->collision_hit);

		if (coll_object && coll_object->type == OT__MONSTER)
		{
			const int NUM_MISSILES = 24;

			for (int i = 0; i < NUM_MISSILES; i++)
			{
				Object* missile = Object_Missile(obj, NULL, SUB__MISSILE_MEGASHOT);
				if (!missile)
				{
					break;
				}

				float angle = Math_DegToRad(NUM_MISSILES) * i;
				missile->dir_x = cos(angle);
				missile->dir_y = sin(angle);

				missile->z = obj->z;
				missile->sprite.z = missile->z;

				//scale to half
				missile->size *= 0.5;
				missile->speed *= 0.5;
				missile->sprite.scale_x *= 0.5;
				missile->sprite.scale_y *= 0.5;

				missile->flags |= OBJ_FLAG__MINI_MISSILE;

				missile->owner = obj->owner;
			}
		}
	}

	obj->flags |= OBJ_FLAG__EXPLODING;

	obj->sprite.playing = true;
}

Object* Object_Missile(Object* obj, Object* target, int type)
{
	const MissileInfo* missile_info = Info_GetMissileInfo(type);

	if (!missile_info)
	{
		return;
	}

	Object* missile = Object_Spawn(OT__MISSILE, type, obj->x, obj->y, obj->z);

	if (!missile)
	{
		return NULL;
	}


	float dir_x = obj->dir_x;
	float dir_y = obj->dir_y;
	float dir_z = obj->dir_z;

	//if we have a target calc dir to target
	if (target)
	{
		float point_x = (target->x) - obj->x;
		float point_y = (target->y) - obj->y;
		float point_z = (target->z + target->height * 0.5) - (obj->z + obj->height * 0.5);

		Math_XYZ_Normalize(&point_x, &point_y, &point_z);

		dir_x = point_x;
		dir_y = point_y;
		dir_z = point_z;
	}
	missile->target = target;
	missile->owner = obj;
	missile->x = (obj->x) + dir_x * 2.0;
	missile->y = (obj->y) + dir_y * 2.0;
	missile->z = (obj->z + 64);
	missile->dir_x = dir_x;
	missile->dir_y = dir_y;
	missile->dir_z = dir_z;
	missile->size = missile_info->size;
	missile->height = missile_info->size;
	missile->speed = missile_info->speed;
	missile->step_height = 0;
	missile->sprite.playing = true;
	missile->sprite.scale_x = missile_info->sprite_scale;
	missile->sprite.scale_y = missile_info->sprite_scale;

	missile->sound_id = Sound_EmitWorld(SOUND__FIREBALL_FOLLOW, missile->x, missile->y, missile->z, missile->dir_x, missile->dir_y, missile->dir_z, 0.025);

	Sound_EmitWorldTemp(SOUND__FIREBALL_THROW, missile->x, missile->y, missile->z, missile->dir_x, missile->dir_y, missile->dir_z, 1);

	Move_SetPosition(missile, missile->x, missile->y, missile->size);

	return missile;
}