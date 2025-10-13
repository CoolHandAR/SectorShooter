#include "g_common.h"

#include "u_math.h"

#define MAX_TRACE_ITEMS 100000
#define TRACE_EPSILON 0.125

typedef struct
{
	int result_items[MAX_TRACE_ITEMS];
} TraceCore;

static TraceCore s_traceCore;

static void Register_TraceItem(int _data_index, BVH_ID _index, int _hit_count)
{
	s_traceCore.result_items[_hit_count] = _data_index;
}

bool Trace_CheckBoxPosition(Object* obj, float x, float y, float size, float* r_floorZ, float* r_ceilZ, float* r_lowFloorZ)
{
	Map* map = Map_GetMap();

	float bbox[2][2];
	Math_SizeToBbox(x, y, size, bbox);

	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, bbox, MAX_TRACE_ITEMS, Register_TraceItem);

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//Is a line
		if (index < 0)
		{
			index = -(index + 1);

			Line* line = &map->line_segs[index];

			if (Line_BoxOnPointSide(line, bbox) >= 0)
			{
				continue;
			}

			//we hit the line
			if (line->back_sector < 0)
			{
				return false;
			}

			Sector* frontsector = &map->sectors[line->front_sector];
			Sector* backsector = &map->sectors[line->back_sector];

			float open_low = max(frontsector->floor, backsector->floor);
			float open_high = min(frontsector->ceil, backsector->ceil);
			float open_range = open_high - open_low;
			float low_floor = 0;

			if (frontsector->floor > backsector->floor)
			{
				low_floor = backsector->floor;
			}
			else
			{
				low_floor = frontsector->floor;
			}

			if (open_high > *r_ceilZ)
			{
				*r_ceilZ = open_high;
			}
			if (open_low < *r_floorZ)
			{
				*r_floorZ = open_low;
			}
			if (low_floor < *r_lowFloorZ)
			{
				*r_lowFloorZ = low_floor;
			}

		}
		//otherwise a object
		else
		{

		}

	}


	return true;
}

int Trace_FindSlideHit(Object* obj, Line* vel_line, float start_x, float start_y, float end_x, float end_y, float size, float* best_frac)
{
	Map* map = Map_GetMap();

	float bbox[2][2];
	if (end_x > start_x)
	{
		bbox[0][0] = start_x - size;
		bbox[1][0] = end_x + size;
	}
	else
	{
		bbox[0][0] = end_x - size;
		bbox[1][0] = start_x + size;
	}
	if (end_y > start_y)
	{
		bbox[0][1] = start_y + size;
		bbox[1][1] = end_y + size;
	}
	else
	{
		bbox[0][1] = end_y - size;
		bbox[1][1] = start_y + size;
	}

	int min_hit = TRACE_NO_HIT;

	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, bbox, MAX_TRACE_ITEMS, Register_TraceItem);

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//Is a line
		if (index < 0)
		{
			int line_index = -(index + 1);

			Line* line = &map->line_segs[line_index];

			if (Line_BoxOnPointSide(line, bbox) != -1)
			{
				continue;
			}

			Sector* frontsector = &map->sectors[line->front_sector];
			Sector* backsector = NULL;
			
			if (line->back_sector >= 0)
			{
				backsector = &map->sectors[line->back_sector];
			}
			
			bool collidable = false;

			if (!backsector)
			{
				if(Line_PointSide(line, start_x + (end_x - start_x), start_y + (end_y - start_y)))
				{
					//continue;
				}

				collidable = true;
			}
			else if (!Check_CanObjectFitInSector(obj, frontsector, backsector))
			{
				collidable = true;
			}

			if (!collidable)
			{
				continue;
			}

			int s1 = Line_PointSide(vel_line, line->x0, line->y0);
			int s2 = Line_PointSide(vel_line, line->x1, line->y1);

			if (s1 == s2)
			{
				continue;
			}

			float frac = Line_InterceptLine(vel_line, line);

			if (frac < 0.0 || frac > 1.0)
			{
				continue;
			}

			if (frac < *best_frac)
			{
				min_hit = index;
				*best_frac = frac;
			}

		}
		//otherwise a object
		else
		{
		}
	}


	return min_hit;
}

int Trace_Line(Object* obj, float start_x, float start_y, float end_x, float end_y, float z, float* r_hitX, float* r_hitY, float* r_hitZ, float* r_frac)
{
	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Trace(&map->spatial_tree, start_x, start_y, end_x, end_y, MAX_TRACE_ITEMS, Register_TraceItem);

	float min_frac = 1.001;
	int min_hit = TRACE_NO_HIT;

	float min_hit_x = end_x;
	float min_hit_y = end_y;
	float min_hit_z = z;

	Line trace_line;
	trace_line.x0 = start_x;
	trace_line.y0 = start_y;
	trace_line.x1 = end_x;
	trace_line.y1 = end_y;
	trace_line.dx = trace_line.x1 - trace_line.x0;
	trace_line.dy = trace_line.y1 - trace_line.y0;
	trace_line.dot = trace_line.dx * trace_line.dx + trace_line.dy * trace_line.dy;

	float attack_range = 10000;
	float top_pitch = Math_DegToRad(35.0);
	float bottom_pitch = -Math_DegToRad(35.0);
	float aim_slope = 0;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//Is a line
		if (index < 0)
		{
			int line_index = -(index + 1);
			Line* line = &map->line_segs[line_index];

			Sector* frontsector = &map->sectors[line->front_sector];
			Sector* backsector = NULL;

			if (line->back_sector >= 0)
			{
				backsector = &map->sectors[line->back_sector];
			}

			int s1 = Line_PointSide(&trace_line, line->x0, line->y0);
			int s2 = Line_PointSide(&trace_line, line->x1, line->y1);

			if (s1 == s2)
			{
				//continue;
			}

			float frac = 0;
			float hit_x = 0;
			float hit_y = 0;

			if (!Line_SegmentInterceptSegmentLine(&trace_line, line, &frac, &hit_x, &hit_y))
			{
				continue;
			}

			if (frac < 0 || frac > 1)
			{
				continue;
			}

			if (backsector)
			{
				float open_low = max(frontsector->floor, backsector->floor);
				float open_high = min(frontsector->ceil, backsector->ceil);
				float open_range = open_high - open_low;

				if (open_range > 0 && open_low < open_high)
				{
					float dist = attack_range * frac;

					float pitch_high = FLT_MAX;
					float pitch_low = -FLT_MAX;

					pitch_low = -Math_XY_Angle(dist, open_low - z);
					//if (pitch_low > bottom_pitch) bottom_pitch = pitch_low;

					pitch_high = -Math_XY_Angle(dist, open_high - z);
					//if (pitch_high > top_pitch) top_pitch = pitch_high;

					//the trace will not hit floor and ceil
					if (open_low < z && open_high > z)
					{
						continue;
					}
				}

			}

			if (frac < min_frac)
			{
				if (backsector)
				{
					float open_low = max(frontsector->floor, backsector->floor);
					float open_high = min(frontsector->ceil, backsector->ceil);
					float open_range = open_high - open_low;

					if (open_range > 0 && open_low < open_high)
					{
						float dist = attack_range * frac;

						float pitch_high = FLT_MAX;
						float pitch_low = -FLT_MAX;

						pitch_low = (open_low - z) * dist;
						if (pitch_low > bottom_pitch) bottom_pitch = pitch_low;

						pitch_high = (open_high - z) * dist;
						if (pitch_high < top_pitch) top_pitch = pitch_high;
					}

				}

				min_frac = frac;
				min_hit = index;
				min_hit_x = hit_x;
				min_hit_y = hit_y;
				min_hit_z = z;
			}

		}
	}

	if (top_pitch >= bottom_pitch)
	{
		if (r_hitX) *r_hitX = min_hit_x;
		if (r_hitY) *r_hitY = min_hit_y;
		if (r_hitZ) *r_hitZ = min_hit_z;
		if (r_frac) *r_frac = min_frac;

		//return min_hit;
	}
	

	float min_obj_frac = 1.001;
	int min_obj_hit = TRACE_NO_HIT;

	float min_obj_hit_x = end_x;
	float min_obj_hit_y = end_y;
	float min_obj_hit_z = z;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		if (index < 0)
		{
			continue;
		}

		Object* trace_obj = Map_GetObject(index);

		if (trace_obj->hp <= 0 || trace_obj == obj)
		{
			continue;
		}

		float bbox[2][2];
		Math_SizeToBbox(trace_obj->x, trace_obj->y, trace_obj->size, bbox);

		float frac = 0;
		float hit_x = 0;
		float hit_y = 0;

		if (!Math_TraceLineVsBox2(start_x, start_y, end_x, end_y, bbox, &hit_x, &hit_y, &frac))
		{
			continue;
		}

		float dist = frac * attack_range;

		float obj_top = trace_obj->z + 64;

		float obj_top_pitch = (obj_top - z) * dist;
		float obj_bottom_pitch = (trace_obj->z - z) * dist;

		if (obj_top_pitch < bottom_pitch)
		{
			//continue;
		}

		if (obj_bottom_pitch < top_pitch)
		{
			if (z < trace_obj->z)
			{

			}
		//	continue;
		}


		if (frac < min_obj_frac)
		{
			min_obj_frac = frac;
			min_obj_hit = index;
			min_obj_hit_x = hit_x;
			min_obj_hit_y = hit_y;
			min_obj_hit_z = trace_obj->z + 32;
		}

	}

	if (min_obj_hit != TRACE_NO_HIT)
	{
		if (r_hitX) *r_hitX = min_obj_hit_x;
		if (r_hitY) *r_hitY = min_obj_hit_y;
		if (r_hitZ) *r_hitZ = min_obj_hit_z;
		if (r_frac) *r_frac = min_obj_frac;

		return min_obj_hit;
	}



	if (r_hitX) *r_hitX = min_hit_x;
	if (r_hitY) *r_hitY = min_hit_y;
	if (r_hitZ) *r_hitZ = min_hit_z;
	if (r_frac) *r_frac = min_frac;
	
	return min_hit;
}
