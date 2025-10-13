#include "g_common.h"

#include "game_info.h"
#include "u_math.h"
#include "sound.h"

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
		if (target->type == OT__TARGET)
		{
			switch (target->sub_type)
			{
			case SUB__TARGET_TELEPORT:
			{
				Sound_EmitWorldTemp(SOUND__TELEPORT, trigger->x, trigger->y, 0, 0);

				Move_Teleport(obj, target->x, target->y);
				break;
			}
			default:
				break;
			}

		}
		else if (target->type == OT__DOOR)
		{
			// 0 == door open
			// 1 == door closed

			//door is open
			if (target->move_timer == 0)
			{
				//close the door
				target->state = DOOR_CLOSE;
			}
			//door is closed
			else if (target->move_timer == 1)
			{
				//open the door
				target->state = DOOR_OPEN;
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
				Game_ChangeLevel();
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
	if (collision_obj->type == OT__PARTICLE || collision_obj->type == OT__TARGET || collision_obj->hp <= 0)
	{
		return true;
	}

	switch (collision_obj->type)
	{
	case OT__DOOR:
	{
		obj->col_object = collision_obj;

		//if door is moving in any way, don't move into it
		if (collision_obj->state != DOOR_SLEEP)
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
	case OT__SPECIAL_TILE:
	{
		if (collision_obj->sub_type == SUB__SPECIAL_TILE_FAKE)
		{
			return true;
		}

		return false;

		break;
	}
	case OT__PICKUP:
	{
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

		//if we have collided with player or a monster
		//perform direct hit damage
		Missile_DirectHit(obj, collision_obj);
	}

	//we can't move
	return false;
}
bool Object_CheckLineToTile(Object* obj, float target_x, float target_y)
{
	const int max_tiles = 0;

	float x_point = obj->x - target_x;
	float y_point = obj->y - target_y;

	float ray_dir_x = x_point;
	float ray_dir_y = y_point;

	//length of ray from one x or y-side to next x or y-side
	float delta_dist_x = (ray_dir_x == 0) ? 1e30 : fabs(1.0 / ray_dir_x);
	float delta_dist_y = (ray_dir_y == 0) ? 1e30 : fabs(1.0 / ray_dir_y);

	int map_x = (int)obj->x;
	int map_y = (int)obj->y;

	int target_tile_x = (int)target_x;
	int target_tile_y = (int)target_y;

	//we have already reached the target
	if (map_x == target_tile_x && map_y == target_tile_y)
	{
		return true;
	}

	//length of ray from current position to next x or y-side
	float side_dist_x = 0;
	float side_dist_y = 0;

	int step_x = 0;
	int step_y = 0;

	if (x_point > 0)
	{
		step_x = -1;
		side_dist_x = (obj->x - map_x) * delta_dist_x;
	}
	else
	{
		step_x = 1;
		side_dist_x = (map_x + 1.0 - obj->x) * delta_dist_x;
	}
	if (y_point > 0)
	{
		step_y = -1;
		side_dist_y = (obj->y - map_y) * delta_dist_y;
	}
	else
	{
		step_y = 1;
		side_dist_y = (map_y + 1.0 - obj->y) * delta_dist_y;
	}

	//perform DDA
	for (int step = 0; step < max_tiles; step++)
	{
		if (map_x == target_tile_x && map_y != target_tile_y)
		{
			side_dist_y += delta_dist_y;
			map_y += step_y;
		}
		else if (map_y == target_tile_y && map_x != target_tile_x)
		{
			side_dist_x += delta_dist_x;
			map_x += step_x;
		}
		else if (map_x != target_tile_x && map_y != target_tile_y)
		{
			if (side_dist_x < side_dist_y)
			{
				side_dist_x += delta_dist_x;
				map_x += step_x;
			}
			else
			{
				side_dist_y += delta_dist_y;
				map_y += step_y;
			}
		}

		//we have reached the target
		if (map_x == target_tile_x && map_y == target_tile_y)
		{
			break;
		}

		//tile blocks the line
		if (Map_GetTile(map_x, map_y) != EMPTY_TILE)
		{
			return false;
		}

		//check for blocking objects
		Object* tile_obj = Map_GetObjectAtTile(map_x, map_y);

		if (!tile_obj)
		{
			continue;
		}

		//tile blocks the line
		if (Object_IsSpecialCollidableTile(tile_obj))
		{
			return false;
		}

		//thing blocks the line
		if (tile_obj->type == OT__THING)
		{
			return false;
		}
	}

	return true;
}

bool Object_CheckLineToTile2(Object* obj, float target_x, float target_y)
{
	const int max_tiles = 0;

	float x_point = obj->x - target_x;
	float y_point = obj->y - target_y;

	float ray_dir_x = x_point;
	float ray_dir_y = y_point;

	//length of ray from one x or y-side to next x or y-side
	float delta_dist_x = (ray_dir_x == 0) ? 1e30 : fabs(1.0 / ray_dir_x);
	float delta_dist_y = (ray_dir_y == 0) ? 1e30 : fabs(1.0 / ray_dir_y);

	int map_x = (int)obj->x;
	int map_y = (int)obj->y;

	int target_tile_x = (int)target_x;
	int target_tile_y = (int)target_y;

	//we have already reached the target
	if (map_x == target_tile_x && map_y == target_tile_y)
	{
		return true;
	}

	//length of ray from current position to next x or y-side
	float side_dist_x = 0;
	float side_dist_y = 0;

	int step_x = 0;
	int step_y = 0;

	if (x_point > 0)
	{
		step_x = -1;
		side_dist_x = (obj->x - map_x) * delta_dist_x;
	}
	else if (x_point < 0)
	{
		step_x = 1;
		side_dist_x = (map_x + 1.0 - obj->x) * delta_dist_x;
	}
	if (y_point > 0)
	{
		step_y = -1;
		side_dist_y = (obj->y - map_y) * delta_dist_y;
	}
	else if (y_point < 0)
	{
		step_y = 1;
		side_dist_y = (map_y + 1.0 - obj->y) * delta_dist_y;
	}

	if (abs(x_point) > 0)
	{
		int x = map_x;
		float y = map_y;

		y += step_y * delta_dist_x;
		x += step_x;

		while (x != target_tile_x)
		{
			if (Map_GetTile(x, y) != EMPTY_TILE)
			{
				return false;
			}

			Object* tile_obj = Map_GetObjectAtTile(x, y);

			if (tile_obj)
			{
				if (Object_IsSpecialCollidableTile(tile_obj))
				{
					return false;
				}
			}

			y += step_y * delta_dist_x;
			x += step_x;
		}
	}
	if (abs(y_point) > 0)
	{
		float x = map_x;
		int y = map_y;

		x += step_x * delta_dist_y;
		y += step_y;

		while (y != target_tile_y)
		{
			if (Map_GetTile(x, y) != EMPTY_TILE)
			{
				return false;
			}
			Object* tile_obj = Map_GetObjectAtTile(x, y);

			if (tile_obj)
			{
				if (Object_IsSpecialCollidableTile(tile_obj))
				{
					return false;
				}
			}

			x += step_x * delta_dist_y;
			y += step_y;
		}
	}

	return true;
}

bool Object_CheckLineToTarget(Object* obj, Object* target)
{
	return false;
}

bool Object_CheckSight(Object* obj, Object* target)
{
	float x_point = obj->x - target->x;
	float y_point = obj->y - target->y;

	//make sure the object is facing the target
	//west
	if (obj->dir_x > 0 && x_point > 0)
	{
		return false;
	}
	//east
	else if (obj->dir_x < 0 && x_point < 0)
	{
		return false;
	}
	//north
	if (obj->dir_y > 0 && y_point > 0)
	{
		return false;
	}
	//south
	else if (obj->dir_y < 0 && y_point < 0)
	{
		return false;
	}

	//calc distance to target
	float d = (obj->x - target->x) * (obj->x - target->x) + (obj->y - target->y) * (obj->y - target->y);

	//very close to target
	if (d <= 10)
	{
		return true;
	}

	//check direct line, 
	if (!Object_CheckLineToTarget(obj, target))
	{
		return false;
	}

	return true;
}
bool Object_IsSpecialCollidableTile(Object* obj)
{
	//check for closed doors
	if (obj->type == OT__DOOR)
	{
		//door is more than halfway closed
		if (obj->move_timer >= 0.5)
		{
			return true;
		}
	}
	//check for special tiles
	else if (obj->type == OT__SPECIAL_TILE)
	{

	}
	//check for special triggers
	else if (obj->type == OT__TRIGGER)
	{
		if (obj->sub_type == SUB__TRIGGER_SWITCH)
		{
			return true;
		}
	}

	return false;
}

void Object_UnlinkSector(Object* obj)
{
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


	obj->sector_next = NULL;
	obj->sector_prev = NULL;
}

void Object_LinkSector(Object* obj)
{
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
}

void Object_Hurt(Object* obj, Object* src_obj, int damage)
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
		//game.monsters_killed++;
		Game_GetGame()->monsters_killed++;
		Monster_SetState(obj, MS__DIE);
	}
	else if (obj->type == OT__PLAYER)
	{
		float dir_x = 0;
		float dir_y = 0;

		Player_Hurt(dir_x, dir_y);
	}
}

Object* Object_Missile(Object* obj, Object* target, int type)
{
	Object* missile = Map_NewObject(OT__MISSILE);

	if (!missile)
	{
		return NULL;
	}

	GameAssets* assets = Game_GetAssets();

	float dir_x = obj->dir_x;
	float dir_y = obj->dir_y;

	//if we have a target calc dir to target
	if (target)
	{
		float point_x = (obj->x) - target->x;
		float point_y = (obj->y) - target->y;

		Math_XY_Normalize(&point_x, &point_y);

		dir_x = -point_x;
		dir_y = -point_y;
	}

	missile->owner = obj;
	missile->sub_type = type;
	missile->x = (obj->x) + dir_x * 2;
	missile->y = (obj->y) + dir_y * 2;
	missile->dir_x = dir_x;
	missile->dir_y = dir_y;
	missile->hp = 1;
	missile->sprite.light = 1;
	missile->sprite.img = &assets->missile_textures;
	missile->size = 0.25;

	return missile;
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

	obj->x = x;
	obj->y = y;
	obj->z = z;

	obj->sprite.x = x + obj->sprite.offset_x;
	obj->sprite.y = y + obj->sprite.offset_y;
	obj->sprite.z = z;

	obj->size = 10;
	obj->spatial_id = -1;

	obj->sub_type = sub_type;

	bool assign_to_spatial_tree = false;

	switch (type)
	{
	case OT__PLAYER:
	{
		break;
	}
	case OT__MONSTER:
	{
		assign_to_spatial_tree = true;

		Game_GetGame()->total_monsters++;

		Monster_Spawn(obj);
		break;
	}
	//fallthrough
	case OT__LIGHT:
	case OT__PICKUP:
	case OT__THING:
	{
		assign_to_spatial_tree = true;

		ObjectInfo* object_info = Info_GetObjectInfo(obj->type, obj->sub_type);
		AnimInfo* anim_info = &object_info->anim_info;

		obj->sprite.img = &assets->object_textures;

		obj->sprite.anim_speed_scale = object_info->anim_speed;
		obj->sprite.frame_count = anim_info->frame_count;
		obj->sprite.looping = anim_info->looping;
		obj->sprite.frame_offset_x = anim_info->x_offset;
		obj->sprite.frame_offset_y = anim_info->y_offset;
		obj->sprite.offset_x = object_info->sprite_offset_x;
		obj->sprite.offset_y = object_info->sprite_offset_y;

		if (obj->sprite.frame_count > 0)
		{
			obj->sprite.playing = true;
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
	case OT__TARGET:
	{
		break;
	}
	case OT__DOOR:
	{
		obj->move_timer = 1; // the door spawns closed
		obj->state = DOOR_SLEEP;
		break;
	}
	case OT__SPECIAL_TILE:
	{
		break;
	}
	case OT__PARTICLE:
	{
		ParticleInfo* particle_info = Info_GetParticleInfo(sub_type);
		AnimInfo* anim_info = &particle_info->anim_info;

		obj->size = 0.5;

		obj->sprite.img = &assets->particle_textures;
		obj->move_timer = particle_info->time;
		obj->sprite.v_offset = Math_randf();
		obj->sprite.scale_x = 0.5;
		obj->sprite.scale_y = 0.5;

		obj->sprite.frame_count = anim_info->frame_count;
		obj->sprite.frame_offset_x = anim_info->x_offset;
		obj->sprite.frame_offset_y = anim_info->y_offset;
		obj->sprite.looping = anim_info->looping;

		obj->sprite.scale_x = particle_info->sprite_scale;
		obj->sprite.scale_y = particle_info->sprite_scale;
		obj->sprite.anim_speed_scale = 1;
		
		obj->sprite.playing = true;

		if (obj->sprite.frame_count > 0)
		{
			obj->sprite.frame = rand() % obj->sprite.frame_count;
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

	Move_SetPosition(obj, x, y);
	Move_ZMove(obj, -100);

	return obj;
}
