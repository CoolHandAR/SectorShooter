#pragma once
#include "g_common.h"
#include <Windows.h>

//LIGHT COMPILING STUFF
typedef enum
{
	LDT__POINT,
	LDT__SPOT,
	LDT__SUN,
	LDT__AREA,
	LDT__MAX
} LightDefType;
typedef struct
{
	LightDefType type;
	float radius;
	float attenuation;
	float direction[3];
	float position[3];
	float bbox[2][2];
	float color[3];
} LightDef;
typedef struct
{
	Linedef* linedef;
	float normal[3];
	float hit[3];
	float frac;
	Vec4 light_sample;
	float color_sample[3];
	bool hit_sky;
} LightTraceResult;

typedef struct
{
	int sector_index;
	int num_lines;
	Linedef** lines;
} LinedefList;

typedef struct
{
	HANDLE thread_handle;
	int hits[10000];
	struct LightGlobal* globals;

	HANDLE active_event;
	HANDLE finished_event;
	HANDLE start_work_event;

	int sector_start;
	int sector_end;
} LightTraceThread;

typedef struct
{
	LightDef* sun_lightdef;

	float* random_vectors;
	int num_random_vectors;

	float* deviance_vectors;
	int num_deviance_vectors;

	LightDef* light_list;
	int num_lights;

	LinedefList* linedef_list;
	int num_linedef_lists;

	Lightmap* floor_back_lightmaps;
	Lightmap* ceil_back_lightmaps;
	Lightmap* line_back_lightmaps;

	int num_back_floor_lightmaps;
	int num_back_ceil_lightmaps;
	int num_back_line_lightmaps;

	LightTraceThread* threads;
	int num_threads;
	int threads_finished;

	int ticks;
	bool shutdown_threads;
	bool work;

	CONDITION_VARIABLE start_work_cv;
	CRITICAL_SECTION start_mutex;

	CONDITION_VARIABLE end_work_cv;
	CRITICAL_SECTION end_mutex;

	HANDLE start_work_event;

	LightTraceThread thread;

	int bounce;
	float bounce_scale;
} LightGlobal;
void LightGlobal_Setup(struct LightGlobal* global);
void LightGlobal_Destruct(struct LightGlobal* global);
void Lightmap_Create(struct LightGlobal* global, Map* map);
void Lightblocks_Create(struct LightGlobal* global, Map* map);
void Lightmap_Sector(LightGlobal* global, LightTraceThread* thread, Sector* sector, int bounce);