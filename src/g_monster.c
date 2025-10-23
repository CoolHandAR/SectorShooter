#include "g_common.h"

#include "game_info.h"
#include "sound.h"
#include "u_math.h"

#include <math.h>
#include <stdlib.h>

#define MONSTER_MELEE_CHECK 5
#define STUCK_TIMER 2

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

static const RotType SPRITE_ROTATION_OFFSET_LUT[11] =
{
	ROT__FORWARD, //0
	ROT__FORWARD_SIDE, //1
	ROT__SIDE, //2,
	ROT__BACK, //3
	ROT__BACK, //4
	ROT__SIDE, //5
	ROT__FORWARD_SIDE, //6
	ROT__FORWARD, //7
	ROT__BACK_SIDE, //8 (3 + 5)
	ROT__BACK_SIDE, //9 (4 + 5)
	ROT__FORWARD, //10
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
		switch (obj->type)
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
		switch (obj->type)
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
		switch (obj->type)
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
		switch (obj->type)
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
		Sound_EmitWorldTemp(index, obj->x, obj->y, obj->z, 0, 0, 0);
	}
}

static MonsterAnimState Monster_GetAnimState(Object* obj)
{
	float view_x, view_y;
	Player_GetView(&view_x, &view_y, NULL, NULL, NULL, NULL);

	float angle = atan2(obj->y - view_y, obj->x - view_x);
	float obj_angle = atan2(-obj->dir_y, -obj->dir_x);
	 
	float rotation = angle - obj_angle;
	rotation += Math_DegToRad(45.0 / 2 * 9 - 180.0 / 16);

	float abs_dir = fabs(rotation);
	int round_dir = roundl(abs_dir);
	float fraction = abs_dir - round_dir;

	int sign = (round_dir >= 7) ? 1 : -1;

	if (obj->state != MS__DIE)
	{
		if (rotation * sign < 0)
		{
			obj->sprite.flip_h = true;
		}
		else
		{
			obj->sprite.flip_h = false;
		}
	}

	round_dir = abs(round_dir - 3);

	if (obj->state != MS__HIT)
	{
		//hack!!
		if ((fraction < 0.1 && round_dir == 3) || (fraction > 0.1 && round_dir == 4))
		{
			round_dir += 5;
		}
	}

	if (obj->state == MS__WALK || obj->state == MS__IDLE)
	{
		RotType offset = MAS__WALK_FORWARD + SPRITE_ROTATION_OFFSET_LUT[round_dir];

		return offset;
	}
	else if (obj->state == MS__MISSILE)
	{
		RotType offset = MAS__ATTACK_FORWARD + SPRITE_ROTATION_OFFSET_LUT[round_dir];

		return offset;
	}
	else if (obj->state == MS__MELEE)
	{
		RotType offset = MAS__MELEE_FORWARD + SPRITE_ROTATION_OFFSET_LUT[round_dir];

		return offset;
	}
	else if (obj->state == MS__HIT)
	{
		if (round_dir == 0 || round_dir == 1 || round_dir == 6 || round_dir == 7)
		{
			return MAS__HIT_FORWARD;
		}
		else if (round_dir == 2 || round_dir == 5)
		{
			return MAS__HIT_SIDE;
		}
		else if (round_dir == 3 || round_dir == 4)
		{
			return MAS__HIT_BACK;
		}

	}
	else if (obj->state == MS__DIE)
	{
		return MAS__DIE;
	}

	return MAS__IDLE;
}

static void Monster_UpdateSpriteAnimation(Object* obj, float delta)
{
	MonsterAnimState anim_state = Monster_GetAnimState(obj);

	const MonsterInfo* info = Info_GetMonsterInfo(obj->sub_type);
	const AnimInfo* anim_info = &info->anim_states[anim_state];

	Sprite* s = &obj->sprite;

	s->frame_count = anim_info->frame_count;
	s->frame_offset_x = anim_info->x_offset;
	s->frame_offset_y = anim_info->y_offset;
	s->looping = anim_info->looping;
	s->anim_speed_scale = 0.5;
	s->playing = true;

	if (obj->state == MS__IDLE)
	{
		s->frame_count = 1;
	}

	Sprite_UpdateAnimation(s, delta);

	if (anim_info->action_fun)
	{
		if (s->frame == anim_info->action_frame && s->action_loop != s->loops)
		{
			//play action sound
			Monster_EmitSound(obj, MSOUND__ATTACK);

			anim_info->action_fun(obj);
			s->action_loop = s->loops;
		}
	}
}

static bool Monster_TryStep(Object* obj, float delta)
{	
	float dir_x = x_diags[obj->dir_enum];
	float dir_y = y_diags[obj->dir_enum];

	obj->speed = 24;

	delta *= obj->speed;

	if (!Move_CheckStep(obj, dir_x * delta, dir_y * delta, obj->size))
	{
		return false;
	}
	
	obj->dir_x = dir_x;
	obj->dir_y = dir_y;

	return true;
}

static bool Monster_Walk(Object* obj, float delta)
{
	delta *= obj->speed;

	if (Move_Object(obj, obj->dir_x * delta, obj->dir_y * delta, true))
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

	float dist = (obj->x - target->x) * (obj->x - target->x) + (obj->y - target->y) * (obj->y - target->y);

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
	
	float x_point = obj->x - target->x;
	float y_point = obj->y - target->y;

	Math_XY_Normalize(&x_point, &y_point);

	obj->dir_x = -x_point;
	obj->dir_y = -y_point;
}

static void Monster_LookForTarget(Object* monster)
{
	Object* player = Player_GetObj();

	if (!player)
	{
		return;
	}

	//make sure player is alive and visible
	if (player->hp <= 0 || !Object_CheckLineToTarget(monster, player))
	{
		return;
	}

	monster->target = player;

	int index = 0;

	//play alert sound
	Monster_EmitSound(monster, MSOUND__ALERT);
}

void Monster_Spawn(Object* obj)
{
	GameAssets* assets = Game_GetAssets();
	MonsterInfo* monster_info = Info_GetMonsterInfo(obj->sub_type);

	if (!monster_info)
	{
		return;
	}
	
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
	obj->sprite.v_offset = monster_info->sprite_v_offset;
	obj->hp = monster_info->spawn_hp;
	obj->size = 5.5;
	obj->speed = monster_info->speed;

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
		int index = 0;

		//play hit sound
		Monster_EmitSound(obj, MSOUND__HIT);

		if (obj->sub_type == SUB__MOB_PINKY || obj->sub_type == SUB__MOB_BRUISER)
		{
			return;
		}

		obj->move_timer = 0;
		obj->attack_timer = 0;
		obj->stop_timer = 0.2;
		obj->flags |= OBJ_FLAG__JUST_HIT;
	}
	else if (state == MS__DIE)
	{
		int index = 0;

		//play death sound
		Monster_EmitSound(obj, MSOUND__DEATH);

		//make sure to forget the target
		obj->target = NULL;
	}

	//set the state
	obj->state = state;
	
	//make sure to reset the animation
	obj->sprite.frame = 0;
	obj->sprite._anim_frame_progress = 0;
	obj->sprite.playing = false;
	obj->sprite.action_loop = 0;

}

void Monster_Update(Object* obj, float delta)
{
	Monster_UpdateSpriteAnimation(obj, delta);

	//dead
	if (obj->hp <= 0)
	{
		//make sure the dead state is set
		Monster_SetState(obj, MS__DIE);
		return;
	}
	//always update z
	Move_ZMove(obj, GRAVITY_SCALE * delta);

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

	obj->stop_timer -= delta;
	obj->attack_timer -= delta;

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
	if (Monster_CheckIfMissilesPossible(obj) && obj->attack_timer <= 0)
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
	
	Object* missile = Object_Missile(obj, target, SUB__MISSILE_FIREBALL);

	missile->owner = obj;

	Sound_EmitWorldTemp(SOUND__FIREBALL_THROW, missile->x, missile->y, missile->z, missile->dir_x, missile->dir_y, missile->dir_z);
}

void Monster_Bruiser_FireBall(Object* obj)
{
	Object* target = obj->target;

	if (!target)
	{
		return;
	}

	Monster_FaceTarget(obj);

	for (int i = -2; i < 2; i++)
	{
		Object* missile = Object_Missile(obj, target, SUB__MISSILE_FIREBALL);
		
		missile->dir_x += i * 0.1;
		missile->dir_y += i * 0.1;

		missile->owner = obj;

		Sound_EmitWorldTemp(SOUND__FIREBALL_THROW, missile->x, missile->y, missile->z, missile->dir_x, missile->dir_y, missile->dir_z);
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

	if (info->melee_damage <= 0)
	{
		return;
	}

	Monster_FaceTarget(obj);

	Object_Hurt(target, obj, info->melee_damage);
}
