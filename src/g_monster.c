#include "g_common.h"

#include "game_info.h"
#include "sound.h"
#include "u_math.h"

#include <math.h>
#include <stdlib.h>

#define MONSTER_STEP_HEIGHT 64
#define MONSTER_MELEE_CHECK 45
#define MONSTER_ANIM_SPEED_SCALE 0.5
#define MONSTER_WEIGHT_SCALE 500
#define MONSTER_SUPER_MULTIPLIER 1.5
#define STUCK_TIMER 2

static int s_SoundPropogationCheck = 0;

static const DirEnum opposite_dirs[DIR_MAX] =
{
	//NONE			//NORTH
	DIR_NONE,		DIR_SOUTH,
	//NORTH-EAST    //NORTH-WEST
	DIR_SOUTH_WEST,	DIR_SOUTH_EAST,
	//EAST			//SOUTH-EAST
	DIR_WEST,		DIR_NORTH_WEST,
	//SOUTH			//SOUTH-WEST
	DIR_NONE,		DIR_NORTH_EAST,
	//WEST			//MAX
	DIR_EAST
};

static const DirEnum angle_dirs[] =
{
	DIR_NORTH_WEST, DIR_NORTH_EAST, DIR_SOUTH_WEST, DIR_SOUTH_EAST
};

static const float x_diags[DIR_MAX] =
{
	//NONE			//NORTH
	0,				0,
	//NORTH-EAST    //NORTH-WEST
	-1,				1,
	//EAST			//SOUTH-EAST
	-1,				-1,
	//SOUTH			//SOUTH-WEST
	0,				1,
	//WEST			//MAX
	1
};
static const float y_diags[DIR_MAX] =
{
	//NONE			//NORTH
	0,				-1,
	//NORTH-EAST    //NORTH-WEST
	-1,				-1,
	//EAST			//SOUTH-EAST
	0,				1,
	//SOUTH			//SOUTH-WEST
	1,				1,
	//WEST			//MAX
	0
};

typedef enum
{
	ROT__FORWARD,
	ROT__FORWARD_SIDE,
	ROT__SIDE,
	ROT__BACK_SIDE,
	ROT__BACK
} RotType;

static const RotType SPRITE_ROTATION_OFFSET_LUT[8] =
{
	ROT__FORWARD, //0
	ROT__FORWARD_SIDE, //1
	ROT__SIDE, //2,
	ROT__BACK_SIDE, //3
	ROT__BACK, //4
	ROT__BACK_SIDE, //5
	ROT__SIDE, //6
	ROT__FORWARD_SIDE, //7
};
typedef enum
{
	MSOUND__ALERT,
	MSOUND__HIT,
	MSOUND__DEATH,
	MSOUND__ATTACK
} MonsterSoundState;

static void Monster_EmitSound(Object* obj, MonsterSoundState state)
{
	int index = -1;

	switch (state)
	{
	case MSOUND__ALERT:
	{
		switch (obj->sub_type)
		{
		case SUB__MOB_IMP:
		{
			index = SOUND__IMP_ALERT;
			break;
		}
		case SUB__MOB_PINKY:
		{
			index = SOUND__PINKY_ALERT;
			break;
		}
		case SUB__MOB_BRUISER:
		{
			index = SOUND__BRUISER_ALERT;
			break;
		}
		default:
			break;
		}
		break;
	}
	case MSOUND__HIT:
	{
		switch (obj->sub_type)
		{
		case SUB__MOB_IMP:
		{
			index = SOUND__IMP_HIT;
			break;
		}
		case SUB__MOB_PINKY:
		{
			index = SOUND__PINKY_HIT;
			break;
		}
		case SUB__MOB_BRUISER:
		{
			index = SOUND__BRUISER_HIT;
			break;
		}
		default:
			break;
		}
		break;
	}
	case MSOUND__DEATH:
	{
		switch (obj->sub_type)
		{
		case SUB__MOB_IMP:
		{
			index = SOUND__IMP_DIE;
			break;
		}
		case SUB__MOB_PINKY:
		{
			index = SOUND__PINKY_DIE;
			break;
		}
		case SUB__MOB_BRUISER:
		{
			index = SOUND__BRUISER_DIE;
			break;
		}
		default:
			break;
		}
		break;
	}
	case MSOUND__ATTACK:
	{
		switch (obj->sub_type)
		{
		case SUB__MOB_IMP:
		{
			index = SOUND__IMP_ATTACK;
			break;
		}
		case SUB__MOB_PINKY:
		{
			index = SOUND__PINKY_ATTACK;
			break;
		}
		case SUB__MOB_BRUISER:
		{
			index = SOUND__BRUISER_ATTACK;
			break;
		}
		default:
			break;
		}
		break;
	}
	default:
		break;
	}

	if (index >= 0)
	{
		Sound_EmitWorldTemp(index, obj->x, obj->y, obj->z, 0, 0, 0, 1);
	}
}
static MonsterAnimState Monster_GetAnimState(Object* obj)
{
	float view_x, view_y, view_angle;
	Player_GetView(&view_x, &view_y, NULL, NULL, &view_angle);

	float view_to_obj_angle = Math_XY_Angle(view_x - obj->x, view_y - obj->y);
	float obj_angle = Math_XY_Angle(obj->dir_x, obj->dir_y);

	int rot = Math_RadToDeg(view_to_obj_angle - obj_angle) + 22.5;

	if (rot > 360)
	{
		rot = 0;
	}
	else if (rot < 0)
	{
		rot = 360 + rot;
	}

	int index = Math_Clampl(rot / 45, 0, 7);

	if (obj->state != MS__DIE)
	{
		if (index >= 5)
		{
			obj->sprite.flip_h = true;
		}
		else
		{
			obj->sprite.flip_h = false;
		}
	}
	
	if (obj->state == MS__WALK || obj->state == MS__IDLE)
	{
		RotType offset = MAS__WALK_FORWARD + SPRITE_ROTATION_OFFSET_LUT[index];

		return offset;
	}
	else if (obj->state == MS__MISSILE)
	{
		RotType offset = MAS__ATTACK_FORWARD + SPRITE_ROTATION_OFFSET_LUT[index];

		return offset;
	}
	else if (obj->state == MS__MELEE)
	{
		RotType offset = MAS__MELEE_FORWARD + SPRITE_ROTATION_OFFSET_LUT[index];

		return offset;
	}
	else if (obj->state == MS__HIT)
	{
		if (index == 0 || index == 1 || index == 7)
		{
			return MAS__HIT_FORWARD;
		}
		else if (index == 2 || index == 6)
		{
			return MAS__HIT_SIDE;
		}
		else if (index == 4 || index == 5 || index == 3)
		{
			return MAS__HIT_BACK;
		}

	}
	else if (obj->state == MS__DIE)
	{
		if (obj->flags & OBJ_FLAG__EXPLODING)
		{
			return MAS__EXPLODE;
		}

		return MAS__DIE;
	}

	return MAS__IDLE;
}

void Monster_UpdateSpriteAnimation(Object* obj, float delta)
{
	MonsterAnimState anim_state = Monster_GetAnimState(obj);

	const MonsterInfo* info = Info_GetMonsterInfo(obj->sub_type);
	const AnimInfo* anim_info = &info->anim_states[anim_state];

	Sprite* s = &obj->sprite;

	s->frame_count = anim_info->frame_count;
	s->frame_offset_x = anim_info->x_offset;
	s->frame_offset_y = anim_info->y_offset;
	s->looping = anim_info->looping;
	s->anim_speed_scale = MONSTER_ANIM_SPEED_SCALE;
	
	if (anim_info->action_fun)
	{
		s->action_frame = anim_info->action_frame;

		if (s->action_loop != s->loops)
		{
			anim_info->action_fun(obj);
			s->action_loop = s->loops;
		}
	}

	if (obj->state == MS__DIE && s->finished && s->frame == s->frame_count - 1)
	{
		s->playing = false;
	}
	else
	{
		s->playing = true;
	}

	if (obj->state == MS__IDLE)
	{
		s->frame_count = 1;
	}
}

static bool Monster_TryStep(Object* obj, float delta)
{	
	float dir_x = x_diags[obj->dir_enum];
	float dir_y = y_diags[obj->dir_enum];

	float delta_speed = obj->speed * delta;

	if (!Move_CheckStep(obj, dir_x * delta_speed, dir_y * delta_speed, obj->size))
	{
		return false;
	}
	
	obj->dir_x = dir_x;
	obj->dir_y = dir_y;

	return true;
}

static bool Monster_Walk(Object* obj, float delta)
{
	if (Move_Object(obj, obj->dir_x, obj->dir_y, delta, false))
	{
		Monster_SetState(obj, MS__WALK);
		return true;
	}

	obj->stuck_timer += delta;

	if (obj->stuck_timer >= STUCK_TIMER)
	{
		if (Move_Unstuck(obj))
		{
			obj->stuck_timer = 0;
			return true;
		}
	}

	Monster_SetState(obj, MS__IDLE);

	return false;
}

static bool Monster_CheckIfMeleePossible(Object* obj)
{
	Object* target = (Object*)obj->target;

	if (!target || target->hp <= 0)
	{
		return false;
	}

	float dist = Math_XY_Distance(obj->x, obj->y, target->x, target->y);

	if (dist <= MONSTER_MELEE_CHECK)
	{
		return true;
	}

	return false;
}

static bool Monster_CheckIfMissilesPossible(Object* obj)
{
	if (obj->sub_type == SUB__MOB_PINKY)
	{
		return false;
	}

	Object* chase_obj = (Object*)obj->target;

	if (!chase_obj)
	{
		return false;
	}

	//check sight
	if (!Object_CheckSight(obj, chase_obj))
	{
		return false;
	}

	return true;
}

static void Monster_NewChaseDir(Object* obj, float delta)
{
	Object* target = obj->target;

	if (!target)
	{
		return;
	}

	float dir_x = target->x - obj->x;
	float dir_y = target->y - obj->y;

	DirEnum old_dir = obj->dir_enum;
	DirEnum opposite_dir = opposite_dirs[old_dir];
	DirEnum opposite_dir_x = 0;
	DirEnum opposite_dir_y = 0;
	DirEnumToDirEnumVector(opposite_dir, &opposite_dir_x, &opposite_dir_y);
	bool dir_tries[DIR_MAX];
	memset(&dir_tries, 0, sizeof(dir_tries));

	//try direct route
	obj->dir_enum = DIR_NONE;

	DirEnum dirs[2];

	static float bias_size = 0.25;

	if (dir_x > bias_size)
	{
		dirs[0] = DIR_WEST;
	}
	else if (dir_x < -bias_size)
	{
		dirs[0] = DIR_EAST;
	}
	else
	{
		dirs[0] = DIR_NONE;
	}
	if (dir_y > bias_size)
	{
		dirs[1] = DIR_SOUTH;
	}
	else if (dir_y < -bias_size)
	{
		dirs[1] = DIR_NORTH;
	}
	else
	{
		dirs[1] = DIR_NONE;
	}

	if (dirs[0] != DIR_NONE && dirs[1] != DIR_NONE)
	{
		int i_x = (dirs[0] == DIR_EAST);
		int i_y = (dirs[1] == DIR_SOUTH) ? 2 : 0;
		DirEnum d = angle_dirs[i_x + i_y];

		if (d != opposite_dir && d != opposite_dir_x && d != opposite_dir_y)
		{
			obj->dir_enum = d;
			dir_tries[obj->dir_enum] = true;
			if (Monster_TryStep(obj, delta))
			{
				return;
			}
		}
	}


	if (rand() > 200)
	{
		//attempt to swap directions
		if (rand() > 150 || fabs(dir_y) > fabs(dir_x))
		{
			int t = dirs[0];
			dirs[0] = dirs[1];
			dirs[1] = t;
		}

		if (dirs[0] != opposite_dir && dirs[0] != opposite_dir_x && dirs[0] != opposite_dir_y && !dir_tries[dirs[0]])
		{
			obj->dir_enum = dirs[0];
			dir_tries[obj->dir_enum] = true;
			if (Monster_TryStep(obj, delta))
			{
				return;
			}
		}
		if (dirs[1] != opposite_dir && dirs[1] != opposite_dir_x && dirs[1] != opposite_dir_y && !dir_tries[dirs[1]])
		{
			obj->dir_enum = dirs[1];
			dir_tries[obj->dir_enum] = true;
			if (Monster_TryStep(obj, delta))
			{
				return;
			}
		}

		//try old dir
		if (old_dir != DIR_NONE && !dir_tries[old_dir])
		{
			obj->dir_enum = old_dir;
			dir_tries[obj->dir_enum] = true;
			if (Monster_TryStep(obj, delta))
			{
				return;
			}
		}
	}
	

	//try random directions
	if (rand() & 1)
	{
		for (int i = DIR_NONE + 1; i < DIR_MAX; i++)
		{
			if (i != opposite_dir && !dir_tries[i])
			{
				obj->dir_enum = i;
				dir_tries[obj->dir_enum] = true;
				if (Monster_TryStep(obj, delta))
				{
					return;
				}
			}
		}
	}
	else
	{
		for (int i = DIR_MAX - 1; i > DIR_NONE; i--)
		{
			if (i != opposite_dir && !dir_tries[i])
			{
				obj->dir_enum = i;
				dir_tries[obj->dir_enum] = true;
				if (Monster_TryStep(obj, delta))
				{
					return;
				}
			}
		}
	}
	if (opposite_dir != DIR_NONE && !dir_tries[opposite_dir])
	{
		obj->dir_enum = opposite_dir;
		if (Monster_TryStep(obj, delta))
		{
			return;
		}
	}

	obj->dir_enum = DIR_NONE;
}

static void Monster_FaceTarget(Object* obj)
{
	Object* target = obj->target;

	if (!target)
	{
		return;
	}
	
	float x_point = target->x - obj->x;
	float y_point = target->y - obj->y;

	Math_XY_Normalize(&x_point, &y_point);

	obj->dir_x = x_point;
	obj->dir_y = y_point;
}

static void Monster_LookForTarget(Object* monster)
{
	Object* player = Player_GetObj();

	if (!player)
	{
		return;
	}

	//make sure player is alive and visible
	if (player->hp <= 0 || !Object_CheckSight(monster, player))
	{
		return;
	}

	Monster_SetTarget(monster, player);
}

void Monster_Spawn(Object* obj)
{
	GameAssets* assets = Game_GetAssets();
	MonsterInfo* monster_info = Info_GetMonsterInfo(obj->sub_type);

	if (!monster_info)
	{
		return;
	}
	
	assert(monster_info->speed >= monster_info->size && "This can cause monsters to walk through walls\n");

	if (obj->sub_type == SUB__MOB_IMP)
	{
		obj->sprite.img = &assets->imp_texture;
	}
	else if (obj->sub_type == SUB__MOB_PINKY)
	{
		obj->sprite.img = &assets->pinky_texture;
	}
	else if (obj->sub_type == SUB__MOB_BRUISER)
	{
		obj->sprite.img = &assets->bruiser_texture;
	}

	obj->sprite.scale_x = monster_info->sprite_scale;
	obj->sprite.scale_y = monster_info->sprite_scale;
	obj->sprite.offset_x = monster_info->sprite_x_offset;
	obj->sprite.offset_y = monster_info->sprite_y_offset;
	obj->sprite.offset_z = monster_info->sprite_z_offset;

	obj->hp = monster_info->spawn_hp;
	obj->size = monster_info->size;
	obj->speed = monster_info->speed;
	obj->height = monster_info->height;
	obj->step_height = MONSTER_STEP_HEIGHT;

	Monster_SetState(obj, MS__IDLE);
}

void Monster_SetState(Object* obj, int state)
{
	if (obj->state == state)
	{
		return;
	}

	if (state == MS__HIT)
	{
		if (obj->hit_timer > 0)
		{
			return;
		}

		//roll
		MonsterInfo* monster_info = Info_GetMonsterInfo(obj->sub_type);

		float hit_chance = monster_info->hit_chance;

		//reduce hit chance in half if super
		if (obj->flags & OBJ_FLAG__SUPER_MOB)
		{
			hit_chance *= 0.5;
		}

		if (rand() % 100 > monster_info->hit_chance)
		{
			return;
		}

		//play hit sound
		Monster_EmitSound(obj, MSOUND__HIT);

		if (obj->sub_type == SUB__MOB_PINKY || obj->sub_type == SUB__MOB_BRUISER)
		{
			return;
		}

		obj->move_timer = 0;
		obj->attack_timer = 0;
		obj->stop_timer = 0.2;
		obj->hit_timer = monster_info->hit_time;
		obj->flags |= OBJ_FLAG__JUST_HIT;
	}
	else if (state == MS__DIE)
	{
		//play death sound
		Monster_EmitSound(obj, MSOUND__DEATH);

		//make sure to forget the target
		obj->target = NULL;
	}

	//set the state
	obj->state = state;
	
	//make sure to reset the animation
	obj->sprite.action_loop = 0;
	obj->sprite.frame = 0;
	obj->sprite._anim_frame_progress = 0;
	obj->sprite.playing = false;
	obj->sprite.finished = false;
}

void Monster_SetTarget(Object* obj, Object* target)
{
	if (!target || target->hp <= 0)
	{
		return;
	}

	obj->target = target;

	//play alert sound
	Monster_EmitSound(obj, MSOUND__ALERT);
}

void Monster_Update(Object* obj, float delta)
{
	Monster_CheckForPushBack(obj, delta);

	obj->stop_timer -= delta;

	//always update z
	Move_ZMove(obj, GRAVITY_SCALE * delta);

	//dead
	if (obj->hp <= 0)
	{
		//make sure the dead state is set
		Monster_SetState(obj, MS__DIE);
		return;
	}

	Object* target = obj->target;
	//no target
	if (!target)
	{
		//set idle
		Monster_SetState(obj, MS__IDLE);

		//attempt to look for new target
		Monster_LookForTarget(obj);
		return;
	}

	//make sure our target is alive
	if (target->hp <= 0)
	{
		//it is dead
		obj->target = NULL;
		return;
	}

	obj->attack_timer -= delta;
	obj->hit_timer -= delta;

	if (obj->stop_timer > 0)
	{
		return;
	}

	//check if possible to melee
	if (Monster_CheckIfMeleePossible(obj))
	{
		Monster_SetState(obj, MS__MELEE);
		obj->stop_timer = 1;
		return;
	}

	//check if possible to launch missiles
	if (obj->attack_timer <= 0 && Monster_CheckIfMissilesPossible(obj))
	{
		Monster_SetState(obj, MS__MISSILE);
		obj->flags |= OBJ_FLAG__JUST_ATTACKED;
		obj->stop_timer = 1;
		obj->attack_timer = 3;
		return;
	}

	//get a new chase dir or move forward in the direction
	if (obj->move_timer <= 0  || !Monster_Walk(obj, delta))
	{
		Monster_NewChaseDir(obj, delta);
		obj->move_timer = Math_randf();
		return;
	}

	obj->move_timer -= delta;
}

void Monster_Imp_FireBall(Object* obj)
{
	Object* target = obj->target;

	if (!target)
	{
		return;
	}

	Monster_FaceTarget(obj);

	//play action sound
	Monster_EmitSound(obj, MSOUND__ATTACK);
	
	if (obj->flags & OBJ_FLAG__SUPER_MOB)
	{
		const float RANDOMNESS = 8.0;

		for (int i = -2; i < 2; i++)
		{
			Object* missile = Object_Missile(obj, target, SUB__MISSILE_FIREBALL);

			missile->dir_x += i * (Math_randf() / RANDOMNESS);
			missile->dir_y += i * (Math_randf() / RANDOMNESS);

			missile->z = (obj->z + 128);

			missile->owner = obj;
		}
	}
	else
	{
		Object* missile = Object_Missile(obj, target, SUB__MISSILE_FIREBALL);
		missile->owner = obj;
	}
}

void Monster_Bruiser_FireBall(Object* obj)
{
	Object* target = obj->target;

	if (!target)
	{
		return;
	}

	Monster_FaceTarget(obj);

	//play action sound
	Monster_EmitSound(obj, MSOUND__ATTACK);

	int iterations = 4;
	float randomness = 8.0;
	float z_offset = 100;

	if (obj->flags & OBJ_FLAG__SUPER_MOB)
	{
		iterations = 8;
		randomness *= 2;
		z_offset = 120;
	}

	bool vertical = false;

	if (rand() % 24 == 2)
	{
		vertical = true;
	}

	for (int i = -iterations; i < iterations; i++)
	{
		Object* missile = Object_Missile(obj, target, SUB__MISSILE_FIREBALL);
		
		missile->dir_x += i * (Math_randf() / randomness);
		missile->dir_y += i * (Math_randf() / randomness);

		if (vertical)
		{
			missile->dir_z += i * (Math_randf() / randomness);
		}

		missile->z = obj->z + z_offset;

		missile->owner = obj;
	}
}

void Monster_Melee(Object* obj)
{
	Object* target = obj->target;

	if (!target || target->hp <= 0)
	{
		return;
	}

	if (!Monster_CheckIfMeleePossible(obj))
	{
		return;
	}
	MonsterInfo* info = Info_GetMonsterInfo(obj->type);

	float melee_damage = info->melee_damage;

	if (obj->flags & OBJ_FLAG__SUPER_MOB)
	{
		melee_damage *= MONSTER_SUPER_MULTIPLIER;
	}

	if (info->melee_damage <= 0)
	{
		return;
	}

	Monster_FaceTarget(obj);

	//play action sound
	Monster_EmitSound(obj, MSOUND__ATTACK);

	Object_Hurt(target, obj, info->melee_damage, false);
}

static void Monster_WakeRecursive(Object* waker, int sector_index)
{
	if (sector_index < 0)
	{
		return;
	}

	Sector* sector = Map_GetSector(sector_index);

	//already checked
	if (sector->sound_propogation_check == s_SoundPropogationCheck)
	{
		return;
	}

	sector->sound_propogation_check = s_SoundPropogationCheck;
	
	int num_hits = Trace_SectorAll(sector);
	int* hits = Trace_GetHitObjects();

	for (int i = 0; i < num_hits; i++)
	{
		int index = hits[i];

		if (index >= 0)
		{
			Object* sector_obj = Map_GetObject(index);

			if (sector_obj && sector_obj->type == OT__MONSTER && sector_obj->hp > 0 && !(sector_obj->flags & OBJ_FLAG__IGNORE_SOUND))
			{
				Monster_SetTarget(sector_obj, waker);
			}
		}
		else
		{
			int line_index = -(index + 1);
			Linedef* line = Map_GetLineDef(line_index);

			//nothing to pass through
			if (line->back_sector < 0)
			{
				continue;
			}
			Sector* backsector = Map_GetSector(line->back_sector);

			float open_low = max(sector->floor, backsector->floor);
			float open_high = min(sector->ceil, backsector->ceil);
			float open_range = open_high - open_low;

			//closed
			if (open_range <= 0)
			{
				continue;
			}

			int next_sector = (line->front_sector == sector_index) ? line->back_sector : line->front_sector;

			Monster_WakeRecursive(waker, next_sector);
		}
	}
	

}

void Monster_WakeAll(Object* waker)
{
	if (waker->sector_index < 0)
	{
		return;
	}
	return;
	s_SoundPropogationCheck++;
	Monster_WakeRecursive(waker, waker->sector_index);
}

void Monster_CheckForPushBack(Object* obj, float delta)
{
	if (obj->stop_timer <= 0)
	{
		obj->vel_x = 0;
		obj->vel_y = 0;
		return;
	}

	MonsterInfo* monster_info = Info_GetMonsterInfo(obj->sub_type);

	if (!monster_info || monster_info->weight <= 0)
	{
		return;
	}

	float weight = monster_info->weight;

	if (obj->flags & OBJ_FLAG__SUPER_MOB)
	{
		weight *= MONSTER_SUPER_MULTIPLIER;
	}
	
	float push_back_speed = delta * (MONSTER_WEIGHT_SCALE / monster_info->weight);

	Move_SetPosition(obj, obj->x + (obj->vel_x * push_back_speed), obj->y + (obj->vel_y * push_back_speed), obj->size);
}

void Monster_ApplyPushback(Object* pusher, Object* monster)
{
	float dx = pusher->x - monster->x;
	float dy = pusher->y - monster->y;
	
	Math_XY_Normalize(&dx, &dy);
	
	monster->vel_x = -dx;
	monster->vel_y = -dy;

}

void Monster_SetAsSuper(Object* obj)
{
	if (obj->type != OT__MONSTER || (obj->flags & OBJ_FLAG__SUPER_MOB))
	{
		return;
	}

	obj->flags |= OBJ_FLAG__SUPER_MOB;
	//almost everything gets multiplied

	obj->hp *= MONSTER_SUPER_MULTIPLIER;
	obj->size *= MONSTER_SUPER_MULTIPLIER;
	obj->speed *= MONSTER_SUPER_MULTIPLIER;
	obj->height *= MONSTER_SUPER_MULTIPLIER;
	obj->step_height *= MONSTER_SUPER_MULTIPLIER;
	obj->sprite.scale_x *= MONSTER_SUPER_MULTIPLIER;
	obj->sprite.scale_y *= MONSTER_SUPER_MULTIPLIER;
	obj->sprite.offset_x *= MONSTER_SUPER_MULTIPLIER;
	obj->sprite.offset_y *= MONSTER_SUPER_MULTIPLIER;
	obj->sprite.offset_z *= MONSTER_SUPER_MULTIPLIER;
}
