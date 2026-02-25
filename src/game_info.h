#ifndef G_INFO_H
#define G_INFO_H
#pragma once

//Lazy hardcoded game data
#include <stddef.h>
#include "g_common.h"

//#define DOOM_PREVIEW

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
	
	SOUND__BLOOD_IMP_ALERT,
	SOUND__BLOOD_IMP_HIT,
	SOUND__BLOOD_IMP_DIE,
	SOUND__BLOOD_IMP_ATTACK,

	SOUND__BOAR_ALERT,
	SOUND__BOAR_HIT,
	SOUND__BOAR_DIE,
	SOUND__BOAR_ATTACK,

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

	SOUND__DESERT_WIND,
	SOUND__TEMPLE_AMBIENCE,
	SOUND__JUNGLE_AMBIENCE,
	SOUND__FIRE,
	SOUND__WINTER_WIND,

	SOUND__TEMPLAR_ALERT,
	SOUND__TEMPLAR_HIT,
	SOUND__TEMPLAR_DIE,
	SOUND__TEMPLAR_ATTACK,

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

	//BLOOD IMP ALERT
	"assets/sfx/blood_imp_alert.wav",

	//BLOOD IMP HIT
	"assets/sfx/blood_imp_hit.wav",

	//BLOOD IMP DIE
	"assets/sfx/blood_imp_die.wav",

	//BLOOD IMP ATTACK
	"assets/sfx/blood_imp_attack.wav",

	//BOAR ALERT
	"assets/sfx/boar_alert.wav",

	//BOAR HIT
	"assets/sfx/boar_hit.wav",

	//BOAR  DIE
	"assets/sfx/boar_die.wav",

	//BOAR  ATTACK
	"assets/sfx/boar_attack.wav",

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
	"assets/sfx/music.mp3",

	//DESERT WIND
	"assets/sfx/desert_wind.mp3",

	//TEMPLE AMBIENCE
	"assets/sfx/temple_ambience.mp3",

	//JUNGLE AMBIENCE
	"assets/sfx/jungle_ambience.mp3",

	//FIRE
	"assets/sfx/fire.wav",

	//WINTER WIND
	"assets/sfx/winter_wind.mp3",

	//TEMPLAR ALERT
	"assets/sfx/templar_act.wav",

	//TEMPLAR HIT
	"assets/sfx/templar_pain.wav",

	//TEMPLAR  DIE
	"assets/sfx/templar_death.wav",

	//TEMPLAR  ATTACK
	"assets/sfx/templar_attack.wav"
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
	int light_frame;
	float light_color[3];
	float shake_scale;
	float shake_time;
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
	int collidable;
	float height;
} ObjectInfo;

typedef struct
{
	int type;
	float radius;
	float attenuation;
	float scale;
	float deviance;
	float z_offset;
	float color[3];
} LightInfo;

typedef struct
{
	int type;
	AnimInfo anim_info;
	float time;
	float sprite_scale;
	float anim_speed_scale;
} ParticleInfo;

typedef struct
{
	int type;
	AnimInfo anim_info;
	float time;
	float sprite_scale;
} DecalInfo;

typedef struct
{
	SoundType sound_type;
	float volume;
	float rolloff;
} SFXInfo;

typedef struct
{
	float sky_scale;
	float sun_z;
	float sun_color[3];
} LightCompilerInfo;

#define PICKUP_SMALLHP_HEAL 20
#define PICKUP_BIGHP_HEAL 50
#define PICKUP_AMMO_GIVE 12
#define PICKUP_ROCKETS_GIVE 5
#define PICKUP_SHOTGUN_AMMO_GIVE 12
#define PICKUP_MACHINEGUN_AMMO_GIVE 50
#define PICKUP_DEVASTATOR_AMMO_GIVE 6
#define PICKUP_INVUNERABILITY_TIME 20
#define PICKUP_QUAD_TIME 20

GunInfo* Info_GetGunInfo(int type);
MonsterInfo* Info_GetMonsterInfo(int type);
ObjectInfo* Info_GetObjectInfo(int type, int sub_type);
LightInfo* Info_GetLightInfo(int sub_type);
ParticleInfo* Info_GetParticleInfo(int sub_type);
DecalInfo* Info_GetDecalInfo(int sub_type);
MissileInfo* Info_GetMissileInfo(int sub_type);
SFXInfo* Info_GetSFXInfo(int sub_type);
LightCompilerInfo* Info_GetLightCompilerInfo(int index);

static const char* LEVELS[] =
{
	"assets/maps/ourmap00.WAD",
	"assets/maps/ourmap01.WAD",
	"assets/maps/ourmap02.WAD",

#ifdef DOOM_PREVIEW
	"assets/maps/E1M1.WAD",
	"assets/maps/E1M2.WAD",
	"assets/maps/E1M3.WAD",
	"assets/maps/MAP01.WAD",
	"assets/maps/MAP07.WAD",
	"assets/maps/MAP13.WAD",
	"assets/maps/MAP18.WAD",
	"assets/maps/MAP30.WAD",
	"assets/maps/test.WAD"
#endif // DOOM_PREVIEW
};
static const char* SKIES[] =
{
	"SKYPNG1",
	"SKYPNG2",
	"SKYPNG3",
	"SKY1",
	"SKY1",
	"SKY1",
	"SKY1",
	"SKY1",
	"SKY1",
	"SKY1",
	"SKY1",
	"SKY1"
};

//SPECIAL ENUMS
typedef enum
{
	SPECIAL__NONE,
	SPECIAL__USE_DOOR = 1,
	SPECIAL__USE_DOOR_NEVER_CLOSE = 31,
	SPECIAL__USE_LIFT = 62,

	SPECIAL__TRIGGER_EXIT = 11,
	SPECIAL__TRIGGER_CRUSHER_LOOP = 25,
	SPECIAL__TRIGGER_DOOR_NEVER_CLOSE = 29,
	SPECIAL__TRIGGER_CRUSHER = 49,
	SPECIAL__TRIGGER_TELEPORT = 97,
	SPECIAL__WALL_AREA_LIGHT = 138
} SpecialType;
typedef enum
{
	SECTOR_SPECIAL__NONE,
	SECTOR_SPECIAL__LIGHT_FLICKER = 1,
	SECTOR_SPECIAL__LIGHT_GLOW = 8,
	SECTOR_SPECIAL__SECRET = 9,
	SECTOR_SPECIAL__LIGHT_AREA = 21,
	SECTOR_SPECIAL__LIGHT_CEIL_AREA = 22,
	SECTOR_SPECIAL__LIGHT_FLOOR_AREA = 23,
} SectorSpecialType;

#endif