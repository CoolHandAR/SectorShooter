#ifndef G_COMMON_H
#define G_COMMON_H
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <GLFW/glfw3.h>

#include "BVH_Tree2D.h"
#include "r_common.h"
#include "sound.h"

#include "u_object_pool.h"

#define DONT_DRAW_HUD
//#define DRAW_LIGHT_POINTS
//#define DRAW_TRACE_POINTS
#define DONT_FILE_LIGHTMAPS
#define DISABLE_LIGHTMAPS
#define TRACE_NO_HIT INT_MAX

#define NULL_INDEX -1
#define MAX_OBJECTS 2048

#define SECTOR_SLEEP 0
#define SECTOR_OPEN 1
#define SECTOR_CLOSE 2
#define SECTOR_AUTOCLOSE_TIME 2
#define GRAVITY_SCALE -500
#define PLAYER_SIZE 8
#define PLAYER_STEP_SIZE 50
#define PLAYER_ACCEL 30
#define PLAYER_FRICTION 0.88
#define PLAYER_HEIGHT 100
#define PLAYER_GRAVITY_SCALE 0.5
#define BLOOD_SPLATTER_TRACE_DIST 170

#define MAX_RENDER_SCALE 3

#define MAX_MAP_NAME 32
#define MAX_STATUS_MESSAGE 32 
#define STATUS_TIME 5
#define LIGHTMAP_LUXEL_SIZE 16.0
#define LIGHTMAP_INV_LUXEL_SIZE 1.0 / LIGHTMAP_LUXEL_SIZE
#define LIGHT_GRID_SIZE 64.0
#define LIGHT_GRID_Z_SIZE 128.0
#define LIGHT_GRID_WORLD_BIAS_MULTIPLIER 2.0

static const char SAVEFOLDER[] = { "\\saves" };

typedef int16_t ObjectID;

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

	int prev_total_secrets;
	int prev_total_monsters;

	int prev_monsters_killed;
	int prev_secrets_found;

	int total_secrets;
	int total_monsters;

	int secrets_found;
	int monsters_killed;

	int level_index;

	char status_msg[MAX_STATUS_MESSAGE];
	float status_msg_timer;

	int tick;
} Game;

//A lazy way to store assets, but good enough for now
typedef struct
{
	Image shotgun_texture;
	Image pistol_texture;
	Image machinegun_texture;
	Image devastator_texture;
	Image object_textures;
	Image missile_textures;
	Image blood_imp_texture;
	Image boar_texture;
	Image bruiser_texture;
	Image templar_texture;
	Image particle_textures;
	Image decal_textures;
	Image menu_texture;
	Texture missing_texture;

	Texture* flat_textures;
	int num_flat_textures;

	Texture* patchy_textures;
	int num_patchy_textures;

	Texture* png_textures;
	int num_png_texures;
} GameAssets;

Game* Game_GetGame();
bool Game_Init();
void Game_Exit();
bool Game_LoadAssets();
void Game_DestructAssets();
void Game_NewTick();
int Game_GetTick();
void Game_LogicUpdate(double delta);
void Game_SmoothUpdate(double lerp, double delta);
void Game_Draw(Image* image, FontData* fd);
void Game_DrawHud(Image* image, FontData* fd, int start_x, int end_x);
void Game_SetState(GameState state);
GameState Game_GetState();
GameAssets* Game_GetAssets();
bool Game_ChangeLevel(int level_index, bool player_keep_stuff);
void Game_NextLevel();
void Game_Save(int slot, char desc[32]);
bool Game_Load(int slot);
int Game_GetLevelIndex();
void Game_SetStatusMessage(char msg[MAX_STATUS_MESSAGE]);
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
void Menu_DrawBackGround(Image* image);
void Menu_Init();

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
	OT__DOOR,
	OT__CRUSHER,
	OT__LIFT,
	OT__LIGHT_STROBER,
	OT__PARTICLE,
	OT__DECAL,
	OT__SFX_EMITTER,
	OT__MAX
} ObjectType;

typedef enum
{
	SUB__NONE,

	//monsters
	SUB__MOB_BLOOD_IMP,
	SUB__MOB_BOAR,
	SUB__MOB_BRUISER,
	SUB__MOB_TEMPLAR,
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
	SUB__MISSILE_STARSTRIKE,
	SUB__MISSILE_MAX,

	//TRIGGER
	SUB__TRIGGER_ONCE,
	SUB__TRIGGER_CHANGELEVEL,
	SUB__TRIGGER_SECRET,
	SUB__TRIGGER_SWITCH,
	SUB__TRIGGER_MAX,

	//DOOR
	SUB__DOOR_VERTICAL,
	SUB__DOOR_MAX,

	//CRUSHER
	SUB__CRUSHER_LOOPING,
	SUB__CRUSHER_ONCE,

	//LIGHTS
	SUB__LIGHT_TORCH,
	SUB__LIGHT_BLUE_TORCH,
	SUB__LIGHT_GREEN_TORCH,
	SUB__LIGHT_LAMP,
	SUB__LIGHT_BLUE_LAMP,
	SUB__LIGHT_GREEN_LAMP,
	SUB__LIGHT_FLAME_URN,
	SUB__LIGHT_MAX,

	//THINGS
	SUB__THING_RED_COLLUMN,
	SUB__THING_BLUE_COLLUMN,
	SUB__THING_RED_FLAG,
	SUB__THING_BLUE_FLAG,
	SUB__THING_CACTUS0,
	SUB__THING_CACTUS1,
	SUB__THING_CACTUS2,
	SUB__THING_DEAD_TREE0,
	SUB__THING_DEAD_TREE1,
	SUB__THING_DEAD_TREE2,
	SUB__THING_DEAD_TREE3,
	SUB__THING_SPINNING_PYRAMID,
	SUB__THING_BUSH0,
	SUB__THING_BUSH1,
	SUB__THING_BUSH2,
	SUB__THING_TREE0,
	SUB__THING_TREE1,
	SUB__THING_TREE2,
	SUB__THING_TREE3,
	SUB__THING_TELEPORTER,
	SUB__THING_STALAG0,
	SUB__THING_STALAG1,
	SUB__THING_STALAG2,
	SUB__THING_WINTER_TREE0,
	SUB__THING_WINTER_TREE1,
	SUB__THING_WINTER_TREE2,
	SUB__THING_WINTER_TREE3,
	SUB__THING_ROCK0,
	SUB__THING_ROCK1,
	SUB__THING_FALLING_SAND,
	SUB__THING_FALLING_SNOW,
	SUB__THING_MAX,

	//LIGHT STROBER
	SUB__LIGHT_STROBER_GLOW,
	SUB__LIGHT_STROBER_FLICKER_RANDOM,

	//PARTICLES
	SUB__PARTICLE_BLOOD,
	SUB__PARTICLE_WALL_HIT,
	SUB__PARTICLE_BLOOD_EXPLOSION,
	SUB__PARTICLE_FIRE_SPARKS,
	SUB__PARTICLE_BULLET_IMPACT,
	SUB__PARTICLE_EXPLOSION,
	SUB__PARTICLE_MAX,

	//DECALS
	SUB__DECAL_WALL_HIT,
	SUB__DECAL_SCORCH,
	SUB__DECAL_BLOOD_SPLATTER,

	//SFX EMITTERS
	SUB__SFX_DESERT_WIND,
	SUB__SFX_TEMPLE_AMBIENCE,
	SUB__SFX_JUNGLE_AMBIENCE,
	SUB__SFX_WINTER_WIND,

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

	OBJ_FLAG__IGNORE_POSITION_CHECK = 1 << 8,
	OBJ_FLAG__IGNORE_SOUND = 1 << 9,
	OBJ_FLAG__FULL_BRIGHT = 1 << 10,
	OBJ_FLAG__SUPER_MOB = 1 << 11,
	OBJ_FLAG__MINI_MISSILE = 1 << 12,
	OBJ_FLAG__JUST_TELEPORTED = 1 << 13,
} ObjectFlag;

typedef struct
{
	int spatial_id;
	int sector_index;

	int unique_id;
	ObjectID id;
	Sprite sprite;

	ObjectType type;
	SubType sub_type;

	float x, y, z;
	float prev_x, prev_y, prev_z;
	float vel_x, vel_y, vel_z;
	float height;
	float step_height;
	float dir_x, dir_y, dir_z;
	float size;
	float speed;

	float floor_clamp;
	float ceil_clamp;

	int hp;

	unsigned int flags;

	//for missiles and certain attacks
	struct Object* owner;

	//for monsters and triggers
	struct Object* target;

	//for rendering when traversing through the subsectors
	struct Object* sector_next;
	struct Object* sector_prev;

	//for rendering when object sprite crosses multiple sectors
	dynamic_array* linked_sector_array;

	//for various collision checks
	int collision_hit;

	//for monsters, special sectors and doors
	float move_timer;
	float attack_timer;
	float stop_timer;
	float stuck_timer;
	float hit_timer;
	DirEnum dir_enum;
	int state;
	
	//for objects with active sound sources (missiles, torches, etc..)
	SoundID sound_id;

	bool sector_linked;
} Object;

typedef enum
{
	MF__NODE_NONE = 0,
	MF__NODE_SUBSECTOR = 0x8000,
	MF__LINE_BLOCKING = 1,
	MF__LINE_DONT_PEG_TOP = 8,
	MF__LINE_DONT_PEG_BOTTOM = 16,
	MF__LINE_SECRET = 32,
	MF__LINE_BLOCK_SOUND = 64,
	MF__LINE_DONT_DRAW = 128,
	MF__LINE_MAPPED = 256,
	MF__MAX
} MapFlags;

typedef struct
{
	float x, y;
} Vertex;

typedef struct
{
	Texture* top_texture;
	Texture* middle_texture;
	Texture* bottom_texture;

	int x_offset;
	int y_offset;
} Sidedef;
typedef struct
{
	float x0, x1;
	float y0, y1;

	float dx, dy;
	float dot;

	float width;

	float bbox[2][2];

	int front_sector;
	int back_sector;

	int index;
	int special;
	int sector_tag;
	int flags;

	int sides[2];

	Lightmap lightmap;
} Linedef;
typedef struct
{
	float x0, x1;
	float y0, y1;

	float offset;

	int front_sector;
	int back_sector;
	
	float width_scale;

	Sidedef* sidedef;
	Linedef* linedef;
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
	Object_Pool* render_object_list;
	dynamic_array* sorted_render_object_list;

	Texture* floor_texture;
	Texture* ceil_texture;

	Lightmap floor_lightmap;
	Lightmap ceil_lightmap;

	int sector_tag;
	int special;
	ObjectID sector_object;

	int sound_propogation_check;
	
	//needed for smooth rendering
	float r_floor, r_ceil;
	float floor, ceil;
	float base_floor, base_ceil;

	//needed for raising and lowering sectors
	float neighbour_sector_value;

	int light_level;

	float bbox[2][2];

	int index;
	bool is_sky;
} Sector;

typedef struct
{
	int line_offset;
	int num_lines;

	Sector* sector;
} Subsector;

typedef struct
{
	Vec3_u16 light;
} Lightblock;

typedef struct
{
	Lightblock* blocks;
	float size[3];
	float inv_size[3];
	
	int block_size[3];

	float origin[3];
	float bounds[3];
} Lightgrid;

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

	int num_linedefs;
	Linedef* linedefs;

	int reject_size;
	unsigned char* reject_matrix;

	Lightgrid lightgrid;

	int num_objects;
	Object objects[MAX_OBJECTS];

	ObjectID free_list[MAX_OBJECTS];
	int num_free_list;

	ObjectID sorted_list[MAX_OBJECTS];
	int num_sorted_objects;

	int unique_id_index;

	int player_spawn_point_x;
	int player_spawn_point_y;
	int player_spawn_point_z;
	int player_spawn_sector;
	float player_spawn_rot;

	float world_bounds[2][2];
	float world_size[2];
	float world_height;
	float world_min_height;
	float world_max_height;

	bool has_sun_entity;
	float sun_angle;
	float sky_color[3];

	char name[MAX_MAP_NAME];
} Map;

Map* Map_GetMap();
Object* Map_NewObject(ObjectType type);
bool Map_LoadFromIndex(int index);
bool Map_Load(const char* filename, const char* skyname, struct LightCompilerInfo* light_compiler_info);
Object* Map_GetObject(ObjectID id);
Object* Map_FindObjectByUniqueID(int unique_id);
Object* Map_GetNextObjectByType(int* r_iterIndex, int type);
void Map_GetSpawnPoint(int* r_x, int* r_y, int* r_z, int* r_sector, float* r_rot);
void Map_UpdateObjects(float delta);
void Map_SmoothUpdate(double lerp, double delta);
void Map_DeleteObject(Object* obj);
Subsector* Map_FindSubsector(float p_x, float p_y);
Sector* Map_FindSector(float p_x, float p_y);
Sector* Map_GetSector(int index);
Sector* Map_GetNextSectorByTag(int* r_iterIndex, int tag);
Line* Map_GetLine(int index);
Linedef* Map_GetLineDef(int index);
Sidedef* Map_GetSideDef(int index);
bool Map_CheckSectorReject(int s1, int s2);
BVH_Tree* Map_GetSpatialTree();
void Map_SetupLightGrid(Lightblock* data);
void Map_UpdateObjectsLight();
void Map_CalcBlockLight(float p_x, float p_y, float p_z, Vec3_u16* dest);
void Map_Destruct();

//Load stuff
bool Load_Doommap(const char* filename, const char* skyname, struct LightCompilerInfo* light_compiler_info, Map* map);
bool Load_DoomIWAD(const char* filename);
bool Load_Lightmap(const char* filename, Map* map);
bool Save_Lightmap(const char* filename, Map* map);

//Player stuff
typedef struct
{
	Sprite gun_sprite;
	Object* obj;

	float view_x, view_y, view_z;
	float prev_x, prev_y;

	float angle;
	int move_x;
	int move_y;
	int slow_move;

	int buck_ammo;
	int bullet_ammo;
	int rocket_ammo;

	float save_timer;
	float alive_timer;
	float gun_timer;
	float hurt_timer;
	float godmode_timer;
	float quad_timer;
	float hp_tick_timer;
	float hit_timer;
	float use_timer;
	float shake_timer;
	float menu_timer;

	float bob;
	float gun_offset_x;
	float gun_offset_y;
	float shake_scale;

	float current_speed;
	float z_vel;

	int stored_hp;

	GunType gun;
	struct GunInfo* gun_info;

	bool gun_check[GUN__MAX];
	Sprite gun_sprites[GUN__MAX];

	float sensitivity;
} PlayerData;

void Player_Init(int keep);
Object* Player_GetObj();
PlayerData* Player_GetPlayerData();
void Player_Rotate(float rot);
void Player_Hurt(float dir_x, float dir_y);
void Player_HandlePickup(Object* obj);
void Player_Update(GLFWwindow* window, float delta);
void Player_LerpUpdate(double lerp, double delta);
void Player_GetView(float* r_x, float* r_y, float* r_z, float* r_yaw, float* r_angle);
void Player_MouseCallback(float x, float y);
void Player_DrawHud(Image* image, FontData* font, int start_x, int end_x);
float Player_GetSensitivity();
void Player_SetSensitivity(float sens);
void Player_SetGun(GunType gun_type);


//Movement stuff
float Move_GetLineDot(float x, float y, Linedef* line);
void Move_Accelerate(Object* obj, float p_moveX, float p_moveY, float p_moveZ);
void Move_ClipVelocity(float x, float y, float* r_dx, float* r_dy, Linedef* clip_line);
bool Move_CheckPosition(Object* obj, float x, float y, float size, int* r_sectorIndex, float* r_floorZ, float* r_ceilZ);
bool Move_SetPosition(Object* obj, float x, float y, float size);
bool Move_CheckStep(Object* obj, float p_stepX, float p_stepY, float p_size);
bool Move_ZMove(Object* obj, float p_moveZ);
bool Move_Object(Object* obj, float p_moveX, float p_moveY, float delta, bool p_slide);
bool Move_Unstuck(Object* obj);
bool Move_Teleport(Object* obj, float x, float y);
bool Move_Sector(Sector* sector, float p_moveFloor, float p_moveCeil, float p_minFloorClamp, float p_maxFloorFlamp, float p_minCeilClamp, float p_maxCeilClamp, bool p_crush);

//Checking
bool Check_CanObjectFitInSector(Object* obj, Sector* sector, Sector* backsector);

//Missile stuff
void Missile_Update(Object* obj, double delta);
void Missile_DirectHit(Object* obj, Object* target);
void Missile_Explode(Object* obj);

//Trace stuff
int* Trace_GetSpecialLines(int* r_length);
int* Trace_GetHitObjects();
bool Trace_CheckBoxPosition(Object* obj, float x, float y, float size, float* r_floorZ, float* r_ceilZ, float* r_lowFloorZ);
int Trace_FindSlideHit(Object* obj, float start_x, float start_y, float end_x, float end_y, float size, float* best_frac);
int Trace_AttackLine(Object* obj, float start_x, float start_y, float end_x, float end_y, float z, float range, float* r_hitX, float* r_hitY, float* r_hitZ, float* r_frac);
bool Trace_CheckLineToTarget(Object* obj, Object* target);
int Trace_FindSpecialLine(float start_x, float start_y, float end_x, float end_y, float z);
int Trace_AreaObjects(Object* obj, float x, float y, float size);
int Trace_SectorObjects(Sector* sector);
int Trace_SectorLines(Sector* sector, bool front_only);
int Trace_SectorAll(Sector* sector);
int Trace_FindLine(float start_x, float start_y, float start_z, float end_x, float end_y, float end_z, bool ignore_sky_plane, int* r_hits, int max_hit_count, float* r_hitX, float* r_hitY, float* r_hitZ, float* r_frac);
int Trace_FindSectors(int ignore_sector_index, float bbox[2][2]);

//Object stuff
void Object_RemoveSectorsFromLinkedArray(Object* obj);
void Object_AddSectorToLinkedArray(Object* obj, Sector* sector);
void Object_UpdateRenderSectors(Object* obj);
bool Object_ZPassCheck(Object* obj, Object* col_obj);
void Object_UnlinkSector(Object* obj);
void Object_LinkSector(Object* obj);
bool Object_IsSectorLinked(Object* obj);
void Object_Hurt(Object* obj, Object* src_obj, int damage, bool explosive);
bool Object_Crush(Object* obj);
bool Object_CheckLineToTarget(Object* obj, Object* target);
bool Object_CheckSight(Object* obj, Object* target, bool check_angle);
Object* Object_Missile(Object* obj, Object* target, int type);
bool Object_HandleObjectCollision(Object* obj, Object* collision_obj);
bool Object_HandleSwitch(Object* obj);
void Object_HandleTriggers(Object* obj, Object* trigger);
Object* Object_Spawn(ObjectType type, SubType sub_type, float x, float y, float z);
int Object_AreaEffect(Object* obj, float radius);
void Object_ConsumePickup(Object* obj);

//SFX Stuff
void SFX_Update(Object* obj);


//Event Stuff
typedef enum
{
	EVENT_TRIGGER__LINE_WALK_OVER,
	EVENT_TRIGGER__LINE_USE,
	EVENT_TRIGGER__LINE_SHOOT
} EventLineTriggerType;

void Event_TriggerSpecialLine(Object* obj, int side, Linedef* line, EventLineTriggerType trigger_type);
void Event_CreateExistingSectorObject(int type, int sub_type, int state, float stop_timer, int sector_index);

void Sector_UpdateSortedList(Sector* sector);
void Sector_RemoveObjectFromRenderList(Sector* sector, Object* object);
void Sector_AddObjectToRenderList(Sector* sector, Object* object);
void Sector_Secret(Sector* sector);
void Sector_CreateLightStrober(Sector* sector, SubType light_type);
float Sector_FindHighestNeighbourCeilling(Sector* sector);
float Sector_FindLowestNeighbourCeilling(Sector* sector);
float Sector_FindLowestNeighbourFloor(Sector* sector);
void Sector_RemoveSectorObject(Sector* sector);
void Crusher_Update(Object* obj, float delta);
void Door_Update(Object* obj, float delta);
void Lift_Update(Object* obj, float delta);
void LightStrober_Update(Object* obj, float delta);

//Dir stuff
DirEnum DirVectorToDirEnum(int x, int y);
DirEnum DirVectorToRoundedDirEnum(int x, int y);
void DirEnumToDirEnumVector(DirEnum dir, DirEnum* r_x, DirEnum* r_y);

//Particle stuff
void Particle_Update(Object* obj, float delta);

//Decal stuff
void Decal_BloodTrace(Object* obj, float x, float y, float z, float p_dir_x, float p_dir_y, float dir_z);
void Decal_Update(Object* obj, float delta);

//Monster stuff
void Monster_Spawn(Object* obj);
void Monster_SetState(Object* obj, int state);
void Monster_SetTarget(Object* obj, Object* target);
void Monster_UpdateSpriteAnimation(Object* obj, float delta);
void Monster_Update(Object* obj, float delta);
void Monster_BloodImp_FireBall(Object* obj);
void Monster_Bruiser_FireBall(Object* obj);
void Monster_Templar_StarStrike(Object* obj);
void Monster_Melee(Object* obj);
void Monster_WakeAll(Object* waker);
void Monster_CheckForPushBack(Object* obj, float delta);
void Monster_ApplyPushback(Object* pusher, Object* monster);
void Monster_SetAsSuper(Object* obj);

//Visual map stuff
void VisualMap_Init();
void VisualMap_Update(GLFWwindow* window, double delta);
void VisualMap_Draw(Image* image, FontData* font);
int VisualMap_GetMode();

//Save/load game stuff
#define SAVE_GAME_NAME_MAX_CHARS 32
#define MAX_SAVE_SLOTS 3
typedef struct
{
	char name[SAVE_GAME_NAME_MAX_CHARS];
	bool is_valid;
} SaveSlot;

bool Save_Game(const char* filename, const char name[SAVE_GAME_NAME_MAX_CHARS]);
bool Load_Game(const char* filename);
bool Scan_SaveGames(SaveSlot slots[MAX_SAVE_SLOTS]);

//BSP STUFF
int BSP_GetNodeSide(BSPNode* node, float p_x, float p_y);

//LINE CHECK STUFF
int Line_PointSide(Linedef* line, float p_x, float p_y);
int Line_PointSideBias(Linedef* line, float p_x, float p_y, float bias);
int Line_BoxOnPointSide(Linedef* line, float p_bbox[2][2]);
float Line_Intercept(float p_x, float p_y, float p_dx, float p_dy, Linedef* line);
float Line_InterceptLine(Linedef* line1, Linedef* line2);
bool Line_SegmentInterceptSegmentLine(Linedef* line1, Linedef* line2, float* r_frac, float* r_interX, float *r_interY);

#endif