#include "g_common.h"
#include "light.h"

#include "game_info.h"
#include "u_math.h"
#include <Windows.h>

//#define ONLY_SUN
//#define DISABLE_AO

//internals
#define BOUNCE_EXIT -3
#define BOUNCE_LIGHTGRID -2
#define BOUNCE_AO -1

#define MAX_THREAD_HITS 10000
#define NUM_THREADS 10

#define AA_SAMPLES 4

#define NUM_BOUNCES 6
#define RADIOSITY_TRACE_DIST 1024 
#define RADIOSITY_SAMPLES 64
#define RADIOSITY_RADIUS 512
#define RADIOSITY_ATTENUATION 1
#define RADIOSITY_SCALE 0.75

#define DEVIANCE_SAMPLES 4
#define SUN_DEVIANCE_SAMPLES 4
#define SUN_DEVIANCE_SCALE 0.01

#define SKY_SCALE 3

//Taken from q3map2
#define AO_CONE_ANGLE 88
#define AO_NUM_ANGLE_STEPS 16
#define AO_NUM_ELEVATION_STEPS 3
#define AO_NUM_VECTORS ( AO_NUM_ANGLE_STEPS * AO_NUM_ELEVATION_STEPS )
#define AO_DEPTH 64
#define AO_GAIN 0.8
#define AO_SCALE 1.5

#define FLOOR_CEILING_TRACE_BIAS -1.0
#define FLOOR_CEILING_TRACE_NORMAL_BIAS 0.2
#define LINE_TRACE_BIAS 0.5
#define LINE_TRACE_NORMAL_BIAS 2.1

#define LIGHTMAP_GAMMA 1.0
#define LIGHTMAP_EXPOSURE 1.0
#define LIGHTMAP_COMPNENSTANTE 1.0

static void ClampLightVector(Vec4* v)
{
	float luma[4] = { 0.299, 0.587, 0.114, 1};
	
	float rgba[4] = { v->r, v->g, v->b, v->a };

	float sat = 1000;

	for (int i = 0; i < 4; i++)
	{
		if (rgba[i] > 255)
		{
			sat = min(sat, (luma[i] - 255e3) / (luma[i] - rgba[i]));
		}
		else if (rgba[i] < 0)
		{
			sat = min(sat, luma[i] / (luma[i] - rgba[i]));
		}
	}
	if (sat != 1.0)
	{
		for (int i = 0; i < 4; i++)
		{
			rgba[i] = (rgba[i] - luma[i]) * sat / 1e3 + luma[i];

			rgba[i] = Math_Clamp(rgba[i], 0, 255);
		}
	}

	v->r = rgba[0];
	v->g = rgba[1];
	v->g = rgba[2];
	v->a = rgba[3];
}

static void ClampLightVector3(Vec4* v)
{
	double rgba[4] = { v->r, v->g, v->b, v->a };
	
	float max_v = max(rgba[0], max(rgba[1], rgba[2]));

	for (int i = 0; i < 4; i++)
	{
		if (max_v > 255 && i < 3)
		{
			rgba[i] *= (255.0 / max_v);
		}

		rgba[i] = Math_Clamp(rgba[i], 0, 255);
	}

	v->r = rgba[0];
	v->g = rgba[1];
	v->g = rgba[2];
	v->a = rgba[3];
}
static void ClampLightVector2(Vec4* v)
{
	float gamma = 1.0 / LIGHTMAP_GAMMA;
	double rgba[4] = { v->r, v->g, v->b, v->a };

	for (int i = 0; i < 3; i++)
	{
		if (rgba[i] < 0)
		{
			rgba[i] = 0;
			continue;
		}
		rgba[i] = pow(rgba[i] / 255.0, gamma) * 255.0;
	}
	float expv = LIGHTMAP_EXPOSURE;
	if (expv > 0)
	{
		float exposure = 1.0 / (float)expv;
		float max_v = max(rgba[0], max(rgba[1], rgba[2]));
		float dif = (1.0 - exp(-max_v * exposure)) * 255.0;
		if (max_v > 0)
		{
			dif /= max_v;
		}
		else
		{
			dif = 0;
		}

		for (int i = 0; i < 4; i++)
		{
			rgba[i] *= dif;
		}
	}
	else
	{
		float max_v = max(rgba[0], max(rgba[1], rgba[2]));

		if (max_v > 255)
		{
			for (int i = 0; i < 3; i++)
			{
				rgba[i] *= 255.0 / max_v;
			}
		}
	}

	for (int i = 0; i < 4; i++)
	{
		rgba[i] = Math_Clamp(rgba[i], 0, 255);
	}


	v->r = rgba[0];
	v->g = rgba[1];
	v->g = rgba[2];
	v->a = rgba[3];
}

static void BiasPositionToSample(float position[3], float normal[3], float dir[3], float dest[3], float bias, float normal_bias)
{
	if (!normal)
	{
		return;
	}

	if (normal_bias != 0)
	{
		float base_normal_bias_d = (1.0 - max(0.0, Math_XYZ_Dot(dir[0], dir[1], dir[2], -normal[0], -normal[1], -normal[2])));
		float biased_normal[3] = { 0, 0, 0 };
		for (int i = 0; i < 3; i++)
		{
			biased_normal[i] = normal[i] * base_normal_bias_d;
			biased_normal[i] = biased_normal[i] * normal_bias;
			dest[i] = position[i] + (dir[i] * bias);
		}
		float biased_normal_d = Math_XYZ_Dot(dir[0], dir[1], dir[2], biased_normal[0], biased_normal[1], biased_normal[2]);
		for (int i = 0; i < 3; i++)
		{
			biased_normal[i] -= dir[i] * biased_normal_d;
			dest[i] += biased_normal[i];
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			dest[i] = position[i] + (dir[i] * bias);
		}
	}
}

static void LightThread_Loop(LightTraceThread* thread)
{
	LightGlobal* global = thread->globals;
	Lightgrid* lightgrid = &Map_GetMap()->lightgrid;

	int bounces_performed = 0;
	bool shutdown_threads = global->shutdown_threads;

	while(!shutdown_threads)	
	{
		//wait for work
		WaitForSingleObject(thread->start_work_event, INFINITE);

		EnterCriticalSection(&global->start_mutex);
		int bounce = global->bounce;
		shutdown_threads = global->shutdown_threads;
		LeaveCriticalSection(&global->start_mutex);

		ResetEvent(thread->finished_event);
		ResetEvent(thread->start_work_event);

		SetEvent(thread->active_event);

		//do the work
		if (bounce >= 0 || bounce == BOUNCE_AO)
		{
			for (int s = thread->sector_start; s < thread->sector_end; s++)
			{
				Sector* sector = Map_GetSector(s);

				//dont map some sectors, mostly that change light
				if (sector->special == SECTOR_SPECIAL__LIGHT_FLICKER || sector->special == SECTOR_SPECIAL__LIGHT_GLOW)
				{
					continue;
				}

				Lightmap_Sector(global, thread, sector, bounce);
			}

			bounces_performed++;
		}
		//light grid
		else if(bounce == BOUNCE_LIGHTGRID && lightgrid->blocks)
		{
			for (int i = thread->grid_start; i < thread->grid_end; i++)
			{
				Lightblock* block = &lightgrid->blocks[i];
				int z = (i / ((int)lightgrid->bounds[0] * (int)lightgrid->bounds[1]));
				int y = (i / (int)lightgrid->bounds[0]) % (int)lightgrid->bounds[1];
				int x = i % (int)lightgrid->bounds[0];

				float position[3];

				position[0] = lightgrid->origin[0] + (x * lightgrid->size[0]);
				position[1] = lightgrid->origin[1] + (y * lightgrid->size[1]);
				position[2] = lightgrid->origin[2] + (z * lightgrid->size[2]);

				Lightblock_Process(global, thread, block, position);
			}
		}
		
		ResetEvent(thread->active_event);
		SetEvent(thread->finished_event);
	}
}

static void LightGlobal_WaitForAllThreads(LightGlobal* global)
{
	if (global->num_threads <= 0)
	{
		return;
	}
	for (int i = 0; i < global->num_threads; i++)
	{
		LightTraceThread* thr = &global->threads[i];

		WaitForSingleObject(thr->finished_event, INFINITE);
	}
}

static void LightGlobal_SetWorkStateForAllThreads(LightGlobal* global, int bounce)
{
	if (global->num_threads <= 0)
	{
		return;
	}

	//set start work event for all threads
	EnterCriticalSection(&global->start_mutex);
	if (bounce == BOUNCE_EXIT) global->shutdown_threads = true;
	global->bounce = bounce;
	global->ticks++;
	LeaveCriticalSection(&global->start_mutex);

	for (int i = 0; i < global->num_threads; i++)
	{
		LightTraceThread* thr = &global->threads[i];

		SetEvent(thr->start_work_event);
		WaitForSingleObject(thr->active_event, INFINITE);
	}
}

static void LightGlobal_GenerateRandomHemishphereVectors(LightGlobal* global)
{
	for (int i = 0; i < global->num_random_vectors; i++)
	{
		float x = 0;
		float y = 0;
		float z = 0;

		Math_GenHemisphereVector(&x, &y, &z);

		global->random_vectors[(i * 3) + 0] = x;
		global->random_vectors[(i * 3) + 1] = y;
		global->random_vectors[(i * 3) + 2] = z;
	}
}
static bool LightGlobal_SetupLights(LightGlobal* global, Map* map)
{
	int light_iter_index = 0;
	int num_lights = 0;
	//calc total lights
	while (Map_GetNextObjectByType(&light_iter_index, OT__LIGHT))
	{
		num_lights++;
	}

	//add sun light
	if (map->sun_color[0] + map->sun_color[1] + map->sun_color[2] > 0)
	{
		num_lights++;
	}

	if (num_lights == 0)
	{
		return false;
	}

	global->light_list = calloc(num_lights, sizeof(LightDef));

	if (!global->light_list)
	{
		return false;
	}

	num_lights = 0;

	//add sun light
	if (map->sun_color[0] + map->sun_color[1] + map->sun_color[2] > 0)
	{
		LightDef* sun_def = &global->light_list[num_lights++];

		float angle = Math_DegToRad(map->sun_angle);

		sun_def->attenuation = 1;
		sun_def->radius = 0;
		sun_def->color[0] = map->sun_color[0];
		sun_def->color[1] = map->sun_color[1];
		sun_def->color[2] = map->sun_color[2];
		sun_def->direction[0] = cos(angle);
		sun_def->direction[1] = sin(angle);
		sun_def->direction[2] = 1;
		sun_def->position[0] = map->sun_position[0];
		sun_def->position[1] = map->sun_position[1];
		sun_def->type = LDT__SUN;

		global->sun_lightdef = sun_def;
	}

	light_iter_index = 0;
	Object* light_obj = NULL;
	//add object lights
	while (light_obj = Map_GetNextObjectByType(&light_iter_index, OT__LIGHT))
	{
#ifdef ONLY_SUN
		break;
#endif // ONLY_SUN

		LightInfo* light_info = Info_GetLightInfo(light_obj->sub_type);

		if (!light_info)
		{
			continue;
		}

		LightDef* light_def = &global->light_list[num_lights++];

		light_def->radius = light_info->radius;
		light_def->attenuation = light_info->attenuation;
		light_def->deviance = 24;
		light_def->position[0] = light_obj->x;
		light_def->position[1] = light_obj->y;
		light_def->position[2] = light_obj->z + light_obj->height;

		light_def->bbox[0][0] = light_obj->x - light_info->radius;
		light_def->bbox[0][1] = light_obj->y - light_info->radius;
		light_def->bbox[1][0] = light_obj->x + light_info->radius;
		light_def->bbox[1][1] = light_obj->y + light_info->radius;

		light_def->color[0] = rand() & 254;
		light_def->color[1] = rand() & 254;
		light_def->color[2] = rand() & 254;

		light_def->type = LDT__POINT;
	}

	if (num_lights == 0)
	{
		return false;
	}

	global->num_lights = num_lights;

	return true;
}

void LightGlobal_Setup(LightGlobal* global)
{
	Map* map = Map_GetMap();

	memset(global, 0, sizeof(LightGlobal));

	if (!LightGlobal_SetupLights(global, map))
	{
		return;
	}

	//setup sector lines
	global->linedef_list = calloc(map->num_sectors, sizeof(LinedefList));
	if (!global->linedef_list)
	{
		return;
	}
	global->num_linedef_lists = map->num_sectors;

	for (int i = 0; i < map->num_sectors; i++)
	{
		Sector* sec = Map_GetSector(i);

		int num_lines = Trace_SectorLines(sec, true);
		if (num_lines <= 0)
		{
			continue;
		}

		int* hits = Trace_GetHitObjects();

		LinedefList* list = &global->linedef_list[i];
		list->sector_index = sec->index;
		list->lines = calloc(num_lines, sizeof(Linedef*));

		if (!list->lines)
		{
			continue;
		}

		for (int k = 0; k < num_lines; k++)
		{
			int line_index = hits[k];
			Line* line = Map_GetLine(line_index);
			Linedef* ldef = NULL;
			for (int l = 0; l < list->num_lines; l++)
			{
				if (list->lines[l] == line->linedef)
				{
					ldef = list->lines[l];
					break;
				}
			}
			if (!ldef)
			{
				list->lines[list->num_lines++] = line->linedef;
			}
		}
	}

	//setup back lightmaps
	global->floor_back_lightmaps = calloc(map->num_sectors, sizeof(Lightmap));
	global->ceil_back_lightmaps = calloc(map->num_sectors, sizeof(Lightmap));
	global->line_back_lightmaps = calloc(map->num_linedefs, sizeof(Lightmap));

	global->num_back_floor_lightmaps = map->num_sectors;
	global->num_back_ceil_lightmaps = map->num_sectors;
	global->num_back_line_lightmaps = map->num_linedefs;


	global->random_vectors = calloc(RADIOSITY_SAMPLES * 3, sizeof(float));

	if (!global->random_vectors)
	{
		return;
	}
	global->num_random_vectors = RADIOSITY_SAMPLES;

	//setup deviance vectors
	global->deviance_vectors = calloc(DEVIANCE_SAMPLES * 3, sizeof(float));

	if (!global->deviance_vectors)
	{
		return;
	}

	global->num_deviance_vectors = DEVIANCE_SAMPLES;

	for (int i = 0; i < global->num_deviance_vectors; i++)
	{
		float len = 0;
		float x = (Math_randf() * 2.0 - 1.0);
		float y = (Math_randf() * 2.0 - 1.0);
		float z = (Math_randf() * 2.0 - 1.0);

		if (i == 0)
		{
			x = 0;
			y = 0;
			z = 0;
		}

		global->deviance_vectors[(i * 3) + 0] = x;
		global->deviance_vectors[(i * 3) + 1] = y;
		global->deviance_vectors[(i * 3) + 2] = z;
	}
	//setup sun deviance vectors
	if (global->sun_lightdef)
	{
		global->sun_deviance_vectors = calloc(SUN_DEVIANCE_SAMPLES * 3, sizeof(float));
		
		if (!global->sun_deviance_vectors)
		{
			return;
		}

		global->num_sun_deviance_vectors = SUN_DEVIANCE_SAMPLES;
		
		float sun_dir_x = global->sun_lightdef->direction[0];
		float sun_dir_y = global->sun_lightdef->direction[1];
		float sun_dir_z = global->sun_lightdef->direction[2];

		float sun_d = sqrt(sun_dir_x * sun_dir_x + sun_dir_y * sun_dir_y);
		float sun_angle = atan2(sun_dir_y, sun_dir_x);
		float sun_elevation = atan2(sun_dir_z, sun_d);

		for (int i = 0; i < global->num_sun_deviance_vectors; i++)
		{
			float x = 0;
			float y = 0;
			float z = 0;

			if (i == 0)
			{
				x = sun_dir_x;
				y = sun_dir_y;
				z = sun_dir_z;
			}
			else
			{
				float da = 0;
				float de = 0;
				do
				{
					da = (Math_randf() * 2.0 - 1.0) * SUN_DEVIANCE_SCALE;
					de = (Math_randf() * 2.0 - 1.0) * SUN_DEVIANCE_SCALE;
				} while (da * da + de * de > (SUN_DEVIANCE_SCALE * SUN_DEVIANCE_SCALE));
				
				float ang = sun_angle + da;
				float elv = sun_elevation + de;

				x = cos(ang) * cos(elv);
				y = sin(ang) * cos(elv);
				z = sin(elv);
			}

			global->sun_deviance_vectors[(i * 3) + 0] = x;
			global->sun_deviance_vectors[(i * 3) + 1] = y;
			global->sun_deviance_vectors[(i * 3) + 2] = z;
		}
	}
	//setup ao vectors
	global->ao_sample_vectors = calloc(AO_NUM_VECTORS * 3, sizeof(float));

	if (!global->ao_sample_vectors)
	{
		return;
	}
	
	global->num_ao_sample_vectors = AO_NUM_VECTORS;

	float ao_angle_step = Math_DegToRad(360.0 / AO_NUM_ANGLE_STEPS);
	float ao_elevation_step = Math_DegToRad(AO_CONE_ANGLE / AO_NUM_ELEVATION_STEPS);

	float ao_angle = 0;
	int ao_index = 0;
	for (int i = 0; i < AO_NUM_ANGLE_STEPS; i++)
	{
		float ao_elevation = ao_elevation_step * 0.5;

		for (int j = 0; j < AO_NUM_ELEVATION_STEPS; j++)
		{
			if (ao_index >= global->num_ao_sample_vectors)
			{
				break;
			}

			float x = sin(ao_elevation) * cos(ao_angle);
			float y = sin(ao_elevation) * sin(ao_angle);
			float z = cos(ao_elevation);

			global->ao_sample_vectors[(ao_index * 3) + 0] = x;
			global->ao_sample_vectors[(ao_index * 3) + 1] = y;
			global->ao_sample_vectors[(ao_index * 3) + 2] = z;

			ao_index++;
			ao_elevation += ao_elevation_step;
		}

		ao_angle += ao_angle_step;
	}

	//setup stuff for multithreading
	InitializeCriticalSection(&global->start_mutex);

	global->start_work_event = CreateEvent(NULL, TRUE, FALSE, NULL);

	//setup threads
	global->num_threads = NUM_THREADS;

	if (global->num_threads > 0)
	{
		global->threads = calloc(global->num_threads, sizeof(LightTraceThread));

		if (!global->threads)
		{
			return;
		}

		float num_threads = global->num_threads;
		int sector_slice = ceil((float)map->num_sectors / num_threads);

		int sector_start = 0;
		int sector_end = sector_slice;
		
		int total_grid_points = map->lightgrid.block_size[0] * map->lightgrid.block_size[1] * map->lightgrid.block_size[2];

		int grid_slice = ceil((float)total_grid_points / num_threads);

		int grid_start = 0;
		int grid_end = grid_slice;

		DWORD thread_id = 0;
		for (int i = 0; i < global->num_threads; i++)
		{
			LightTraceThread* thread = &global->threads[i];
			thread->sector_start = sector_start;
			thread->sector_end = sector_end;

			sector_start = sector_end;
			sector_end += sector_slice;

			sector_start = Math_Clampl(sector_start, 0, map->num_sectors);
			sector_end = Math_Clampl(sector_end, 0, map->num_sectors);

			thread->grid_start = grid_start;
			thread->grid_end = grid_end;

			grid_start = grid_end;
			grid_end += grid_slice;

			grid_start = Math_Clampl(grid_start, 0, total_grid_points);
			grid_end = Math_Clampl(grid_end, 0, total_grid_points);

			thread->globals = global;

			thread->active_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			thread->finished_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			thread->start_work_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			thread->thread_handle = CreateThread(NULL, 0, LightThread_Loop, thread, 0, &thread_id);
		}

		printf("Setting up %i lightmap threads \n", global->num_threads);
	}
	else
	{
		//just setup thread on global
		global->thread = calloc(1, sizeof(LightTraceThread));

		if (!global->thread)
		{
			return;
		}
	}

}
void LightGlobal_Destruct(LightGlobal* global)
{
	//shut down threads
	global->shutdown_threads = true;
	LightGlobal_SetWorkStateForAllThreads(global, BOUNCE_EXIT);
	LightGlobal_WaitForAllThreads(global);

	for (int i = 0; i < global->num_threads; i++)
	{
		LightTraceThread* thr = &global->threads[i];

		WaitForSingleObject(thr->thread_handle, INFINITE);
		CloseHandle(thr->thread_handle);
		CloseHandle(thr->active_event);
		CloseHandle(thr->finished_event);
		CloseHandle(thr->start_work_event);
	}

	printf("Shut down %i lightmap threads \n", global->num_threads);

	DeleteCriticalSection(&global->start_mutex);
	CloseHandle(global->start_work_event);

	if (global->thread) free(global->thread);
	if (global->threads) free(global->threads);
	if (global->light_list) free(global->light_list);
	if (global->random_vectors) free(global->random_vectors);
	if (global->deviance_vectors) free(global->deviance_vectors);
	if (global->ao_sample_vectors) free(global->ao_sample_vectors);

	if (global->ceil_back_lightmaps)
	{
		for (int i = 0; i < global->num_back_ceil_lightmaps; i++)
		{
			Lightmap* lm = &global->ceil_back_lightmaps[i];

			if (lm->data)
			{
				free(lm->data);
			}
		}
		free(global->ceil_back_lightmaps);
	}
	if (global->floor_back_lightmaps)
	{
		for (int i = 0; i < global->num_back_floor_lightmaps; i++)
		{
			Lightmap* lm = &global->floor_back_lightmaps[i];

			if (lm->data)
			{
				free(lm->data);
			}
		}
		free(global->floor_back_lightmaps);
	}
	if (global->line_back_lightmaps)
	{
		for (int i = 0; i < global->num_back_line_lightmaps; i++)
		{
			Lightmap* lm = &global->line_back_lightmaps[i];

			if (lm->data)
			{
				free(lm->data);
			}
		}
		free(global->line_back_lightmaps);
	}

	if (global->linedef_list)
	{
		for (int i = 0; i < global->num_linedef_lists; i++)
		{
			LinedefList* list = &global->linedef_list[i];

			if (list->lines) free(list->lines);
		}

		free(global->linedef_list);
	}
}

static bool TraceLine(LightGlobal* global, LightTraceThread* thread, LightTraceResult* result, float start_x, float start_y, float start_z, float end_x, float end_y, float end_z, bool ignore_sky_plane, bool need_color_info)
{
	memset(result, 0, sizeof(LightTraceResult));
	result->frac = 1;
	
	int hit = Trace_FindLine(start_x, start_y, start_z, end_x, end_y, end_z, ignore_sky_plane, thread->hits, 10000, &result->hit[0], &result->hit[1], &result->hit[2], &result->frac);

	if (hit == TRACE_NO_HIT)
	{
		return false;
	}
	//return true;
	Line* hit_line = Map_GetLine(hit);
	
	if (!hit_line->linedef)
	{
		return false;
	}

	Sidedef* sidedef = hit_line->sidedef;
	Linedef* linedef = hit_line->linedef;
	Sector* frontsector = Map_GetSector(linedef->front_sector);
	Sector* backsector = NULL;
	if (linedef->back_sector >= 0)
	{
		backsector = Map_GetSector(linedef->back_sector);
	}

	//check for invisible line
	if (backsector)
	{
		if (frontsector->floor == backsector->floor && frontsector->ceil == backsector->ceil)
		{
			return false;
		}
	}
	
	unsigned char* texture_sample = NULL;
	Vec4_u8* light_sample = NULL;
	
	float open_low = frontsector->floor;
	float open_high = frontsector->ceil;
	float open_range = open_high - open_low;

	if (backsector)
	{
		open_low = max(frontsector->floor, backsector->floor);
		open_high = min(frontsector->ceil, backsector->ceil);
		open_range = open_high - open_low;
	}

	//hit ceilling
	if (result->hit[2] > frontsector->ceil)
	{
		result->normal[0] = 0;
		result->normal[1] = 0;
		result->normal[2] = -1;
		result->hit[2] = frontsector->ceil;

		if (frontsector->is_sky)
		{
			if (ignore_sky_plane)
			{
				return false;
			}
			result->hit_type = LST__SKY;
		}
		else
		{
			result->hit_type = LST__CEIL;
		}

		if (need_color_info)
		{
			Lightmap* lightmap = &global->ceil_back_lightmaps[frontsector->index];

			if (lightmap->data && !frontsector->is_sky)
			{
				int lx = ((result->hit[0] - (frontsector->bbox[1][0] * 2)) / LIGHTMAP_LUXEL_SIZE) + lightmap->width;
				int ly = ((result->hit[1] + (frontsector->bbox[1][1] * 2)) / LIGHTMAP_LUXEL_SIZE);

				lx = Math_Clamp(lx, 0, lightmap->width - 1);
				ly = Math_Clamp(ly, 0, lightmap->height - 1);

				light_sample = &lightmap->data[lx + ly * lightmap->width];
			}
			if (frontsector->ceil_texture)
			{
				//hit sky
				if (frontsector->is_sky)
				{
					texture_sample = Image_Get(&frontsector->ceil_texture->img, (int)(result->hit[0] * 2.0), (int)(result->hit[1] * 2.0));
				}
				else
				{
					texture_sample = Image_Get(&frontsector->ceil_texture->img, (int)(result->hit[0] * 2.0) & 63, (int)(result->hit[1] * 2.0) & 63);
				}
			}
		}
	}
	//hit floor
	else if (result->hit[2] < frontsector->floor)
	{
		result->normal[0] = 0;
		result->normal[1] = 0;
		result->normal[2] = 1;
		result->hit[2] = frontsector->floor;
		result->hit_type = LST__FLOOR;

		if (need_color_info)
		{
			Lightmap* lightmap = &global->floor_back_lightmaps[frontsector->index];

			if (lightmap->data)
			{
				int lx = ((result->hit[0] - (frontsector->bbox[1][0] * 2)) / LIGHTMAP_LUXEL_SIZE) + lightmap->width;
				int ly = ((result->hit[1] + (frontsector->bbox[1][1] * 2)) / LIGHTMAP_LUXEL_SIZE);

				lx = Math_Clamp(lx, 0, lightmap->width - 1);
				ly = Math_Clamp(ly, 0, lightmap->height - 1);

				light_sample = &lightmap->data[lx + ly * lightmap->width];
			}
			if (frontsector->floor_texture)
			{
				texture_sample = Image_Get(&frontsector->floor_texture->img, (int)(result->hit[0] * 2.0) & 63, (int)(result->hit[1] * 2.0) & 63);
			}
		}		
	}
	//hit wall
	else
	{
		result->normal[0] = linedef->dy;
		result->normal[1] = -linedef->dx;
		result->normal[2] = 0;
		result->hit_type = LST__WALL;

		if (need_color_info)
		{
			if (sidedef)
			{
				int tx = 0;
				int ty = 0;

				float z = result->hit[2];
				ty = (z - frontsector->floor) / (frontsector->ceil - frontsector->floor);

				float texwidth = sqrt(linedef->dx * linedef->dx + linedef->dy * linedef->dy) * 2;

				float u = 0;
				float t = 0;

				if (fabs(linedef->dx) > fabs(linedef->dy))
				{
					t = (result->hit[0] - linedef->x1) / linedef->dx;
				}
				else
				{
					t = (result->hit[1] - linedef->y1) / linedef->dy;
				}
				u = t + u * t;
				u = fabs(u);
				u *= texwidth;

				tx = u;

				if (backsector)
				{
					if (z < open_low)
					{
						if (sidedef->bottom_texture)
						{
							texture_sample = Image_Get(&sidedef->bottom_texture->img, tx & sidedef->bottom_texture->width_mask, ty & sidedef->bottom_texture->height_mask);
						}
					}
					else if (z > open_high)
					{
						if (sidedef->top_texture)
						{
							texture_sample = Image_Get(&sidedef->top_texture->img, tx & sidedef->top_texture->width_mask, ty & sidedef->top_texture->height_mask);
						}
					}
					else
					{
						if (sidedef->middle_texture)
						{
							texture_sample = Image_Get(&sidedef->middle_texture->img, tx & sidedef->middle_texture->width_mask, ty & sidedef->middle_texture->height_mask);
						}
					}
				}
				else
				{
					if (sidedef->middle_texture)
					{
						texture_sample = Image_Get(&sidedef->middle_texture->img, tx & sidedef->middle_texture->width_mask, ty & sidedef->middle_texture->height_mask);
					}
				}

				Lightmap* lightmap = &global->line_back_lightmaps[linedef->index];

				if (lightmap->data)
				{
					light_sample = Lightmap_Get(lightmap, tx / LIGHTMAP_LUXEL_SIZE, ty / LIGHTMAP_LUXEL_SIZE);
				}
			}
		}
		
	}
	
	if (texture_sample)
	{
		result->color_sample[0] = texture_sample[0];
		result->color_sample[1] = texture_sample[1];
		result->color_sample[2] = texture_sample[2];
	}
	if (light_sample)
	{
		result->light_sample.r = light_sample->r;
		result->light_sample.g = light_sample->g;
		result->light_sample.b = light_sample->b;
		result->light_sample.a = light_sample->a;
	}

	result->sector = frontsector;
	result->linedef = linedef;

	Math_XYZ_Normalize(&result->normal[0], &result->normal[1], &result->normal[2]);
	
	return true;
}

static Vec4 calc_light(float light_dir[3], float light_color[3], float normal[3], float attenuation)
{
	float dot = 1;

	if (normal)
	{
		dot = Math_XYZ_Dot(normal[0], normal[1], normal[2], light_dir[0], light_dir[1], light_dir[2]);
	}

	float diff = max(dot, 0.0);

	Vec4 light = Vec4_Zero();

	light.r = diff * (light_color[0] * attenuation);
	light.g = diff * (light_color[1] * attenuation);
	light.b = diff * (light_color[2] * attenuation);
	light.a = diff * (255.0 * attenuation);

	return light;
}
static Vec4 calc_point_light(float light_position[3], float light_color[3], float light_radius, float light_attenuation, float normal[3], float position[3])
{
	float light_vec[3] = { light_position[0] - position[0], light_position[1] - position[1], light_position[2] - position[2] };
	float distance = Math_XYZ_Length(light_vec[0], light_vec[1], light_vec[2]);

	float inv_radius = 1.0 / light_radius;
	float normalized_dist = distance * inv_radius;
	float atten = pow(max(1.0 - normalized_dist, 0), light_attenuation);

	if (atten > 0)
	{
		Math_XYZ_Normalize(&light_vec[0], &light_vec[1], &light_vec[2]);

		return calc_light(light_vec, light_color, normal, atten);
	}
	
	return Vec4_Zero();
}

static float calc_hit_radiosity(float position[3], float normal[3], LightTraceResult* trace)
{
	if (RADIOSITY_RADIUS <= 0)
	{
		return 0;
	}

	float vec[3] = { trace->hit[0] - position[0], trace->hit[1] - position[1], trace->hit[2] - position[2] };
	Math_XYZ_Normalize(&vec[0], &vec[1], &vec[2]);

	float angle = 1;

	if (normal)
	{
		angle = Math_XYZ_Dot(normal[0], normal[1], normal[2], vec[0], vec[1], vec[2]);
	}

	if (angle <= 0)
	{
		return angle * RADIOSITY_SCALE;
	}

	float distance = Math_XYZ_Length(vec[0], vec[1], vec[2]);

	if (distance <= 0)
	{
		return 0;
	}

	float inv_radius = 1.0 / RADIOSITY_RADIUS;
	float normalized_dist = distance * inv_radius;
	float atten = pow(max(1.0 - normalized_dist, 0), RADIOSITY_ATTENUATION);

	return (angle * atten) * RADIOSITY_SCALE;
}
static void Lightmap_CopyLightmap(Lightmap* dest, Lightmap* source)
{
	if (!source->data || source->width <= 0 || source->height <= 0)
	{
		return;
	}

	if (!dest->data)
	{
		dest->data = calloc(source->width * source->height, sizeof(Vec4));

		if (!dest->data)
		{
			return;
		}

		dest->width = source->width;
		dest->height = source->height;
	}
	else
	{
		assert((source->width == dest->width && source->height == dest->height) && "Back lightmap mismatch");
	}

	memcpy(dest->data, source->data, sizeof(Vec4) * source->width * source->height);
}
static void Lightmap_ConvertFloatingToShort(Lightmap* lm)
{
	if (!lm->data || lm->width <= 0 || lm->height <= 0)
	{
		return;
	}

	Vec3_u16* bytes = calloc(lm->width * lm->height, sizeof(Vec3_u16));

	if (!bytes)
	{
		return;
	}
	
	Vec4* cast_data = lm->float_data;

	//for each luxel
	for (int x = 0; x < lm->width; x++)
	{
		for (int y = 0; y < lm->height; y++)
		{
			Vec4 sample = cast_data[x + y * lm->width];

			Vec4_Clamp(&sample, 0, MAX_LIGHT_VALUE - 255);

			Vec3_u16* dest = &bytes[x + y * lm->width];

			dest->r = (unsigned short)sample.r;
			dest->g = (unsigned short)sample.g;
			dest->b = (unsigned short)sample.b;
		}
	}

	//free the old floating point data
	free(lm->data);

	lm->data = bytes;
}

static void Lightmap_SwapLightmaps(LightGlobal* global, Map* map)
{
	for (int i = 0; i < map->num_sectors; i++)
	{
		Sector* sector = Map_GetSector(i);

		Lightmap* back_floor_lightmap = &global->floor_back_lightmaps[i];
		Lightmap* back_ceil_lightmap = &global->ceil_back_lightmaps[i];

		if (sector->floor_lightmap.data)
		{
			Lightmap_CopyLightmap(back_floor_lightmap, &sector->floor_lightmap);
		}
		if (sector->ceil_lightmap.data)
		{
			Lightmap_CopyLightmap(back_ceil_lightmap, &sector->ceil_lightmap);
		}
	}
	
	for (int i = 0; i < map->num_linedefs; i++)
	{
		Linedef* linedef = Map_GetLineDef(i);
		
		Lightmap* back_lightmap = &global->line_back_lightmaps[i];

		if (linedef->lightmap.data)
		{
			Lightmap_CopyLightmap(back_lightmap, &linedef->lightmap);
		}
	}
}
static void Lightmap_CopyFinalLightmaps(Map* map)
{
	for (int i = 0; i < map->num_sectors; i++)
	{
		Sector* sector = Map_GetSector(i);

		Lightmap* floor_lightmap = &sector->floor_lightmap;
		Lightmap* ceil_lightmap = &sector->ceil_lightmap;

		if (floor_lightmap->data)
		{
			Lightmap_ConvertFloatingToShort(floor_lightmap);
		}
		if (ceil_lightmap->data)
		{
			Lightmap_ConvertFloatingToShort(ceil_lightmap);
		}
	}

	for (int i = 0; i < map->num_linedefs; i++)
	{
		Linedef* linedef = Map_GetLineDef(i);

		if (linedef->lightmap.data)
		{
			Lightmap_ConvertFloatingToShort(&linedef->lightmap);
		}
	}
}
static bool Lightmap_CheckIfFullDark(Lightmap* lm, int x, int y)
{
	if (!lm->data)
	{
		return true;
	}
	x = Math_Clampl(x, 0, lm->width - 1);
	y = Math_Clampl(y, 0, lm->height - 1);

	Vec4* cast_data = lm->float_data;
	Vec4* ptr = &cast_data[x + y * lm->width];

	if (ptr && ptr->r + ptr->g + ptr->b > 0)
	{
		return false;
	}

	return true;
}
static void Lightmap_Set(Lightmap* lm, int x_tiles, int y_tiles, int x, int y, Vec4 light)
{
	if (light.a <= 0)
	{
		return;
	}

	if (!lm->data)
	{
		lm->data = calloc(x_tiles * y_tiles, sizeof(Vec4));

		if (!lm->data)
		{
			return;
		}

		lm->width = x_tiles;
		lm->height = y_tiles;
	}
	else
	{
		assert((x_tiles == lm->width && y_tiles == lm->height));
	}

	x = Math_Clampl(x, 0, x_tiles - 1);
	y = Math_Clampl(y, 0, y_tiles - 1);

	Vec4* cast_data = lm->float_data;

	Vec4* ptr = &cast_data[x + y * x_tiles];
	Vec4 ptr_light = *ptr;

	light.r += (float)ptr_light.r;
	light.g += (float)ptr_light.g;
	light.b += (float)ptr_light.b;
	light.a += (float)ptr_light.a;

	ptr->r = light.r;
	ptr->g = light.g;
	ptr->b = light.b;
	ptr->a = light.a;
}
static void Lightmap_SetAo(Lightmap* lm, int x_tiles, int y_tiles, int x, int y, float ao)
{
	if (!lm->data)
	{
		return;
	}
	else
	{
		assert((x_tiles == lm->width && y_tiles == lm->height));
	}

	x = Math_Clampl(x, 0, x_tiles - 1);
	y = Math_Clampl(y, 0, y_tiles - 1);

	Vec4* cast_data = lm->float_data;

	Vec4* ptr = &cast_data[x + y * x_tiles];
	Vec4 ptr_light = *ptr;

	ptr->r *= ao;
	ptr->g *= ao;
	ptr->b *= ao;
	ptr->a *= ao;
}
static float Lightmap_CalcAo(LightGlobal* global, LightTraceThread* thread, Linedef* target_line, Sector* sector, float position[3], float normal[3])
{
	if (global->num_ao_sample_vectors <= 0)
	{
		return 1;
	}

	float ao_depth = AO_DEPTH;
	float inv_depth = 1.0 / ao_depth;

	float rt[3];
	float up[3];

	float gather_ao = 0;

	memset(rt, 0, sizeof(rt));
	memset(up, 0, sizeof(up));

	if (normal[0] == 0 && normal[1] == 0)
	{
		if (normal[2] == 1)
		{
			rt[0] = 1;
			up[1] = 1;
		}
		else if (normal[2] == -1)
		{
			rt[0] = -1;
			rt[1] = 1;
		}
	}
	else
	{
		float world_up[3] = { 0, 0, 1 };
		Math_XYZ_Cross(normal[0], normal[1], normal[2], world_up[0], world_up[1], world_up[2], &rt[0], &rt[1], &rt[2]);
		Math_XYZ_Normalize(&rt[0], &rt[1], &rt[2]);

		Math_XYZ_Cross(rt[0], rt[1], rt[2], normal[0], normal[1], normal[2], &up[0], &up[1], &up[2]);
		Math_XYZ_Normalize(&up[0], &up[1], &up[2]);
	}

	float start_x = position[0] + normal[0] * 1;
	float start_y = position[1] + normal[1] * 1;
	float start_z = position[2] + normal[2] * 1;

	for (int i = 0; i < global->num_ao_sample_vectors; i++)
	{
		float x = global->ao_sample_vectors[(i * 3) + 0];
		float y = global->ao_sample_vectors[(i * 3) + 1];
		float z = global->ao_sample_vectors[(i * 3) + 2];

		//tangent space
		float dir_x = rt[0] * x + up[0] * y + normal[0] * z;
		float dir_y = rt[1] * x + up[1] * y + normal[1] * z;
		float dir_z = rt[2] * x + up[2] * y + normal[2] * z;

		float end_x = start_x + (dir_x * ao_depth);
		float end_y = start_y + (dir_y * ao_depth);
		float end_z = start_z + (dir_z * ao_depth);

		LightTraceResult trace_result;
		if (!TraceLine(global, thread, &trace_result, start_x, start_y, start_z, end_x, end_y, end_z, true, false))
		{
			//nothing was hit
			continue;
		}
		
		if (trace_result.hit_type == LST__WALL)
		{
			Linedef* linedef = trace_result.linedef;

			//avoid self
			if (target_line)
			{
				if (linedef == target_line)
				{
					continue;
				}
			}
		}
		else if (trace_result.sector)
		{
			if (trace_result.hit_type == LST__CEIL)
			{
				if (trace_result.sector->ceil == position[2])
				{
					continue;
				}
			}
			else if (trace_result.hit_type == LST__FLOOR)
			{
				if (trace_result.sector->floor == position[2])
				{
					continue;
				}
			}
		}

		float delta[3];
		delta[0] = trace_result.hit[0] - start_x;
		delta[1] = trace_result.hit[1] - start_y;
		delta[2] = trace_result.hit[2] - start_z;

		float len = Math_XYZ_Length(delta[0], delta[1], delta[2]);

		gather_ao += 1.0 - inv_depth * len;
	}

	//direct
	{
		float end_x = start_x + (normal[0] * ao_depth);
		float end_y = start_y + (normal[1] * ao_depth);
		float end_z = start_z + (normal[2] * ao_depth);

		LightTraceResult trace_result;
		if (TraceLine(global, thread, &trace_result, start_x, start_y, start_z, end_x, end_y, end_z, true, false))
		{
			//if (trace_result.linedef != target_line)
			{
				float delta[3];
				delta[0] = trace_result.hit[0] - start_x;
				delta[1] = trace_result.hit[1] - start_y;
				delta[2] = trace_result.hit[2] - start_z;

				float len = Math_XYZ_Length(delta[0], delta[1], delta[2]);

				gather_ao += 1.0 - inv_depth * len;
			}
		}
	}

	if (gather_ao <= 0)
	{
		return 1;
	}

	float ao = pow(gather_ao / (float)(global->num_ao_sample_vectors + 1), AO_GAIN);

	if (ao > 1.0)
	{
		ao = 1.0;
	}

	ao *= AO_SCALE;
	if (ao > 1.0)
	{
		ao = 1.0;
	}

	return 1.0 - ao;
}
static Vec4 Lightmap_CalcDirectLight(LightGlobal* global, LightTraceThread* thread, Linedef* target_line, LightDef* light, float position[3], float normal[3], LightSurfType surf_type)
{
	Map* map = Map_GetMap();

	bool is_sun = light->type == LDT__SUN;

	int num_samples = (is_sun) ? global->num_sun_deviance_vectors : global->num_deviance_vectors;
	Vec4 total_light = Vec4_Zero();

	for (int i = 0; i < num_samples; i++)
	{
		float dev_x = 0;
		float dev_y = 0;
		float dev_z = 0;

		if (is_sun)
		{
			dev_x = global->sun_deviance_vectors[(i * 3) + 0];
			dev_y = global->sun_deviance_vectors[(i * 3) + 1];
			dev_z = global->sun_deviance_vectors[(i * 3) + 2];
		}
		else
		{
			dev_x = global->deviance_vectors[(i * 3) + 0] * light->deviance;
			dev_y = global->deviance_vectors[(i * 3) + 1] * light->deviance;
			dev_z = global->deviance_vectors[(i * 3) + 2] * light->deviance;
		}
		
		float sample_light_pos[3] = { light->position[0] + dev_x, light->position[1] + dev_y, light->position[2] + dev_z };

		if (is_sun)
		{
			sample_light_pos[0] = dev_x;
			sample_light_pos[1] = dev_y;
			sample_light_pos[2] = dev_z;
		}
	
		Vec4 light_color = Vec4_Zero();

		//sun light
		if (is_sun)
		{
			light_color = calc_light(sample_light_pos, light->color, normal, light->attenuation);
		}
		else
		{
			light_color = calc_point_light(sample_light_pos, light->color, light->radius, light->attenuation, normal, position);
		}

		//no light contribution at all
		if (light_color.a <= 0)
		{
			continue;
		}
		float light_trace_pos[3] = { sample_light_pos[0], sample_light_pos[1], sample_light_pos[2] };
		float target_pos[3] = { position[0], position[1], position[2] };

		bool skip_sky_plane = false;

		if (is_sun)
		{
			dev_z = 2;
			Math_XYZ_Normalize(&dev_x, &dev_y, &dev_z);

			light_trace_pos[0] = dev_x * (SHRT_MAX * 1);
			light_trace_pos[1] = dev_y * (SHRT_MAX * 1);
			light_trace_pos[2] = dev_z * (SHRT_MAX * 1);
		}

		//bias position to light
		if (normal)
		{
			float light_dir[3] = { light_trace_pos[0] - target_pos[0], light_trace_pos[1] - target_pos[1], light_trace_pos[2] - target_pos[2] };
			Math_XYZ_Normalize(&light_dir[0], &light_dir[1], &light_dir[2]);

			if (is_sun)
			{
				if (surf_type == LST__WALL)
				{
					BiasPositionToSample(target_pos, normal, light_dir, target_pos, LINE_TRACE_BIAS, 64);
				}
				else if (surf_type == LST__FLOOR || surf_type == LST__CEIL)
				{
					BiasPositionToSample(target_pos, normal, light_dir, target_pos, -3, 3);
				}
			}
			else
			{
				if (surf_type == LST__WALL)
				{
					BiasPositionToSample(target_pos, normal, light_dir, target_pos, LINE_TRACE_BIAS, LINE_TRACE_NORMAL_BIAS);
				}
				else if (surf_type == LST__FLOOR || surf_type == LST__CEIL)
				{
					BiasPositionToSample(target_pos, normal, light_dir, target_pos, FLOOR_CEILING_TRACE_BIAS, FLOOR_CEILING_TRACE_NORMAL_BIAS);
				}
			}
		}

		if (is_sun)
		{
			//trace from point to sun light instead
			Math_XYZ_Swap(&target_pos[0], &target_pos[1], &target_pos[2], &light_trace_pos[0], &light_trace_pos[1], &light_trace_pos[2]);
		}

		//trace to target
		LightTraceResult trace_result;
			
		//we hit something
		if (TraceLine(global, thread, &trace_result, light_trace_pos[0], light_trace_pos[1], light_trace_pos[2], target_pos[0], target_pos[1], target_pos[2], skip_sky_plane, false))
		{
			//we don't care if we hit the sky
			if (trace_result.hit_type != LST__SKY)
			{
				if (is_sun)
				{
					//tracing to wall
					if (target_line)
					{
						//we did not the target
						if (trace_result.linedef != target_line)
						{
							continue;
						}
					}
					else
					{
						continue;
					}
					
				}

				//tracing to wall
				if (target_line)
				{
					//we did not the target
					if (trace_result.linedef != target_line)
					{
						continue;
					}
				}
				//tracing to sector ceil or floor
				else
				{
					float tile_box[2][2];
					//Math_SizeToBbox(target_pos[0], target_pos[1], 0.25, tile_box);

					//we hit a wall
					if (trace_result.hit_type == LST__WALL)
					{
						//check if the tile is under or over the line
						//if (!Math_BoxIntersectsBox(trace_result.linedef->bbox, tile_box))
						{
							continue;
						}	
					}
					else if(trace_result.hit_type == LST__CEIL)
					{
						if (!trace_result.sector)
						{
							continue;
						}

						if (trace_result.sector->ceil != position[2])
						{
							continue;
						}

					}
					else if (trace_result.hit_type == LST__FLOOR)
					{
						if (!trace_result.sector)
						{
							continue;
						}

						if (trace_result.sector->floor != position[2])
						{
							continue;
						}
					}

				}
			}
		}

		Vec4_Add(&total_light, light_color);
	}

	Vec4_DivScalar(&total_light, num_samples);

	return total_light;
}

static Vec4 Lightmap_CalcRadiosity(LightGlobal* global, LightTraceThread* thread, float position[3], float normal[3], Linedef* trace_line, LightSurfType surf_type)
{
	Vec4 total_light = Vec4_Zero();

	float start_x = position[0];
	float start_y = position[1];
	float start_z = position[2];

	if (normal)
	{
		start_x = position[0] + normal[0] * 1;
		start_y = position[1] + normal[1] * 1;
		start_z = position[2] + normal[2] * 1;
	}

	for (int i = 0; i < global->num_random_vectors; i++)
	{
		float r_x = global->random_vectors[(i * 3) + 0];
		float r_y = global->random_vectors[(i * 3) + 1];
		float r_z = global->random_vectors[(i * 3) + 2];

		if (normal)
		{
			if (Math_XYZ_Dot(r_x, r_y, r_z, normal[0], normal[1], normal[2]) < 0)
			{
				r_x = -r_x;
				r_y = -r_y;
				r_z = -r_z;
			}
		}
	
		float end_x = start_x + (r_x * RADIOSITY_TRACE_DIST);
		float end_y = start_y + (r_y * RADIOSITY_TRACE_DIST);
		float end_z = start_z + (r_z * RADIOSITY_TRACE_DIST);

		LightTraceResult trace;
		//didn't hit anything
		if (!TraceLine(global, thread, &trace, start_x, start_y, start_z, end_x, end_y, end_z, false, true))
		{
			continue;
		}
		if (trace_line)
		{
			if (trace.linedef == trace_line)
			{
				continue;
			}
		}

		if (trace.hit_type == LST__SKY)
		{
			Map* map = Map_GetMap();

			Vec4 sky_ambient = Vec4_Zero();
			sky_ambient.r = map->sky_color[0] * SKY_SCALE;
			sky_ambient.g = map->sky_color[1] * SKY_SCALE;
			sky_ambient.b = map->sky_color[2] * SKY_SCALE;
			sky_ambient.a = max(sky_ambient.r, max(sky_ambient.g, sky_ambient.b));

			Vec4_Add(&total_light, sky_ambient);
		}
		else
		{
			if (trace.light_sample.a > 0)
			{
				float atten = calc_hit_radiosity(position, normal, &trace);
				
				if (atten > 0)
				{
					float norm_light_a = trace.light_sample.a / 255.0;

					Vec4 trace_light = Vec4_Zero();
					trace_light.r = (trace.color_sample[0] * norm_light_a) * atten;
					trace_light.g = (trace.color_sample[1] * norm_light_a) * atten;
					trace_light.b = (trace.color_sample[2] * norm_light_a) * atten;
					trace_light.a = (trace.light_sample.a);

					Vec4_Add(&total_light, trace_light);
				}
			}
		}

		
	}

	Vec4_DivScalar(&total_light, global->num_random_vectors);

	return total_light;
}

static void Lightmap_SampleFloorAndCeilAtPoint(LightGlobal* global, LightTraceThread* thread, Sector* sector, LightDef* light, float position[3], Vec4* floor_light, Vec4* ceil_light)
{
	const float ceil_normal[3] = { 0, 0, -1 };
	const float floor_normal[3] = { 0, 0, 1 };

	float sample_pos[3] = { position[0], position[1], position[2] };

	//floor
	sample_pos[2] = sector->floor;
	Vec4 floor_result = Vec4_Zero();

	if (light)
	{
		floor_result = Lightmap_CalcDirectLight(global, thread, NULL, light, sample_pos, floor_normal, LST__FLOOR);
	}
	else
	{
		floor_result = Lightmap_CalcRadiosity(global, thread, sample_pos, floor_normal, NULL, LST__FLOOR);
	}

	Vec4_Add(floor_light, floor_result);

	//ceil
	if (!sector->is_sky)
	{
		sample_pos[2] = sector->ceil;
		Vec4 ceil_result = Vec4_Zero();

		if (light)
		{
			ceil_result = Lightmap_CalcDirectLight(global, thread, NULL, light, sample_pos, ceil_normal, LST__CEIL);
		}
		else
		{
			ceil_result = Lightmap_CalcRadiosity(global, thread, sample_pos, ceil_normal, NULL, LST__CEIL);
		}

		Vec4_Add(ceil_light, ceil_result);
	}
}

static void Lightmap_FloorAndCeillingPass(LightGlobal* global, LightTraceThread* thread, Sector* sector, LightDef* light,
	int x_tiles, int y_tiles, float x_step, float y_step, int bounce)
{
	Lightmap* floor_lightmap = &sector->floor_lightmap;
	Lightmap* ceil_lightmap = &sector->ceil_lightmap;

	int num_samples = AA_SAMPLES;
	int half_samples = num_samples / 2;

	float ceil_normal[3] = { 0, 0, -1 };
	float floor_normal[3] = { 0, 0, 1 };

	float position[3] = { sector->bbox[0][0], sector->bbox[0][1], sector->floor };

	y_step /= 2.0;
	x_step /= 2.0;

	float sample_x_step = 0;
	float sample_y_step = 0;

	if (num_samples > 1)
	{
		sample_x_step = x_step / ((float)num_samples / 2.0);
		sample_y_step = y_step / ((float)num_samples / 2.0);
	}

	float sample_x_offset = (float)half_samples * sample_x_step;
	float sample_y_offset = (float)half_samples * sample_y_step;

	for (int x = 0; x < x_tiles; x++)
	{
		position[1] = sector->bbox[1][1];

		for (int y = 0; y < y_tiles; y++)
		{
			float sample_pos[3] = { position[0], position[1], position[2]};
			Vec4 sample_floor_light = Vec4_Zero();
			Vec4 sample_ceil_light = Vec4_Zero();

			//direct light pass
			if (bounce == 0)
			{
				//sample at center
				Lightmap_SampleFloorAndCeilAtPoint(global, thread, sector, light, sample_pos, &sample_floor_light, &sample_ceil_light);

				//aa
				if (num_samples == 4)
				{
					//left
					sample_pos[0] = position[0] - sample_x_step;
					Lightmap_SampleFloorAndCeilAtPoint(global, thread, sector, light, sample_pos, &sample_floor_light, &sample_ceil_light);

					//right
					sample_pos[0] = position[0] + sample_x_step;
					Lightmap_SampleFloorAndCeilAtPoint(global, thread, sector, light, sample_pos, &sample_floor_light, &sample_ceil_light);

					sample_pos[0] = position[0];

					//top
					sample_pos[1] = position[1] + sample_y_step;
					Lightmap_SampleFloorAndCeilAtPoint(global, thread, sector, light, sample_pos, &sample_floor_light, &sample_ceil_light);

					//bottom
					sample_pos[1] = position[1] - sample_y_step;
					Lightmap_SampleFloorAndCeilAtPoint(global, thread, sector, light, sample_pos, &sample_floor_light, &sample_ceil_light);

					Vec4_DivScalar(&sample_floor_light, num_samples + 1);
					if (!sector->is_sky) Vec4_DivScalar(&sample_ceil_light, num_samples + 1);
				}
	
				Lightmap_Set(floor_lightmap, x_tiles, y_tiles, x, y, sample_floor_light);
				Lightmap_Set(ceil_lightmap, x_tiles, y_tiles, x, y, sample_ceil_light);
			}
			//gi pass
			else if(bounce > 0)
			{
				//sample at center
				Lightmap_SampleFloorAndCeilAtPoint(global, thread, sector, NULL, sample_pos, &sample_floor_light, &sample_ceil_light);

				Lightmap_Set(floor_lightmap, x_tiles, y_tiles, x, y, sample_floor_light);
				if (!sector->is_sky)
				{
					Lightmap_Set(ceil_lightmap, x_tiles, y_tiles, x, y, sample_ceil_light);
				}
			}
			//ao pass
			else if (bounce == BOUNCE_AO)
			{
				//floor
				if (!Lightmap_CheckIfFullDark(floor_lightmap, x, y))
				{
					sample_pos[2] = sector->floor;
					float ao = Lightmap_CalcAo(global, thread, NULL, sector, sample_pos, floor_normal);
					Lightmap_SetAo(floor_lightmap, x_tiles, y_tiles, x, y, ao);
				}
				//ceil
				if (!Lightmap_CheckIfFullDark(ceil_lightmap, x, y))
				{
					sample_pos[2] = sector->ceil;
					float ao = Lightmap_CalcAo(global, thread, NULL, sector, sample_pos, ceil_normal);
					Lightmap_SetAo(ceil_lightmap, x_tiles, y_tiles, x, y, ao);
				}
			}
			
			position[1] += -y_step;
		}
		position[0] += x_step;
	}
}

static void Lightmap_SampleLinePoint(LightGlobal* global, LightTraceThread* thread, Linedef* line, LightDef* light, float position[3], float normal[3], Vec4* line_light)
{
	Vec4 light_result = Vec4_Zero();

	if (light)
	{
		light_result = Lightmap_CalcDirectLight(global, thread, line, light, position, normal, LST__WALL);
	}
	else
	{
		light_result = Lightmap_CalcRadiosity(global, thread, position, normal, NULL, LST__WALL);
	}

	Vec4_Add(line_light, light_result);
}

static void Lightmap_LinePass(LightGlobal* global, LightTraceThread* thread, Linedef* line, LightDef* light, int bounce)
{
	int num_samples = AA_SAMPLES;
	int half_samples = num_samples / 2;

	if (bounce == 0 && light->type == LDT__POINT)
	{
		if (!Math_BoxIntersectsBox(line->bbox, light->bbox))
		{
			return;
		}
	}

	Sector* frontsector = Map_GetSector(line->front_sector);
	Sector* backsector = NULL;

	if (line->back_sector >= 0)
	{
		backsector = Map_GetSector(line->back_sector);
	}

	if (backsector)
	{
		if (frontsector->floor == backsector->floor && frontsector->ceil == backsector->ceil)
		{
			return;
		}
	}

	float min_floor = frontsector->floor;
	float max_ceil = frontsector->ceil;

	if (backsector)
	{
		min_floor = min(frontsector->floor, backsector->floor);
		max_ceil = max(frontsector->ceil, backsector->ceil);
	}

	float dx = line->x0 - line->x1;
	float dy = line->y0 - line->y1;
	float dz = frontsector->base_ceil - frontsector->base_floor;

	float normal[3] = { dy, -dx, 0 };

	Math_XYZ_Normalize(&normal[0], &normal[1], &normal[2]);

	float width_scale = sqrtf(dx * dx + dy * dy);
	int x_tiles = ceil((width_scale * 2.0) / LIGHTMAP_LUXEL_SIZE);
	int y_tiles = ceil(((frontsector->base_ceil - frontsector->base_floor) / 2.0) / LIGHTMAP_LUXEL_SIZE);

	if (bounce == 0 && line->lightmap.data && (line->lightmap.width != x_tiles || line->lightmap.height != y_tiles))
	{
		return;
	}

	if (x_tiles == 0 && y_tiles == 0)
	{
		return;
	}
	else if (x_tiles <= 0)
	{
		x_tiles = 1;
	}
	else if (y_tiles <= 0)
	{
		y_tiles = 1;
	}

	float x_step = (dx / x_tiles);
	float y_step = (dy / x_tiles);
	float z_step = -(dz / y_tiles);

	float open_low = frontsector->floor;
	float open_high = frontsector->ceil;
	float open_range = open_high - open_low;

	if (backsector)
	{
		open_low = max(frontsector->floor, backsector->floor);
		open_high = min(frontsector->ceil, backsector->ceil);
		open_range = open_high - open_low;
	}

	float position[3] = { line->x1, line->y1, open_high };

	float sample_x_step = 0;
	float sample_y_step = 0;
	float sample_z_step = 0;

	if (num_samples > 1)
	{
		sample_x_step = x_step / half_samples;
		sample_y_step = y_step / half_samples;
		sample_z_step = z_step / half_samples;
	}

	for (int x = 0; x < x_tiles; x++)
	{
		position[2] = open_high;

		for (int y = 0; y < y_tiles; y++)
		{
			if (backsector)
			{
				//skip empty spaces
				if (position[2] > open_low && open_high < position[2])
				{
					position[2] += z_step;
					continue;
				}
			}

			//direct light pass
			if (bounce == 0)
			{
				float sample_pos[3] = { position[0], position[1], position[2] };

				Vec4 sample_light = Vec4_Zero();

				//sample at center
				Lightmap_SampleLinePoint(global, thread, line, light, position, normal, &sample_light);

				if (num_samples == 4)
				{
					//left
					sample_pos[0] = position[0] - sample_x_step;
					sample_pos[1] = position[1] - sample_y_step;
					Lightmap_SampleLinePoint(global, thread, line, light, sample_pos, normal, &sample_light);
					//right
					sample_pos[0] = position[0] + sample_x_step;
					sample_pos[1] = position[1] + sample_y_step;
					Lightmap_SampleLinePoint(global, thread, line, light, sample_pos, normal, &sample_light);

					sample_pos[0] = position[0];
					sample_pos[1] = position[1];

					//top
					sample_pos[2] = position[2] + sample_z_step;
					Lightmap_SampleLinePoint(global, thread, line, light, sample_pos, normal, &sample_light);
					//bottom
					sample_pos[2] = position[2] - sample_z_step;

					Vec4_DivScalar(&sample_light, num_samples + 1);

				}

				Lightmap_Set(&line->lightmap, x_tiles, y_tiles, x, y, sample_light);
			}
			//bounce pass
			else if(bounce > 0)
			{
				Vec4 light = Vec4_Zero();
				Lightmap_SampleLinePoint(global, thread, line, NULL, position, normal, &light);
				Lightmap_Set(&line->lightmap, x_tiles, y_tiles, x, y, light);
			}
			//ao pass
			else if (bounce == BOUNCE_AO)
			{
				if (!Lightmap_CheckIfFullDark(&line->lightmap, x, y))
				{
					float ao = Lightmap_CalcAo(global, thread, line, frontsector, position, normal);
					Lightmap_SetAo(&line->lightmap, x_tiles, y_tiles, x, y, ao);
				}
			}

			position[2] += z_step;
		}
		
		position[0] += x_step;
		position[1] += y_step;
	}
}


static void Lightmap_LightPass(LightGlobal* global, LightTraceThread* thread, Sector* sector, int bounce, float sector_dx, float sector_dy,
	int sector_x_tiles, int sector_y_tiles, float sector_x_step, float sector_y_step, LightDef* light)
{
	//only lightmap floor and ceil if it's open
	if (sector->base_ceil - sector->base_floor > 0)
	{
		Lightmap_FloorAndCeillingPass(global, thread, sector, light, sector_x_tiles, sector_y_tiles, sector_x_step, sector_y_step, bounce);
	}

	//for each linedef
	LinedefList* list = &global->linedef_list[sector->index];
	for (int k = 0; k < list->num_lines; k++)
	{
		Linedef* line = list->lines[k];

		if (line->front_sector != sector->index)
		{
			continue;
		}
		Lightmap_LinePass(global, thread, line, light, bounce);
	}
}

void Lightmap_Sector(LightGlobal* global, LightTraceThread* thread, Sector* sector, int bounce)
{
	float sector_dx = (sector->bbox[1][0] - sector->bbox[0][0]) * 2.0;
	float sector_dy = (sector->bbox[1][1] - sector->bbox[0][1]) * 2.0;

	int sector_x_tiles = ceil(sector_dx / LIGHTMAP_LUXEL_SIZE);
	int sector_y_tiles = ceil(sector_dy / LIGHTMAP_LUXEL_SIZE);

	float sector_x_step = sector_dx / (sector_x_tiles);
	float sector_y_step = sector_dy / (sector_y_tiles);

	//direct light pass
	if (bounce == 0)
	{
		//for each light
		for (int i = 0; i < global->num_lights; i++)
		{
			LightDef* light = &global->light_list[i];

			if (light->type == LDT__POINT)
			{
				if (!Math_BoxIntersectsBox(light->bbox, sector->bbox))
				{
					continue;
				}
			}
	
			Lightmap_LightPass(global, thread, sector, bounce, sector_dx, sector_dy, sector_x_tiles, sector_y_tiles, sector_x_step, sector_y_step, light);
		}
	}
	//gi or ao pass
	else
	{
		Lightmap_LightPass(global, thread, sector, bounce, sector_dx, sector_dy, sector_x_tiles, sector_y_tiles, sector_x_step, sector_y_step, NULL);
	}
}

static void Lightmap_DispatchLightmapWork(LightGlobal* global, Map* map, int bounce)
{
	if (global->num_threads > 0)
	{
		//make threads do the work
		LightGlobal_SetWorkStateForAllThreads(global, bounce);

		//wait for them to finish
		LightGlobal_WaitForAllThreads(global);
	}
	//do it our selves
	else
	{
		for (int s = 0; s < map->num_sectors; s++)
		{
			Sector* sector = Map_GetSector(s);

			//dont map some sectors, mostly that change light
			if (sector->special == SECTOR_SPECIAL__LIGHT_FLICKER || sector->special == SECTOR_SPECIAL__LIGHT_GLOW)
			{
				continue;
			}

			Lightmap_Sector(global, global->thread, sector, bounce);
		}
	}
}

void Lightmap_Create(LightGlobal* global, Map* map)
{
	if (global->num_lights <= 0)
	{
		return;
	}

	int bounces = NUM_BOUNCES;
	if (bounces <= 0)
	{
		return;
	}
	
	printf("Creating Lightmaps with %i bounces, %i threads\n", bounces, global->num_threads);

	double start = glfwGetTime();

	for (int b = 0; b < bounces; b++)
	{
		if (b > 0)
		{
			LightGlobal_GenerateRandomHemishphereVectors(global);
		}
		
		Lightmap_DispatchLightmapWork(global, map, b);
		
		//then swap lightmaps to back buffer
		Lightmap_SwapLightmaps(global, map);
	}

	//AO
#ifndef DISABLE_AO
	Lightmap_DispatchLightmapWork(global, map, BOUNCE_AO);
#endif // !DISABLE_AO

	//swap all lightmaps from floating to bytes
	Lightmap_CopyFinalLightmaps(map);

	double end = glfwGetTime();

	printf("Finished creating lightmaps. Time: %f \n", end - start);
}

void Lightblock_Process(LightGlobal* global, LightTraceThread* thread, Lightblock* block, float position[3])
{
	Vec4 total_light = Vec4_Zero();
	//calc direct light
	//for each light
	for (int i = 0; i < global->num_lights; i++)
	{
		LightDef* light = &global->light_list[i];

		if (light->type == LDT__POINT)
		{
			if (!Math_BoxContainsPoint(light->bbox, position))
			{
				continue;
			}
		}

		Vec4 result_light = Lightmap_CalcDirectLight(global, thread, NULL, light, position, NULL, LST__POINT);

		Vec4_Add(&total_light, result_light);
	}

	Vec4 radiosity = Lightmap_CalcRadiosity(global, thread, position, NULL, NULL, LST__POINT);
	Vec4_ScaleScalar(&radiosity, 4);
	//Vec4_Add(&total_light, radiosity);

	//ClampLightVector3(&total_light);

	block->light.r = Math_Clampl(total_light.r, 0, 255);
	block->light.g = Math_Clampl(total_light.g, 0, 255);
	block->light.b = Math_Clampl(total_light.b, 0, 255);
	block->light.a = Math_Clampl(total_light.a, 0, 255);
}

void Lightblocks_Create(LightGlobal* global, Map* map)
{
	if (global->num_lights <= 0)
	{
		return;
	}

	global->num_deviance_vectors = 1;

	Lightgrid* lightgrid = &map->lightgrid;

	printf("Creating Lightgrid, %i threads\n", global->num_threads);

	double start = glfwGetTime();

	//let the threads do it
	if (global->num_threads > 0)
	{
		//make threads do the work
		LightGlobal_SetWorkStateForAllThreads(global, BOUNCE_LIGHTGRID);

		//wait for them to finish
		LightGlobal_WaitForAllThreads(global);
	}
	else
	{
		//do it our selves
		for (int x = 0; x < lightgrid->block_size[0]; x++)
		{
			for (int y = 0; y < lightgrid->block_size[1]; y++)
			{
				for (int z = 0; z < lightgrid->block_size[2]; z++)
				{
					int flat_index = ((int)lightgrid->bounds[0] * (int)lightgrid->bounds[1] * z) + ((int)lightgrid->bounds[0] * y) + x;
					Lightblock* block = &lightgrid->blocks[flat_index];
					float position[3];

					position[0] = lightgrid->origin[0] + (x * lightgrid->size[0]);
					position[1] = lightgrid->origin[1] + (y * lightgrid->size[1]);
					position[2] = lightgrid->origin[2] + (z * lightgrid->size[2]);

					Lightblock_Process(global, global->thread, block, position);
				}
			}
		}
	}

	double end = glfwGetTime();

	printf("Finished creating lightgrid. Time: %f \n", end - start);

	Map_UpdateObjectsLight();
}
