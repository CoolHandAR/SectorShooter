#include "g_common.h"
#include "light.h"

#include "game_info.h"
#include "u_math.h"
#include <Windows.h>

#define MAX_THREAD_HITS 10000
#define NUM_THREADS 8
#define AA_SAMPLES 4
#define NUM_BOUNCES 4
#define RADIOSITY_SAMPLES 64
#define RADIOSITY_RADIUS 1024
#define RADIOSITY_ATTENUATION 1
#define DEVIANCE_SAMPLES 8
#define DEVIANCE_SCALE 0.02
#define FLOOR_CEILING_TRACE_BIAS 0.02
#define LINE_TRACE_BIAS -0.1

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

static void ClampLightVector2(Vec4* v)
{
	double rgba[4] = { v->r, v->g, v->b, v->a };

	for (int i = 0; i < 4; i++)
	{
		rgba[i] = rgba[i] / 255.0;

		rgba[i] = 1.0 - exp(-rgba[i] * 1);
		rgba[i] = pow(rgba[i], 1.0 / 1.2);

		rgba[i] = rgba[i] * 255.0;

		rgba[i] = Math_Clamp(rgba[i], 0, 255);
	}
	

	v->r = rgba[0];
	v->g = rgba[1];
	v->g = rgba[2];
	v->a = rgba[3];
}

static void LightThread_Loop(LightTraceThread* thread)
{
	LightGlobal* global = thread->globals;

	int bounces_performed = 0;
	bool shutdown_threads = global->shutdown_threads;

	while(!shutdown_threads)	
	{
		//wait for work
		WaitForSingleObject(thread->start_work_event, INFINITE);

		EnterCriticalSection(&global->start_mutex);
		int bounce = global->bounce;
		float bounce_scale = global->bounce_scale;
		shutdown_threads = global->shutdown_threads;
		LeaveCriticalSection(&global->start_mutex);

		ResetEvent(thread->finished_event);
		ResetEvent(thread->start_work_event);

		SetEvent(thread->active_event);

		//do the work
		//
		if (bounce >= 0)
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
		else if(bounce == -1)
		{

		}
		
		ResetEvent(thread->active_event);
		SetEvent(thread->finished_event);
	}

	float s = 0;
}

static void LightGlobal_WaitForAllThreads(LightGlobal* global)
{
	for (int i = 0; i < global->num_threads; i++)
	{
		LightTraceThread* thr = &global->threads[i];

		WaitForSingleObject(thr->finished_event, INFINITE);
	}
}

static void LightGlobal_SetWorkStateForAllThreads(LightGlobal* global, int bounce)
{
	//set start work event for all threads
	EnterCriticalSection(&global->start_mutex);
	if (bounce == -2) global->shutdown_threads = true;
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

void LightGlobal_Setup(LightGlobal* global)
{
	Map* map = Map_GetMap();

	memset(global, 0, sizeof(LightGlobal));

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
		return;
	}

	global->light_list = calloc(num_lights, sizeof(LightDef));

	if (!global->light_list)
	{
		return;
	}

	num_lights = 0;

	//add sun light
	if (map->sun_color[0] + map->sun_color[1] + map->sun_color[2] > 0)
	{
		LightDef* sun_def = &global->light_list[num_lights++];

		sun_def->attenuation = 1;
		sun_def->radius = 0;
		sun_def->color[0] = map->sun_color[0];
		sun_def->color[1] = map->sun_color[1];
		sun_def->color[2] = map->sun_color[2];
		sun_def->direction[0] = cos(map->sun_angle);
		sun_def->direction[1] = sin(map->sun_angle);
		sun_def->direction[2] = 1;
		sun_def->type = LDT__SUN;

		global->sun_lightdef = sun_def;
	}

	light_iter_index = 0;
	Object* light_obj = NULL;
	//add object lights
	while (light_obj = Map_GetNextObjectByType(&light_iter_index, OT__LIGHT))
	{
		//break;
		LightInfo* light_info = Info_GetLightInfo(light_obj->sub_type);

		if (!light_info)
		{
			continue;
		}

		LightDef* light_def = &global->light_list[num_lights++];

		light_def->radius = light_info->radius;
		light_def->attenuation = light_info->attenuation;
		light_def->position[0] = light_obj->x;
		light_def->position[1] = light_obj->y;
		light_def->position[2] = light_obj->z + light_obj->height;

		light_def->bbox[0][0] = light_obj->x - light_info->radius;
		light_def->bbox[0][1] = light_obj->y - light_info->radius;
		light_def->bbox[1][0] = light_obj->x + light_info->radius;
		light_def->bbox[1][1] = light_obj->y + light_info->radius;

		light_def->color[0] = 255;
		light_def->color[1] = 255;
		light_def->color[2] = 255;

		light_def->type = LDT__POINT;
	}

	global->num_lights = num_lights;

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

	//setup hemisephere random vecs
	for (int i = 0; i < global->num_random_vectors; i++)
	{
		float u = (float)rand() / (float)RAND_MAX;
		float v = (float)rand() / (float)RAND_MAX;

		float theta = 2.0 * Math_PI * u;
		float phi = acos(2.0 * v - 1.0);
		global->random_vectors[(i * 3) + 0] = cos(theta) * sin(phi);
		global->random_vectors[(i * 3) + 1] = cos(phi);
		global->random_vectors[(i * 3) + 2] = sin(theta) * sin(phi);
	}

	//setup deviance vectors
	global->deviance_vectors = calloc(DEVIANCE_SAMPLES * 4, sizeof(float));

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

		len = Math_XYZ_Length(x, y, z);

		if (i == 0)
		{
			x = 0;
			y = 0;
			z = 0;
		}

		global->deviance_vectors[(i * 4) + 0] = x;
		global->deviance_vectors[(i * 4) + 1] = y;
		global->deviance_vectors[(i * 4) + 2] = z;
		global->deviance_vectors[(i * 4) + 3] = len;
	}

	//setup stuff for multithreading
	InitializeConditionVariable(&global->start_work_cv);
	InitializeCriticalSection(&global->start_mutex);

	InitializeConditionVariable(&global->end_work_cv);
	InitializeCriticalSection(&global->end_mutex);

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
		int slice = ceil((float)map->num_sectors / num_threads);

		int sector_start = 0;
		int sector_end = slice;

		DWORD thread_id = 0;
		for (int i = 0; i < global->num_threads; i++)
		{
			LightTraceThread* thread = &global->threads[i];
			thread->sector_start = sector_start;
			thread->sector_end = sector_end;

			sector_start = sector_end;
			sector_end += slice;

			sector_start = Math_Clampl(sector_start, 0, map->num_sectors);
			sector_end = Math_Clampl(sector_end, 0, map->num_sectors);

			thread->globals = global;

			thread->active_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			thread->finished_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			thread->start_work_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			thread->thread_handle = CreateThread(NULL, 0, LightThread_Loop, thread, 0, &thread_id);
		}

		printf("Setting up %i lightmap threads \n", global->num_threads);
	}

}
void LightGlobal_Destruct(LightGlobal* global)
{
	//shut down threads
	global->shutdown_threads = true;
	LightGlobal_SetWorkStateForAllThreads(global, -2);
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
	DeleteCriticalSection(&global->end_mutex);
	CloseHandle(global->start_work_event);


	if (global->threads) free(global->threads);
	if (global->light_list) free(global->light_list);
	if (global->random_vectors) free(global->random_vectors);
	if (global->deviance_vectors) free(global->deviance_vectors);

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

static bool TraceLine(LightGlobal* global, LightTraceThread* thread, LightTraceResult* result, float start_x, float start_y, float start_z, float end_x, float end_y, float end_z, bool ignore_sky_plane)
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
		if (ignore_sky_plane)
		{
			return false;
		}

		result->normal[0] = 0;
		result->normal[1] = 0;
		result->normal[2] = -1;
		result->hit[2] = frontsector->ceil;

		if (frontsector->is_sky)
		{
			result->hit_sky = true;
		}

		Lightmap* lightmap = &global->ceil_back_lightmaps[frontsector->index];

		if (lightmap->data && !frontsector->is_sky)
		{
			int lx = ((result->hit[0] - (frontsector->bbox[1][0] * 2)) / LIGHTMAP_LUXEL_SIZE) + lightmap->width;
			int ly = ((result->hit[1] + (frontsector->bbox[1][1] * 2)) / LIGHTMAP_LUXEL_SIZE) + 1;

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
	//hit floor
	else if (result->hit[2] < frontsector->floor)
	{
		result->normal[0] = 0;
		result->normal[1] = 0;
		result->normal[2] = 1;
		result->hit[2] = frontsector->floor;

		Lightmap* lightmap = &global->floor_back_lightmaps[frontsector->index];

		if (lightmap->data)
		{
			int lx = ((result->hit[0] - (frontsector->bbox[1][0] * 2)) / LIGHTMAP_LUXEL_SIZE) + lightmap->width;
			int ly = ((result->hit[1] + (frontsector->bbox[1][1] * 2)) / LIGHTMAP_LUXEL_SIZE) + 1;

			lx = Math_Clamp(lx, 0, lightmap->width - 1);
			ly = Math_Clamp(ly, 0, lightmap->height - 1);

			light_sample = &lightmap->data[lx + ly * lightmap->width];
		}
		if (frontsector->floor_texture)
		{
			texture_sample = Image_Get(&frontsector->floor_texture->img, (int)(result->hit[0] * 2.0) & 63, (int)(result->hit[1] * 2.0) & 63);
		}
		
	}
	//hit wall
	else
	{
		result->normal[0] = linedef->dy;
		result->normal[1] = -linedef->dx;
		result->normal[2] = 0;


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

static float calc_hit_radiosity(float position[3], float normal[3], float dir_x, float dir_y, float dir_z, LightTraceResult* trace)
{
	if (RADIOSITY_RADIUS <= 0)
	{
		return 0;
	}

	float vec[3] = { trace->hit[0] - position[0], trace->hit[1] - position[1], trace->hit[2] - position[2] };
	float distance = Math_XYZ_Length(vec[0], vec[1], vec[2]);

	float angle = Math_XYZ_Dot(trace->normal[0], trace->normal[1], trace->normal[2], dir_x, dir_y, dir_z);

	if (angle <= 0)
	{
		return 0;
	}

	if (distance <= 0)
	{
		return angle;
	}

	float inv_radius = 1.0 / RADIOSITY_RADIUS;
	float normalized_dist = distance * inv_radius;
	float atten = pow(max(1.0 - normalized_dist, 0), RADIOSITY_ATTENUATION);
	
	return angle * atten;
}
static void Lightmap_Trim(Lightmap* lm)
{
	return;
	if (!lm->data || lm->width == 0 || lm->height == 0)
	{
		return;
	}

	int min_x = lm->width;
	int min_y = lm->height;

	int max_x = 0;
	int max_y = 0;
	
	for (int x = 0; x < lm->width; x++)
	{
		for (int y = 0; y < lm->height; y++)
		{
			//unsigned char texel = lm->data[x + y * lm->width];

			//if (texel > 0)
			{
				min_x = min(min_x, x);
				min_y = min(min_y, y);

				max_x = max(max_x, x);
				max_y = max(max_y, y);
			}
		}
	}

	int trim_width = (max_x - min_x) + 1;
	int trim_height = (max_y - min_y) + 1;

	if (trim_width == lm->width && trim_height == lm->height)
	{
		return;
	}
	if (trim_width <= 0 || trim_height <= 0)
	{
		return;
	}

	unsigned char* new_data = calloc(trim_width * trim_height, sizeof(unsigned char));

	if (!new_data)
	{
		return;
	}

	int x_offset = lm->width - trim_width;
	int y_offset = lm->height - trim_height;

	for (int x = 0; x < trim_width; x++)
	{
		for (int y = 0; y < trim_height; y++)
		{
			//new_data[x + y * trim_width] = lm->data[(x + min_x) + (y + min_y) * lm->width];
		}
	}

	//free old data
	free(lm->data);

	lm->data = new_data;

	lm->width = trim_width;
	lm->height = trim_height;

	float s = 0;
}

static void Lightmap_CopyLightmap(Lightmap* dest, Lightmap* source)
{
	if (!source->data || source->width <= 0 || source->height <= 0)
	{
		return;
	}

	if (!dest->data)
	{
		dest->data = calloc(source->width * source->height, sizeof(Vec4_u8));

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

	memcpy(dest->data, source->data, sizeof(Vec4_u8) * source->width * source->height);
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
static void Lightmap_Set(Lightmap* lm, int x_tiles, int y_tiles, int x, int y, Vec4 light)
{
	if (light.a <= 0)
	{
		return;
	}

	if (!lm->data)
	{
		lm->data = calloc(x_tiles * y_tiles, sizeof(Vec4_u8));

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

	Vec4_Clamp(&light, 0, 255);

	Vec4_u8* ptr = &lm->data[x + y * x_tiles];
	Vec4_u8 ptr_light = *ptr;

	light.r += (float)ptr_light.r;
	light.g += (float)ptr_light.g;
	light.b += (float)ptr_light.b;
	light.a += (float)ptr_light.a;

	Vec4_Clamp(&light, 0, 255);
	//ClampLightVector2(&light);

	ptr->r = (unsigned char)light.r;
	ptr->g = (unsigned char)light.g;
	ptr->b = (unsigned char)light.b;
	ptr->a = (unsigned char)light.a;
}
static float Lightmap_CalcDirt(LightGlobal* global, Linedef* target_line, float position[3], float normal[3])
{
	return 1;

	float dirt_gain = 1;
	float dirt_scale = 1;
	float dirt_depth = 64.0;
	float inv_depth = 1.0 / dirt_depth;

	float rt[3];
	float up[3];

	float gather_dirt = 0;

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

	for (int i = 0; i < 48; i++)
	{
		float angle = Math_randf() * Math_DegToRad(360.0);
		float elevation = Math_randf() * Math_DegToRad(88.0);

		float x = cos(angle) * sin(elevation);
		float y = sin(angle) * sin(elevation);
		float z = cos(elevation);

		//tangent space
		float dir_x = rt[0] * x + up[0] * y + normal[0] * z;
		float dir_y = rt[1] * x + up[1] * y + normal[1] * z;
		float dir_z = rt[2] * x + up[2] * y + normal[2] * z;

		float end_x = start_x + (dir_x * dirt_depth);
		float end_y = start_y + (dir_y * dirt_depth);
		float end_z = start_z + (dir_z * dirt_depth);

		LightTraceResult trace_result;
		if (!TraceLine(global, NULL, &trace_result, start_x, start_y, start_z, end_x, end_y, end_z, false))
		{
			//nothing was hit
			continue;
		}

		if (trace_result.linedef == target_line)
		{
			//continue;
		}

		float delta[3];
		delta[0] = trace_result.hit[0] - start_x;
		delta[1] = trace_result.hit[1] - start_y;
		delta[2] = trace_result.hit[2] - start_z;

		float len = Math_XYZ_Length(delta[0], delta[1], delta[2]);

		gather_dirt += 1.0 - inv_depth * len;
	}

	//direct
	{
		float end_x = start_x + (normal[0] * dirt_depth);
		float end_y = start_y + (normal[1] * dirt_depth);
		float end_z = start_z + (normal[2] * dirt_depth);

		LightTraceResult trace_result;
		if (TraceLine(global, NULL, &trace_result, start_x, start_y, start_z, end_x, end_y, end_z, false))
		{
			//if (trace_result.linedef != target_line)
			{
				float delta[3];
				delta[0] = trace_result.hit[0] - start_x;
				delta[1] = trace_result.hit[1] - start_y;
				delta[2] = trace_result.hit[2] - start_z;

				float len = Math_XYZ_Length(delta[0], delta[1], delta[2]);

				gather_dirt += 1.0 - inv_depth * len;
			}
		}
	}

	if (gather_dirt <= 0)
	{
		return 1;
	}

	float dirt = pow(gather_dirt / (48 + 1), dirt_gain);

	if (dirt > 1.0)
	{
		dirt = 1.0;
	}

	dirt *= dirt_scale;
	if (dirt > 1.0)
	{
		dirt = 1.0;
	}

	return 1.0 - dirt;
}
static Vec4 Lightmap_CalcDirectLight(LightGlobal* global, LightTraceThread* thread, Linedef* target_line, LightDef* light, float position[3], float normal[3], float bias)
{
	Map* map = Map_GetMap();

	Vec4 total_light = Vec4_Zero();

	float deviance_offset_scale = light->radius * DEVIANCE_SCALE;

	bool is_sun = light->type == LDT__SUN;

	for (int i = 0; i < global->num_deviance_vectors; i++)
	{
		float dev_x = global->deviance_vectors[(i * 4) + 0];
		float dev_y = global->deviance_vectors[(i * 4) + 1];
		float dev_z = global->deviance_vectors[(i * 4) + 2];
		float len = global->deviance_vectors[(i * 4) + 3];

		if (is_sun)
		{
			//dev_x *= 2;
			//dev_y *= 2;
			//dev_z *= 2;
		}
		else
		{
			dev_x *= deviance_offset_scale;
			dev_y *= deviance_offset_scale;
			dev_z *= deviance_offset_scale;
		}
		
		float sample_light_pos[3] = { light->position[0] + dev_x, light->position[1] + dev_y, light->position[2] + dev_z };

		if (is_sun)
		{
			sample_light_pos[0] = light->direction[0];
			sample_light_pos[1] = light->direction[1];
			sample_light_pos[2] = light->direction[2];

			//Math_XYZ_Normalize(&light->direction[0], &light->direction[1], &light->direction[2]);
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
			light_trace_pos[0] *= 32222;
			light_trace_pos[1] *= 32222;
			light_trace_pos[2] *= 32222;

			//light_trace_pos[0] = Math_Clamp(light_trace_pos[0], map->world_bounds[0][0], map->world_bounds[1][0]);
			//light_trace_pos[1] = Math_Clamp(light_trace_pos[1], map->world_bounds[0][1], map->world_bounds[1][1]);
			//light_trace_pos[2] = Math_Clamp(light_trace_pos[2], map->world_min_height, map->world_max_height);

			skip_sky_plane = true;
		}

		//trace to target
		LightTraceResult trace_result;

		float bias_offset = 1.0 + bias;

		if (bias_offset == 0)
		{
			bias_offset = 0.1;
		}

		float normal_bias = 0.05;
		float pos_bias = 0.05;

	
		//we hit something
		if (TraceLine(global, thread, &trace_result, light_trace_pos[0], light_trace_pos[1], light_trace_pos[2], target_pos[0] / 1.01, target_pos[1] / 1.01, target_pos[2] / 1.01, skip_sky_plane))
		{
			//we dont' care if we hit the sky
			if (!trace_result.hit_sky)
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
				//tracing to sector ceil or floor
				else
				{
					//we hit a wall
					continue;
				}
			}
		}

		Vec4_Add(&total_light, light_color);
	}

	Vec4_DivScalar(&total_light, global->num_deviance_vectors);

	return total_light;
}

static Vec4 Lightmap_CalcRadiosity(LightGlobal* global, LightTraceThread* thread, float position[3], float normal[3], float bias, bool is_line)
{
	Vec4 total_light = Vec4_Zero();
	float bias_offset = 1.0 + bias;

	if (bias_offset == 0)
	{
		bias_offset = 0.1;
	}

	float start_x = position[0];
	float start_y = position[1];
	float start_z = position[2];

	if (is_line && normal)
	{
		start_x = position[0] + normal[0] * bias_offset;
		start_y = position[1] + normal[1] * bias_offset;
		start_z = position[2] + normal[2] * bias_offset;
	}
	else
	{
		start_x = position[0] / bias_offset;
		start_y = position[1] / bias_offset;
		start_z = position[2] / bias_offset;
	}

	for (int i = 0; i < global->num_random_vectors; i++)
	{
		float r_x = global->random_vectors[(i * 3) + 0];
		float r_y = global->random_vectors[(i * 3) + 1];
		float r_z = global->random_vectors[(i * 3) + 2];

		if (Math_XYZ_Dot(r_x, r_y, r_z, normal[0], normal[1], normal[2]) < 0)
		{
			r_x = -r_x;
			r_y = -r_y;
			r_z = -r_z;
		}

		float end_x = start_x + (r_x * 8192);
		float end_y = start_y + (r_y * 8192);
		float end_z = start_z + (r_z * 8192);

		LightTraceResult trace;
		//didn't hit anything
		if (!TraceLine(global, thread, &trace, start_x, start_y, start_z, end_x, end_y, end_z, false))
		{
			continue;
		}

		if (trace.hit_sky)
		{
			Map* map = Map_GetMap();

			Vec4 sky_ambient = Vec4_Zero();
			sky_ambient.r = map->sky_color[0] / 2.0;
			sky_ambient.g = map->sky_color[1] / 2.0;
			sky_ambient.b = map->sky_color[2] / 2.0;
			sky_ambient.a = max(sky_ambient.r, max(sky_ambient.g, sky_ambient.b));

			Vec4_Add(&total_light, sky_ambient);
		}
		else
		{
			if (trace.light_sample.a > 0)
			{
				float atten = calc_hit_radiosity(position, normal, r_x, r_y, r_z, &trace);

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

	float sample_x_step = x_step / (num_samples / 2);
	float sample_y_step = y_step / (num_samples / 2);

	for (int x = 0; x < x_tiles; x++)
	{
		position[1] = sector->bbox[1][1];

		for (int y = 0; y < y_tiles; y++)
		{
			float sample_pos[3] = { position[0] - (half_samples * sample_x_step), position[1] - (half_samples * sample_y_step), position[2]};
			Vec4 sample_floor_light = Vec4_Zero();
			Vec4 sample_ceil_light = Vec4_Zero();

			//direct light pass
			if (bounce == 0)
			{
				for (int sample = 0; sample < num_samples; sample++)
				{
					//floor
					sample_pos[2] = sector->floor;
					Vec4 floor_result = Lightmap_CalcDirectLight(global, thread, NULL, light, sample_pos, floor_normal, FLOOR_CEILING_TRACE_BIAS);

					//ceil
					if (!sector->is_sky)
					{
						sample_pos[2] = sector->ceil;
						Vec4 ceil_result = Lightmap_CalcDirectLight(global, thread, NULL, light, sample_pos, ceil_normal, FLOOR_CEILING_TRACE_BIAS);
						Vec4_Add(&sample_ceil_light, ceil_result);
					}
					
					Vec4_Add(&sample_floor_light, floor_result);
					
					sample_pos[0] += sample_x_step;
					sample_pos[1] += sample_y_step;
				}

				Vec4_DivScalar(&sample_floor_light, num_samples);
				if(!sector->is_sky) Vec4_DivScalar(&sample_ceil_light, num_samples);

				
				if (sample_floor_light.a > 0)
				{
					position[2] = sector->floor;
					float floor_dirt = Lightmap_CalcDirt(global, NULL, position, floor_normal);

					Vec4_ScaleScalar(&sample_floor_light, floor_dirt);
				}
				if (sample_ceil_light.a > 0 && !sector->is_sky)
				{
					position[2] = sector->ceil;
					float ceil_dirt = Lightmap_CalcDirt(global, NULL, position, ceil_normal);

					Vec4_ScaleScalar(&sample_floor_light, ceil_dirt);
				}
				Lightmap_Set(floor_lightmap, x_tiles, y_tiles, x, y, sample_floor_light);
				Lightmap_Set(ceil_lightmap, x_tiles, y_tiles, x, y, sample_ceil_light);
			}
			//gi pass
			else
			{
				//floor
				position[2] = sector->floor;
				Vec4 floor_light = Lightmap_CalcRadiosity(global, thread, position, floor_normal, FLOOR_CEILING_TRACE_BIAS, false);
				Lightmap_Set(floor_lightmap, x_tiles, y_tiles, x, y, floor_light);

				//ceil
				if (!sector->is_sky)
				{
					position[2] = sector->ceil;
					Vec4 ceil_light = Lightmap_CalcRadiosity(global, thread, position, ceil_normal, FLOOR_CEILING_TRACE_BIAS, false);
					Lightmap_Set(ceil_lightmap, x_tiles, y_tiles, x, y, ceil_light);
				}
			}
			
			position[1] += -y_step;
		}
		position[0] += x_step;
	}
}

static void Lightmap_LinePass(LightGlobal* global, LightTraceThread* thread, Linedef* line, LightDef* light, int bounce)
{
	int num_samples = AA_SAMPLES;
	int half_samples = num_samples / 2;

	if (bounce == 0 && light->type == LDT__POINT)
	{
		float line_box[2][2];
		line_box[0][0] = min(line->x0, line->x1);
		line_box[0][1] = min(line->y0, line->y1);
		line_box[1][0] = max(line->x0, line->x1);
		line_box[1][1] = max(line->y0, line->y1);

		if (!Math_BoxIntersectsBox(line_box, light->bbox))
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

	float sample_x_step = x_step / half_samples;
	float sample_y_step = y_step / half_samples;
	float sample_z_step = z_step / half_samples;

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
				float sample_pos[3] = { position[0] - (half_samples * sample_x_step), position[1] - (half_samples * sample_y_step), position[2] - (half_samples * sample_z_step) };

				Vec4 sample_light = Vec4_Zero();

				for (int sample = 0; sample < num_samples; sample++)
				{
					Vec4 light_result = Lightmap_CalcDirectLight(global, thread, line, light, sample_pos, normal, LINE_TRACE_BIAS);

					Vec4_Add(&sample_light, light_result);

					sample_pos[0] += sample_x_step;
					sample_pos[1] += sample_y_step;
					sample_pos[2] += sample_z_step;
				}

				Vec4_DivScalar(&sample_light, num_samples);

				if (sample_light.a > 0)
				{
					float dirt = Lightmap_CalcDirt(global, line, position, normal);

					Vec4_ScaleScalar(&sample_light, dirt);
				}
				Lightmap_Set(&line->lightmap, x_tiles, y_tiles, x, y, sample_light);
			}
			//bounce pass
			else
			{
				Vec4 light = Lightmap_CalcRadiosity(global, thread, position, normal, LINE_TRACE_BIAS, true);
				Lightmap_Set(&line->lightmap, x_tiles, y_tiles, x, y, light);
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
	Lightmap_FloorAndCeillingPass(global, thread, sector, light, sector_x_tiles, sector_y_tiles, sector_x_step, sector_y_step, bounce);

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
	//gi pass
	else
	{
		Lightmap_LightPass(global, thread, sector, bounce, sector_dx, sector_dy, sector_x_tiles, sector_y_tiles, sector_x_step, sector_y_step, NULL);
	}
}
void Lightmap_Create(LightGlobal* global, Map* map)
{
	if (global->num_lights <= 0)
	{
		return;
	}

	double start = glfwGetTime();

	int bounces = NUM_BOUNCES;
	if (bounces <= 0)
	{
		return;
	}

	printf("Creating Lightmaps with %i bounces, %i threads\n", bounces, global->num_threads);

	float bounce_scale_step = 1.0 / (float)bounces;
	float bounce_scale = 1.0;

	global->bounce_scale = 1;

	for (int b = 0; b < bounces; b++)
	{
		if (global->num_threads > 0)
		{
			//make threads do the work
			LightGlobal_SetWorkStateForAllThreads(global, b);

			//wait for them to finih
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

				Lightmap_Sector(global, &global->thread, sector, b);
			}
		}
		
		//then swap lightmaps to back buffer
		Lightmap_SwapLightmaps(global, map);

		//global->bounce_scale = bounce_scale;

		if (b > 0)
		{
			bounce_scale -= bounce_scale_step;
		}
	}

	double end = glfwGetTime();

	printf("Finished creating lightmaps. Time: %f \n", end - start);
}

static void Lightblock_Process(LightGlobal* global, Lightblock* block, float position[3])
{
	Vec4 total_light = Vec4_Zero();
	//calc direct light
	//sun
	float sun_pos[3] = { 1, 1, 1 };
	//total_light += Lightmap_CalcDirectLight(NULL, -1, -1, sun_pos, position, NULL, 0);
	//for each light
	for (int i = 0; i < global->num_lights; i++)
	{
		LightDef* light = &global->light_list[i];

		if (!Math_BoxContainsPoint(light->bbox, position))
		{
			continue;
		}

		Vec4 result_light = Lightmap_CalcDirectLight(global, &global->thread, NULL, light, position, NULL, 0);

		Vec4_Add(&total_light, result_light);
	}

	Vec4_DivScalar(&total_light, global->num_lights + 1);

	float n[3] = { 1, 1, 1 };

	Vec4 radiosity = Lightmap_CalcRadiosity(global, &global->thread, position, n, 0, false);

	Vec4_ScaleScalar(&radiosity, 4);
	Vec4_Add(&total_light, radiosity);

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
	global->bounce_scale = 1;

	Lightgrid* lightgrid = &map->lightgrid;

	memset(lightgrid, 0, sizeof(Lightgrid));

	int x_blocks = ceil(map->world_size[0] / LIGHT_GRID_SIZE) + 1;
	int y_blocks = ceil(map->world_size[1] / LIGHT_GRID_SIZE) + 1;
	int z_blocks = ceil(map->world_height / LIGHT_GRID_SIZE) + 1;

	lightgrid->blocks = calloc(x_blocks * y_blocks * z_blocks, sizeof(Lightblock));

	if (!lightgrid->blocks)
	{
		return;
	}

	Map_SetupLightGrid(x_blocks, y_blocks, z_blocks);

	int grid_step[3];
	grid_step[0] = 1;
	grid_step[1] = lightgrid->bounds[0];
	grid_step[2] = lightgrid->bounds[0] * lightgrid->bounds[1];

	for (int x = 0; x < x_blocks; x++)
	{
		for (int y = 0; y < y_blocks; y++)
		{
			for (int z = 0; z < z_blocks; z++)
			{
				Lightblock* block = &lightgrid->blocks[x * grid_step[0] + y * grid_step[1] + z * grid_step[2]];

				float position[3];

				position[0] = lightgrid->origin[0] + (x * LIGHT_GRID_SIZE);
				position[1] = lightgrid->origin[1] + (y * LIGHT_GRID_SIZE);
				position[2] = lightgrid->origin[2] + (z * LIGHT_GRID_SIZE);

				Lightblock_Process(global, block, position);
			}
		}
	}

	Map_UpdateObjectsLight();
}
