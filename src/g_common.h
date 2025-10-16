#ifndef G_COMMON_H
#define G_COMMON_H
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <GLFW/glfw3.h>
#include "BVH_Tree2D.h"

#include "r_common.h"

#define TRACE_NO_HIT INT_MAX

#define EMPTY_TILE 0
#define DOOR_TILE 2
#define NULL_INDEX -1
#define MAX_OBJECTS 1024
#define TILE_SIZE 64

#define DOOR_SLEEP 0
#define DOOR_OPEN 1
#define DOOR_CLOSE 2
#define DOOR_AUTOCLOSE_TIME 5

#define MIN_LIGHT 20

#define MAX_RENDER_SCALE 3

typedef int16_t ObjectID;

typedef struct
{
	Image img;
	CollumnImage collumn_image;
	unsigned char name[10];
	int width_mask;
	int height_mask;
} Texture;

typedef enum
{
	GS__NONE,

	GS__MENU,
	GS__LEVEL,
	GS__LEVEL_END,
	GS__FINALE,
	GS__EXIT
} GameState;

typedef struct
{
	GameState state;

	float secret_timer;

	int prev_total_secrets;
	int prev_total_monsters;

	int prev_monsters_killed;
	int prev_secrets_found;

	int total_secrets;
	int total_monsters;

	int secrets_found;
	int monsters_killed;
} Game;

//A lazy way to store assets, but good enough for now
typedef struct
{
	Image wall_textures;
	Image shotgun_texture;
	Image pistol_texture;
	Image machinegun_texture;
	Image devastator_texture;
	Image object_textures;
	Image missile_textures;
	Image imp_texture;
	Image pinky_texture;
	Image bruiser_texture;
	Image particle_textures;
	Image menu_texture;
	Image missing_texture;

	Texture* flat_textures;
	int num_flat_textures;

	Texture test_texture;

	Texture* patchy_textures;
	int num_patchy_textures;
} GameAssets;

Game* Game_GetGame();
bool Game_Init();
void Game_Exit();
bool Game_LoadAssets();
void Game_DestructAssets();
void Game_Update(float delta);
void Game_Draw(Image* image, FontData* fd);
void Game_DrawHud(Image* image, FontData* fd);
void Game_SetState(GameState state);
GameState Game_GetState();
GameAssets* Game_GetAssets();
void Game_ChangeLevel();
void Game_Reset(bool to_start);
void Game_SecretFound();
Texture* Game_FindTextureByName(const char p_name[8]);

//Menu stuff
void Menu_Update(float delta);
void Menu_Draw(Image* image, FontData* fd);
void Menu_LevelEnd_Update(float delta, int secret_goal, int secret_max, int monster_goal, int monster_max);
void Menu_LevelEnd_Draw(Image* image, FontData* fd);
void Menu_Finale_Update(float delta);
void Menu_Finale_Draw(Image* image, FontData* fd);

typedef enum
{
	GUN__NONE,

	GUN__PISTOL,
	GUN__MACHINEGUN,
	GUN__SHOTGUN,
	GUN__DEVASTATOR,

	GUN__MAX
} GunType;

typedef enum
{
	DIR_NONE,
	DIR_NORTH,
	DIR_NORTH_EAST,
	DIR_NORTH_WEST,
	DIR_EAST,
	DIR_SOUTH_EAST,
	DIR_SOUTH,
	DIR_SOUTH_WEST,
	DIR_WEST,
	DIR_MAX
} DirEnum;

typedef enum
{
	OT__NONE,
	OT__PLAYER,
	OT__MONSTER,
	OT__LIGHT,
	OT__THING,
	OT__PICKUP,
	OT__MISSILE,
	OT__TRIGGER,
	OT__TARGET,
	OT__DOOR,
	OT__PARTICLE,
	OT__MAX
} ObjectType;

typedef enum
{
	SUB__NONE,

	//monsters
	SUB__MOB_IMP,
	SUB__MOB_PINKY,
	SUB__MOB_BRUISER,
	SUB__MOB_MAX,

	//pickups
	SUB__PICKUP_SMALLHP,
	SUB__PICKUP_BIGHP,
	SUB__PICKUP_AMMO,
	SUB__PICKUP_ROCKETS,
	SUB__PICKUP_INVUNERABILITY,
	SUB__PICKUP_QUAD_DAMAGE,
	SUB__PICKUP_SHOTGUN,
	SUB__PICKUP_MACHINEGUN,
	SUB__PICKUP_DEVASTATOR,
	SUB__PICKUP_MAX,

	//missiles
	SUB__MISSILE_FIREBALL,
	SUB__MISSILE_MEGASHOT,
	SUB__MISSILE_MAX,

	//TRIGGER
	SUB__TRIGGER_ONCE,
	SUB__TRIGGER_CHANGELEVEL,
	SUB__TRIGGER_SECRET,
	SUB__TRIGGER_SWITCH,
	SUB__TRIGGER_MAX,

	//TARGET
	SUB__TARGET_TELEPORT,
	SUB__TARGET_MAX,

	//DOOR
	SUB__DOOR_VERTICAL,
	SUB__DOOR_MAX,

	//LIGHTS
	SUB__LIGHT_TORCH,
	SUB__LIGHT_LAMP,
	SUB__LIGHT_MAX,

	//THINGS
	SUB__THING_RED_COLLUMN,
	SUB__THING_BLUE_COLLUMN,
	SUB__THING_RED_FLAG,
	SUB__THING_BLUE_FLAG,
	SUB__THING_MAX,

	//PARTICLES
	SUB__PARTICLE_BLOOD,
	SUB__PARTICLE_WALL_HIT,
	SUB__PARTICLE_MAX,

	SUB__MAX
} SubType;

typedef enum
{
	OBJ_FLAG__NONE = 0,

	OBJ_FLAG__JUST_ATTACKED = 1 << 0,
	OBJ_FLAG__EXPLODING = 1 << 1,
	OBJ_FLAG__JUST_HIT = 1 << 2,

	OBJ_FLAG__DOOR_NEVER_CLOSE = 1 << 3,
	OBJ_FLAG__TRIGGER_SWITCHED_ON = 1 << 4,

	OBJ_FLAG__GODMODE = 1 << 5,

	OBJ_FLAG__DONT_COLLIDE_WITH_OBJECTS = 1 << 6,
	OBJ_FLAG__DONT_COLLIDE_WITH_LINES = 1 << 7,
} ObjectFlag;

typedef struct
{
	int spatial_id;
	int sector_index;

	ObjectID id;
	Sprite sprite;

	ObjectType type;
	SubType sub_type;

	float x, y, z;
	float vel_x, vel_y, vel_z;
	float angle;
	float height;
	float step_height;
	float dropoff_height;
	float dir_x, dir_y, dir_z;
	float size;
	float speed;

	int hp;

	int flags;

	//for missiles and certain attacks
	struct Object* owner;

	//for monsters and triggers
	struct Object* target;

	//for various collision checks
	struct Object* col_object;

	//for rendering when traversing through the subsectors
	struct Object* sector_next;
	struct Object* sector_prev;

	//for monsters and doors
	float move_timer;
	float attack_timer;
	float stop_timer;
	float stuck_timer;
	DirEnum dir_enum;
	int state;
} Object;

typedef enum
{
	MS__NODE_NONE = 0,
	MF__NODE_SUBSECTOR = 0x8000,
	MF__MAX
} MapFlags;

typedef struct
{
	float x, y;
} Vertex;

typedef struct
{
	int x_offset;
	int y_offset;

	Texture* top_texture;
	Texture* middle_texture;
	Texture* bottom_texture;

	int sector_index;
} Sidedef;
typedef struct
{
	bool skip_draw;

	float x0, x1;
	float y0, y1;

	float dx, dy;
	float dot;

	float offset;
	int front_sector;
	int back_sector;
	
	int special;

	float width_scale;
	float bbox[2][2];

	Sidedef* sidedef;
} Line;

typedef struct
{
	float line_x, line_y, line_dx, line_dy;
	float bbox[2][4];

	int children[2];
} BSPNode;

typedef struct
{
	Object* object_list;

	Texture* floor_texture;
	Texture* ceil_texture;

	int sprite_add_index;

	float height;
	float floor, ceil;

	int light_level;

	float bbox[2][2];

	int index;
	bool is_sky;
} Sector;

typedef struct
{
	int line_offset;
	int num_lines;

	float bbox[2][2];

	Sector* sector;
} Subsector;

typedef struct
{
	BVH_Tree spatial_tree;

	int num_sectors;
	Sector* sectors;

	int num_sub_sectors;
	Subsector* sub_sectors;

	int num_line_segs;
	Line* line_segs;

	int num_nodes;
	BSPNode* bsp_nodes;

	int num_sidedefs;
	Sidedef* sidedefs;

	int reject_size;
	unsigned char* reject_matrix;

	int num_objects;
	Object objects[MAX_OBJECTS];

	ObjectID free_list[MAX_OBJECTS];
	int num_free_list;

	ObjectID sorted_list[MAX_OBJECTS];
	int num_sorted_objects;

	int player_spawn_point_x;
	int player_spawn_point_y;
	int player_spawn_point_z;
	int player_spawn_sector;
	float player_spawn_rot;

	int level_index;

	bool dirty_temp_light;
} Map;

void Map_SetDirtyTempLight();
int Map_GetLevelIndex();
Map* Map_GetMap();
Object* Map_NewObject(ObjectType type);
bool Map_LoadFromIndex(int index);
bool Map_Load(const char* filename);
Object* Map_GetObject(ObjectID id);
void Map_SetTempLight(int x, int y, int size, int light);
void Map_GetSize(int* r_width, int* r_height);
void Map_GetSpawnPoint(int* r_x, int* r_y, int* r_z, int* r_sector, float* r_rot);
void Map_UpdateObjects(float delta);
void Map_DeleteObject(Object* obj);
Subsector* Map_FindSubsector(float p_x, float p_y);
Sector* Map_FindSector(float p_x, float p_y);
Sector* Map_GetSector(int index);
bool Map_CheckSectorReject(int s1, int s2);
void Map_Destruct();


//Load stuff
bool Load_Doommap(const char* filename, Map* map);
bool Load_DoomIWAD(const char* filename, Map* map);

//Player stuff
void Player_Init(int keep);
Object* Player_GetObj();
void Player_Rotate(float rot);
void Player_Hurt(float dir_x, float dir_y);
void Player_HandlePickup(Object* obj);
void Player_Update(GLFWwindow* window, float delta);
void Player_GetView(float* r_x, float* r_y, float* r_z, float* r_yaw, float* r_angle);
void Player_MouseCallback(float x, float y);
void Player_Draw(Image* image, FontData* font);
float Player_GetSensitivity();
void Player_SetSensitivity(float sens);


//Movement stuff
float Move_GetLineDot(float x, float y, Line* line);
void Move_ClipVelocity(float x, float y, float* r_dx, float* r_dy, Line* clip_line);
bool Move_CheckPosition(Object* obj, float x, float y, int* r_sectorIndex);
bool Move_SetPosition(Object* obj, float x, float y);
bool Move_CheckStep(Object* obj, float p_stepX, float p_stepY, float p_size);
bool Move_ZMove(Object* obj, float p_moveZ);
bool Move_Object(Object* obj, float p_moveX, float p_moveY, bool p_slide);
bool Move_Unstuck(Object* obj);
bool Move_Teleport(Object* obj, float x, float y);
bool Move_Door(Object* obj, float delta);

//Checking
bool Check_CanObjectFitInSector(Object* obj, Sector* sector, Sector* backsector);

//Missile stuff
void Missile_Update(Object* obj, float delta);
void Missile_DirectHit(Object* obj, Object* target);
void Missile_Explode(Object* obj);

//Trace stuff
bool Trace_CheckBoxPosition(Object* obj, float x, float y, float size, float* r_floorZ, float* r_ceilZ, float* r_lowFloorZ);
int Trace_FindSlideHit(Object* obj, Line* vel_line, float start_x, float start_y, float end_x, float end_y, float size, float* best_frac);
int Trace_Line(Object* obj, float start_x, float start_y, float end_x, float end_y, float z, float* r_hitX, float* r_hitY, float* r_hitZ, float* r_frac);
bool Trace_CheckLineToTarget(Object* obj, Object* target);
int Trace_AreaObjects(Object* obj, float x, float y, float size);


//Object stuff
bool Object_ZPassCheck(Object* obj, Object* col_obj);
void Object_UnlinkSector(Object* obj);
void Object_LinkSector(Object* obj);
void Object_Hurt(Object* obj, Object* src_obj, int damage);
bool Object_CheckLineToTarget(Object* obj, Object* target);
bool Object_CheckSight(Object* obj, Object* target);
Object* Object_Missile(Object* obj, Object* target, int type);
bool Object_HandleObjectCollision(Object* obj, Object* collision_obj);
bool Object_HandleSwitch(Object* obj);
void Object_HandleTriggers(Object* obj, Object* trigger);
Object* Object_Spawn(ObjectType type, SubType sub_type, float x, float y, float z);
int Object_AreaEffect(Object* obj, float radius);

//Dir stuff
DirEnum DirVectorToDirEnum(int x, int y);
DirEnum DirVectorToRoundedDirEnum(int x, int y);
void DirEnumToDirEnumVector(DirEnum dir, DirEnum* r_x, DirEnum* r_y);

//Particle stuff
void Particle_Update(Object* obj, float delta);

//Monster stuff
void Monster_Spawn(Object* obj);
void Monster_SetState(Object* obj, int state);
void Monster_Update(Object* obj, float delta);
void Monster_Imp_FireBall(Object* obj);
void Monster_Bruiser_FireBall(Object* obj);
void Monster_Melee(Object* obj);

//BSP STUFF
int BSP_GetNodeSide(BSPNode* node, float p_x, float p_y);

//LINE CHECK STUFF
int Line_PointSide(Line* line, float p_x, float p_y);
int Line_BoxOnPointSide(Line* line, float p_bbox[2][2]);
float Line_Intercept(float p_x, float p_y, float p_dx, float p_dy, Line* line);
float Line_InterceptLine(Line* line1, Line* line2);
bool Line_SegmentInterceptSegmentLine(Line* line1, Line* line2, float* r_frac, float* r_interX, float *r_interY);

#endif