#include "game_info.h"

extern void Monster_Bruiser_FireBall(struct Object* obj);
extern void Monster_Imp_FireBall(struct Object* obj);
extern void Monster_Melee(struct Object* obj);


static const MonsterInfo MONSTER_INFO[] =
{
	//IMP
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
			Monster_Imp_FireBall, 1, 4, 0, 3, 0,

			//ATTACK FORWARD SIDE
			Monster_Imp_FireBall, 1, 4, 1, 3, 0,

			//ATTACK SIDE
			Monster_Imp_FireBall, 1, 4, 2, 3, 0,

			//ATTACK BACK SIDE
			Monster_Imp_FireBall, 1, 4, 3, 3, 0,

			//ATTACK BACK
			Monster_Imp_FireBall, 2, 4, 4, 4, 0,

			//MELEE FORWARD
			Monster_Melee, 1, 4, 0, 3, 1,

			//MELEE FORWARD SIDE
			Monster_Melee, 1, 4, 1, 3, 1,

			//MELEE SIDE
			Monster_Melee, 1, 4, 2, 3, 1,

			//MELEE BACK SIDE
			Monster_Melee, 1, 4, 3, 3, 1,

			//MELEE BACK
			Monster_Melee, 2, 4, 4, 4, 1,

			//HIT FORWARD
			NULL, 0, 0, 5, 1, 0,

			//HIT SIDE
			NULL, 0, 1, 5, 1, 0,

			//HIT BACK
			NULL, 0, 2, 5, 1, 0,

			//DIE
			NULL, 0, 0, 6, 4, 0,

			//EXPLODE
			NULL, 0, 0, 7, 8, 0,
		},
		60, //SPAWN HP
		24, //SPEED,
		14, //MELEE DAMAGE
		18, //SIZE,
		120, //HEIGHT
		24, //WEIGHT
		70, //HIT CHANCE
		1, //HIT TIME
		32, //SPRITE SCALE
		0, //SPRITE X OFFSET
		0, //SPRITE Y OFFSET
		0, //SPRITE Z OFFSET
	},
	//PINKY
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
			Monster_Melee, 1, 4, 0, 3, 1,

			//MELEE FORWARD SIDE
			Monster_Melee, 1, 4, 1, 3, 1,

			//MELEE SIDE
			Monster_Melee, 1, 4, 2, 3, 1,

			//MELEE BACK SIDE
			Monster_Melee, 1, 4, 3, 3, 1,

			//MELEE BACK
			Monster_Melee, 2, 4, 4, 4, 1,

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
		60, //SPAWN HP
		24, //SPEED,
		14, //MELEE DAMAGE
		24, //SIZE,
		24, //HEIGHT
		24, //WEIGHT
		24, //HIT CHANCE
		24, //HIT TIME
		32, //SPRITE SCALE
		0, //SPRITE X OFFSET
		0, //SPRITE Y OFFSET
		0, //SPRITE Z OFFSET
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
			Monster_Bruiser_FireBall, 1, 4, 0, 3, 0,

			//ATTACK FORWARD SIDE
			Monster_Bruiser_FireBall, 1, 4, 1, 3, 0,

			//ATTACK SIDE
			Monster_Bruiser_FireBall, 1, 4, 2, 3, 0,

			//ATTACK BACK SIDE
			Monster_Bruiser_FireBall, 1, 4, 3, 3, 0,

			//ATTACK BACK
			Monster_Bruiser_FireBall, 2, 4, 4, 4, 0,

			//MELEE FORWARD
			Monster_Melee, 1, 8, 0, 3, 1,

			//MELEE FORWARD SIDE
			Monster_Melee, 1, 8, 1, 3, 1,

			//MELEE SIDE
			Monster_Melee, 1, 8, 2, 3, 1,

			//MELEE BACK SIDE
			Monster_Melee, 1, 8, 3, 3, 1,

			//MELEE BACK
			Monster_Melee, 2, 8, 4, 3, 1,

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
		60, //SPAWN HP
		24, //SPEED,
		14, //MELEE DAMAGE
		24, //SIZE,
		64, //HEIGHT
		24, //WEIGHT
		24, //HIT CHANCE
		24, //HIT TIME
		32, //SPRITE SCALE
		0, //SPRITE X OFFSET
		0, //SPRITE Y OFFSET
		0, //SPRITE Z OFFSET
	},
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
	8, //SPEED
	0.25, //SIZE,
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
	8, //SPEED
	0.25, //SIZE,
	40, //EXPLOSION DAMAGE
	40, //EXPLOSION SIZE
	70, //DIRECT HIT DAMAGE
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
		64, //SPRITE OFFSET_Z
		32, //SPRITE SCALE
		32, //SIZE
	},
	//BLUE TORCH
	{
		SUB__LIGHT_BLUE_TORCH,
		//ANIM
		{
			NULL, 0, 0, 1, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		64, //SPRITE OFFSET_Z
		32, //SPRITE SCALE
		32, //SIZE
	},
	//GREEN TORCH
	{
		SUB__LIGHT_GREEN_TORCH,
		//ANIM
		{
			NULL, 0, 0, 1, 4, 1,
		},
		0.5, //ANIM SPEED
		0.5, //SPRITE OFFSET X
		0.5, //SPRITE OFFSET Y
		64, //SPRITE OFFSET_Z
		32, //SPRITE SCALE
		32, //SIZE
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
		32, //SPRITE OFFSET_Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET_Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
		32, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
	},
	//TELEPORTER
	{
		SUB__TARGET_TELEPORT,
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
		64, //SPRITE OFFSET Z
		32, //SPRITE SCALE
		32, //SIZE
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
	},
};

static const ParticleInfo PARTICLE_INFOS[] =
{
	//BLOOD SPLATTER
	{
		SUB__PARTICLE_BLOOD,
		//ANIM INFO
		{
			NULL, 0, 0, 0, 3, 0,
		},
		0.5, //time
		10, //sprite scale
	},
	//WALL HIT
	{
		SUB__PARTICLE_WALL_HIT,
		//ANIM INFO
		{
			NULL, 0, 0, 1, 4, 0,
		},
		0.5, //time
		10, //sprite scale
	}
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
	}
};

static const LightInfo LIGHT_INFOS[] =
{
	//TORCH
	{
		SUB__LIGHT_TORCH,
		512, //radius
		1, //attenuation
		1, // scale
		24, //deviance
		255, 255, 255, //color
	},
	//BLUE TORCH
	{
		SUB__LIGHT_BLUE_TORCH,
		512, //radius
		1, //attenuation
		1, // scale
		12, //deviance
		0, 0, 255, //color
	},
	//GREEN TORCH
	{
		SUB__LIGHT_GREEN_TORCH,
		512, //radius
		1, //attenuation
		1, // scale
		12, //deviance
		0, 255, 0, //color
	},
	//LAMP
	{
		SUB__LIGHT_LAMP,
		800, //radius
		1, //attenuation
		1, // scale
		12, //deviance
		255, 255, 255, //color
	}
};

int type;
float radius;
float attenuation;
float scale;
float deviance;
float color[3];

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
	int index = type - SUB__MOB_IMP;

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
