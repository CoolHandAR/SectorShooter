#include "g_common.h"

#include "game_info.h"
#include "u_math.h"
#include "sound.h"

#define TOO_CLOSE_DISTANCE 256

bool Object_HandleSwitch(Object* obj)
{
	if (obj->type != OT__TRIGGER)
	{
		return false;
	}
	if (obj->sub_type != SUB__TRIGGER_SWITCH)
	{
		return false;
	}

	Object* target = obj->target;

	if (target)
	{
		//only allow switching if door is fully closed or opened
		if (target->type == OT__DOOR)
		{
			if (target->move_timer > 0 && target->move_timer < 1)
			{
				return false;
			}
		}
	}

	obj->flags ^= OBJ_FLAG__TRIGGER_SWITCHED_ON;

	return true;
}

void Object_HandleTriggers(Object* obj, Object* trigger)
{
	//not a trigger
	if (trigger->type != OT__TRIGGER)
	{
		return;
	}

	if (trigger->sub_type == SUB__TRIGGER_SWITCH)
	{
		if (!Object_HandleSwitch(trigger))
		{
			return;
		}
	}

	Object* target = trigger->target;

	if (target)
	{
		if (target->type == OT__DOOR)
		{
			// 0 == door open
			// 1 == door closed

			//door is open
			if (target->move_timer == 0)
			{
				//close the door
				target->state = SECTOR_CLOSE;
			}
			//door is closed
			else if (target->move_timer == 1)
			{
				//open the door
				target->state = SECTOR_OPEN;
			}

			if (trigger->sub_type == SUB__TRIGGER_ONCE)
			{
				target->flags |= OBJ_FLAG__DOOR_NEVER_CLOSE;
			}
		}
	}
	//some do not have targets
	else
	{
		if (obj->type == OT__PLAYER)
		{
			if (trigger->sub_type == SUB__TRIGGER_CHANGELEVEL)
			{
				//Game_ChangeLevel();
			}
			else if (trigger->sub_type == SUB__TRIGGER_SECRET)
			{
				Game_SecretFound();
			}
		}

	}

	//delete the object if it triggers once
	if (trigger->sub_type == SUB__TRIGGER_ONCE || trigger->sub_type == SUB__TRIGGER_SECRET)
	{
		Map_DeleteObject(trigger);
	}
}

bool Object_HandleObjectCollision(Object* obj, Object* collision_obj)
{
	//return true if we can move

	if (!collision_obj)
	{
		return true;
	}

	//ignore self
	if (obj == collision_obj)
	{
		return true;
	}

	//some objects are ignored from collisions
	if (collision_obj->type == OT__PARTICLE || collision_obj->type == OT__DECAL || collision_obj->hp <= 0)
	{
		return true;
	}

	switch (collision_obj->type)
	{
	case OT__DOOR:
	{

		//if door is moving in any way, don't move into it
		if (collision_obj->state != SECTOR_SLEEP)
		{
			return false;
		}
		//door is closed, no movement
		if (collision_obj->move_timer >= 1)
		{
			return false;
		}
		//door is completely opened, move
		else if (collision_obj->move_timer <= 0)
		{
			return true;
		}

		break;
	}
	case OT__MONSTER:
	{
		if (obj->type == OT__PLAYER)
		{
			//check z 
			if (!Object_ZPassCheck(obj, collision_obj))
			{
				return false;
			}
			else
			{
				return true;
			}
		}
		//don't collide with monsters of same type
		else if (obj->type == OT__MONSTER && obj->sub_type == collision_obj->sub_type)
		{
			return true;
		}

		break;
	}
	case OT__PICKUP:
	{
		//check z 
		if (Object_ZPassCheck(obj, collision_obj))
		{
			return true;
		}
		//pickup pickups for player
		if (obj->type == OT__PLAYER)
		{
			Player_HandlePickup(collision_obj);
		}

		//pickups dont block movement
		return true;

		break;
	}
	case OT__TRIGGER:
	{
		if (collision_obj->sub_type == SUB__TRIGGER_SWITCH)
		{
			return false;
		}
		else
		{
			Object_HandleTriggers(obj, collision_obj);

			//dont block movement
			return true;
		}

		break;
	}
	case OT__MISSILE:
	{
		Object* collision_owner = collision_obj->owner;

		//dont collide if we are the owner
		if (collision_owner == obj)
		{
			return true;
		}
		//dont collide if we are a missile
		if (obj->type == OT__MISSILE)
		{
			return true;
		}

		//dont collide with monster of same type
		if (collision_owner)
		{
			if (collision_owner->type == OT__MONSTER && obj->type == OT__MONSTER && collision_owner->sub_type == obj->sub_type)
			{
				return true;
			}
		}

		//check z 
		if (Object_ZPassCheck(obj, collision_obj))
		{
			return true;
		}

		//perform direct hit damage
		Missile_DirectHit(collision_obj, obj);

		//explode the missile
		Missile_Explode(collision_obj);

		//missiles dont block movement
		return true;

		break;
	}
	case OT__LIGHT:
	{
		if (collision_obj->sub_type == SUB__LIGHT_LAMP)
		{
			return true;
		}

		break;
	}
	default:
		break;
	}

	//we are a missile
	if (obj->type == OT__MISSILE)
	{
		Object* owner = obj->owner;

		//dont collide if we are the owner
		if (owner == collision_obj)
		{
			return true;
		}
		//dont collide if hit a missile
		if (collision_obj->type == OT__MISSILE)
		{
			return true;
		}

		if (owner)
		{
			//dont collide with monster of same type
			if (collision_obj->type == OT__MONSTER && owner->type == OT__MONSTER && collision_obj->sub_type == owner->sub_type)
			{
				return true;
			}
		}
		//check z 
		if (Object_ZPassCheck(obj, collision_obj))
		{
			return true;
		}


		//if we have collided with player or a monster
		//perform direct hit damage
		Missile_DirectHit(obj, collision_obj);
	}

	//we can't move
	return false;
}
bool Object_CheckLineToTarget(Object* obj, Object* target)
{
	if (!obj || !target)
	{
		return false;
	}

	if (!Map_CheckSectorReject(obj->sector_index, target->sector_index))
	{
		return false;
	}

	return Trace_CheckLineToTarget(obj, target);
}

bool Object_CheckSight(Object* obj, Object* target, bool check_angle)
{
	if (!obj || !target)
	{
		return false;
	}

	//calc distance to target
	float d = Math_XY_Distance(obj->x, obj->y, target->x, target->y);

	//very close to target
	if (d <= TOO_CLOSE_DISTANCE)
	{
		return true;
	}

	float x_point = obj->x - target->x;
	float y_point = obj->y - target->y;

	if (check_angle)
	{
		//make sure the object is facing the target
		float target_to_obj_angle = Math_XY_Angle(target->x - obj->x, target->y - obj->y);
		float obj_angle = Math_XY_Angle(obj->dir_x, obj->dir_y);

		int rot = Math_RadToDeg(target_to_obj_angle - obj_angle);

		if (rot < -90 || rot > 90)
		{
			return false;
		}
	}

	//check direct line, 
	if (!Object_CheckLineToTarget(obj, target))
	{
		return false;
	}

	return true;
}


void Object_RemoveSectorsFromLinkedArray(Object* obj)
{
	if (!obj->linked_sector_array)
	{
		return;
	}

	for (int i = 0; i < dA_size(obj->linked_sector_array); i++)
	{
		short* id = dA_at(obj->linked_sector_array, i);

		if (*id >= 0)
		{
			Sector* sector = Map_GetSector(*id);

			Sector_RemoveObjectFromRenderList(sector, obj);
		}
	}

	dA_clear(obj->linked_sector_array);
}

void Object_AddSectorToLinkedArray(Object* obj, Sector* sector)
{
	if (!obj->linked_sector_array)
	{
		obj->linked_sector_array = dA_INIT(short, 1);
	}

	short* id = dA_emplaceBack(obj->linked_sector_array);

	*id = (short)sector->index;
}

void Object_UpdateRenderSectors(Object* obj)
{
	//no need to link invisible objects
	if (obj->sector_index < 0 || !obj->sprite.img)
	{
		return;
	}

	Object_RemoveSectorsFromLinkedArray(obj);

	float bbox[2][2];

	float width = obj->sprite.scale_x / 2.0;
	float height = obj->sprite.scale_y / 2.0;

	bbox[0][0] = obj->x - width;
	bbox[0][1] = obj->y - height;

	bbox[1][0] = obj->x + width;
	bbox[1][1] = obj->y + height;

	int sectors_found = Trace_FindSectors(obj->sector_index, bbox);
	int* hits = Trace_GetHitObjects();

	for (int i = 0; i < sectors_found; i++)
	{
		if (obj->sector_index == hits[i])
		{
			continue;
		}

		Sector* sector = Map_GetSector(hits[i]);

		Sector_AddObjectToRenderList(sector, obj);
		Object_AddSectorToLinkedArray(obj, sector);
	}

}

bool Object_ZPassCheck(Object* obj, Object* col_obj)
{
	//our bottom will not hit top of their body,
	if (obj->z > col_obj->z + col_obj->height)
	{
		return true;
	}
	//our top will not hit bottom of their body
	if (obj->z + obj->height < col_obj->z)
	{
		return true;
	}

	return false;
}

void Object_UnlinkSector(Object* obj)
{
	if (!obj->sector_linked)
	{
		return;
	}

	Object* sector_prev = obj->sector_prev;
	Object* sector_next = obj->sector_next;

	if (sector_next)
	{
		sector_next->sector_prev = obj->sector_prev;
	}
	if (sector_prev)
	{
		sector_prev->sector_next = obj->sector_next;
	}
	else if(obj->sector_index >= 0)
	{
		//set to head
		Map* map = Map_GetMap();
		Sector* sector = &map->sectors[obj->sector_index];

		if (sector->object_list == obj)
		{
			sector->object_list = obj->sector_next;
		}
	}

	obj->sector_linked = false;
	obj->sector_next = NULL;
	obj->sector_prev = NULL;
}

void Object_LinkSector(Object* obj)
{
	assert(!obj->sector_linked && "Object already linked\n");

	//no need to link invisible objects
	if (obj->sector_index < 0 || !obj->sprite.img)
	{
		return;
	}

	Map* map = Map_GetMap();
	Sector* sector = &map->sectors[obj->sector_index];

	obj->sector_prev = NULL; //head
	obj->sector_next = sector->object_list;

	if (sector->object_list)
	{
		sector->object_list->sector_prev = obj;
	}

	sector->object_list = obj;

	obj->sector_linked = true;
}

bool Object_IsSectorLinked(Object* obj)
{
	if (obj->sector_linked)
	{
		return true;
	}

	if (obj->sector_prev)
	{
		return true;
	}
	if (obj->sector_next)
	{
		return true;
	}
	else if (obj->sector_index >= 0)
	{
		Sector* sector = Map_GetSector(obj->sector_index);

		if (sector->object_list == obj)
		{
			return true;
		}
	}

	return false;
}

void Object_Hurt(Object* obj, Object* src_obj, int damage, bool explosive)
{
	//already dead or no damage
	if (obj->hp <= 0 || damage <= 0)
	{
		return;
	}
	//is godmode
	if (obj->flags & OBJ_FLAG__GODMODE)
	{
		return;
	}

	if (src_obj)
	{
		//dont hurt ourselves
		if (src_obj->owner == obj)
		{
			return;
		}

		//if hurt by a player or monster, set as target
		if (src_obj->type == OT__PLAYER || src_obj->type == OT__MONSTER)
		{
			if (src_obj->sub_type != obj->sub_type)
			{
				obj->target = src_obj;
			}
		}
		//if hurt by a missile target the missiles owner
		else if (src_obj->type == OT__MISSILE)
		{
			Object* owner = src_obj->owner;

			if (owner && (owner->type == OT__PLAYER || owner->type == OT__MONSTER) && owner->sub_type != obj->sub_type)
			{
				obj->target = owner;
			}
		}
	}

	obj->hp -= damage;

	//set hurt state
	if (obj->hp > 0)
	{
		if (obj->type == OT__MONSTER)
		{
			Monster_ApplyPushback(src_obj, obj);
			Monster_SetState(obj, MS__HIT);
		}
		else if (obj->type == OT__PLAYER)
		{
			float dir_x = 0;
			float dir_y = 0;

			Player_Hurt(dir_x, dir_y);
		}
		return;
	}

	//set die state
	if (obj->type == OT__MONSTER)
	{
		Game_GetGame()->monsters_killed++;

		if (explosive)
		{
			obj->flags |= OBJ_FLAG__EXPLODING;

			Object_Spawn(OT__PARTICLE, SUB__PARTICLE_EXPLOSION, obj->x + Math_randf() * 2.0 - 1.0, obj->y + Math_randf() * 2.0 - 1.0, (obj->z + obj->height * 0.5) + Math_randf() * 2.0 - 1.0);

			const int NUM_BLOOD_TRACES = 12;

			for (int i = 0; i < NUM_BLOOD_TRACES; i++)
			{
				float angle = Math_DegToRad(NUM_BLOOD_TRACES) * i;
				
				Decal_BloodTrace(obj, obj->x, obj->y, obj->z + obj->height * 0.5, cos(angle), sin(angle), Math_randf() * 2.0 - 1.0);
			}
		}

		Monster_SetState(obj, MS__DIE);

		Object_Spawn(OT__PARTICLE, SUB__PARTICLE_BLOOD_EXPLOSION, obj->x + Math_randf() * 2.0 - 1.0, obj->y + Math_randf() * 2.0 - 1.0, (obj->z + obj->height * 0.5) + Math_randf() * 2.0 - 1.0);
	}
	else if (obj->type == OT__PLAYER)
	{
		float dir_x = 0;
		float dir_y = 0;

		Player_Hurt(dir_x, dir_y);
	}
}

bool Object_Crush(Object* obj)
{
	if (obj->hp <= 0)
	{
		return true;
	}

	if (obj->type == OT__PLAYER || obj->type == OT__MONSTER)
	{
		Object_Hurt(obj, NULL, 100000, true);

		return true;
	}
	else if (obj->type == OT__THING)
	{
		return false;
	}


	return true;
}

Object* Object_Spawn(ObjectType type, SubType sub_type, float x, float y, float z)
{
	GameAssets* assets = Game_GetAssets();

	if (type == OT__NONE)
	{
		return NULL;
	}

	Object* obj = Map_NewObject(type);

	if (!obj)
	{
		return NULL;
	}

	obj->prev_x = x;
	obj->prev_y = y;
	obj->prev_z = z;

	obj->x = x;
	obj->y = y;
	obj->z = z;

	obj->sub_type = sub_type;

	//init defaults
	obj->sprite.decal_line_index = -1;
	obj->height = 70;
	obj->step_height = 50;
	obj->size = 22;
	obj->spatial_id = -1;

	bool assign_to_spatial_tree = false;
	bool handle_position = true;

	float zmove = -1000;

	switch (type)
	{
	case OT__PLAYER:
	{
		obj->size = PLAYER_SIZE;
		assign_to_spatial_tree = true;
		break;
	}
	case OT__MONSTER:
	{
		assign_to_spatial_tree = true;

		Monster_Spawn(obj);
		Game_GetGame()->total_monsters++;

		break;
	}
	//fallthrough
	case OT__LIGHT:
	case OT__PICKUP:
	case OT__THING:
	{
		ObjectInfo* object_info = Info_GetObjectInfo(obj->type, obj->sub_type);
		AnimInfo* anim_info = &object_info->anim_info;

		//SETUP SPRITE
		obj->sprite.img = &assets->object_textures;
		obj->sprite.anim_speed_scale = object_info->anim_speed;
		obj->sprite.frame_count = anim_info->frame_count;
		obj->sprite.looping = anim_info->looping;
		obj->sprite.frame_offset_x = anim_info->x_offset;
		obj->sprite.frame_offset_y = anim_info->y_offset;
		obj->sprite.offset_x = object_info->sprite_offset_x;
		obj->sprite.offset_y = object_info->sprite_offset_y;
		obj->sprite.offset_z = object_info->sprite_offset_z;
		obj->sprite.scale_x = object_info->sprite_scale;
		obj->sprite.scale_y = object_info->sprite_scale;
		obj->sprite.playing = true;
		
		obj->size = object_info->size;
		obj->height = object_info->height;

		if (type == OT__PICKUP)
		{
			assign_to_spatial_tree = true;
		}
		else if (object_info->collidable && object_info->size > 0)
		{
			assign_to_spatial_tree = true;
		}

		if (type == OT__LIGHT)
		{
			obj->sound_id = Sound_EmitWorld(SOUND__FIRE, obj->x, obj->y, obj->z, 0, 0, 0, 0.025);
			Sound_SetRolloff(obj->sound_id, 0.5);

			obj->flags |= OBJ_FLAG__FULL_BRIGHT;
		}

		break;
	}
	case OT__TRIGGER:
	{
		Object* target = obj->target;

		if (obj->sub_type == SUB__TRIGGER_SECRET)
		{
			Game_GetGame()->total_secrets++;
		}
		break;
	}
	case OT__PARTICLE:
	{
		ParticleInfo* particle_info = Info_GetParticleInfo(sub_type);
		AnimInfo* anim_info = &particle_info->anim_info;

		obj->size = 0.5;

		obj->sprite.img = &assets->particle_textures;
		obj->move_timer = particle_info->time;
		obj->sprite.scale_x = 0.5;
		obj->sprite.scale_y = 0.5;

		obj->sprite.frame_count = anim_info->frame_count;
		obj->sprite.frame_offset_x = anim_info->x_offset;
		obj->sprite.frame_offset_y = anim_info->y_offset;
		obj->sprite.looping = anim_info->looping;

		obj->sprite.scale_x = particle_info->sprite_scale;
		obj->sprite.scale_y = particle_info->sprite_scale;

		if (sub_type == SUB__PARTICLE_BLOOD)
		{
			if (obj->sprite.frame_count > 0)
			{
				obj->sprite.frame = rand() % obj->sprite.frame_count - 1;
			}
		}
		else
		{
			obj->sprite.playing = true;
			obj->sprite.anim_speed_scale = particle_info->anim_speed_scale;
		}
			 

		obj->flags |= OBJ_FLAG__IGNORE_POSITION_CHECK;
		zmove = 0;

		break;
	}
	case OT__DECAL:
	{
		DecalInfo* decal_info = Info_GetDecalInfo(sub_type);
		AnimInfo* anim_info = &decal_info->anim_info;

		obj->size = 0.5;

		obj->sprite.img = &assets->decal_textures;
		obj->move_timer = decal_info->time;
		obj->sprite.scale_x = 0.5;
		obj->sprite.scale_y = 0.5;

		obj->sprite.frame_count = anim_info->frame_count;
		obj->sprite.frame_offset_x = anim_info->x_offset;
		obj->sprite.frame_offset_y = anim_info->y_offset;
		obj->sprite.looping = anim_info->looping;

		obj->sprite.scale_x = decal_info->sprite_scale;
		obj->sprite.scale_y = decal_info->sprite_scale;
		obj->sprite.anim_speed_scale = 0;

		if (obj->sprite.frame_count > 0)
		{
			obj->sprite.frame = rand() % (obj->sprite.frame_count - 1);
		}

		obj->flags |= OBJ_FLAG__IGNORE_POSITION_CHECK;
		obj->flags |= OBJ_FLAG__FULL_BRIGHT;
		zmove = 0;

		break;
	}
	case OT__MISSILE:
	{
		obj->owner = obj;
		obj->hp = 1;
		obj->flags |= OBJ_FLAG__FULL_BRIGHT;
		obj->sprite.img = &assets->missile_textures;
		obj->size = 5.25;

		//Object_missile will hand the position
		handle_position = false;
		break;
	}
	case OT__DOOR:
	{
		obj->state = SECTOR_SLEEP;
		handle_position = false;
		break;
	}
	case OT__LIGHT_STROBER:
	case OT__LIFT:
	case OT__CRUSHER:
	{
		handle_position = false;
		break;
	}
	case OT__SFX_EMITTER:
	{
		SFXInfo* sfx_info = Info_GetSFXInfo(obj->sub_type);

		if (sfx_info)
		{
			obj->sound_id = Sound_EmitWorld(sfx_info->sound_type, obj->x, obj->y, obj->z, 0, 0, 0, sfx_info->volume);

			Sound_SetRolloff(obj->sound_id, sfx_info->rolloff);
		}
		break;
	}
	default:
		break;
	}

	if (assign_to_spatial_tree)
	{
		float box[2][2];
		Math_SizeToBbox(x, y, obj->size, box);

		Map* map = Map_GetMap();

		obj->spatial_id = BVH_Tree_Insert(&map->spatial_tree, box, obj->id);
	}

	if (handle_position)
	{
		Move_SetPosition(obj, x, y, obj->size);
		Move_ZMove(obj, zmove);

		//make sure the sound is located at the correct z coord
		if (obj->sound_id >= 0)
		{
			Sound_SetTransform(obj->sound_id, obj->x, obj->y, obj->z, 0, 0, 0);
		}
	}

	if (obj->flags & OBJ_FLAG__FULL_BRIGHT)
	{
		if (obj->type == OT__LIGHT)
		{
			LightInfo* light_info = Info_GetLightInfo(obj->sub_type);

			if (light_info)
			{
				obj->sprite.light.r = light_info->color[0];
				obj->sprite.light.g = light_info->color[1];
				obj->sprite.light.b = light_info->color[2];
			}
		}
		else
		{
			obj->sprite.light.r = 255;
			obj->sprite.light.g = 255;
			obj->sprite.light.b = 255;
		}
	}


	obj->sprite.x = obj->x;
	obj->sprite.y = obj->y;
	obj->sprite.z = obj->z;

	return obj;
}

int Object_AreaEffect(Object* obj, float radius)
{
	return Trace_AreaObjects(obj, obj->x, obj->y, radius);
}

void Object_ConsumePickup(Object* obj)
{
	if (obj->type != OT__PICKUP)
	{
		return;
	}

	if (Object_IsSectorLinked(obj) || obj->linked_sector_array)
	{
		Render_LockObjectMutex(true);

		Object_UnlinkSector(obj);

		if (obj->linked_sector_array)
		{
			Object_RemoveSectorsFromLinkedArray(obj);

			dA_Destruct(obj->linked_sector_array);
			obj->linked_sector_array = NULL;
		}

		Render_UnlockObjectMutex(true);
	}

	//keep for the save file
	obj->hp = 0;
	obj->sprite.img = NULL;
}
