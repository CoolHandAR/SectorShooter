#include "game_info.h"

extern void Monster_Bruiser_FireBall(struct Object* obj);
extern void Monster_BloodImp_FireBall(struct Object* obj);
extern void Monster_Melee(struct Object* obj);


static const MonsterInfo MONSTER_INFO[] =
{
	//BLOOD IMP
	{
		//ANIM
		{
			//IDLE
			NULL, 0, 0, 0, 1, 1,

			//WALK FORWARD
			NULL, 0, 0, 0, 4, 1,

			//WALK FORWARD SIDE
			NULL, 0, 0, 1, 4, 1,

			//WALK SIDE
			NULL, 0, 0, 2, 4, 1,

			//WALK BACK SIDE
			NULL, 0, 0, 3, 4, 1,

			//WALK BACK
			NULL, 0, 0, 4, 4, 1,

			//ATTACK FORWARD
			Monster_BloodImp_FireBall, 0, 4, 0, 3, 0,

			//ATTACK FORWARD SIDE
			Monster_BloodImp_FireBall, 0, 4, 1, 3, 0,

			//ATTACK SIDE
			Monster_BloodImp_FireBall, 0, 4, 2, 3, 0,

			//ATTACK BACK SIDE
			Monster_BloodImp_FireBall, 0, 4, 3, 3, 0,

			//ATTACK BACK
			Monster_BloodImp_FireBall, 0, 4, 4, 4, 0,

			//MELEE FORWARD
			Monster_Melee, 1, 4, 0, 3, 1,

			//MELEE FORWARD SIDE
			Monster_Melee, 1, 4, 1, 3, 1,

			//MELEE SIDE
			Monster_Melee, 1, 4, 2, 3, 1,

			//MELEE BACK SIDE
			Monster_Melee, 1, 4, 3, 3, 1,

			//MELEE BACK
			Monster_Melee, 1, 4, 4, 4, 1,

			//HIT FORWARD
			NULL, 0, 0, 5, 1, 0,

			//HIT SIDE
			NULL, 0, 2, 5, 1, 0,

			//HIT BACK
			NULL, 0, 3, 5, 1, 0,

			//DIE
			NULL, 0, 4, 5, 5, 0,

			//EXPLODE
			NULL, 0, 2, 6, 8, 0,
		},
		60, //SPAWN HP
		34, //SPEED,
		14, //MELEE DAMAGE
		12, //SIZE,
		120, //HEIGHT
		24, //WEIGHT
		70, //HIT CHANCE
		1, //HIT TIME
		32, //SPRITE SCALE
		0, //SPRITE X OFFSET
		0, //SPRITE Y OFFSET
		70, //SPRITE Z OFFSET
	},
	//BOAR
	{
		//ANIM
		{
			//IDLE
			NULL, 0, 0, 0, 1, 1,

			//WALK FORWARD
			NULL, 0, 0, 0, 4, 1,

			//WALK FORWARD SIDE
			NULL, 0, 0, 1, 4, 1,

			//WALK SIDE
			NULL, 0, 0, 2, 4, 1,

			//WALK BACK SIDE
			NULL, 0, 0, 3, 4, 1,

			//WALK BACK
			NULL, 0, 0, 4, 4, 1,

			//ATTACK FORWARD
			NULL, 0, 0, 0, 0, 0,

			//ATTACK FORWARD SIDE
			NULL, 0, 0, 0, 0, 0,

			//ATTACK SIDE
			NULL, 0, 0, 0, 0, 0,

			//ATTACK BACK SIDE
			NULL, 0, 0, 0, 0, 0,

			//ATTACK BACK
			NULL, 0, 0, 0, 0, 0,

			//MELEE FORWARD
			Monster_Melee, 0, 4, 0, 3, 1,

			//MELEE FORWARD SIDE
			Monster_Melee, 0, 4, 1, 3, 1,

			//MELEE SIDE
			Monster_Melee, 0, 4, 2, 3, 1,

			//MELEE BACK SIDE
			Monster_Melee, 0, 4, 3, 3, 1,

			//MELEE BACK
			Monster_Melee, 0, 4, 4, 4, 1,

			//HIT FORWARD
			NULL, 0, 0, 0, 0, 0,

			//HIT SIDE
			NULL, 0, 0, 0, 0, 0,

			//HIT BACK
			NULL, 0, 0, 0, 0, 0,

			//DIE
			NULL, 0, 0, 5, 6, 0,

			//EXPLODE
			NULL, 0, 0, 5, 6, 0,
		},
		180, //SPAWN HP
		100, //SPEED,
		35, //MELEE DAMAGE
		20, //SIZE,
		123, //HEIGHT
		50, //WEIGHT
		70, //HIT CHANCE
		0, //HIT TIME
		32, //SPRITE SCALE
		0, //SPRITE X OFFSET
		0, //SPRITE Y OFFSET
		70, //SPRITE Z OFFSET
	},
	//BRUISER
	{
		//ANIM
		{
			//IDLE
			NULL, 0, 0, 0, 1, 1,

			//WALK FORWARD
			NULL, 0, 0, 0, 4, 1,

			//WALK FORWARD SIDE
			NULL, 0, 0, 1, 4, 1,

			//WALK SIDE
			NULL, 0, 0, 2, 4, 1,

			//WALK BACK SIDE
			NULL, 0, 0, 3, 4, 1,

			//WALK BACK
			NULL, 0, 0, 4, 4, 1,

			//ATTACK FORWARD
			Monster_Bruiser_FireBall, 0, 3, 0, 4, 0,

			//ATTACK FORWARD SIDE
			Monster_Bruiser_FireBall, 0, 3, 1, 4, 0,

			//ATTACK SIDE
			Monster_Bruiser_FireBall, 0, 3, 2, 4, 0,

			//ATTACK BACK SIDE
			Monster_Bruiser_FireBall, 0, 3, 3, 4, 0,

			//ATTACK BACK
			Monster_Bruiser_FireBall, 0, 3, 4, 4, 0,

			//MELEE FORWARD
			Monster_Melee, 1, 8, 0, 3, 1,

			//MELEE FORWARD SIDE
			Monster_Melee, 1, 8, 1, 3, 1,

			//MELEE SIDE
			Monster_Melee, 1, 8, 2, 3, 1,

			//MELEE BACK SIDE
			Monster_Melee, 1, 8, 3, 3, 1,

			//MELEE BACK
			Monster_Melee, 1, 8, 4, 3, 1,

			//HIT FORWARD
			NULL, 0, 7, 0, 1, 0,

			//HIT SIDE
			NULL, 0, 7, 2, 1, 0,

			//HIT BACK
			NULL, 0, 7, 4, 1, 0,

			//DIE
			NULL, 0, 0, 5, 14, 0,

			//EXPLODE
			NULL, 0, 0, 5, 14, 0,
		},
		400, //SPAWN HP
		50, //SPEED,
		35, //MELEE DAMAGE
		20, //SIZE,
		150, //HEIGHT
		100, //WEIGHT
		60, //HIT CHANCE
		1, //HIT TIME
		80, //SPRITE SCALE
		0, //SPRITE X OFFSET
		0, //SPRITE Y OFFSET
		150, //SPRITE Z OFFSET
	},
	//TEMPLAR
	{
		//ANIM
		{
			//IDLE
			NULL, 0, 0, 0, 2, 1,

			//WALK FORWARD
			NULL, 0, 0, 0, 2, 1,

			//WALK FORWARD SIDE
			NULL, 0, 0, 1, 2, 1,

			//WALK SIDE
			NULL, 0, 0, 2, 2, 1,

			//WALK BACK SIDE
			NULL, 0, 0, 3, 2, 1,

			//WALK BACK
			NULL, 0, 0, 4, 2, 1,

			//ATTACK FORWARD
			Monster_Templar_StarStrike, 4, 2, 0, 8, 0,

			//ATTACK FORWARD SIDE
			Monster_Templar_StarStrike, 4, 2, 1, 8, 0,

			//ATTACK SIDE
			Monster_Templar_StarStrike, 4, 2, 2, 8, 0,

			//ATTACK BACK SIDE
			Monster_Templar_StarStrike, 4, 2, 3, 8, 0,

			//ATTACK BACK
			Monster_Templar_StarStrike, 4, 2, 4, 8, 0,

			//MELEE FORWARD
			Monster_Melee, 4, 2, 0, 8, 0,

			//MELEE FORWARD SIDE
			Monster_Melee, 4, 2, 1, 8, 0,

			//MELEE SIDE
			Monster_Melee, 4, 2, 2, 8, 0,

			//MELEE BACK SIDE
			Monster_Melee, 4, 2, 3, 8, 0,

			//MELEE BACK
			Monster_Melee, 4, 2, 4, 8, 0,

			//HIT FORWARD
			NULL, 0, 2, 0, 1, 0,

			//HIT SIDE
			NULL, 0, 2, 2, 1, 0,

			//HIT BACK
			NULL, 0, 2, 4, 1, 0,

			//DIE
			NULL, 0, 0, 5, 18, 0,

			//EXPLODE
			NULL, 0, 0, 5, 18, 0,
		},
		150, //SPAWN HP
		40, //SPEED,
		35, //MELEE DAMAGE
		20, //SIZE,
		150, //HEIGHT
		50, //WEIGHT
		25, //HIT CHANCE
		1, //HIT TIME
		60, //SPRITE SCALE
		0, //SPRITE X OFFSET
		0, //SPRITE Y OFFSET
		123, //SPRITE Z OFFSET
	}
};

static const MissileInfo MISSILE_INFO[] =
{
	//SIMPLE FIREBALL
	{
		SUB__MISSILE_FIREBALL,
		//ANIM
		{
		//FLY
		NULL, 0, 0, 0, 2, 1,

		//EXPLODE
		NULL, 0, 2, 0, 3, 0,
	},
	256, //SPEED
	5, //SIZE,
	0, //EXPLOSION DAMAGE
	0, //EXPLOSION SIZE
	10, //DIRECT HIT DAMAGE
	32, //SPRITE SCALE
	},
	//MEGA SHOT
	{
	SUB__MISSILE_MEGASHOT,
	//ANIM
	{
		//FLY
		NULL, 0, 0, 1, 2, 1,

		//EXPLODE
		NULL, 0, 2, 1, 3, 0,
	},
	384, //SPEED
	5, //SIZE,
	0, //EXPLOSION DAMAGE
	0, //EXPLOSION SIZE
	80, //DIRECT HIT DAMAGE
	64, //SPRITE SCALE
	},
	//START STRIKE
	{
	SUB__MISSILE_STARSTRIKE,
	//ANIM
	{
		//FLY
		NULL, 0, 0, 2, 2, 1,

		//EXPLODE
		NULL, 0, 2, 2, 6, 0,
	},
	128, //SPEED
	8, //SIZE,
	0, //EXPLOSION DAMAGE
	0, //EXPLOSION SIZE
	40, //DIRECT HIT DAMAGE
	32, //SPRITE SCALE
	},
};
static const ObjectInfo OBJECT_INFOS[] =
{
	//TORCH
	{
		SUB__LIGHT_TORCH,
		//ANIM
		{
			NULL, 0, 0, 1, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		80, //SPRITE OFFSET_Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		100, //HEIGHT
	},
	//BLUE TORCH
	{
		SUB__LIGHT_BLUE_TORCH,
		//ANIM
		{
			NULL, 0, 0, 14, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		80, //SPRITE OFFSET_Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		100, //HEIGHT
	},
	//GREEN TORCH
	{
		SUB__LIGHT_GREEN_TORCH,
		//ANIM
		{
			NULL, 0, 0, 15, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		80, //SPRITE OFFSET_Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		100, //HEIGHT
	},
	//LAMP
	{
		SUB__LIGHT_LAMP,
		//ANIM
		{
			NULL, 0, 0, 0, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		-64, //SPRITE OFFSET_Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		12, //HEIGHT
	},
	//BLUE LAMP
	{
		SUB__LIGHT_BLUE_LAMP,
		//ANIM
		{
			NULL, 0, 0, 0, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		-32, //SPRITE OFFSET_Z
		32, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//GREEN LAMP
	{
		SUB__LIGHT_GREEN_LAMP,
		//ANIM
		{
			NULL, 0, 0, 0, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		-32, //SPRITE OFFSET_Z
		32, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//SMALL HP
	{
		SUB__PICKUP_SMALLHP,
		//ANIM
		{
			NULL, 0, 0, 2, 0, 0,
		},
		0.0, //ANIM SPEED
		0.0, //SPRITE OFFSET X
		0.0, //SPRITE OFFSET Y
		80, //SPRITE OFFSET_Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//BIG HP
	{
		SUB__PICKUP_BIGHP,
		//ANIM
		{
			NULL, 0, 1, 2, 0, 0,
		},
		0.0, //ANIM SPEED
		0.0, //SPRITE OFFSET X
		0.0, //SPRITE OFFSET Y
		80, //SPRITE OFFSET Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//RED COLLUMN
	{
		SUB__THING_RED_COLLUMN,
		//ANIM
		{
			NULL, 0, 0, 4, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		88, //SPRITE OFFSET Z
		44, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//BLUE COLLUMN
	{
		SUB__THING_BLUE_COLLUMN,
		//ANIM
		{
			NULL, 0, 1, 4, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		88, //SPRITE OFFSET Z
		44, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//RED FLAG
	{
		SUB__THING_RED_FLAG,
		//ANIM
		{
			NULL, 0, 2, 4, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//BLUE FLAG
	{
		SUB__THING_BLUE_FLAG,
		//ANIM
		{
			NULL, 0, 3, 4, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//CACTUS0
	{
		SUB__THING_CACTUS0,
		//ANIM
		{
			NULL, 0, 0, 8, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//CACTUS1
	{
		SUB__THING_CACTUS1,
		//ANIM
		{
			NULL, 0, 1, 8, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//CACTUS2
	{
		SUB__THING_CACTUS2,
		//ANIM
		{
			NULL, 0, 2, 8, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//DEADTREE0
	{
		SUB__THING_DEAD_TREE0,
		//ANIM
		{
			NULL, 0, 3, 8, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//DEADTREE1
	{
		SUB__THING_DEAD_TREE1,
		//ANIM
		{
			NULL, 0, 0, 9, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//DEADTREE2
	{
		SUB__THING_DEAD_TREE2,
		//ANIM
		{
			NULL, 0, 1, 9, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//DEADTREE3
	{
		SUB__THING_DEAD_TREE3,
		//ANIM
		{
			NULL, 0, 2, 9, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//SPINNING PYRAMID
	{
		SUB__THING_SPINNING_PYRAMID,
		//ANIM
		{
			NULL, 0, 0, 10, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//FLAME URN
	{
		SUB__LIGHT_FLAME_URN,
		//ANIM
		{
			NULL, 0, 0, 11, 3, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//BUSH0
	{
		SUB__THING_BUSH0,
		//ANIM
		{
			NULL, 0, 0, 12, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//BUSH1
	{
		SUB__THING_BUSH1,
		//ANIM
		{
			NULL, 0, 1, 12, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		256, //SPRITE OFFSET Z
		128, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		64, //HEIGHT
	},
	//BUSH2
	{
		SUB__THING_BUSH2,
		//ANIM
		{
			NULL, 0, 2, 12, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//TREE0
	{
		SUB__THING_TREE0,
		//ANIM
		{
			NULL, 0, 3, 12, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		256, //SPRITE OFFSET Z
		128, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//TREE1
	{
		SUB__THING_TREE1,
		//ANIM
		{
			NULL, 0, 0, 13, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//TREE2
	{
		SUB__THING_TREE2,
		//ANIM
		{
			NULL, 0, 1, 13, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		256, //SPRITE OFFSET Z
		128, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//TREE3
	{
		SUB__THING_TREE3,
		//ANIM
		{
			NULL, 0, 2, 13, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		256, //SPRITE OFFSET Z
		128, //SPRITE SCALE
		12, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//TELEPORTER
	{
		SUB__THING_TELEPORTER,
		//ANIM
		{
			NULL, 0, 0, 3, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//STALAG0
	{
		SUB__THING_STALAG0,
		//ANIM
		{
			NULL, 0, 0, 16, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		8, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//STALAG1
	{
		SUB__THING_STALAG1,
		//ANIM
		{
			NULL, 0, 1, 16, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		-55, //SPRITE OFFSET Z
		40, //SPRITE SCALE
		8, //SIZE
		0, //COLLIDABLE
		24, //HEIGHT
	},
	//STALAG2
	{
		SUB__THING_STALAG2,
		//ANIM
		{
			NULL, 0, 2, 16, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		8, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//WINTER_TREE0
	{
		SUB__THING_WINTER_TREE0,
		//ANIM
		{
			NULL, 0, 3, 16, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		160, //SPRITE OFFSET Z
		80, //SPRITE SCALE
		15, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//WINTER_TREE1
	{
		SUB__THING_WINTER_TREE1,
		//ANIM
		{
			NULL, 0, 0, 17, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		160, //SPRITE OFFSET Z
		80, //SPRITE SCALE
		15, //SIZE
		1, //COLLIDABLE
		32, //HEIGHT
	},
	//WINTER_TREE2
	{
		SUB__THING_WINTER_TREE2,
		//ANIM
		{
			NULL, 0, 1, 17, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		160, //SPRITE OFFSET Z
		80, //SPRITE SCALE
		15, //SIZE
		1, //COLLIDABLE
		60, //HEIGHT
	},
	//WINTER_TREE3
	{
		SUB__THING_WINTER_TREE3,
		//ANIM
		{
			NULL, 0, 2, 17, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		160, //SPRITE OFFSET Z
		80, //SPRITE SCALE
		15, //SIZE
		1, //COLLIDABLE
		50, //HEIGHT
	},
	//ROCK0
	{
		SUB__THING_ROCK0,
		//ANIM
		{
			NULL, 0, 3, 17, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		160, //SPRITE OFFSET Z
		80, //SPRITE SCALE
		15, //SIZE
		1, //COLLIDABLE
		50, //HEIGHT
	},
	//ROCK1
	{
		SUB__THING_ROCK1,
		//ANIM
		{
			NULL, 0, 0, 18, 0, 0,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		120, //SPRITE OFFSET Z
		60, //SPRITE SCALE
		15, //SIZE
		1, //COLLIDABLE
		50, //HEIGHT
	},
	//FALLING SAND
	{
		SUB__THING_FALLING_SAND,
		//ANIM
		{
			NULL, 0, 1, 18, 11, 1,
		},
		1.1, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		-120, //SPRITE OFFSET Z
		60, //SPRITE SCALE
		15, //SIZE
		0, //COLLIDABLE
		50, //HEIGHT
	},
	//FALLING SNOW
	{
		SUB__THING_FALLING_SNOW,
		//ANIM
		{
			NULL, 0, 0, 21, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		400, //SPRITE OFFSET Z
		200, //SPRITE SCALE
		15, //SIZE
		0, //COLLIDABLE
		50, //HEIGHT
	},
	//INVUNERABILITY PICKUP
	{
		SUB__PICKUP_INVUNERABILITY,
		//ANIM
		{
			NULL, 0, 0, 5, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//QUAD PICKUP
	{
		SUB__PICKUP_QUAD_DAMAGE,
		//ANIM
		{
			NULL, 0, 0, 6, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//SHOTGUN PICKUP
	{
		SUB__PICKUP_SHOTGUN,
		//ANIM
		{
			NULL, 0, 0, 7, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		80, //SPRITE OFFSET Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//MACHINE PICKUP
	{
		SUB__PICKUP_MACHINEGUN,
		//ANIM
		{
			NULL, 0, 1, 7, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		80, //SPRITE OFFSET Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//AMMO PICKUP
	{
		SUB__PICKUP_AMMO,
		//ANIM
		{
			NULL, 0, 2, 2, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		80, //SPRITE OFFSET Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//DEVASTATOR PICKUP
	{
		SUB__PICKUP_DEVASTATOR,
		//ANIM
		{
			NULL, 0, 2, 7, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		80, //SPRITE OFFSET Z
		40, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
	//ROCKET PICKUP
	{
		SUB__PICKUP_ROCKETS,
		//ANIM
		{
			NULL, 0, 3, 2, 0, 0,
		},
		0.0, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		128, //SPRITE OFFSET Z
		64, //SPRITE SCALE
		32, //SIZE
		0, //COLLIDABLE
		32, //HEIGHT
	},
};

static const GunInfo GUN_INFOs[GUN__MAX] =
{
	//NONE
	{
		GUN__NONE,
		0, //DMG
		0, //SPREAD
		0, //CD,
		0, //SCREEN X
		0, //SCREEN Y
		0, //SCALE
		0, //LIGHT FRAME
		{0, 0, 0}, //LIGHT COLOR
		0, //SHAKE SCALE,
		0 //SHAKE TIME
	},
	//PISTOL
	{
		GUN__PISTOL,
		12, //DMG
		1, //SPREAD
		0.3, //CD,
		0.55, //SCREEN X
		0.6, //SCREEN Y
		1.5, //SCALE
		1, //LIGHT FRAME
		{82, 70, 62}, //LIGHT COLOR
		0.15, //SHAKE SCALE,
		0.15 //SHAKE TIME
	},
	//MACHINE GUN
	{
		GUN__MACHINEGUN,
		15, //DMG
		1, //SPREAD
		0.17, //CD,
		0.2, //SCREEN X
		0.18, //SCREEN Y
		1.5, //SCALE
		1, //LIGHT FRAME
		{105, 98, 90}, //LIGHT COLOR
		0.15, //SHAKE SCALE,
		0.12 //SHAKE TIME
	},
	//SHOTGUN
	{
		GUN__SHOTGUN,
		6, //DMG
		1, //SPREAD
		1.25, //CD,
		0.05, //SCREEN X
		0.2, //SCREEN Y
		1.2, //SCALE
		1, //LIGHT FRAME
		{128, 93, 73}, //LIGHT COLOR
		0.56, //SHAKE SCALE,
		0.12 //SHAKE TIME
	},
	//DEVASTATOR
	{
		GUN__DEVASTATOR,
		0, //DMG
		1, //SPREAD
		1.70, //CD,
		0.25, //SCREEN X
		0.4, //SCREEN Y
		1.5, //SCALE
		1, //LIGHT FRAME
		{100, 100, 150}, //LIGHT COLOR
		0.45, //SHAKE SCALE,
		0.25 //SHAKE TIME
	},
};

static const ParticleInfo PARTICLE_INFOS[] =
{
	//BLOOD SPLATTER
	{
		SUB__PARTICLE_BLOOD,
		//ANIM INFO
		{
			NULL, 0, 0, 0, 20, 0,
		},
		4.5, //time
		18, //sprite scale
		6 //anim speed scale
	},
	//WALL HIT
	{
		SUB__PARTICLE_WALL_HIT,
		//ANIM INFO
		{
			NULL, 0, 0, 5, 4, 0,
		},
		0.5, //time
		10, //sprite scale
		2 //anim speed scale
	},
	//BLOOD EXPLOSION
	{
		SUB__PARTICLE_BLOOD_EXPLOSION,
		//ANIM INFO
		{
			NULL, 0, 0, 6, 4, 0,
		},
		0.3, //time
		14, //sprite scale
		3 //anim speed scale
	},
	//FIRE SPARKS
	{
		SUB__PARTICLE_FIRE_SPARKS,
		//ANIM INFO
		{
			NULL, 0, 0, 7, 8, 1,
		},
		FLT_MAX, //time
		7, //sprite scale
		0.5 //anim speed scale
	},
	//BULLET IMPACT
	{
		SUB__PARTICLE_BULLET_IMPACT,
		//ANIM INFO
		{
			NULL, 0, 0, 9, 4, 0,
		},
		0.12, //time
		12, //sprite scale
		1.8 //anim speed scale
	},
	//EXPLOSION
	{
		SUB__PARTICLE_EXPLOSION,
		//ANIM INFO
		{
			NULL, 0, 0, 10, 4, 0,
		},
		0.25, //time
		14, //sprite scale
		1.9 //anim speed scale
	},
};

static const DecalInfo DECAL_INFOS[] =
{
	//WALL HIT
	{
		SUB__DECAL_WALL_HIT,
		//ANIM INFO
		{
			NULL, 0, 0, 0, 5, 0,
		},
		5.5, //time
		32, //sprite scale
	},
	//SCORCH
	{
		SUB__DECAL_SCORCH,
		//ANIM INFO
		{
			NULL, 0, 0, 1, 2, 0,
		},
		5.5, //time
		32, //sprite scale
	},
	//BLOOD SPLATTER
	{
		SUB__DECAL_BLOOD_SPLATTER,
		//ANIM INFO
		{
			NULL, 0, 2, 1, 3, 0,
		},
		8.5, //time
		32, //sprite scale
	},
};

static const LightInfo LIGHT_INFOS[] =
{
	//TORCH
	{
		SUB__LIGHT_TORCH,
		512, //radius
		1.0, //attenuation
		1, // scale
		24, //deviance
		255, 190, 88, //color
	},
	//BLUE TORCH
	{
		SUB__LIGHT_BLUE_TORCH,
		512, //radius
		1, //attenuation
		1, // scale
		24, //deviance
		70, 70, 255, //color
	},
	//GREEN TORCH
	{
		SUB__LIGHT_GREEN_TORCH,
		512, //radius
		1, //attenuation
		1, // scale
		24, //deviance
		70, 255, 70, //color
	},
	//LAMP
	{
		SUB__LIGHT_LAMP,
		350, //radius
		1.25, //attenuation
		1, // scale
		12, //deviance
		255, 190, 88, //color
	},
	//BLUE LAMP
 	{
		SUB__LIGHT_BLUE_LAMP,
		350, //radius
		1.25, //attenuation
		1, // scale
		12, //deviance
		70, 70, 255, //color
	},
	//GREEN LAMP
	{
		SUB__LIGHT_GREEN_LAMP,
		350, //radius
		1.25, //attenuation
		1, // scale
		12, //deviance
		70, 255, 70, //color
	},
	//FLAME URN
	{
		SUB__LIGHT_FLAME_URN,
		612, //radius
		1.5, //attenuation
		1, // scale
		12, //deviance
		255, 120, 10, //color
	}
};

static const SFXInfo SFX_INFOS[] =
{
	//DESERT WIND
	{
		SOUND__DESERT_WIND, //TYPE
		0.10, //VOLUME
		0.02, //ROLLOFF
	},
	//TEMPLE AMBIENCE
	{
		SOUND__TEMPLE_AMBIENCE, //TYPE
		0.01, //VOLUME
		0.001, //ROLLOFF
	},
	//JUNGLE AMBIENCE
	{
		SOUND__JUNGLE_AMBIENCE, //TYPE
		0.08, //VOLUME
		0.01, //ROLLOFF
	},
	//WINTER WIND
	{
		SOUND__WINTER_WIND, //TYPE
		0.08, //VOLUME
		0.01, //ROLLOFF
	}
};

static const LightCompilerInfo LIGHT_COMPILER_INFOS[] =
{
	//MAP00
	{
		0.3, //SKY SCALE
		2, //SUN Z
		{255, 204, 51}, //SUN COLOR
	},
	//MAP01
	{
		0.4, //SKY SCALE
		1.8, //SUN Z
		{254, 221, 150}, //SUN COLOR
	},
	//MAP02
	{
		0.2, //SKY SCALE
		2.2, //SUN Z
		{234, 220, 220}, //SUN COLOR
	},
	{
		0.0, //SKY SCALE
		2, //SUN Z
		{255, 255, 255}, //SUN COLOR
	},
};
GunInfo* Info_GetGunInfo(int type)
{
	int index = type;

	if (index < 0 || index >= GUN__MAX)
	{
		index = 0;
	}

	return &GUN_INFOs[index];
}

MonsterInfo* Info_GetMonsterInfo(int type)
{
	if (type < 0)
	{
		return 0;
	}

	int arr_size = sizeof(MONSTER_INFO) / sizeof(MONSTER_INFO[0]);
	int index = type - SUB__MOB_BLOOD_IMP;

	if (index < 0)
	{
		index = 0;
	}
	else if (index >= arr_size)
	{
		index = arr_size - 1;
	}

	return &MONSTER_INFO[index];
}

ObjectInfo* Info_GetObjectInfo(int type, int sub_type)
{
	if (type < 0 || sub_type < 0)
	{
		return &OBJECT_INFOS[0];
	}

	//lazy flat search
	int arr_size = sizeof(OBJECT_INFOS) / sizeof(OBJECT_INFOS[0]);
	
	for (int i = 0; i < arr_size; i++)
	{
		ObjectInfo* info = &OBJECT_INFOS[i];

		if (info->type == sub_type)
		{
			return info;
		}
	}

	
	return &OBJECT_INFOS[0];
}

LightInfo* Info_GetLightInfo(int sub_type)
{
	if (sub_type < 0)
	{
		return 0;
	}

	int arr_size = sizeof(LIGHT_INFOS) / sizeof(LIGHT_INFOS[0]);
	int index = sub_type - SUB__LIGHT_TORCH;
	
	if (index < 0)
	{
		index = 0;
	}
	else if (index >= arr_size)
	{
		index = arr_size - 1;
	}

	return &LIGHT_INFOS[index];
}

ParticleInfo* Info_GetParticleInfo(int sub_type)
{
	int arr_size = sizeof(PARTICLE_INFOS) / sizeof(PARTICLE_INFOS[0]);
	int index = sub_type - SUB__PARTICLE_BLOOD;

	if (index < 0)
	{
		index = 0;
	}
	else if (index >= arr_size)
	{
		index = arr_size - 1;
	}

	return &PARTICLE_INFOS[index];
}

DecalInfo* Info_GetDecalInfo(int sub_type)
{
	int arr_size = sizeof(DECAL_INFOS) / sizeof(DECAL_INFOS[0]);
	int index = sub_type - SUB__DECAL_WALL_HIT;

	if (index < 0)
	{
		index = 0;
	}
	else if (index >= arr_size)
	{
		index = arr_size - 1;
	}

	return &DECAL_INFOS[index];
}

MissileInfo* Info_GetMissileInfo(int sub_type)
{
	int arr_size = sizeof(MISSILE_INFO) / sizeof(MISSILE_INFO[0]);
	int index = sub_type - SUB__MISSILE_FIREBALL;

	if (index < 0)
	{
		index = 0;
	}
	else if (index >= arr_size)
	{
		index = arr_size - 1;
	}

	return &MISSILE_INFO[index];
}

SFXInfo* Info_GetSFXInfo(int sub_type)
{
	int arr_size = sizeof(SFX_INFOS) / sizeof(SFX_INFOS[0]);
	int index = sub_type - SUB__SFX_DESERT_WIND;

	if (index < 0)
	{
		index = 0;
	}
	else if (index >= arr_size)
	{
		index = arr_size - 1;
	}

	return &SFX_INFOS[index];
}

LightCompilerInfo* Info_GetLightCompilerInfo(int index)
{
	int arr_size = sizeof(LIGHT_COMPILER_INFOS) / sizeof(LIGHT_COMPILER_INFOS[0]);
	
	if (index < 0)
	{
		index = 0;
	}
	else if (index >= arr_size)
	{
		index = arr_size - 1;
	}

	return &LIGHT_COMPILER_INFOS[index];
}
