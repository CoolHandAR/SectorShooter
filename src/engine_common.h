#pragma once

typedef struct
{
	int num_lines;
	int line_offset;

	int sector_index;
} Subsector2;

typedef struct
{
	float floor_height, ceil_height;
	short floor_texture, ceil_texture;

} Sector2;

typedef struct
{
	float x0, y0;
	float x1, y1;

	float offset;
	int front_sector_index;
	int back_sector_index;

	float width_scale;
} Line2;

typedef struct
{
	float line_x, line_y, line_dx, line_dy;

	int children[2];
	float bbox[2][4];
} BSPNode2;