#include "r_common.h"
#include "g_common.h"

#include "u_math.h"

enum
{
	BOXTOP,
	BOXBOTTOM,
	BOXLEFT,
	BOXRIGHT
};	// bbox coordinates

static bool Scene_checkBBOX(DrawingArgs* args, int width, int height, float p_x, float p_y, float p_sin, float p_cos, float* bspcoord)
{
	//src adapted from https://github.com/ZDoom/gzdoom/blob/master/src/rendering/swrenderer/scene/r_opaque_pass.cpp#L338

	static const int checkcoord[12][4] =
	{
		{ 3,0,2,1 },
		{ 3,0,2,0 },
		{ 3,1,2,0 },
		{ 0 },
		{ 2,0,2,1 },
		{ 0,0,0,0 },
		{ 3,1,3,0 },
		{ 0 },
		{ 2,0,3,1 },
		{ 2,1,3,1 },
		{ 2,1,3,0 }
	};

	int boxx = 0;
	int boxy = 0;
	int boxpos = 0;

	if (p_x <= bspcoord[BOXLEFT])
	{
		boxx = 0;
	}
	else if (p_x < bspcoord[BOXRIGHT])
	{
		boxx = 1;
	}
	else
	{
		boxx = 2;
	}

	if (p_y >= bspcoord[BOXTOP])
	{
		boxy = 0;
	}
	else if (p_y > bspcoord[BOXBOTTOM])
	{
		boxy = 1;
	}
	else
	{
		boxy = 2;
	}
	boxpos = (boxy << 2) + boxx;

	if (boxpos == 5)
	{
		return true;
	}
	
	float x1 = bspcoord[checkcoord[boxpos][0]] - p_x;
	float y1 = bspcoord[checkcoord[boxpos][1]] - p_y;
	float x2 = bspcoord[checkcoord[boxpos][2]] - p_x;
	float y2 = bspcoord[checkcoord[boxpos][3]] - p_y;

	if (y1 * (x1 - x2) + x1 * (y2 - y1) >= -MATH_EQUAL_EPSILON)
	{
		return true;
	}

	float rx1 = x1 * p_sin - y1 * p_cos;
	float rx2 = x2 * p_sin - y2 * p_cos;
	float ry1 = x1 * p_cos + y1 * p_sin;
	float ry2 = x2 * p_cos + y2 * p_sin;

	//behing the view
	if (ry1 <= 0 && ry2 <= 0)
	{
		return false;
	}

	ry1 = x1 * args->tan_cos + y1 * args->tan_sin;
	ry2 = x2 * args->tan_cos + y2 * args->tan_sin;

	int sx1 = 0;
	int sx2 = 0;

	int half_width = width / 2;

	if (rx1 >= -ry1)
	{
		if (rx1 > ry1) return false;
		if (ry1 == 0) return false;
		sx1 = Math_RoundToInt(half_width + rx1 * half_width / ry1);
	}
	else
	{
		if (rx2 < -ry2) return false;
		if (rx1 - rx2 - ry2 + ry1 == 0) return false;
		sx1 = 0;
	}
	if (rx2 <= ry2)
	{
		if (rx2 < -ry2) return false;
		if (ry2 == 0) return false;
		sx2 = Math_RoundToInt(half_width + rx2 * half_width / ry2);
	}
	else
	{
		if (rx1 > ry1) return false;
		if (ry2 - ry1 - rx2 + rx1 == 0) return false;
		sx2 = width;
	}
	//not visible
	if (sx2 <= sx1)
	{
		return false;
	}

	Cliprange* start = args->render_data->clip_segs.solidsegs;
	while (start->last < sx2)
	{
		start++;
	}
	if (sx1 >= start->first && sx2 <= start->last)
	{
		return false;
	}

	return true;
}

static bool Scene_IsLineInvisible(Line* line, Sector* sector, Sector* backsector, float p_floor0, float p_ceil0, float p_floor1, float p_ceil1, float p_backFloor0, float p_backCeil0,
	float p_backFloor1, float p_backCeil1)
{
	if (!backsector)
	{
		return false;
	}

	if (p_backCeil0 <= p_floor0 && p_backCeil1 <= p_floor1)
	{
		return false;
	}
	if (p_backFloor0 >= p_ceil0 && p_backFloor1 >= p_ceil1)
	{
		return false;
	}

	if (sector->light_level != backsector->light_level)
	{
		return false;
	}
	if (sector->floor != backsector->floor || sector->ceil != backsector->ceil)
	{
		return false;
	}
	if (sector->floor_texture != backsector->floor_texture)
	{
		return false;
	}
	if (sector->ceil_texture != backsector->ceil_texture)
	{
		return false;
	}


	return true;
}

static void Scene_StoreDrawCollumn(DrawCollumns* collumns, Texture* texture, short x, short y1, short y2, int tx, float depth, float ty_pos, float ty_step, unsigned char light)
{
	if (collumns->index >= collumns->size)
	{
		return;
	}

	DrawCollumn* c = &collumns->collumns[collumns->index++];

	c->x = x;
	c->tx = tx;
	c->y1 = y1;
	c->y2 = y2;
	c->depth = depth;
	c->ty_pos = ty_pos;
	c->ty_step = ty_step;
	c->texture = texture;
	c->light = light;
}

static void Scene_DrawPlane(Image* image, DrawPlane* plane, LineDrawArgs* args, int x1, int x2)
{
	//adapted from https://github.com/ZDoom/gzdoom/blob/master/src/rendering/swrenderer/plane/r_planerenderer.cpp#L45

	if (!plane->visible)
	{
		return;
	}

	//set depth and set image to black
	if (plane->light <= 0)
	{
		float* depth_buff = args->draw_args->depth_buffer;
		for (int x = x1; x < x2; x++)
		{
			int t = plane->ytop[x];
			int b = plane->ybottom[x];

			size_t index = (size_t)x + (size_t)(t) * (size_t)image->width;

			for (int y = t; y < b; y++)
			{
				size_t i = index * 4;

				image->data[index + 0] = 0;
				image->data[index + 1] = 0;
				image->data[index + 2] = 0;

				depth_buff[index] = fabs(plane->viewheight * args->draw_args->yslope[y]);

				index += image->width;
			}
		}

		return;
	}

	DrawingArgs* draw_args = args->draw_args;

	int x = x2 - 1;
	int t2 = plane->ytop[x];
	int b2 = plane->ybottom[x];

	if (b2 > t2)
	{
		short* se = &draw_args->render_data->span_end[t2];
		for (int i = 0; i < b2 - t2; i++)
		{
			se[i] = x;
		}
	}
	
	for (--x; x >= x1; --x)
	{
		int t1 = plane->ytop[x];
		int b1 = plane->ybottom[x];

		const int xr = x + 1;
		int stop = 0;

		stop = min(t1, b2);
		while (t2 < stop)
		{
			int y = t2++;
			int x2 = draw_args->render_data->span_end[y];

			Video_DrawPlaneSpan(image, plane, args, y, xr, x2);
		}
		stop = max(b1, t2);
		while (b2 > stop)
		{
			int y = --b2;
			int x2 = draw_args->render_data->span_end[y];

			Video_DrawPlaneSpan(image, plane, args, y, xr, x2);
		}

		stop = min(t2, b1);
		while (t1 < stop)
		{
			draw_args->render_data->span_end[t1++] = x;
		}
		stop = max(b2, t2);
		while (b1 > stop)
		{
			draw_args->render_data->span_end[--b1] = x;
		}

		t2 = plane->ytop[x];
		b2 = plane->ybottom[x];
	}
	while (t2 < b2)
	{
		int y = --b2;
		int x2 = draw_args->render_data->span_end[y];

		Video_DrawPlaneSpan(image, plane, args, y, x1, x2);
	}

}

void Scene_DrawLineSeg(Image* image, int first, int last, LineDrawArgs* args)
{
	//lay it out on the stack
	DrawingArgs* draw_args = args->draw_args;
	Line* line = args->line;
	Sector* sector = args->sector;
	Sidedef* sidedef = line->sidedef;
		
	bool is_backsector = (line->back_sector >= 0);
	bool dont_peg_top = (line->flags & MF__LINE_DONT_PEG_TOP);
	bool dont_peg_bottom = (line->flags & MF__LINE_DONT_PEG_BOTTOM);

	//texture ptrs
	Texture* top_texture = sidedef->top_texture;
	Texture* mid_texture = sidedef->middle_texture;
	Texture* bot_texture = sidedef->bottom_texture;
	Texture* floor_texture = sector->floor_texture;
	Texture* ceil_texture = sector->ceil_texture;

	int mid_height_mask = 0;

	if (mid_texture)
	{
		mid_height_mask = (mid_texture->height_mask == 63) ? 63 : 127;
	}

	float ceil_step = args->ceil_step;
	float floor_step = args->floor_step;
	float depth_step = args->depth_step;

	float backceil_step = args->backceil_step;
	float backfloor_step = args->backfloor_step;

	int sy_ceil = args->sy_ceil;
	int sy_floor = args->sy_floor;

	int back_sy_ceil = args->back_sy_ceil;
	int back_sy_floor = args->back_sy_floor;

	int light = sector->light_level;

	float u0 = args->u0;
	float u1 = args->u1;

	float tz1 = args->tz1;
	float tz2 = args->tz2;

	//precompute some stuff
	float x_pos = fabs(first - args->x1);
	float x_pos2 = fabs(args->x2 - first);
	float texheight = (args->sector_height) * 0.5;

	DrawPlane* floor_plane = &draw_args->render_data->floor_plane;
	DrawPlane* ceil_plane = &draw_args->render_data->ceil_plane;

	for (int x = first; x < last; x++)
	{
		int tx = (u0 * (x_pos2 * tz2) + u1 * (x_pos * tz1)) / (x_pos2 * tz2 + x_pos * tz1);
		int lx = tx;

		tx += sidedef->x_offset;

		short ctop = draw_args->yclip_top[x];
		short cbot = draw_args->yclip_bottom[x];

		float yceil = (int)(x_pos * ceil_step) + sy_ceil;
		float yfloor = (int)(x_pos * floor_step) + sy_floor;
		
		//clamped
		int c_yceil = Math_Clamp(yceil, ctop, cbot);
		int c_yfloor = Math_Clamp(yfloor, ctop, cbot);
		
		float yl = 1.0 / max(fabs(yfloor - yceil), 0.001);
		float ty_step = texheight * yl;
		float depth = (int)(x_pos * depth_step) + args->tz1;

		int wall_light = Math_Clampl(light - (depth * DEPTH_SHADING_SCALE), 0, 255);

		if (ceil_texture)
		{
			if (sector->is_sky)
			{
				Video_DrawSkyPlaneStripe(image, args->draw_args->depth_buffer, ceil_texture, x, ctop, c_yceil, args);
			}
			else
			{
				ceil_plane->ytop[x] = ctop;
				ceil_plane->ybottom[x] = c_yceil;

				if (ceil_plane->ytop[x] < ceil_plane->ybottom[x])
				{
					ceil_plane->visible = true;
				}
			}
			
		}
		if (floor_texture)
		{
			floor_plane->ytop[x] = c_yfloor;
			floor_plane->ybottom[x] = cbot;

			if (floor_plane->ytop[x] < floor_plane->ybottom[x])
			{
				floor_plane->visible = true;
			}
		}

		if (is_backsector)
		{
			float back_yceil = (int)(x_pos * backceil_step) + back_sy_ceil;
			float back_yfloor = (int)(x_pos * backfloor_step) + back_sy_floor;

			int c_back_yceil = Math_Clampl(back_yceil, ctop, cbot);
			int c_back_yfloor = Math_Clampl(back_yfloor, ctop, cbot);

			if(top_texture && !args->is_both_sky)
			{
				float ty_pos = fabs(c_yceil - yceil) * ty_step;
				float ly_pos = ty_pos;

				ty_pos += sidedef->y_offset;

				int mask = 127;

				if (!dont_peg_top)
				{
					ty_pos += args->backsector_ceil_height + top_texture->img.height;
					mask = top_texture->height_mask;
				}

				Video_DrawWallCollumn(image, args->draw_args->depth_buffer, top_texture, x, c_yceil, c_back_yceil, depth, tx, ty_pos, ty_step, lx, ly_pos, wall_light, mask, &line->linedef->lightmap);
			}

			if (bot_texture)
			{
				float ty_pos = 0;
				float ly_pos = 0;

				if (texheight > args->backsector_floor_height && !dont_peg_bottom)
				{
					ty_pos = (fabs(c_back_yfloor - yceil)) * ty_step;
					ly_pos = ty_pos;
					ty_pos -= args->backsector_floor_height;
				}
				else
				{
					ty_pos = (fabs(c_back_yfloor - back_yfloor)) * ty_step;
					ly_pos = ty_pos;
					ty_pos += texheight - (sector->base_ceil - sector->base_floor) * 0.5;
				}
				
				ty_pos += sidedef->y_offset;
				
				Video_DrawWallCollumn(image, args->draw_args->depth_buffer, bot_texture, x, c_back_yfloor, c_yfloor, depth, tx, ty_pos, ty_step, lx, ly_pos, wall_light, bot_texture->height_mask, &line->linedef->lightmap);
			}
			
			if (!args->is_both_sky)
			{
				draw_args->yclip_top[x] = Math_Clampl(max(c_yceil, c_back_yceil), ctop, image->height - 1);
			}
			
			draw_args->yclip_bottom[x] = Math_Clampl(min(c_yfloor, c_back_yfloor), 0, cbot);

			if (mid_texture)
			{
				//reclamp
				c_yceil = Math_Clamp(yceil, draw_args->yclip_top[x], draw_args->yclip_bottom[x]);
				c_yfloor = Math_Clamp(yfloor, draw_args->yclip_top[x], draw_args->yclip_bottom[x]);

				float ty_pos = (fabs(c_yceil - yceil)) * ty_step;

				ty_pos -= texheight;

				Scene_StoreDrawCollumn(&draw_args->render_data->draw_collums, mid_texture, x, c_yceil, c_yfloor, tx, depth, ty_pos, ty_step, wall_light);
			}
		}
		else
		{
			float ty_pos = (fabs(c_yceil - yceil)) * ty_step;
			float ly_pos = ty_pos;

			ty_pos += sidedef->y_offset;

			if (dont_peg_bottom)
			{
				ty_pos -= texheight;
			}		

			Video_DrawWallCollumn(image, args->draw_args->depth_buffer, mid_texture, x, c_yceil, c_yfloor, depth, tx, ty_pos, ty_step, lx, ly_pos, wall_light, mid_height_mask, &line->linedef->lightmap);
		}

		x_pos++;
		x_pos2--;
	}

	line->flags |= MF__LINE_MAPPED;
}

void Scene_ClipAndDraw(ClipSegments* p_clip, int first, int last, bool solid, LineDrawArgs* args, Image* image)
{
	//clipping code adapted from https://github.com/ZDoom/gzdoom/blob/master/src/rendering/swrenderer/segments/r_clipsegment.cpp#L97
	ClipSegments* clip = p_clip;
	Cliprange* start = clip->solidsegs;

	while (start->last < first)
	{
		start++;
	}

	if (first < start->first)
	{
		if (last <= start->first)
		{
			if (args) Scene_DrawLineSeg(image, first, last, args);

			if (solid)
			{
				if (last == start->first)
				{
					start->first = first;
				}
				else
				{
					Cliprange* next = clip->newend;
					clip->newend++;

					while (next != start)
					{
						*next = *(next - 1);
						next--;
					}
					next->first = first;
					next->last = last;
				}
			}
			return;
		}

		if (args) Scene_DrawLineSeg(image, first, start->first, args);
		
		if (solid)
		{
			start->first = first;
		}
	}

	if (last <= start->last)
	{
		return;
	}

	Cliprange* next = start;
	while (last >= (next + 1)->first)
	{
		if (args) Scene_DrawLineSeg(image, next->last, (next + 1)->first, args);
		next++;

		if (last <= next->last)
		{
			last = next->last;
			goto crunch;
		}
	}

	if(args) Scene_DrawLineSeg(image, next->last, last, args);

crunch:

	if (solid)
	{
		start->last = last;

		if (next != start)
		{
			int i = 0;
			int j = 0;
			for (i = 1, j = (int)(clip->newend - next); j > 0; i++, j--)
			{
				start[i] = next[i];
			}
			clip->newend = start + i;
		}
	}
}
bool Scene_RenderLine(Image* image, Map* map, Sector* sector, Line* line, DrawingArgs* args)
{
	//clipping parts adapted from https://github.com/ZDoom/gzdoom/blob/master/src/rendering/swrenderer/line/r_wallsetup.cpp#L52
	float vx2 = line->x0 - args->view_x;
	float vy2 = line->y0 - args->view_y;

	float vx1 = line->x1 - args->view_x;
	float vy1 = line->y1 - args->view_y;

	if (vy1 * (vx1 - vx2) + vx1 * (vy2 - vy1) >= 0)
	{
		return false;
	}

	float tx1 = vx1 * args->view_sin - vy1 * args->view_cos;
	float tz1 = vx1 * args->view_cos + vy1 * args->view_sin;

	float tx2 = vx2 * args->view_sin - vy2 * args->view_cos;
	float tz2 = vx2 * args->view_cos + vy2 * args->view_sin;

	//completely behind the view plane
	if (tz1 <= 0 && tz2 <= 0)
	{
		return false;
	}

	float tanZ1 = vx1 * args->tan_cos + vy1 * args->tan_sin;
	float tanZ2 = vx2 * args->tan_cos + vy2 * args->tan_sin;

	float texwidth = line->width_scale * 2.00;

	float u0 = 0;
	float u1 = 1;

	float fsx1, fsz1, fsx2, fsz2;

	if (tx1 >= -tanZ1)
	{
		if (tx1 > tanZ1) return false;
		if (tanZ1 == 0) return false;
		fsx1 = (float)image->half_width + tx1 * (float)image->half_width / tanZ1;
		fsz1 = tanZ1;
		u0 = 0.0f;
	}
	else
	{
		if (tx2 < -tanZ2) return false;
		float den = tx1 - tx2 - tanZ2 + tanZ1;
		if (den == 0) return false;
		fsx1 = 0;
		u0 = (tx1 + tanZ1) / den;
		fsz1 = tanZ1 + (tanZ2 - tanZ1) * u0;
	}
	
	if (fsz1 <= 0)
	{
		return false;
	}
	if (tx2 <= tanZ2)
	{
		if (tx2 < -tanZ2) return false;
		if (tanZ2 == 0) return false;
		fsx2 = (float)image->half_width + tx2 * (float)image->half_width / tanZ2;
		fsz2 = tanZ2;
		u1 = 1.0f;
	}
	else
	{
		if (tx1 > tanZ1) return false;
		float den = tanZ2 - tanZ1 - tx2 + tx1;
		if (den == 0) return false;
		fsx2 = (float)image->width;
		u1 = (tx1 - tanZ1) / den;
		fsz2 = tanZ1 + (tanZ2 - tanZ1) * u1;
	}
	
	if (fsz2 <= 0)
	{
		return false;
	}

	int x1 = Math_RoundToInt(fsx1);
	int x2 = Math_RoundToInt(fsx2);

	//not visible
	if (x1 >= x2 || x2 <= args->start_x || x1 >= args->end_x)
	{
		return false;
	}

	{
		float t1 = 0, t2 = 0;
		if (fabs(line->side_dx) > fabs(line->side_dy))
		{
			t1 = (line->x1 - line->side_x1) / line->side_dx;
			t2 = (line->x0 - line->side_x1) / line->side_dx;
		}
		else
		{
			t1 = (line->y1 - line->side_y1) / line->side_dy;
			t2 = (line->y0 - line->side_y1) / line->side_dy;
		}
		u0 = t1 + u0 * (t2 - t1);
		u1 = t1 + u1 * (t2 - t1);
	}
	u0 = fabs(u0);
	u1 = fabs(u1);

	u0 *= texwidth;
	u1 *= texwidth;

	float yscale1 = args->v_fov * (1.0 / fsz1);
	float yscale2 = args->v_fov * (1.0 / fsz2);

	float yceil = sector->r_ceil - args->view_z;
	float yfloor = sector->r_floor - args->view_z;

	int ceil_y1 = (image->half_height) - (int)((yceil)*yscale1);
	int ceil_y2 = (image->half_height) - (int)((yceil)*yscale2);

	int floor_y1 = (image->half_height) - (int)((yfloor)*yscale1);
	int floor_y2 = (image->half_height) - (int)((yfloor)*yscale2);

	int begin_x = max(x1, args->start_x);
	int end_x = min(x2, args->end_x);

	float xl = 1.0 / fabs(x2 - x1);

	Sector* backsector = NULL;

	if (line->back_sector >= 0)
	{
		backsector = &map->sectors[line->back_sector];
	}

	tz1 = fsz1;
	tz2 = fsz2;

	//setup planes
	DrawPlane* floor_plane = &args->render_data->floor_plane;
	DrawPlane* ceil_plane = &args->render_data->ceil_plane;

	floor_plane->lightmap = &sector->floor_lightmap;
	floor_plane->light = sector->light_level;
	floor_plane->visible = false;
	floor_plane->texture = sector->floor_texture;
	floor_plane->viewheight = yfloor;

	floor_plane->ytop = args->floor_plane_ytop;
	floor_plane->ybottom = args->floor_plane_ybottom;

	ceil_plane->lightmap = &sector->ceil_lightmap;
	ceil_plane->light = sector->light_level;
	ceil_plane->visible = false;
	ceil_plane->texture = sector->ceil_texture;
	ceil_plane->viewheight = yceil;

	ceil_plane->ytop = args->ceil_plane_ytop;
	ceil_plane->ybottom = args->ceil_plane_ybottom;
	
	for (int x = args->start_x; x < args->end_x; x++)
	{
		floor_plane->ytop[x] = 0;
		floor_plane->ybottom[x] = 0;

		ceil_plane->ytop[x] = 0;
		ceil_plane->ybottom[x] = 0;
	}

	//setup line draw ags
	LineDrawArgs line_draw_args;
	memset(&line_draw_args, 0, sizeof(line_draw_args));
	line_draw_args.x1 = x1;
	line_draw_args.x2 = x2;
	line_draw_args.tz1 = tz1;
	line_draw_args.tz2 = tz2;
	line_draw_args.u0 = u0;
	line_draw_args.u1 = u1;
	line_draw_args.sy_ceil = ceil_y1;
	line_draw_args.sy_floor = floor_y1;
	line_draw_args.ceil_step = (ceil_y2 - ceil_y1) * xl;
	line_draw_args.floor_step = (floor_y2 - floor_y1) * xl;
	line_draw_args.depth_step = (tz2 - tz1) * xl;
	line_draw_args.line = line;
	line_draw_args.sector = sector;
	line_draw_args.draw_args = args;
	line_draw_args.sector_height = sector->r_ceil - sector->r_floor;
	line_draw_args.world_bottom = yfloor;
	line_draw_args.world_top = yceil;
	line_draw_args.lowest_ceilling = sector->floor;

	//for floor and ceil plane rendering
	{
		float plane_angle = args->view_angle - Math_DegToRad(90.0);

		float x_step = cos(plane_angle) / args->focal_length_x;
		float y_step = -sin(plane_angle) / args->focal_length_x;

		plane_angle += Math_PI / 2.0;

		float plane_cos = cos(plane_angle);
		float plane_sin = -sin(plane_angle);

		float x = 0;

		x = x2 - image->half_width + 0.5;

		float right_x_pos = plane_cos + x * x_step;
		float right_y_pos = plane_sin + x * y_step;

		x = x1 - image->half_width + 0.5;

		float left_x_pos = plane_cos + x * x_step;
		float left_y_pos = plane_sin + x * y_step;

		line_draw_args.plane_base_pos_x = left_x_pos;
		line_draw_args.plane_base_pos_y = left_y_pos;

		line_draw_args.plane_step_scale_x = (right_x_pos - left_x_pos) / (x2 - x1);
		line_draw_args.plane_step_scale_y = (right_y_pos - left_y_pos) / (x2 - x1);

		line_draw_args.plane_angle = args->view_angle - Math_DegToRad(90);

		line_draw_args.sky_plane_y_step = 1.0 / (float)args->v_fov;
		line_draw_args.sky_plane_y_step *= VIEW_FOV / 90.0;
	}

	if (backsector)
	{
		float ybacksector_ceil = backsector->r_ceil - args->view_z;
		float ybacksector_floor = backsector->r_floor - args->view_z;

		int backceil_y1 = (image->half_height) - (int)((ybacksector_ceil)*yscale1);
		int backceil_y2 = (image->half_height) - (int)((ybacksector_ceil)*yscale2);

		int backfloor_y1 = (image->half_height) - (int)((ybacksector_floor)*yscale1);
		int backfloor_y2 = (image->half_height) - (int)((ybacksector_floor)*yscale2);

		line_draw_args.back_sy_ceil = backceil_y1;
		line_draw_args.back_sy_floor = backfloor_y1;
		line_draw_args.backceil_step = (backceil_y2 - backceil_y1) * xl;
		line_draw_args.backfloor_step = (backfloor_y2 - backfloor_y1) * xl;

		line_draw_args.backsector_floor_height = (backsector->base_ceil - backsector->floor) * 0.5;
		//line_draw_args.backsector_floor_height -= (sector->base_ceil - sector->base_floor) * 0.5;

		line_draw_args.backsector_ceil_height = (backsector->r_ceil - backsector->base_floor) * 0.5;
		line_draw_args.backsector_ceil_height -= (sector->base_ceil - sector->base_floor) * 0.5;
		line_draw_args.world_high = ybacksector_ceil;
		line_draw_args.world_low = ybacksector_floor;
		line_draw_args.lowest_ceilling = min(sector->ceil, backsector->ceil);
		
		if (sector->ceil_texture == backsector->ceil_texture && (sector->is_sky && backsector->is_sky))
		{
			line_draw_args.is_both_sky = true;
		}

		if (Scene_IsLineInvisible(line, sector, backsector, floor_y1, ceil_y1, floor_y2, ceil_y2, backfloor_y1, backceil_y1, backfloor_y2, backceil_y2))
		{
			return false;
		}

		bool is_solid = false;

		if (backsector->floor >= sector->ceil || backsector->ceil <= sector->floor)
		{
			is_solid = true;
		}

		Scene_ClipAndDraw(&line_draw_args.draw_args->render_data->clip_segs, begin_x, end_x, is_solid, &line_draw_args, image);
	}
	else
	{
		Scene_ClipAndDraw(&line_draw_args.draw_args->render_data->clip_segs, begin_x, end_x, true, &line_draw_args, image);
	}

	//draw floor plane
	Scene_DrawPlane(image, floor_plane, &line_draw_args, begin_x, end_x);

	//draw ceil plane
	Scene_DrawPlane(image, ceil_plane, &line_draw_args, begin_x, end_x);

	return true;
}

void Scene_ProcessSubsector(Image* image, Map* map, Subsector* sub_sector, DrawingArgs* args)
{
	if (!RenderUtl_CheckVisitedSectorBitset(args->render_data, sub_sector->sector->index))
	{
		Render_LockObjectMutex(false);

		int light = sub_sector->sector->light_level;

		//add object sprites to queue, to be drawn later on
		Object* obj = sub_sector->sector->object_list;

		while (obj)
		{
			Sprite* sprite = &obj->sprite;

			RenderUtl_AddSpriteToQueue(args->render_data, sprite, light, obj->type == OT__DECAL);

			obj = obj->sector_next;
		}

		Render_UnlockObjectMutex(false);

		RenderUtl_SetVisitedSectorBitset(args->render_data, sub_sector->sector->index);
	}

	//keep sector info on the stack
	Sector sector = *sub_sector->sector;

	//render line segments
	for (int i = 0; i < sub_sector->num_lines; i++)
	{
		Line* line = &map->line_segs[sub_sector->line_offset + i];

		if (line->skip_draw)
		{
			continue;
		}

		Scene_RenderLine(image, map, &sector, line, args);
	}
}

int Scene_ProcessBSPNode(Image* image, Map* map, int node_index, DrawingArgs* args)
{
	if (node_index & MF__NODE_SUBSECTOR)
	{
		if (node_index < 0)
		{
			node_index = 0;
		}
		else
		{
			node_index &= ~MF__NODE_SUBSECTOR;
		}
		Subsector* subsector = &map->sub_sectors[node_index];

		Scene_ProcessSubsector(image, map, subsector, args);

		return 1;
	}

	int total = 0;

	BSPNode* node = &map->bsp_nodes[node_index];

	int side = BSP_GetNodeSide(node, args->view_x, args->view_y);
	total += Scene_ProcessBSPNode(image, map, node->children[side], args);

	side = side ^ 1;

	if(Scene_checkBBOX(args, image->width, image->height, args->view_x, args->view_y, args->view_sin, args->view_cos, node->bbox[side]))
	{
		total += Scene_ProcessBSPNode(image, map, node->children[side], args);
	}

	return total;
}

void Scene_DrawDrawCollumns(Image* image, DrawCollumns* collumns, float* depth_buffer)
{
	for (int i = 0; i < collumns->index; i++)
	{
		DrawCollumn* c = &collumns->collumns[i];

		Texture* tex = c->texture;

		Video_DrawWallCollumnDepth(image, tex, depth_buffer, c->x, c->y1, c->y2, c->depth, c->tx, c->ty_pos, c->ty_step, c->light, tex->height_mask);
	}
}
