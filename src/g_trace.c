#include "g_common.h"

#include "u_math.h"

#define MAX_TRACE_ITEMS 100000
#define MAX_SPECIAL_LINES 1000
#define TRACE_EPSILON 0.125

typedef struct
{
	int result_items[MAX_TRACE_ITEMS];
	int special_line_indices[MAX_SPECIAL_LINES];
	int num_hit_special_lines;
} TraceCore;

static TraceCore s_traceCore;

static void Register_TraceItem(int _data_index, BVH_ID _index, int _hit_count)
{
	s_traceCore.result_items[_hit_count] = _data_index;
}

static void Trace_SetupTraceLine(Line* trace_line, float start_x, float start_y, float end_x, float end_y)
{
	trace_line->x0 = start_x;
	trace_line->y0 = start_y;
	trace_line->x1 = end_x;
	trace_line->y1 = end_y;
	trace_line->dx = trace_line->x1 - trace_line->x0;
	trace_line->dy = trace_line->y1 - trace_line->y0;
	trace_line->dot = trace_line->dx * trace_line->dx + trace_line->dy * trace_line->dy;
}

static bool Trace_LineIntersectLine(Map* map, Line* trace_line, Line* line, float* r_hitX, float* r_hitY, float* r_frac)
{
	Sector* frontsector = &map->sectors[line->front_sector];
	Sector* backsector = NULL;

	if (line->back_sector >= 0)
	{
		backsector = &map->sectors[line->back_sector];
	}

	int s1 = Line_PointSide(trace_line, line->x0, line->y0);
	int s2 = Line_PointSide(trace_line, line->x1, line->y1);

	if (s1 == s2)
	{
		return false;
	}

	float frac = 0;
	float hit_x = 0;
	float hit_y = 0;

	if (!Line_SegmentInterceptSegmentLine(trace_line, line, &frac, &hit_x, &hit_y))
	{
		return false;
	}

	if (r_hitX) *r_hitX = hit_x;
	if (r_hitY) *r_hitY = hit_y;
	if (r_frac) *r_frac = frac;

	return true;
}

static bool Trace_LinePassThruSectorCheck(Sector* frontsector, Sector* backsector, float z)
{
	if (backsector)
	{
		float open_low = max(frontsector->floor, backsector->floor);
		float open_high = min(frontsector->ceil, backsector->ceil);
		float open_range = open_high - open_low;

		if (open_range > 0 && open_low < open_high)
		{
			//the trace will not hit floor and ceil
			if (open_low < z && open_high > z)
			{
				return true;
			}
		}
	}

	return false;
}


int* Trace_GetSpecialLines(int* r_length)
{
	*r_length = s_traceCore.num_hit_special_lines;

	return s_traceCore.special_line_indices;
}

int* Trace_GetHitObjects()
{
	return s_traceCore.result_items;
}

bool Trace_CheckBoxPosition(Object* obj, float x, float y, float size, float* r_floorZ, float* r_ceilZ, float* r_lowFloorZ)
{
	s_traceCore.num_hit_special_lines = 0;

	Map* map = Map_GetMap();

	float bbox[2][2];
	Math_SizeToBbox(x, y, size, bbox);

	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, bbox, MAX_TRACE_ITEMS, Register_TraceItem);

	float min_obj_frac = 1.001;
	int min_obj_hit = TRACE_NO_HIT;

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

			if (line->flags & MF__LINE_BLOCKING)
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

			if (line->special > 0 && s_traceCore.num_hit_special_lines < MAX_SPECIAL_LINES)
			{
				s_traceCore.special_line_indices[s_traceCore.num_hit_special_lines++] = index;
			}

		}
		//otherwise a object
		else
		{
			if (!(obj->flags & OBJ_FLAG__DONT_COLLIDE_WITH_OBJECTS) && index != obj->id)
			{
				Object* trace_obj = Map_GetObject(index);

				if (!Object_HandleObjectCollision(obj, trace_obj))
				{
					trace_obj->col_object = obj;
					obj->col_object = trace_obj;
					return false;
				}
				
			}
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

	float min_obj_frac = 1.001;
	int min_obj_hit = TRACE_NO_HIT;
	float min_obj_hit_x = end_x;
	float min_obj_hit_y = end_y;
	float min_obj_hit_z = z;

	Line trace_line;
	Trace_SetupTraceLine(&trace_line, start_x, start_y, end_x, end_y);

	float attack_range = 10000;
	float top_pitch = Math_DegToRad(35.0);
	float bottom_pitch = -Math_DegToRad(35.0);

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

			float frac = 0;
			float hit_x = 0;
			float hit_y = 0;

			if (!Trace_LineIntersectLine(map, &trace_line, line, &hit_x, &hit_y, &frac))
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
					//the trace will not hit floor and ceil
					if (open_low < z && open_high > z)
					{
						continue;
					}
				}
			}

			if (frac < min_frac)
			{
				min_frac = frac;
				min_hit = index;
				min_hit_x = hit_x;
				min_hit_y = hit_y;
				min_hit_z = z;
			}

		}
		else
		{
			Object* trace_obj = Map_GetObject(index);

			//skip dead and shooter obj
			if (trace_obj->hp <= 0 || trace_obj == obj)
			{
				continue;
			}

			if (trace_obj->type != OT__MONSTER)
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

			float obj_dist = attack_range * min_obj_frac;
			if (obj_dist == 0) obj_dist = 0.001;
			float obj_bottom_pitch = (trace_obj->z - z) / obj_dist;
			float obj_top_pitch = ((trace_obj->z + 64) - z) / obj_dist;


			if (frac < min_obj_frac)
			{
				min_obj_frac = frac;
				min_obj_hit = index;
				min_obj_hit_x = hit_x;
				min_obj_hit_y = hit_y;
				min_obj_hit_z = trace_obj->z + 44;
			}
		}
	}

	if (min_obj_hit != TRACE_NO_HIT)
	{
		if (min_hit != TRACE_NO_HIT && min_hit < 0)
		{
			Line* line = &map->line_segs[-(min_hit + 1)];

			Sector* frontsector = &map->sectors[line->front_sector];
			Sector* backsector = NULL;

			if (line->back_sector >= 0)
			{
				backsector = &map->sectors[line->back_sector];
			}

			if (backsector)
			{
				Object* trace_obj = Map_GetObject(min_obj_hit);

				//if backsector
				//check if line is below the view pitch
				float open_low = max(frontsector->floor, backsector->floor);
				float open_high = min(frontsector->ceil, backsector->ceil);
				float open_range = open_high - open_low;

				if (open_range > 0)
				{
					float line_dist = attack_range * min_frac;
					if (line_dist == 0) line_dist = 0.001;

					float obj_dist = attack_range * min_obj_frac;
					if (obj_dist == 0) obj_dist = 0.001;
				
					float line_pitch_low = (open_low - z) / line_dist;
					float line_pitch_high = (open_high - z) / line_dist;

					float obj_bottom_pitch = (trace_obj->z - z) / obj_dist;
					float obj_top_pitch = ((trace_obj->z + 64) - z) / obj_dist;

					if (obj_bottom_pitch < line_pitch_high && line_pitch_high < Math_DegToRad(35.0))
					{
						min_hit = min_obj_hit;
						min_frac = min_obj_frac;
						min_hit_x = min_obj_hit_x;
						min_hit_y = min_obj_hit_y;
						min_hit_z = min_obj_hit_z;
					}
					else if (obj_top_pitch > line_pitch_low && line_pitch_low > Math_DegToRad(35.0))
					{
						min_hit = min_obj_hit;
						min_frac = min_obj_frac;
						min_hit_x = min_obj_hit_x;
						min_hit_y = min_obj_hit_y;
						min_hit_z = min_obj_hit_z;
					}
				}
				
			}
			else
			{
				//no backsector
				//just check if obj is closer
				if (min_obj_frac < min_frac)
				{
					min_hit = min_obj_hit;
					min_frac = min_obj_frac;
					min_hit_x = min_obj_hit_x;
					min_hit_y = min_obj_hit_y;
					min_hit_z = min_obj_hit_z;
				}
			}
		}
		else
		{
			//no line was hit at all, so the object is the closest
			min_hit = min_obj_hit;
			min_frac = min_obj_frac;
			min_hit_x = min_obj_hit_x;
			min_hit_y = min_obj_hit_y;
			min_hit_z = min_obj_hit_z;
		}
	}


	if (r_hitX) *r_hitX = min_hit_x;
	if (r_hitY) *r_hitY = min_hit_y;
	if (r_hitZ) *r_hitZ = min_hit_z;
	if (r_frac) *r_frac = min_frac;
	
	return min_hit;
}

bool Trace_CheckLineToTarget(Object* obj, Object* target)
{
	float z = obj->z;
	float start_x = obj->x;
	float start_y = obj->y;

	float end_x = target->x;
	float end_y = target->y;

	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Trace(&map->spatial_tree, start_x, start_y, end_x, end_y, MAX_TRACE_ITEMS, Register_TraceItem);

	float min_line_frac = 1.001;
	int min_line_hit = TRACE_NO_HIT;

	float min_target_frac = 1.001;
	int min_target_hit = TRACE_NO_HIT;

	Line trace_line;
	trace_line.x0 = start_x;
	trace_line.y0 = start_y;
	trace_line.x1 = end_x;
	trace_line.y1 = end_y;
	trace_line.dx = trace_line.x1 - trace_line.x0;
	trace_line.dy = trace_line.y1 - trace_line.y0;
	trace_line.dot = trace_line.dx * trace_line.dx + trace_line.dy * trace_line.dy;

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

			float frac = 0;
			float hit_x = 0;
			float hit_y = 0;

			if (!Trace_LineIntersectLine(map, &trace_line, line, &hit_x, &hit_y, &frac))
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
					//the trace will not hit floor and ceil
					if (open_low < z && open_high > z)
					{
						continue;
					}
				}

			}

			if (frac < min_line_frac)
			{
				min_line_frac = frac;
				min_line_hit = index;
			}

		}
		else
		{
			if (index == target->id)
			{
				Object* trace_obj = Map_GetObject(index);

				float bbox[2][2];
				Math_SizeToBbox(trace_obj->x, trace_obj->y, trace_obj->size, bbox);

				float frac = 0;
	
				if (!Math_TraceLineVsBox2(start_x, start_y, end_x, end_y, bbox, NULL, NULL, &frac))
				{
					return false;
				}

				min_target_frac = frac;
				min_target_hit = index;
			}
			
		}
	}

	if (min_target_hit != TRACE_NO_HIT && min_target_hit == target->id)
	{
		if (min_line_hit != TRACE_NO_HIT && min_line_hit < 0)
		{
			Line* line = &map->line_segs[-(min_line_hit + 1)];

			Sector* frontsector = &map->sectors[line->front_sector];
			Sector* backsector = NULL;

			if (line->back_sector >= 0)
			{
				backsector = &map->sectors[line->back_sector];
			}

			if (backsector)
			{
				Object* trace_obj = Map_GetObject(min_target_hit);

				//if backsector
				float open_low = max(frontsector->floor, backsector->floor);
				float open_high = min(frontsector->ceil, backsector->ceil);
				float open_range = open_high - open_low;

				if (open_range > 0)
				{
					return true;
				}
			}
			else
			{
				//no backsector
				//just check if obj is closer
				if (min_target_frac < min_line_frac)
				{
					return true;
				}
			}
		}
		else
		{
			//no line was hit at all, so the object is the closest
			return true;
		}
	}

	return false;
}

int Trace_FindSpecialLine(float start_x, float start_y, float end_x, float end_y, float z)
{
	float min_frac = 1.001;
	int min_hit = TRACE_NO_HIT;
	float min_hit_x = end_x;
	float min_hit_y = end_y;
	float min_hit_z = z;

	Line trace_line;
	Trace_SetupTraceLine(&trace_line, start_x, start_y, end_x, end_y);

	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Trace(Map_GetSpatialTree(), start_x, start_y, end_x, end_y, MAX_TRACE_ITEMS, Register_TraceItem);

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];
		//ignore objects
		if (index >= 0)
		{
			continue;
		}

		int line_index = -(index + 1);
		Line* line = Map_GetLine(line_index);

		float frac = 0;
		if (!Trace_LineIntersectLine(map, &trace_line, line, NULL, NULL, &frac))
		{
			continue;
		}

		//check if non special line intercepts the trace
		if (line->special <= 0)
		{
			Sector* frontsector = &map->sectors[line->front_sector];
			Sector* backsector = NULL;

			if (line->back_sector >= 0)
			{
				backsector = &map->sectors[line->back_sector];
			}

			if (backsector)
			{
				float open_low = max(frontsector->floor, backsector->floor);
				float open_high = min(frontsector->ceil, backsector->ceil);
				float open_range = open_high - open_low;

				//the trace will not hit floor and ceil
				if (open_range > 0 && open_low < z && open_high > z)
				{
					continue;
				}
			}
		}

		if (frac < min_frac)
		{
			min_frac = frac;
			min_hit = index;
		}
	}

	if (min_hit != TRACE_NO_HIT)
	{
		int line_index = -(min_hit + 1);
		Line* line = Map_GetLine(line_index);

		//closest line was not special
		if (line->special <= 0)
		{
			return TRACE_NO_HIT;
		}
	}

	return min_hit;
}

int Trace_AreaObjects(Object* obj, float x, float y, float size)
{
	float bbox[2][2];
	Math_SizeToBbox(x, y, size, bbox);

	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, bbox, MAX_TRACE_ITEMS, Register_TraceItem);

	int num_collisions = 0;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//ignore lines and self
		if (index < 0 || index == obj->id)
		{
			continue;
		}

		Object* trace_obj = Map_GetObject(index);

		//check direct line
		if (!Trace_CheckLineToTarget(obj, trace_obj))
		{
			continue;
		}
	
		//handle collision
		if (!Object_HandleObjectCollision(obj, trace_obj))
		{
			num_collisions++;
		}

	}

	return num_collisions;
}

int Trace_SectorObjects(Sector* sector)
{
	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, sector->bbox, MAX_TRACE_ITEMS, Register_TraceItem);

	int num_collisions = 0;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//ignore lines
		if (index < 0)
		{
			continue;
		}

		Object* trace_obj = Map_GetObject(index);

		if (trace_obj->hp <= 0)
		{
			continue;
		}
		
		s_traceCore.result_items[num_collisions++] = index;
	}

	return num_collisions;
}

int Trace_SectorLines(Sector* sector)
{
	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, sector->bbox, MAX_TRACE_ITEMS, Register_TraceItem);

	int num_collisions = 0;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//ignore objects
		if (index >= 0)
		{
			continue;
		}

		int line_index = -(index + 1);
		Line* line = &map->line_segs[line_index];

		if (line->back_sector == sector->index || line->front_sector == sector->index)
		{
			s_traceCore.result_items[num_collisions++] = line_index;
		}
	}

	return num_collisions;
}
