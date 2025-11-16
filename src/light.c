#include "g_common.h"

#include "u_math.h"
#include <Windows.h>

#define MAX_THREAD_HITS 10000
#define MAX_THREADS 4

typedef struct
{
	Linedef* linedef;
	float normal[3];
	float hit[3];
	float frac;
	float light_sample;
	float color_sample[3];
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
	int hits[MAX_THREAD_HITS];
	struct LightGlobal* globals;
} LightTraceThread;

typedef struct
{
	float* random_vectors;
	int num_random_vectors;

	int* light_list;
	int num_lights;

	LinedefList* linedef_list;
	int num_linedef_lists;

	LightTraceThread* threads;
	int num_threads;
} LightGlobal;

static bool TraceLine(LightTraceResult* result, float start_x, float start_y, float start_z, float end_x, float end_y, float end_z)
{
	memset(result, 0, sizeof(result));
	result->frac = 1;
	result->light_sample = 1;

	int hit = Trace_FindLine(start_x, start_y, start_z, end_x, end_y, end_z, &result->hit[0], &result->hit[1], &result->hit[2], &result->frac);

	if (hit == TRACE_NO_HIT)
	{
		return false;
	}
	
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
	unsigned char* light_sample = NULL;
	int tx = 0;
	int ty = 0;

	//hit ceilling
	if (result->hit[2] > frontsector->ceil)
	{
		result->normal[0] = 0;
		result->normal[1] = 0;
		result->normal[2] = -1;
		result->hit[2] = frontsector->ceil;
	}
	//hit floor
	else if (result->hit[2] < frontsector->floor)
	{
		result->normal[0] = 0;
		result->normal[1] = 0;
		result->normal[2] = 1;
		result->hit[2] = frontsector->floor;
	}
	//hit wall
	else
	{
		result->normal[0] = linedef->dy;
		result->normal[1] = -linedef->dx;
		result->normal[2] = 0;

		if (sidedef)
		{
			float z = result->hit[2];
			tx = (z - frontsector->floor) / (frontsector->ceil - frontsector->floor);
			ty = 0;
			if (fabs(linedef->dx) > fabs(linedef->dy))
			{
				ty = (result->hit[0] - linedef->x0) / linedef->dx;
			}
			else if (linedef->dy != 0)
			{
				ty = (result->hit[1] - linedef->y0) / linedef->dy;
			}

			if (backsector)
			{

			}
			else
			{
				if (sidedef->middle_texture)
				{
					texture_sample = Image_Get(&sidedef->middle_texture->img, tx & sidedef->middle_texture->width_mask, ty & sidedef->middle_texture->height_mask);
				}
			}
		}
	}
	
	if (linedef->lightmap.data)
	{
		light_sample = Lightmap_Get(&linedef->lightmap, tx / LIGHTMAP_LUXEL_SIZE, ty / LIGHTMAP_LUXEL_SIZE);
	}

	if (texture_sample)
	{
		result->color_sample[0] = texture_sample[0];
		result->color_sample[1] = texture_sample[1];
		result->color_sample[2] = texture_sample[2];
	}
	if (light_sample)
	{
		result->light_sample = *light_sample;
	}

	result->color_sample[0] *= result->light_sample;
	result->color_sample[1] *= result->light_sample;
	result->color_sample[2] *= result->light_sample;

	result->linedef = linedef;

	return true;
}

static int calc_light(float light_dir[3], float normal[3], float attenuation)
{
	float dot = Math_XYZ_Dot(normal[0], normal[1], normal[2], light_dir[0], light_dir[1], light_dir[2]);
	float diff = max(dot, 0.0);

	int light = Math_Clampl(diff * (255.0 * attenuation), 0, 255);

	return light;
}
static int calc_point_light(float light_position[3], float light_radius, float light_attenuation, float normal[3], float position[3])
{
	float light_vec[3] = { light_position[0] - position[0], light_position[1] - position[1], light_position[2] - position[2] };
	float distance = Math_XYZ_Length(light_vec[0], light_vec[1], light_vec[2]);

	float inv_radius = 1.0 / light_radius;
	float normalized_dist = distance * inv_radius;
	float atten = pow(max(1.0 - normalized_dist, 0), light_attenuation);

	if (atten > 0)
	{
		Math_XYZ_Normalize(&light_vec[0], &light_vec[1], &light_vec[2]);

		return calc_light(light_vec, normal, atten);
	}
	
	return 0;
}

static int calc_rad(float position[3], float normal[3], float light_radius, float light_attenuation, float hit_normal[3], float hit_position[3])
{
	float vec[3] = { position[0] - hit_position[0], position[1] - hit_position[1], position[2] - hit_position[2] };
	float distance = Math_XYZ_Length(vec[0], vec[1], vec[2]);

	float inv_radius = 1.0 / light_radius;
	float normalized_dist = distance * inv_radius;
	float atten = pow(max(1.0 - normalized_dist, 0), light_attenuation);

	return calc_light(normal, hit_normal, atten);
}

static void LightGlobal_Setup(LightGlobal* global)
{
	Map* map = Map_GetMap();

	memset(global, 0, sizeof(LightGlobal));

	int num_lights = 0;
	//calc total lights
	while (Map_GetNextObjectByType(&num_lights, OT__LIGHT));

	if (num_lights == 0)
	{
		return;
	}

	global->light_list = calloc(num_lights, sizeof(int));

	if (!global->light_list)
	{
		return;
	}

	num_lights = 0;
	Object* light_obj = NULL;
	while (light_obj = Map_GetNextObjectByType(&num_lights, OT__LIGHT))
	{
		global->light_list[num_lights - 1] = light_obj->id;
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



	//setup random vecs
	for (int i = 0; i < 100; i++)
	{
		float x = (rand() % 1000000) / 1e6;
		float y = (rand() % 1000000) / 1e6;

		double phi = acos(2 * y - 1);
		//tvec[n][0] = cos(theta) * sin(phi);
		//tvec[n][2] = sin(theta) * sin(phi);
		//tvec[n][1] = cos(phi);
	}

}
static void LightGlobal_Destruct(LightGlobal* global)
{
	if (global->light_list) free(global->light_list);
	if (global->random_vectors) free(global->random_vectors);

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
static void LightMap_Blur(Lightmap* lightmap)
{
	if (!lightmap->data)
	{
		return;
	}

	unsigned char* copy = calloc((lightmap->width * lightmap->height), sizeof(unsigned char));

	if (!copy)
	{
		return;
	}

	int dir_x = 1;
	int dir_y = 1;

	int size = 4;
	int half_size = size / 2;

	for (int x = 0; x < lightmap->width; x++)
	{
		for (int y = 0; y < lightmap->height; y++)
		{
			int result = 0;

			for (int i = -half_size; i < half_size; i++)
			{
				if (i == 0)
				{
					continue;
				}

				float radius = (float)i;

				int tx = x + dir_x * radius;
				int ty = y + dir_y * radius;

				tx = Math_Clampl(tx, 0, lightmap->width - 1);
				ty = Math_Clampl(ty, 0, lightmap->height - 1);

				unsigned int sample = lightmap->data[tx + ty * lightmap->width];

				result += sample;
			}

			result /= size - 1;

			copy[x + y * lightmap->width] = Math_Clampl(result, 0, 255);
		}
	}

	free(lightmap->data);
	lightmap->data = copy;
}

static void Lightmap_Trim(Lightmap* lm)
{

}

static void Lightmap_Set(Lightmap* lm, int x_tiles, int y_tiles, int x, int y, int light)
{
	if (light <= 0)
	{
		return;
	}

	if (!lm->data)
	{
		lm->data = calloc(x_tiles * y_tiles, sizeof(unsigned char));

		if (!lm->data)
		{
			return;
		}
	}

	x = Math_Clampl(x, 0, x_tiles - 1);
	y = Math_Clampl(y, 0, y_tiles - 1);
	light = Math_Clampl(light, 0, 255);

	lm->data[x + y * x_tiles] = light;
}

static int Lightmap_CalcDirectLight(Linedef* target_line, float light_radius, float light_attenuation, float light_pos[3], float position[3], float normal[3])
{
	int light = calc_point_light(light_pos, light_radius, light_attenuation, normal, position);

	//no light contribution at all
	if (light <= 0)
	{
		return 0;
	}

	//trace to target
	LightTraceResult trace_result;

	//we hit something
	if (TraceLine(&trace_result, light_pos[0], light_pos[1], light_pos[2], position[0], position[1], position[2]))
	{
		//tracing to wall
		if (target_line)
		{
			//we did not the target
			if (trace_result.linedef != target_line)
			{
				return 0;
			}
		}
		//tracing to sector ceil or floor
		else
		{
			//we hit a wall
			return 0;
		}
	}

	return light;
}

static void Lightmap_FloorAndCeillingPass(Sector* sector, float light_radius, float light_attenuation, float light_pos[3],
	int x_tiles, int y_tiles, float x_step, float y_step, int bounce)
{
	float ceil_normal[3] = { 0, 0, -1 };
	float floor_normal[3] = { 0, 0, 1 };

	float position[3] = { sector->bbox[0][0] * 2, sector->bbox[0][1] * 2, sector->floor };
	float light_p[3] = { light_pos[0] * 2, light_pos[1] * 2, light_pos[2] };

	for (int x = 0; x < x_tiles; x++)
	{
		for (int y = 0; y < y_tiles; y++)
		{
			//direct light pass
			if (bounce == 0)
			{
				//floor
				position[2] = sector->floor;
				int floor_light = Lightmap_CalcDirectLight(NULL, light_radius, light_attenuation, light_p, position, floor_normal);
				Lightmap_Set(&sector->floor_lightmap, x_tiles, y_tiles, x, y, floor_light);

				//ceil
				position[2] = sector->ceil;
				int ceil_light = Lightmap_CalcDirectLight(NULL, light_radius, light_attenuation, light_p, position, ceil_normal);
				Lightmap_Set(&sector->ceil_lightmap, x_tiles, y_tiles, x, y, ceil_light);
			}
			//bounce pass
			else
			{

			}

			position[1] += -y_step;
		}
		position[0] += x_step;
	}
}

static void Lightmap_Sector2(Sector* sector, int bounce)
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
		int light_index = 0;
		Object* light = NULL;
		while (light = Map_GetNextObjectByType(&light_index, OT__LIGHT))
		{
			float light_radius = 400;
			float light_atten = 1;
			float light_pos[3] = { light->x, light->y, light->z };

			Lightmap_FloorAndCeillingPass(sector, light_radius, light_atten, light_pos, sector_x_tiles, sector_y_tiles, sector_x_step, sector_y_step, bounce);
		}
	}
	//gi pass
	else
	{

	}
}

Linedef* linedefs[1000];
float light_samples[32][3];
int nrandomvectors = 128;
float tvec[128][3];

static void Lightmap_Sector(Sector* sector, int bounce)
{
	// Create uniformly distributed random unit vectors
	for (unsigned n = 0; n < nrandomvectors; ++n)
	{
		double u = (rand() % 1000000) / 1e6; // 0..1
		double v = (rand() % 1000000) / 1e6; // 0..1
		double theta = 2 * 3.141592653 * u;
		double phi = acos(2 * v - 1);
		tvec[n][0] = cos(theta) * sin(phi);
		tvec[n][2] = sin(theta) * sin(phi);
		tvec[n][1] = cos(phi);
	}

	int num_sector_lines = Trace_SectorLines(sector, false);
	int* hits = Trace_GetHitObjects();

	int* b = calloc(num_sector_lines, sizeof(int));

	memcpy(b, hits, sizeof(int) * num_sector_lines);


	int light_num = 0;
	Object* light_object = Map_GetNextObjectByType(&light_num, OT__LIGHT);


	float light_dir[3] = { 0, 0, -1 };
	float light_pos[3] = { light_object->x * 2, light_object->y * 2, sector->floor + 400 };

	{
		float dx = (sector->bbox[1][0] - sector->bbox[0][0]) * 2.0;
		float dy = (sector->bbox[1][1] - sector->bbox[0][1]) * 2.0;

		int x_tiles = ceil(dx / LIGHTMAP_LUXEL_SIZE);
		int y_tiles = ceil(dy / LIGHTMAP_LUXEL_SIZE);

		if (!sector->floor_lightmap.data)
		{
			sector->floor_lightmap.data = calloc(x_tiles * y_tiles, sizeof(unsigned char));
		}
		if (!sector->ceil_lightmap.data)
		{
			sector->ceil_lightmap.data = calloc(x_tiles * y_tiles, sizeof(unsigned char));
		}
		

		float x_step = dx / (x_tiles);
		float y_step = dy / (y_tiles);

		//x_step = Math_sign_float(dx) * LIGHTMAP_LUXEL_SIZE;
		//y_step = Math_sign_float(dy) * LIGHTMAP_LUXEL_SIZE;

		float position[3] = { sector->bbox[0][0] * 2, sector->bbox[0][1] * 2, sector->floor};


		float ceil_normal[3] = {0, 0, -1};
		float floor_normal[3] = { 0, 0, 1 };

		if (bounce == 0)
		{
			for (int x = 0; x < x_tiles; x++)
			{
				position[1] = sector->bbox[1][1] * 2;
				for (int y = 0; y < y_tiles; y++)
				{
					position[2] = sector->floor;

					int trace_line_index = Trace_FindLine(light_object->x, light_object->y, sector->floor + 50, position[0] / 2.1, position[1] / 2.1, position[2], NULL, NULL, NULL, NULL);

					int floor_light = calc_point_light(light_pos, 580, 0.5, floor_normal, position);
					if (trace_line_index != TRACE_NO_HIT)
					{
						floor_light = 0;
					}

					//int floor_light = calc_point_light(light_pos, 580, 0, floor_normal, position);
					position[2] = sector->ceil;

					trace_line_index = Trace_FindLine(light_object->x, light_object->y, sector->floor + 50, position[0] / 2.1, position[1] / 2.1, position[2], NULL, NULL, NULL, NULL);

					int ceil_light = calc_point_light(light_pos, 580, 0.5, ceil_normal, position);

					if (trace_line_index != TRACE_NO_HIT)
					{
						ceil_light = 0;
					}

					sector->floor_lightmap.data[x + y * x_tiles] = floor_light;
					sector->ceil_lightmap.data[x + y * x_tiles] = ceil_light;

					position[1] += -y_step;
				}
				position[0] += x_step;
			}

			sector->floor_lightmap.width = x_tiles;
			sector->floor_lightmap.height = y_tiles;

			sector->ceil_lightmap.width = x_tiles;
			sector->ceil_lightmap.height = y_tiles;
		}
		else
		{
			for (int x = 0; x < x_tiles; x++)
			{
				position[1] = sector->bbox[1][1] * 2;
				for (int y = 0; y < y_tiles; y++)
				{
					position[2] = sector->floor;

					int diff_light = 0;

					for (int k = 0; k < nrandomvectors; k++)
					{
						float start_x = position[0] / 2.1;
						float start_y = position[1] / 2.1;
						float start_z = position[2];

						float random_x = tvec[k][0];
						float random_y = tvec[k][1];
						float random_z = tvec[k][2];

						if (Math_XYZ_Dot(random_x, random_y, random_z, floor_normal[0], floor_normal[1], floor_normal[2]) < 0)
						{
							//random_x = -random_x;
							//random_y = -random_y;
							//random_z = -random_z;
						}

						float end_x = start_x + random_x * 2512;
						float end_y = start_y + random_y * 2512;
						float end_z = start_z + random_z * 2512;

						float hit_x = 0, hit_y = 0, hit_z = 0;
						int trace_line_index = Trace_FindLine(start_x, start_y, start_z, end_x, end_y, end_z, &hit_x, &hit_y, &hit_z, NULL);

						if (trace_line_index == TRACE_NO_HIT)
						{
							continue;
						}

						Line* trace_line = Map_GetLine(trace_line_index);

						if (!trace_line->linedef->lightmap.data)
						{
							continue;
						}

						float trace_normal[3] = { trace_line->linedef->dy, -trace_line->linedef->dx, 0 };

						if (trace_line->linedef->front_sector != sector->index)
						{
							trace_normal[0] = -trace_line->linedef->dy;
							trace_normal[1] = trace_line->linedef->dx;
						}

						Math_XYZ_Normalize(&trace_normal[0], &trace_normal[1], &trace_normal[2]);

						float hit_pos[3] = { hit_x, hit_y, hit_z };

						float p[3] = { start_x, start_y, start_z };

						diff_light += calc_point_light(hit_pos, 30, 0, floor_normal, p);

						
					}

					diff_light = Math_Clamp(diff_light / nrandomvectors, 0, 255);

					int old_light = sector->floor_lightmap.data[x + y * x_tiles];

					sector->floor_lightmap.data[x + y * x_tiles] = Math_Clamp(old_light + diff_light, 0, 255);
					

					position[1] += -y_step;
				}
				position[0] += x_step;
			}
		}

		
	}

	int num_linedefs = 0;
	for (int i = 0; i < num_sector_lines; i++)
	{
		int line_index = b[i];
		Line* line = Map_GetLine(line_index);

		int k = 0;
		for (k = 0; k < num_linedefs; k++)
		{
			if (line->linedef == linedefs[k])
			{
				break;
			}
		}

		if (k == num_linedefs)
		{
			linedefs[num_linedefs++] = line->linedef;
		}
	}
	
	for (int i = 0; i < num_linedefs; i++)
	{
		Linedef* line = linedefs[i];

		if (!line)
		{
			continue;
		}

		if (bounce == 0 && line->lightmap.data)
		{
			continue;
		}

		Sector* backsector = NULL;

		if (line->back_sector >= 0)
		{
			backsector = Map_GetSector(line->back_sector);
		}

		float dx = line->x0 - line->x1;
		float dy = line->y0 - line->y1;

		float normal[3] = { dy, -dx, 0 };

		if (line->front_sector != sector->index)
		{
			normal[0] = -dy;
			normal[1] = dx;
		}

		Math_XYZ_Normalize(&normal[0], &normal[1], &normal[2]);


		float max_ceil = sector->ceil;
		float min_floor = sector->floor;
		float open_low = sector->floor;
		float open_high = sector->ceil;
		float open_range = open_high - open_low;

		if (backsector)
		{
			max_ceil = max(sector->ceil, backsector->ceil);
			min_floor = min(sector->floor, backsector->floor);
			open_low = max(sector->floor, backsector->floor);
			open_high = min(sector->ceil, backsector->ceil);
			open_range = open_high - open_low;
		}

		float width_scale = sqrtf(dx * dx + dy * dy);
		int x_tiles = ceil((width_scale * 2.0) / LIGHTMAP_LUXEL_SIZE);
		int y_tiles = ceil(((max_ceil - min_floor) / 2.0) / LIGHTMAP_LUXEL_SIZE);

		if (x_tiles == 0 && y_tiles == 0)
		{
			continue;
		}
		else if (x_tiles == 0)
		{
			x_tiles = 1;
		}
		else if (y_tiles == 0)
		{
			y_tiles = 1;
		}

		if (!line->lightmap.data)
		{
			line->lightmap.data = calloc(x_tiles * y_tiles, sizeof(unsigned char));
		}
		
		

		float n[2] = { dx, dy };
		Math_XY_Normalize(&n[0], &n[1]);

		float dz = max_ceil - min_floor;;

		float lx_step = (dx / x_tiles);
		float ly_step = (dy / x_tiles);
		float lz_step = (dz / y_tiles);

		float lx_pos = line->x1;
		float ly_pos = line->y1;

		float lx_pos2 = line->x1;
		float ly_pos2 = line->y1;



		Map* map = Map_GetMap();



		float light_pos2[3] = { light_object->x, light_object->y, 100 };



		if (bounce == 0)
		{
			for (int x = 0; x < x_tiles; x++)
			{
				float position[3] = { lx_pos, ly_pos, 0 };
				float center[2] = { 0, 0 };

				int light = Math_Clamp(lx_pos, 0, 255);

				float lz_pos = sector->ceil;


				for (int y = 0; y < y_tiles; y++)
				{
					position[2] = lz_pos;

					if (backsector)
					{
						if (lz_pos > open_low && lz_pos < open_high)
						{
							//lz_pos += -lz_step;
							//continue;
						}
					}

					light = calc_point_light(light_pos2, 400, 0.0, normal, position);

					if (light <= 0)
					{
						lz_pos += -lz_step;
						continue;
					}


					int trace_line_index = Trace_FindLine(light_object->x, light_object->y, 100, lx_pos2 / 1.01, ly_pos2 / 1.01, lz_pos, NULL, NULL, NULL, NULL);

					if (trace_line_index != TRACE_NO_HIT)
					{
						Line* trace_line = Map_GetLine(trace_line_index);

						if (trace_line->linedef != line)
						{
							lz_pos += -lz_step;
							continue;
						}
					}


					

					line->lightmap.data[x + y * x_tiles] = Math_Clamp(light + (int)line->lightmap.data[x + y * x_tiles], 0, 255);

					//line->lightmap.data[x + y * x_tiles] = rand() & 255;

					lz_pos += -lz_step;


				}

				//Object_Spawn(OT__PARTICLE, SUB__PARTICLE_WALL_HIT, lx_pos2, ly_pos2, sector->floor + 60);
				//break;

				lx_pos += lx_step;
				ly_pos += ly_step;

				lx_pos2 += lx_step;
				ly_pos2 += ly_step;
			}
			line->lightmap.width = x_tiles;
			line->lightmap.height = y_tiles;
		}
		else
		{
			for (int x = 0; x < x_tiles; x++)
			{
				float position[3] = { lx_pos, ly_pos, 0 };
				float center[2] = { 0, 0 };

				int light = Math_Clamp(lx_pos, 0, 255);

				float lz_pos = sector->ceil;


				for (int y = 0; y < y_tiles; y++)
				{
					position[2] = lz_pos;

					int old_light = (int)line->lightmap.data[x + y * x_tiles];

					if (old_light <= 0)
					{
						//lz_pos += -lz_step;
						//continue;
					}
					light = 0;

					int num_hits = 0;
					for (int k = 0; k < nrandomvectors; k++)
					{
						float start_x = lx_pos2 / 1.05;
						float start_y = ly_pos2 / 1.05;
						float start_z = lz_pos;

						float random_x = tvec[k][0];
						float random_y = tvec[k][1];
						float random_z = tvec[k][2];

						if (Math_XYZ_Dot(random_x, random_y, random_z, normal[0], normal[1], normal[2]) < 0)
						{
							//random_x = -random_x;
							//random_y = -random_y;
							//random_z = -random_z;
						}

						float end_x = start_x + random_x * 2512;
						float end_y = start_y + random_y * 2512;
						float end_z = start_z + random_z * 2512;

						float hit_x = 0, hit_y = 0, hit_z = 0;
						int trace_line_index = Trace_FindLine(start_x, start_y, start_z, end_x, end_y, end_z, &hit_x, &hit_y, &hit_z, NULL);

						if (trace_line_index == TRACE_NO_HIT)
						{
							continue;
						}

						Line* trace_line = Map_GetLine(trace_line_index);
						
						if (!trace_line->linedef->lightmap.data)
						{
							continue;
						}

						float trace_normal[3] = { trace_line->linedef->dy, -trace_line->linedef->dx, 0 };

						if (trace_line->linedef->front_sector != sector->index)
						{
							trace_normal[0] = -trace_line->linedef->dy;
							trace_normal[1] = trace_line->linedef->dx;
						}

						Math_XYZ_Normalize(&trace_normal[0], &trace_normal[1], &trace_normal[2]);
					
						float hit_pos[3] = { hit_x, hit_y, hit_z };

						float p[3] = { start_x, start_y, start_z };

						light += calc_rad(p, normal, 112, 2, trace_normal, hit_pos);

						num_hits++;
					}

					int diff_light = 0;
					if (num_hits > 0)
					{
						diff_light = Math_Clamp((light / nrandomvectors) / 8, 0, 255);

					}

					



					line->lightmap.data[x + y * x_tiles] = Math_Clamp((diff_light) + old_light, 0, 255);

					lz_pos += -lz_step;


				}

				//Object_Spawn(OT__PARTICLE, SUB__PARTICLE_WALL_HIT, lx_pos2, ly_pos2, sector->floor + 60);
				//break;

				lx_pos += lx_step;
				ly_pos += ly_step;

				lx_pos2 += lx_step;
				ly_pos2 += ly_step;
			}
		}
	

	}
	free(b);
}

void Lightmap_Create(Map* map)
{
	LightGlobal global;
	LightGlobal_Setup(&global);

	int bounces = 1;
	for (int b = 0; b < bounces; b++)
	{
		for (int s = 0; s < map->num_sectors; s++)
		{
			Sector* sector = Map_GetSector(s);

			Lightmap_Sector(sector, b);

		}
	}

	LightGlobal_Destruct(&global);
}

static void Lightblock_Process(Lightblock* block, int x_pos, int y_pos, int z_pos)
{

}

void Lightblocks_Create(Map* map)
{
	int x_blocks = ceil(map->world_size[0] / 64.0) + 1;
	int y_blocks = ceil(map->world_size[1] / 64.0) + 1;
	int z_blocks = ceil(map->world_height / 64.0) + 1;

	
	for (int x = 0; x < x_blocks; x++)
	{
		for (int y = 0; y < y_blocks; y++)
		{
			Object_Spawn(OT__PARTICLE, SUB__PARTICLE_WALL_HIT, map->world_bounds[0][0] + (x * 64), map->world_bounds[0][1] + (y * 64), map->world_min_height + (0 * 64));
			for (int z = 0; z < z_blocks; z++)
			{
				int x_pos = map->world_bounds[0][0] + (x * 64);
				int y_pos = map->world_bounds[0][1] + (y * 64);
				int z_pos = map->world_min_height + (z * 64);

				Lightblock b;
				Lightblock_Process(&b, x_pos, y_pos, z_pos);

				//Object_Spawn(OT__PARTICLE, SUB__PARTICLE_WALL_HIT, map->world_bounds[0][0] + (x * 64), map->world_bounds[0][1] + (y * 64), map->world_min_height + (z * 64));
			}
		}
	}


}
