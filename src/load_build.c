#include "g_common.h"
#include "engine_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct
{
	short wallptr, wallnum;
	long ceilingz, floorz;
	short ceilingstat, floorstat;
	short ceilingpicnum, ceilingheinum;
	signed char ceilingshade;
	char ceilingpal, ceilingxpanning, ceilingypanning;
	short floorpicnum, floorheinum;
	signed char floorshade;
	char floorpal, floorxpanning, floorypanning;
	char visibility, filler;
	short lotag, hitag, extra;
}sectortype;

typedef struct
{
	long x, y;
	short point2, nextwall, nextsector, cstat;
	short picnum, overpicnum;
	signed char shade;
	char pal, xrepeat, yrepeat, xpanning, ypanning;
	short lotag, hitag, extra;
} walltype;

static float scaleLongToFloat(long l, float s)
{
	float result = (l / s);

	int sign = (l >= 0) ? 1 : -1;

	return result * sign;
}
static float scaleLongToFloatLimit(long limit, long l, float s)
{
	float result = ((limit - l) / s);

	int sign = (l >= 0) ? 1 : -1;

	return result;
}
#define sign(v) (((v) > 0) - ((v) < 0))    // sign: Return the sign of a value (-1, 0 or 1)
#define vxs(x0,y0, x1,y1) ((x0)*(y1) - (x1)*(y0)) // vxs: Vector cross product
// Overlap:  Determine whether the two number ranges overlap.
#define Overlap(a0,a1,b0,b1) (min(a0,a1) <= max(b0,b1) && min(b0,b1) <= max(a0,a1))
// IntersectBox: Determine whether two 2D-boxes intersect.
#define IntersectBox(x0,y0, x1,y1, x2,y2, x3,y3) (Overlap(x0,x1,x2,x3) && Overlap(y0,y1,y2,y3))
// PointSide: Determine which side of a line the point is on. Return value: -1, 0 or 1.
#define PointSide(px,py, x0,y0, x1,y1) sign(vxs((x1)-(x0), (y1)-(y0), (px)-(x0), (py)-(y0)))
// Intersect: Calculate the point of intersection between two lines.
#define Intersect(x1,y1, x2,y2, x3,y3, x4,y4) ((Vertex) { \
    vxs(vxs(x1,y1, x2,y2), (x1)-(x2), vxs(x3,y3, x4,y4), (x3)-(x4)) / vxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)), \
    vxs(vxs(x1,y1, x2,y2), (y1)-(y2), vxs(x3,y3, x4,y4), (y3)-(y4)) / vxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)) })


static int PointOnSide(float p_x, float p_y, float s_x, float s_y, float s_dx, float s_dy)
{
	if (!s_dx)
	{
		if (p_x > s_x - 2.0 && p_x < s_x + 2.0)
		{
			return -1;
		}
		if (p_x < s_x)
		{
			return s_dy > 0;
		}
		return s_dy < 0;
	}
	if (!s_dy)
	{
		if (p_y > s_y - 2.0 && p_y < s_y + 2.0)
		{
			return -1;
		}
		if (p_y < s_y)
		{
			return s_dx < 0;
		}
		return s_dx > 0;
	}

	float dx = s_x - p_x;
	float dy = s_y - p_y;

	float a = s_dx * s_dx + s_dy * s_dy;
	float b = 2.0 * (s_dx * dx + s_dy * dy);
	float c = dx * dx + dy * dy - 2.0 * 2.0;
	float d = b * b - 4 * a * c;

	if (d > 0)
	{
		return -1;
	}

	dx = p_x - s_x;
	dy = p_y - s_y;

	float left = s_dy * dx;
	float right = dy * s_dx;

	if (fabs(left - right) < 0.5)
	{
		return -1;
	}
	if (right < left)
	{
		return 0;
	}

	return 1;
}
static int LineOnSide(Line* l1, Line* split_line)
{
	int s1 = PointOnSide(l1->x0, l1->y0, split_line->x0, split_line->y0, split_line->dx, split_line->dy);
	int s2 = PointOnSide(l1->x1, l1->y1, split_line->x0, split_line->y0, split_line->dx, split_line->dy);

	float dx = 0;
	float dy = 0;

	if (s1 == s2)
	{
		if (s1 == -1)
		{
			dx = l1->dx;
			dy = l1->dy;

			if (sign(dx) == sign(split_line->dx) && sign(dy) == sign(split_line->dy))
			{
				return 0;
			}

			return 1;
		}
		return s1;
	}

	if (s1 == -1)
	{
		return s2;
	}
	if (s2 == -1)
	{
		return s1;
	}

	return -2;
}

static int GradeSplit(Line* lines, int num, Line* split_line, int best_grade)
{
	int side = 0;
	int front_count = 0;
	int back_count = 0;
	int grade = 0;

	for (int i = 0; i < num; i++)
	{
		Line* line = &lines[i];

		if (line == split_line)
		{
			side = 0;
		}
		else
		{
			side = LineOnSide(line, split_line);
		}

		switch (side)
		{
		case 0:
		{
			front_count++;
			break;
		}
		case 1:
		{
			back_count++;
			break;
		}
		case -2:
		{
			front_count++;
			back_count++;
			break;
		}
		default:
			break;
		}

		int max = max(front_count, back_count);
		int new_c = (front_count + back_count) - num;
		grade = max + new_c *  1;

		if (grade > best_grade)
		{
			return grade;
		}
	}

	if (front_count == 0 || back_count == 0)
	{
		return INT_MAX;
	}

	return grade;
}

static Line CutLine(Line* line, Line* split_line)
{
	Line new_line = *line;

	float den = line->dy * split_line->dx - line->dx * split_line->dy;
	float num = (line->x0 - split_line->x0) * line->dy + (split_line->y0 - line->y0) * line->dx;
	float frac = num / den;

	float intersect_x = line->x0 + round(line->dx * frac);
	float intersect_y = line->y0 + round(line->dy * frac);

	float offset = line->offset + round(frac * sqrt(line->dx * line->dx + line->dy * line->dy));
	int side = PointOnSide(line->x0, line->y0, split_line->x0, split_line->y0, split_line->dx, split_line->dy);

	if (side == 0)
	{
		line->x1 = intersect_x;
		line->y1 = intersect_y;

		new_line.x0 = intersect_x;
		new_line.y0 = intersect_y;
		new_line.offset = offset;
	}
	else
	{
		line->x0 = intersect_x;
		line->y0 = intersect_y;
		line->offset = offset;

		new_line.x1 = intersect_x;
		new_line.y1 = intersect_y;
	}

	return new_line;
}

static void SplitLines(Line* lines, int num, Line* split_line, Line** front, int* r_numFront, Line** back, int* r_numBack)
{
	int num_front = 0;
	int num_back = 0;
	for (int i = 0; i < num; i++)
	{
		Line* test_line = &lines[i];

		int side = 0;

		if (test_line == split_line)
		{
			side = 0;
		}
		else
		{
			side = LineOnSide(test_line, split_line);
		}
		switch (side)
		{
		case 0:
		{
			num_front++;
			break;
		}
		case 1:
		{
			num_back++;
			break;
		}
		case -2:
		{
			num_front++;
			num_back++;
			break;
		}
		default:
			break;
		}
	}

	Line* f_lines = calloc(num_front, sizeof(Line));
	Line* b_lines = calloc(num_back, sizeof(Line));

	num_front = 0;
	num_back = 0;

	for (int i = 0; i < num; i++)
	{
		Line* test_line = &lines[i];

		int side = 0;

		if (test_line == split_line)
		{
			side = 0;
		}
		else
		{
			side = LineOnSide(test_line, split_line);
		}
		switch (side)
		{
		case 0:
		{
			f_lines[num_front++] = *test_line;
			break;
		}
		case 1:
		{
			b_lines[num_back++] = *test_line;
			break;
		}
		case -2:
		{
			f_lines[num_front++] = *test_line;

			Line new_line = CutLine(test_line, split_line);

			b_lines[num_back++] = new_line;
			break;
		}
		default:
			break;
		}
	}

	*front = f_lines;
	*back = b_lines;
	*r_numBack = num_back;
	*r_numFront = num_front;
}

static BSPNode* BuildBSPRecursive(Line* lines, int num, Sector* sector, Map* map)
{
	BSPNode* node = calloc(1, sizeof(BSPNode));

	if (!node || num <= 0)
	{
		return NULL;
	}
	
	int best_grade = INT_MAX;
	Line* best_line = NULL;
	int step = 1;
	for (int i = 0; i < num; i += step)
	{
		Line* test_line = &lines[i];

		int grade = GradeSplit(lines, num, test_line, best_grade);

		if (grade < best_grade)
		{
			best_grade = grade;
			best_line = test_line;
		}

	}

	if (best_grade == INT_MAX)
	{
		node->lines = lines;
		node->num_lines = num;

		return node;
	}

	assert(best_line);

	node->line_x = best_line->x0;
	node->line_y = best_line->y0;
	node->line_dx = best_line->dx;
	node->line_dy = best_line->dy;

	Line* front = NULL;
	Line* back = NULL;
	int num_front = 0;
	int num_back = 0;
	SplitLines(lines, num, best_line, &front, &num_front, &back, &num_back);

	node->children[0] = BuildBSPRecursive(front, num_front, sector, map);
	node->children[1] = BuildBSPRecursive(back, num_back, sector, map);

	return node;
}

static Line* setupLines(Sector* sector, Map* map, int* r_numLines)
{
	//count nun lines
	int num_lines = 0;
	for (int i = 0; i < sector->npoints; i++)
	{
		Vertex* v0 = &map->vertices[sector->offset + i];
		Vertex* v1 = &map->vertices[v0->next];

		num_lines++;
	}

	Line* lines = calloc(num_lines, sizeof(Line));

	if (!lines)
	{
		*r_numLines = 0;
		return NULL;
	}

	for (int i = 0; i < sector->npoints; i++)
	{
		Vertex* v0 = &map->vertices[sector->offset + i];
		Vertex* v1 = &map->vertices[v0->next];

		Line* line = &lines[i];

		line->x0 = v0->x;
		line->y0 = v0->y;

		line->x1 = v1->x;
		line->y1 = v1->y;

		line->dx = v1->x - v0->x;
		line->dy = v1->y - v0->y;
	}

	*r_numLines = num_lines;

	return lines;
}

static void ProcessSubsector(BSPNode* node, float box[2][2])
{
	for (int i = 0; i < node->num_lines; i++)
	{
		Line* l = &node->lines[i];

		box[0][0] = min(box[0][0], l->x0);
		box[0][1] = min(box[0][1], l->y0);
		box[0][0] = min(box[0][0], l->x1);
		box[0][1] = min(box[0][1], l->y1);

		box[1][0] = max(box[1][0], l->x0);
		box[1][1] = max(box[1][1], l->y0);
		box[1][0] = max(box[1][0], l->x1);
		box[1][1] = max(box[1][1], l->y1);
	}
}
static void ProcessNode(BSPNode* node, float totalbox[2][2])
{
	if (!node)
	{
		return;
	}

	float sub_box_left[2][2];
	float sub_box_right[2][2];

	memset(sub_box_left, 0, sizeof(sub_box_left));
	memset(sub_box_right, 0, sizeof(sub_box_right));

	if (node->lines)
	{
		ProcessSubsector(node, totalbox);
		return;
	}

	ProcessNode(node->children[0], sub_box_left);
	for (int i = 0; i < 2; i++)
	{
		for (int k = 0; k < 2; k++)
		{
			node->box_left[i][k] = sub_box_left[i][k];
		}		
	}

	ProcessNode(node->children[1], sub_box_right);
	for (int i = 0; i < 2; i++)
	{
		for (int k = 0; k < 2; k++)
		{
			node->box_right[i][k] = sub_box_right[i][k];
		}
	}

	totalbox[0][0] = min(sub_box_left[0][0], sub_box_right[0][0]);
	totalbox[0][1] = min(sub_box_left[0][1], sub_box_right[0][1]);

	totalbox[1][0] = max(sub_box_left[1][0], sub_box_right[1][0]);
	totalbox[1][1] = max(sub_box_left[1][1], sub_box_right[1][1]);
}

static void BuildBSPForSector(Sector* sector, Map* map)
{
	int num_lines = 0;
	Line* lines = setupLines(sector, map, &num_lines);

	if (!lines)
	{
		return;
	}

	BSPNode* node = BuildBSPRecursive(lines, num_lines, sector, map);
	
	float total_box[2][2];
	memset(total_box, 0, sizeof(total_box));
	ProcessNode(node, total_box);

	sector->node = node;
}



bool Load_BuildMap(const char* filename, Map* map)
{
	bool result = true;

	FILE* file = fopen(filename, "rb");

	if (!file)
	{
		return false;
	}
	sectortype* sectors = NULL;
	walltype* walls = NULL;

	long mapversion = 0;
	fread(&mapversion, sizeof(mapversion), 1, file);
	
	long posx, posy, posz;
	short ang, cursectnum, numsectors, numwalls, numsprites;

	fread(&posx, sizeof(posx), 1, file);
	fread(&posy, sizeof(posy), 1, file);
	fread(&posz, sizeof(posz), 1, file);
	fread(&ang, sizeof(ang), 1, file);
	fread(&cursectnum, sizeof(cursectnum), 1, file);

	//read sectors
	fread(&numsectors, sizeof(numsectors), 1, file);
	sectors = malloc(sizeof(sectortype) * numsectors);
	if (!sectors)
	{
		result = false;
		goto cleanup;
	}
	fread(sectors, sizeof(sectortype), numsectors, file);

	//read walls;
	fread(&numwalls, sizeof(numwalls), 1, file);
	walls = malloc(sizeof(walltype) * numwalls);
	if (!walls)
	{
		result = false;
		goto cleanup;
	}
	fread(walls, sizeof(walltype), numwalls, file);

	map->player_spawn_point_x = posx >> 8;
	map->player_spawn_point_y = posy >> 8;
	map->player_spawn_point_z = posz >> 8;
	map->player_spawn_rot = ang;
	map->player_spawn_sector = (cursectnum >= 0) ? cursectnum : 0;

	map->NumSectors = numsectors;

	map->sectors = malloc(sizeof(Sector) * numsectors);

	float mins[2];
	float maxs[2];

	for (int i = 0; i < 2; i++)
	{
		mins[i] = 0 >> 8;
		maxs[i] = 0 >> 8;
	}

	for (int i = 0; i < numwalls; i++)
	{
		walltype* buildwall = &walls[i];
		Vertex* ourwall = &map->vertices[i];

		ourwall->x = buildwall->x >> 8;
		ourwall->y = buildwall->y >> 8;

		mins[0] = min(mins[0], ourwall->x);
		mins[1] = min(mins[1], ourwall->y);

		maxs[0] = max(maxs[0], ourwall->x);
		maxs[1] = max(maxs[1], ourwall->y);
	}

	for (int i = 0; i < numsectors; i++)
	{
		sectortype* buildsector = &sectors[i];
		Sector* oursector = &map->sectors[i];

		oursector->ceil = scaleLongToFloatLimit(0x4000, buildsector->ceilingz, 2048);
		oursector->floor = scaleLongToFloatLimit(0x4000, buildsector->floorz, 2048);
		oursector->npoints = buildsector->wallnum;

		oursector->offset = buildsector->wallptr;

		for (int k = 0; k < buildsector->wallnum; k++)
		{
			walltype* buildwall = &walls[buildsector->wallptr + k];
			Vertex* ourwall = &map->vertices[oursector->offset + k];

			ourwall->x = buildwall->x >> 8;
			ourwall->y = buildwall->y >> 8;

			mins[0] = min(mins[0], ourwall->x);
			mins[1] = min(mins[1], ourwall->y);

			maxs[0] = max(maxs[0], ourwall->x);
			maxs[1] = max(maxs[1], ourwall->y);

			oursector->neighbors[k] = buildwall->nextsector;
			ourwall->next = buildwall->point2;
		}

		//BuildBSPForSector(oursector, map);
	}


	map->player_spawn_point_x -= mins[0];
	map->player_spawn_point_y -= mins[1];

cleanup:
	fclose(file);
	if (sectors) free(sectors);
	if (walls) free(walls);

	return result;
}