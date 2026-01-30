#include "g_common.h"

#include "u_math.h"

#define MAX_TRACE_ITEMS 10000
#define MAX_TRACE_SORT_ITEMS 10000
#define MAX_SPECIAL_LINES 1000
#define MAX_HIT_OBJECTS 5000
#define TRACE_EPSILON 0.125

typedef struct
{
	float frac;
	int index;
} TraceSortItem;

typedef struct
{
	int result_items[MAX_TRACE_ITEMS];

	int special_line_indices[MAX_SPECIAL_LINES];
	int num_hit_special_lines;

	int hit_objects[MAX_HIT_OBJECTS];
	int num_hit_objects;

	TraceSortItem sort_items[MAX_TRACE_SORT_ITEMS];
	int num_sort_items;
} TraceCore;

static TraceCore s_traceCore;

static void Trace_SetupTraceLine(Linedef* trace_line, float start_x, float start_y, float end_x, float end_y)
{
	trace_line->x0 = start_x;
	trace_line->y0 = start_y;
	trace_line->x1 = end_x;
	trace_line->y1 = end_y;
	trace_line->dx = trace_line->x1 - trace_line->x0;
	trace_line->dy = trace_line->y1 - trace_line->y0;
	trace_line->dot = trace_line->dx * trace_line->dx + trace_line->dy * trace_line->dy;
}

static void Trace_ResetSortItems()
{
	s_traceCore.num_sort_items = 0;
}

static void Trace_AddTraceSortItem(float frac, int index)
{
	if (s_traceCore.num_sort_items >= MAX_TRACE_SORT_ITEMS)
	{
		return;
	}
	
	TraceSortItem* sitem = &s_traceCore.sort_items[s_traceCore.num_sort_items++];

	sitem->frac = frac;
	sitem->index = index;
}

static bool Trace_LineIntersectLine(Map* map, Linedef* trace_line, Linedef* line, float* r_hitX, float* r_hitY, float* r_frac)
{
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
	return s_traceCore.hit_objects;
}
bool Trace_CheckBoxPosition(Object* obj, float x, float y, float size, float* r_floorZ, float* r_ceilZ, float* r_lowFloorZ)
{
	s_traceCore.num_hit_special_lines = 0;

	Map* map = Map_GetMap();

	float point[2] = { x, y };

	float bbox[2][2];
	Math_SizeToBbox(x, y, size, bbox);

	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, bbox, MAX_TRACE_ITEMS, s_traceCore.result_items);

	float min_obj_frac = 1.001;
	int min_obj_hit = TRACE_NO_HIT;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//Is a line
		if (index < 0)
		{
			obj->collision_hit = index;

			index = -(index + 1);

			Linedef* line = Map_GetLineDef(index);
			
			int box_side = Line_BoxOnPointSide(line, bbox);

			if (box_side >= 0)
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
				if (obj->type != OT__MISSILE)
				{
					return false;
				}
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

			if (open_high < *r_ceilZ)
			{
				*r_ceilZ = open_high;
			}
			if (open_low > *r_floorZ)
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
					trace_obj->collision_hit = obj->id;
					obj->collision_hit = trace_obj->id;
					return false;
				}
				
			}
		}

	}


	return true;
}

int Trace_FindSlideHit(Object* obj, float start_x, float start_y, float end_x, float end_y, float size, float* best_frac)
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
		bbox[0][1] = start_y - size;
		bbox[1][1] = end_y + size;
	}
	else
	{
		bbox[0][1] = end_y - size;
		bbox[1][1] = start_y + size;
	}

	Linedef vel_line;
	Trace_SetupTraceLine(&vel_line, start_x, start_y, end_x, end_y);

	int min_hit = TRACE_NO_HIT;

	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, bbox, MAX_TRACE_ITEMS, s_traceCore.result_items);

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//Is a line
		if (index < 0)
		{
			int line_index = -(index + 1);

			Linedef* line = Map_GetLineDef(line_index);

			Sector* frontsector = &map->sectors[line->front_sector];
			Sector* backsector = NULL;
			
			if (line->back_sector >= 0)
			{
				backsector = &map->sectors[line->back_sector];
			}
			
			if (Check_CanObjectFitInSector(obj, frontsector, backsector))
			{
				if (!(line->flags & MF__LINE_BLOCKING))
				{
					continue;
				}
			}
			
			if (Line_BoxOnPointSide(line, bbox) >= 0)
			{
				continue;
			}

			int s1 = Line_PointSide(line, vel_line.x0, vel_line.y0);
			int s2 = Line_PointSide(line, vel_line.x1, vel_line.y1);

			if (s1 == s2)
			{
				continue;
			}

			float frac = Line_InterceptLine(&vel_line, line);

			if (frac < 0.0 || frac > 1.0)
			{
				continue;
			}

			if (!backsector)
			{
				if (Line_PointSide(line, start_x, start_y))
				{
					continue;
				}
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
			Object* hit_obj = Map_GetObject(index);

			if (hit_obj->hp > 0 && hit_obj->size > 0 && hit_obj != obj)
			{
				float hit_obj_bbox[2][2];
				Math_SizeToBbox(hit_obj->x, hit_obj->y, hit_obj->size, hit_obj_bbox);

				float frac = 0;

				if (Math_TraceLineVsBox2(vel_line.x0, vel_line.y0, vel_line.x1, vel_line.y1, hit_obj_bbox, NULL, NULL, &frac))
				{
					if (frac < *best_frac)
					{
						min_hit = index;
						*best_frac = frac;
					}
				}

			}
			

		}
	}


	return min_hit;
}

int Trace_AttackLine(Object* obj, float start_x, float start_y, float end_x, float end_y, float z, float range, float* r_hitX, float* r_hitY, float* r_hitZ, float* r_frac)
{
	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Trace(&map->spatial_tree, start_x, start_y, end_x, end_y, MAX_TRACE_ITEMS, s_traceCore.result_items);

	Trace_ResetSortItems();

	Linedef trace_line;
	Trace_SetupTraceLine(&trace_line, start_x, start_y, end_x, end_y);

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//Is a line
		if (index < 0)
		{
			int line_index = -(index + 1);
			Linedef* line = Map_GetLineDef(line_index);

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

				if (open_range > 0 && open_low < open_high)
				{
					//the trace will not hit floor and ceil
					if (open_low < z && open_high > z)
					{
						continue;
					}
				}
			}

			if (frontsector->is_sky && z > frontsector->ceil)
			{
				continue;
			}
			else if (backsector && backsector->is_sky && z > backsector->ceil)
			{
				continue;
			}

			float frac = 0;

			if (!Trace_LineIntersectLine(map, &trace_line, line, NULL, NULL, &frac))
			{
				continue;
			}

			Trace_AddTraceSortItem(frac, index);
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

			if (!Math_TraceLineVsBox2(start_x, start_y, end_x, end_y, bbox, NULL, NULL, &frac))
			{
				continue;
			}

			Trace_AddTraceSortItem(frac, index);
		}
	}

	float min_frac = 1.001;
	int min_hit = TRACE_NO_HIT;

	float top_pitch = -Math_DegToRad(45);
	float bottom_pitch = Math_DegToRad(45);

	for (int i = 0; i < s_traceCore.num_sort_items; i++)
	{
		float min_dist = 1.001;
		bool go_further = true;

		TraceSortItem* min_sort_item = NULL;

		for (int k = 0; k < s_traceCore.num_sort_items; k++)
		{
			TraceSortItem* sort_item = &s_traceCore.sort_items[k];

			if (sort_item->frac < min_dist)
			{
				min_dist = sort_item->frac;
				min_sort_item = sort_item;
			}
		}

		if (!min_sort_item)
		{
			break;
		}
		
		int index = min_sort_item->index;
		float frac = min_sort_item->frac;

		if (index < 0)
		{
			int line_index = -(index + 1);
			Linedef* line = Map_GetLineDef(line_index);

			Sector* frontsector = Map_GetSector(line->front_sector);
			Sector* backsector = NULL;

			if (line->back_sector >= 0)
			{
				backsector = Map_GetSector(line->back_sector);
				float open_low = max(frontsector->floor, backsector->floor);
				float open_high = min(frontsector->ceil, backsector->ceil);
				float open_range = open_high - open_low;

				//closed window
				if (open_range <= 0 || open_low >= open_high)
				{
					go_further = false;
				}
				else
				{
					float line_dist = range * frac;

					if (frontsector->floor != backsector->floor)
					{
						float line_pitch_low = -Math_XY_Angle(line_dist, open_low - z);

						if (line_pitch_low < bottom_pitch)
						{
							bottom_pitch = line_pitch_low;
						}
					}
					if (frontsector->ceil != backsector->ceil)
					{
						float line_pitch_high = -Math_XY_Angle(line_dist, open_high - z);

						if (line_pitch_high > top_pitch)
						{
							top_pitch = line_pitch_high;
						}
					}
					
					if (top_pitch >= bottom_pitch)
					{
						go_further = false;
					}
				}
				
			}
			else
			{
				go_further = false;
			}
		}
		else
		{
			Object* trace_obj = Map_GetObject(index);

			float obj_dist = range * frac;

			float obj_bottom_pitch = -Math_XY_Angle(obj_dist, trace_obj->z - z);
			float obj_top_pitch = -Math_XY_Angle(obj_dist, (trace_obj->z + trace_obj->height) - z);

			if (obj_top_pitch < bottom_pitch && obj_bottom_pitch > top_pitch)
			{
				min_frac = frac;
				min_hit = index;
				break;
			}
		}

		min_sort_item->frac = FLT_MAX;

		if (frac < min_frac)
		{
			min_frac = frac;
			min_hit = index;
		}

		if (!go_further)
		{
			break;
		}
	}
	
	float min_hit_x = end_x;
	float min_hit_y = end_y;
	float min_hit_z = z;

	if (min_hit != TRACE_NO_HIT)
	{
		min_hit_x = trace_line.x0 + trace_line.dx * min_frac;
		min_hit_y = trace_line.y0 + trace_line.dy * min_frac;

		if (min_hit >= 0)
		{
			Object* trace_obj = Map_GetObject(min_hit);

			min_hit_z = trace_obj->z + trace_obj->height * 0.5;
		}
		else
		{
			min_hit_z = z;
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
	int num_traces = BVH_Tree_Cull_Trace(&map->spatial_tree, start_x, start_y, end_x, end_y, MAX_TRACE_ITEMS, s_traceCore.result_items);

	Linedef trace_line;
	Trace_SetupTraceLine(&trace_line, start_x, start_y, end_x, end_y);

	Trace_ResetSortItems();

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//Is a line
		if (index < 0)
		{
			int line_index = -(index + 1);
			Linedef* line = Map_GetLineDef(line_index);

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

				if (open_range > 0 && open_low < open_high)
				{
					//the trace will not hit floor and ceil
					if (open_low < z && open_high > z)
					{
						continue;
					}
				}
			}
			
			float frac = 0;

			if (!Trace_LineIntersectLine(map, &trace_line, line, NULL, NULL, &frac))
			{
				continue;
			}

			Trace_AddTraceSortItem(frac, index);
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
				Trace_AddTraceSortItem(frac, index);
			}	
		}
	}

	for (int i = 0; i < s_traceCore.num_sort_items; i++)
	{
		float min_dist = 1.001;

		TraceSortItem* min_sort_item = NULL;

		for (int k = 0; k < s_traceCore.num_sort_items; k++)
		{
			TraceSortItem* sort_item = &s_traceCore.sort_items[k];

			if (sort_item->frac < min_dist)
			{
				min_dist = sort_item->frac;
				min_sort_item = sort_item;
			}
		}

		if (!min_sort_item)
		{
			break;
		}

		int index = min_sort_item->index;
		float frac = min_sort_item->frac;

		if (index < 0)
		{
			int line_index = -(index + 1);
			Linedef* line = Map_GetLineDef(line_index);

			Sector* frontsector = Map_GetSector(line->front_sector);
			Sector* backsector = NULL;

			if (line->back_sector >= 0)
			{
				backsector = Map_GetSector(line->back_sector);
				float open_low = max(frontsector->floor, backsector->floor);
				float open_high = min(frontsector->ceil, backsector->ceil);
				float open_range = open_high - open_low;

				//closed window
				if (open_range <= 0)
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			Object* trace_obj = Map_GetObject(index);

			if (trace_obj == target)
			{
				return true;
			}
		}

		min_sort_item->frac = FLT_MAX;
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

	Linedef trace_line;
	Trace_SetupTraceLine(&trace_line, start_x, start_y, end_x, end_y);

	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Trace(Map_GetSpatialTree(), start_x, start_y, end_x, end_y, MAX_TRACE_ITEMS, s_traceCore.result_items);

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];
		//ignore objects
		if (index >= 0)
		{
			continue;
		}

		int line_index = -(index + 1);
		Linedef* line = Map_GetLineDef(line_index);

		float frac = 0;
		if (!Trace_LineIntersectLine(map, &trace_line, line, NULL, NULL, &frac))
		{
			continue;
		}

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

		if (frac <= min_frac)
		{
			min_frac = frac;
			min_hit = index;
		}
	}

	if (min_hit != TRACE_NO_HIT)
	{
		int line_index = -(min_hit + 1);
		Linedef* line = Map_GetLineDef(line_index);

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

	int num_traces = BVH_Tree_Cull_Box(Map_GetSpatialTree(), bbox, MAX_TRACE_ITEMS, s_traceCore.result_items);

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
	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, sector->bbox, MAX_TRACE_ITEMS, s_traceCore.result_items);

	int num_collisions = 0;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		//ignore lines
		if (index < 0)
		{
			continue;
		}
		
		if (num_collisions >= MAX_HIT_OBJECTS)
		{
			printf("Reached max hit object limit\n");
			break;
		}

		s_traceCore.hit_objects[num_collisions++] = index;
	}

	return num_collisions;
}

int Trace_SectorLines(Sector* sector, bool front_only)
{
	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Box(&map->spatial_tree, sector->bbox, MAX_TRACE_ITEMS, s_traceCore.result_items);

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
		Linedef* line = Map_GetLineDef(line_index);

		if (num_collisions >= MAX_HIT_OBJECTS)
		{
			printf("Reached max hit object limit\n");
			break;
		}

		if (front_only)
		{
			if (line->front_sector == sector->index)
			{
				s_traceCore.hit_objects[num_collisions++] = line_index;
			}
		}
		else
		{
			if (line->back_sector == sector->index || line->front_sector == sector->index)
			{
				s_traceCore.hit_objects[num_collisions++] = line_index;
			}
		}
	}

	return num_collisions;
}

int Trace_SectorAll(Sector* sector)
{
	int num_traces = BVH_Tree_Cull_Box(Map_GetSpatialTree(), sector->bbox, MAX_TRACE_ITEMS, s_traceCore.result_items);

	int num_collisions = 0;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];

		if (num_collisions >= MAX_HIT_OBJECTS)
		{
			printf("Reached max hit object limit\n");
			break;
		}

		//is line
		if (index < 0)
		{
			int line_index = -(index + 1);
			Linedef* line = Map_GetLineDef(line_index);

			if (line->back_sector == sector->index || line->front_sector == sector->index)
			{
				s_traceCore.hit_objects[num_collisions++] = index;
			}
		}
		else
		{
			//object
			Object* trace_obj = Map_GetObject(index);

			if (trace_obj->sector_index == sector->index)
			{
				s_traceCore.hit_objects[num_collisions++] = index;
			}
		}
	}

	return num_collisions;
}
int Trace_FindLine(float start_x, float start_y, float start_z, float end_x, float end_y, float end_z, bool ignore_sky_plane, int* r_hits, int max_hit_count, float* r_hitX, float* r_hitY, float* r_hitZ, float* r_frac)
{
	Map* map = Map_GetMap();
	int num_traces = BVH_Tree_Cull_Trace(&map->spatial_tree, start_x, start_y, end_x, end_y, max_hit_count, r_hits);

	Linedef trace_line;
	Trace_SetupTraceLine(&trace_line, start_x, start_y, end_x, end_y);

	float dz = end_z - start_z;

	float min_frac = 1.001;
	int min_hit = TRACE_NO_HIT;
	float min_hit_x = FLT_MAX;
	float min_hit_y = FLT_MAX;
	float min_hit_z = FLT_MAX;

	for (int i = 0; i < num_traces; i++)
	{
		int index = r_hits[i];

		//ignore objects
		if (index >= 0)
		{
			continue;
		}

		int line_index = -(index + 1);
		Linedef* line = Map_GetLineDef(line_index);

		Sector* frontsector = &map->sectors[line->front_sector];
		Sector* backsector = NULL;

		if (line->back_sector >= 0)
		{
			backsector = &map->sectors[line->back_sector];
		}

		float hit_x = 0;
		float hit_y = 0;
		float frac = 1;

		int s1 = Line_PointSide(line, trace_line.x0, trace_line.y0);
		int s2 = Line_PointSide(line, trace_line.x1, trace_line.y1);

		if (s1 == s2)
		{
			continue;
		}

		frac = Line_InterceptLine(&trace_line, line);

		if (frac < 0.0 || frac > 1.0)
		{
			continue;
		}

		if (!Trace_LineIntersectLine(map, &trace_line, line, &hit_x, &hit_y, &frac))
		{
			continue;
		}

		float z = start_z + dz * frac;

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
					int side = Line_PointSide(line, start_x, start_y);
					Sidedef* sidedef = Map_GetSideDef(line->sides[side]);
					
					Sector* side_sector = frontsector;

					if (side == 1)
					{
						side_sector = backsector;
					}

					//check for middle texture
					if (sidedef->middle_texture)
					{
						int tx = 0;
						int ty = (z - side_sector->floor) / (side_sector->ceil - side_sector->floor);

						float u = 0;
						float t = 0;

						float dx = line->x0 - line->x1;
						float dy = line->y0 - line->y1;

						float texwidth = line->width * 2;

						if (fabs(dx) > fabs(dy))
						{
							t = (hit_x - line->x1) / dx;
						}
						else
						{
							t = (hit_y - line->y1) / dy;
						}
						u = t + u * t;
						u = fabs(u);
						u *= texwidth;

						tx = u;

						tx += sidedef->x_offset;
						ty += sidedef->y_offset;

						unsigned char* texture_sample = Image_Get(&sidedef->middle_texture->img, tx & sidedef->middle_texture->width_mask, ty & sidedef->middle_texture->height_mask);

						if (texture_sample[3] < 127)
						{
							continue;
						}
					}
					else
					{
						continue;
					}
				}
			}
		}

		if (ignore_sky_plane)
		{
			if (frontsector->is_sky && z > frontsector->ceil)
			{
				continue;
			}
			else if (backsector && backsector->is_sky && z > backsector->ceil)
			{
				continue;
			}
		}

		if (frac < min_frac)
		{
			min_frac = frac;
			min_hit_x = hit_x;
			min_hit_y = hit_y;
			min_hit_z = z;
			min_hit = line_index;
		}
	}

	if (r_frac) *r_frac = min_frac;
	if (r_hitX) *r_hitX = min_hit_x;
	if (r_hitY) *r_hitY = min_hit_y;
	if (r_hitZ) *r_hitZ = min_hit_z;

	return min_hit;
}

int Trace_FindSectors(int ignore_sector_index, float bbox[2][2])
{
	int num_traces = BVH_Tree_Cull_Box(Map_GetSpatialTree(), bbox, MAX_TRACE_ITEMS, s_traceCore.result_items);

	int prev_sector_added = -1;
	int num_sectors = 0;

	for (int i = 0; i < num_traces; i++)
	{
		int index = s_traceCore.result_items[i];
		
		//ignore objects
		if (index >= 0)
		{
			continue;
		}

		index = -(index + 1);

		Linedef* line = Map_GetLineDef(index);

		if (Line_BoxOnPointSide(line, bbox) >= 0)
		{
			continue;
		}

		Sector* frontsector = Map_GetSector(line->front_sector);
		Sector* backsector = NULL;

		if (line->back_sector >= 0)
		{
			backsector = Map_GetSector(line->back_sector);
		}

		if (num_sectors < MAX_HIT_OBJECTS && ignore_sector_index != frontsector->index && frontsector->index != prev_sector_added && Math_BoxIntersectsBox(frontsector->bbox, bbox))
		{
			bool match = false;
			for (int k = 0; k < num_sectors; k++)
			{
				if (s_traceCore.hit_objects[k] == frontsector->index)
				{
					match = true;
					break;
				}
			}
			if (!match)
			{
				s_traceCore.hit_objects[num_sectors++] = frontsector->index;
				prev_sector_added = frontsector->index;
			}

		}
		if (backsector)
		{
			if (num_sectors < MAX_HIT_OBJECTS && ignore_sector_index != backsector->index && backsector->index != prev_sector_added && Math_BoxIntersectsBox(backsector->bbox, bbox))
			{
				bool match = false;
				for (int k = 0; k < num_sectors; k++)
				{
					if (s_traceCore.hit_objects[k] == backsector->index)
					{
						match = true;
						break;
					}
				}
				if (!match)
				{
					s_traceCore.hit_objects[num_sectors++] = backsector->index;
					prev_sector_added = backsector->index;
				}
			}
		}	
	}

	return num_sectors;
}
