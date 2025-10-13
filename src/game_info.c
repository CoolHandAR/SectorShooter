#include "game_info.h"

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
