#ifndef G_INFO_H
#define G_INFO_H
#pragma once

//Lazy hardcoded game data
#include <stddef.h>
#include "g_common.h"

typedef enum
{
	MS__IDLE,
	MS__WALK,
	MS__MELEE,
	MS__MISSILE,
	MS__HIT,
	MS__DIE,
	MS__MAX
} MonsterState;

typedef enum
{
	MAS__IDLE,
	   
	MAS__WALK_FORWARD,
	MAS__WALK_FORWARD_SIDE,
	MAS__WALK_SIDE,
	MAS__WALK_BACK_SIDE,
	MAS__WALK_BACK,
	   
	MAS__ATTACK_FORWARD,
	MAS__ATTACK_FORWARD_SIDE,
	MAS__ATTACK_SIDE,
	MAS__ATTACK_BACK_SIDE,
	MAS__ATTACK_BACK,

	MAS__MELEE_FORWARD,
	MAS__MELEE_FORWARD_SIDE,
	MAS__MELEE_SIDE,
	MAS__MELEE_BACK_SIDE,
	MAS__MELEE_BACK,
	   
	MAS__HIT_FORWARD,
	MAS__HIT_SIDE,
	MAS__HIT_BACK,
	   
	MAS__DIE,
	MAS__EXPLODE,
	   
	MAS__MAX
} MonsterAnimState;


typedef void (*ActionFun)(struct Object* obj);

typedef struct
{
	ActionFun action_fun;
	int action_frame;
	int x_offset;
	int y_offset;
	int frame_count;
	int looping;
} AnimInfo;

typedef struct
{
	AnimInfo anim_states[MAS__MAX];
	int spawn_hp;
	int speed;
	int melee_damage;
	float size;
	float height;
	float weight;
	int hit_chance;
	float hit_time;
	float sprite_scale;
	float sprite_x_offset;
	float sprite_y_offset;
	float sprite_z_offset;
} MonsterInfo;

typedef struct
{
	int type;
	AnimInfo anim_info[2];
	int speed;
	float size;
	int explosion_damage;
	int explosion_size;
	int direct_damage;
	float sprite_scale;
} MissileInfo;

typedef enum
{
	SOUND__NONE,

	SOUND__FIREBALL_THROW,
	SOUND__FIREBALL_EXPLODE,
	SOUND__FIREBALL_FOLLOW,
	
	SOUND__IMP_ALERT,
	SOUND__IMP_HIT,
	SOUND__IMP_DIE,
	SOUND__IMP_ATTACK,

	SOUND__PINKY_ALERT,
	SOUND__PINKY_HIT,
	SOUND__PINKY_DIE,
	SOUND__PINKY_ATTACK,

	SOUND__BRUISER_ALERT,
	SOUND__BRUISER_HIT,
	SOUND__BRUISER_DIE,
	SOUND__BRUISER_ATTACK,

	SOUND__SHOTGUN_SHOOT,
	SOUND__PISTOL_SHOOT,
	SOUND__MACHINEGUN_SHOOT,
	SOUND__DEVASTATOR_SHOOT,
	SOUND__NO_AMMO,

	SOUND__DOOR_ACTION,

	SOUND__PLAYER_PAIN,
	SOUND__PLAYER_DIE,

	SOUND__SECRET_FOUND,

	SOUND__TELEPORT,
	
	SOUND__PICKUP_HP,
	SOUND__PICKUP_SPECIAL,
	SOUND__PICKUP_AMMO,

	SOUND__MUSIC1,

	SOUND__MAX
} SoundType;

static const char* SOUND_INFO[SOUND__MAX] =
{
	//NONE
	NULL,

	//FIREBALL THROW
	"assets/sfx/fireball_throw.wav",

	//FIREBALL EXPLODE
	"assets/sfx/fireball_explode.wav",

	//FIREBALL FOLLOW
	"assets/sfx/fireball_follow.wav",

	//IMP ALERT
	"assets/sfx/imp_alert.wav",

	//IMP HIT
	"assets/sfx/imp_hit.wav",

	//IMP DIE
	"assets/sfx/imp_die.wav",

	//IMP ATTACK
	"assets/sfx/imp_attack.wav",

	//PINKY ALERT
	"assets/sfx/pinky_alert.wav",

	//PINKY HIT
	"assets/sfx/pinky_hit.wav",

	//PINKY  DIE
	"assets/sfx/pinky_die.wav",

	//PINKY  ATTACK
	"assets/sfx/pinky_attack.wav",

	//BRUISER ALERT
	"assets/sfx/bruiser_act.wav",

	//BRUISER HIT
	"assets/sfx/bruiser_pain.wav",

	//BRUISER  DIE
	"assets/sfx/bruiser_death.wav",

	//BRUISER  ATTACK
	"assets/sfx/bruiser_attack.wav",

	//SHOTGUN SHOOT
	"assets/sfx/shotgun_shoot.wav",

	//PISTOL SHOOT
	"assets/sfx/pistol_shoot.wav",

	//MACHINEGUN SHOOT
	"assets/sfx/machinegun_shoot.wav",

	//DEVASTATOR SHOOT
	"assets/sfx/devastator_shoot.wav",

	//NO AMMO
	"assets/sfx/no_ammo.wav",

	//DOOR ACTION
	"assets/sfx/door_action.wav",

	//PLAYER PAIN
	"assets/sfx/player_pain.wav",

	//PLAYER DEATH
	"assets/sfx/player_death.wav",

	//SECRET FOUND
	"assets/sfx/secret.wav",

	//TELEPORT
	"assets/sfx/teleport.wav",

	//PICKUP HP
	"assets/sfx/pickup_hp.wav",

	//PICKUP SPECIAL
	"assets/sfx/pickup_special.wav",

	//PICKUP AMMO
	"assets/sfx/pickup_ammo.wav",

	//MUSIC1
	"assets/sfx/music.mp3"
};


typedef struct
{	
	int type;
	int damage;
	int spread;
	float cooldown;
	float screen_x;
	float screen_y;
	float scale;
} GunInfo;

typedef struct
{	
	int type;
	AnimInfo anim_info;
	float anim_speed;
	float sprite_offset_x;
	float sprite_offset_y;
	float sprite_offset_z;
	float sprite_scale;
	float size;
} ObjectInfo;

typedef struct
{
	int type;
	float radius;
	float attenuation;
	float scale;
} LightInfo;

typedef struct
{
	int type;
	AnimInfo anim_info;
	float time;
	float sprite_scale;
} ParticleInfo;

typedef struct
{
	int type;
	AnimInfo anim_info;
	float time;
	float sprite_scale;
} DecalInfo;


#define PICKUP_SMALLHP_HEAL 20
#define PICKUP_BIGHP_HEAL 50
#define PICKUP_AMMO_GIVE 12
#define PICKUP_ROCKETS_GIVE 12
#define PICKUP_SHOTGUN_AMMO_GIVE 12
#define PICKUP_MACHINEGUN_AMMO_GIVE 50
#define PICKUP_DEVASTATOR_AMMO_GIVE 12
#define PICKUP_INVUNERABILITY_TIME 15
#define PICKUP_QUAD_TIME 15

GunInfo* Info_GetGunInfo(int type);
MonsterInfo* Info_GetMonsterInfo(int type);
ObjectInfo* Info_GetObjectInfo(int type, int sub_type);
LightInfo* Info_GetLightInfo(int sub_type);
ParticleInfo* Info_GetParticleInfo(int sub_type);
DecalInfo* Info_GetDecalInfo(int sub_type);
MissileInfo* Info_GetMissileInfo(int sub_type);

static const char* LEVELS[] =
{
	"test.WAD",
	"E1M1.WAD",
	"E1M2.WAD",
	"E1M3.WAD",
	"M13.WAD",
	"ourmap00.WAD",
	"ourmap01.WAD",
	"ourmap02.WAD"
};

//SPECIAL ENUMS
typedef enum
{
	SPECIAL__NONE,
	SPECIAL__USE_DOOR = 1,
	SPECIAL__USE_DOOR_NEVER_CLOSE = 31,
	SPECIAL__USE_LIFT = 62,

	SPECIAL__TRIGGER_CRUSHER = 49, 
	
} SpecialType;
typedef enum
{
	SECTOR_SPECIAL__NONE,
	SECTOR_SPECIAL__LIGHT_FLICKER = 1,
	SECTOR_SPECIAL__LIGHT_GLOW = 8,

} SectorSpecialType;

#endif