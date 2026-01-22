#ifndef LIGHT_H
#define LIGHT_H
#pragma once

#include <Windows.h>

#include "g_common.h"
#include "dynamic_array.h"

#define LIGHTTRACE_MAX_HITS 100000

//LIGHTMAPPER STUFF
typedef enum
{
	LDT__POINT,
	LDT__SPOT,
	LDT__SUN,
	LDT__AREA,
	LDT__MAX
} LightDefType;
typedef enum
{
	LST__NONE,
	LST__CEIL,
	LST__FLOOR,
	LST__WALL,
	LST__SKY,
	LST__POINT,
	LST__MAX
}LightSurfType;
typedef struct
{
	LightDefType type;
	float deviance;
	float radius;
	float attenuation;
	float direction[3];
	float position[3];
	float bbox[2][2];
	float color[3];

	//for area lights
	LightSurfType area_surf_type;
	int area_surf_index;
} LightDef;
typedef struct
{
	Linedef* linedef;
	Sector* sector;
	float normal[3];
	float hit[3];
	float frac;
	Vec4 light_sample;
	Vec4 color_sample;
	LightSurfType hit_type;
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
	int hits[LIGHTTRACE_MAX_HITS];
	struct LightGlobal* globals;

	HANDLE active_event;
	HANDLE finished_event;
	HANDLE start_work_event;

	int sector_start;
	int sector_end;

	int grid_start;
	int grid_end;
} LightTraceThread;

typedef struct
{
	float sky_scale;

	LightDef* sun_lightdef;

	float* random_vectors;
	int num_random_vectors;

	float* deviance_vectors;
	int num_deviance_vectors;

	float* sun_deviance_vectors;
	int num_sun_deviance_vectors;

	float* ao_sample_vectors;
	int num_ao_sample_vectors;

	dynamic_array* light_list;

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

	CRITICAL_SECTION start_mutex;
	HANDLE start_work_event;

	LightTraceThread* thread;

	int bounce;
} LightGlobal;

void LightGlobal_Setup(struct LightGlobal* global, struct LightCompilerInfo* compiler_info);
void LightGlobal_Destruct(struct LightGlobal* global);
void Lightmap_Create(struct LightGlobal* global, Map* map);
void Lightblocks_Create(struct LightGlobal* global, Map* map);
void Lightmap_Sector(LightGlobal* global, LightTraceThread* thread, Sector* sector, int bounce);
void Lightblock_Process(LightGlobal* global, LightTraceThread* thread, Lightblock* block, float position[3]);

#endif // !LIGHT_H