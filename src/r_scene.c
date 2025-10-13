#include "r_common.h"
#include "g_common.h"

#include "u_math.h"

#define hfov (0.72f*1080)  // Affects the horizontal field of vision
#define vfov (.2f)    // Affects the vertical field of vision
#define W 1920
#define H 1080

static float yslope[H];
static unsigned char LIGHT_LUT2[256][256];

enum
{
	BOXTOP,
	BOXBOTTOM,
	BOXLEFT,
	BOXRIGHT
};	// bbox coordinates

static bool Scene_checkBBOX(ClipSegments* clipsegments, int width, int height, float p_x, float p_y, float p_sin, float p_cos, float* bspcoord)
{
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

	float x2 = bspcoord[checkcoord[boxpos][0]] - p_x;
	float y2 = bspcoord[checkcoord[boxpos][1]] - p_y;
	float x1 = bspcoord[checkcoord[boxpos][2]] - p_x;
	float y1 = bspcoord[checkcoord[boxpos][3]] - p_y;

	float rx1 = x1 * p_sin - y1 * p_cos;
	float rx2 = x2 * p_sin - y2 * p_cos;
	float ry1 = x1 * p_cos + y1 * p_sin;
	float ry2 = x2 * p_cos + y2 * p_sin;

	if (ry1 <= 0 && ry2 <= 0)
	{
		return false;
	}

	ry1 = (hfov) / ry1;
	ry2 = (hfov) / ry2;

	int sx1 = 0;
	int sx2 = 0;
	int half_width = width / 2;
	int half_height = height / 2;

	sx1 = half_width - (int)(rx1 * ry1);
	sx2 = half_width - (int)(rx2 * ry2);

	if (sx2 == sx1) 
	{
		return false;
	}

	if (sx2 > sx1)
	{
		sx1 = max(sx1, 0);
		sx2 = min(sx2, width);

		if (sx2 <= sx1)
		{
			return false;
		}

		Cliprange2* start = clipsegments->solidsegs;
		int s = 0;
		while (start->last < sx2)
		{
			s++;
			start++;
		}

		if (sx1 >= start->first && sx2 <= start->last)
		{
			return false;
		}
	}

	return true;
}

static unsigned Pack_Color(unsigned char c[4])
{
	unsigned color = 0;

	color |= ((unsigned)c[0] << 24u);
	color |= ((unsigned)c[1] << 16u);
	color |= ((unsigned)c[2] << 8u);
	color |= ((unsigned)c[3]);

	return color;
}
static unsigned applyLight(unsigned color, int l)
{
	unsigned r = (color & 0xff000000u) >> 24u;
	unsigned g = (color & 0xff0000u) >> 16u;
	unsigned b = (color & 0xff00u) >> 8u;
	unsigned a = (color & 0xffu);

	color = 0;

	color |= (LIGHT_LUT2[r][l] << 24u);
	color |= (LIGHT_LUT2[g][l] << 16u);
	color |= (LIGHT_LUT2[b][l] << 8u);
	color |= (LIGHT_LUT2[a][l]);

	return color;
}
static void Scene_DrawWallCollumn(Image* image, float* depth_buffer, Texture* texture, int x, int y1, int y2, int depth, int tx, float ty_pos, float ty_step, int light, int height_mask)
{
	tx &= texture->width_mask;

	unsigned char* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1 + 1) * (size_t)image->width;

	for (int y = y1 + 1; y < y2; y++)
	{
		int ty = (int)ty_pos & height_mask;

		unsigned char* data = Image_Get(&texture->img, tx, ty);

		size_t i = index * 4;

		dest[i + 0] = LIGHT_LUT2[data[0]][light];
		dest[i + 1] = LIGHT_LUT2[data[1]][light];
		dest[i + 2] = LIGHT_LUT2[data[2]][light];

		depth_buffer[index] = depth;

		ty_pos += ty_step;
		index += image->width;
	}
}
static void Scene_DrawWallCollumnDepth(Image* image, Texture* texture, float* depth_buffer, int x, int y1, int y2, int z, int tx, float ty_pos, float ty_step, int light, int height_mask)
{
	tx &= texture->width_mask;

	unsigned* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1 + 1) * (size_t)image->width;
	size_t depth_index = (size_t)x + (size_t)(y1 + 1);

	//int height_mask = texture->height_mask;

	for (int y = y1 + 1; y < y2; y++)
	{
		int ty = (int)ty_pos & height_mask;

		unsigned* data = Image_Get(&texture->img, tx, ty);
		
		if (z >= depth_buffer[x + y * image->width])
		{
			continue;
		}
		depth_buffer[x + y * image->width] = z;

		dest[index += image->width] = *data;

		ty_pos += ty_step;
	}
}


static void Scene_DrawWallCollumn2(Image* image, CollumnImage* texture, int x, int y1, int y2, int tx, float ty_pos, float ty_step, int light)
{
	tx &= texture->width - 1;

	unsigned* dest = image->data;;
	size_t index = (size_t)x + (size_t)(y1 + 1) * (size_t)image->width;

	CollumnData* collumn = &texture->collumn_data[tx];
	unsigned* data = &texture->data[collumn->offset];


	for (int y = y1 + 1; y < y2; y++)
	{
		int ty = (int)ty_pos & (127);

		//unsigned* data = Image_Get(texture, tx, ty);
		//data = &texture->data[collumn->offset + ty];
		dest[index += image->width] = texture->data[collumn->offset + ty];

		ty_pos += ty_step;
	}
}

static void Scene_DrawPlaneStripe(Image* image, float* depth_buffer, Texture* texture, int x, int y1, int y2, int depth, float height, float p_cos, float p_sin, float p_x, float p_y, int light)
{
	if (y2 <= y1)
	{
		return;
	}

	unsigned* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1) * (size_t)image->width;

	float x_slope = (W / 2.0 - (float)x) / (float)(hfov);
	float inv = 1.0 / 32.0;

	for (int y = y1; y <= y2; y++)
	{
		float mapx, mapz;

		mapz = height * yslope[y];
		mapx = mapz * x_slope;

		float rtx = mapz * p_cos + mapx * p_sin;
		float rtz = mapz * p_sin - mapx * p_cos;

		mapz = rtz + p_y;
		mapx = rtx + p_x;

		mapz *= inv;
		mapx *= inv;

		int tx = (int)(TILE_SIZE * (mapx - (int)mapx)) & (TILE_SIZE - 1);
		int ty = (int)(TILE_SIZE * (mapz - (int)mapz)) & (TILE_SIZE - 1);

		unsigned* texdata = Image_Get(&texture->img, ty, tx);
		unsigned* d = Image_Get(image, x, y);

		*d = *texdata;

		depth_buffer[x + y * image->width] = depth;
	}
}

static void Scene_StoreDrawCollumn(DrawCollumns* collumns, Texture* texture, short x, short y1, short y2, int tx, int depth, float ty_pos, float ty_step)
{
	DrawCollumn* c = &collumns->collumns[collumns->index++];

	c->x = x;
	c->tx = tx;
	c->y1 = y1;
	c->y2 = y2;
	c->depth = depth;
	c->ty_pos = ty_pos;
	c->ty_step = ty_step;
	c->texture = texture;
}

void Scene_ResetClip(ClipSegments* clip, short left, short right)
{
	clip->solidsegs[0].first = -0x7fff;
	clip->solidsegs[0].last = left;
	clip->solidsegs[1].first = right;
	clip->solidsegs[1].last = 0x7fff;

	clip->newend = clip->solidsegs + 2;
}
void Scene_DrawLineSeg(Image* image, int first, int last, LineDrawArgs* args)
{
	//lay it out on the stack
	DrawingArgs* draw_args = args->draw_args;
	Line* line = args->line;
	Sector* sector = args->sector;
	Sidedef* sidedef = line->sidedef;
	
	bool is_backsector = (line->back_sector >= 0);

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

	//precompute some stuff
	float x_pos = fabs(first - args->x1);
	float texheight = sector->height * 0.5;

	for (int x = first; x < last; x++)
	{
		int tx = (args->u0 * (fabs(args->x2 - x) * args->tz2) + args->u1 * (fabs(x - args->x1) * args->tz1)) / (fabs(args->x2 - x) * args->tz2 + fabs(x - args->x1) * args->tz1);

		tx += sidedef->x_offset;

		short ctop = draw_args->yclip_top[x];
		short cbot = draw_args->yclip_bottom[x];

		float yceil = (int)(x_pos * ceil_step) + sy_ceil;
		float yfloor = (int)(x_pos * floor_step) + sy_floor;
		
		//clamped
		int c_yceil = Math_Clamp(yceil, ctop, cbot);
		int c_yfloor = Math_Clamp(yfloor, ctop, cbot);
		
		float yl = 1.0 / max(fabs(yfloor - yceil), 0.001);
		float depth = (int)(x_pos * depth_step) + args->tz1;

		if (ceil_texture)
		{
			Scene_DrawPlaneStripe(image, args->draw_args->depth_buffer, ceil_texture, x, ctop, c_yceil - 1, depth, args->ceilz, args->draw_args->view_cos, args->draw_args->view_sin, args->draw_args->view_x, args->draw_args->view_y, light);
		}
		if (floor_texture)
		{
			Scene_DrawPlaneStripe(image, args->draw_args->depth_buffer, floor_texture, x, c_yfloor, cbot, depth, args->floorz, args->draw_args->view_cos, args->draw_args->view_sin, args->draw_args->view_x, args->draw_args->view_y, light);
		}

		if (is_backsector)
		{
			float back_yceil = (x_pos * backceil_step) + back_sy_ceil;
			float back_yfloor = (x_pos * backfloor_step) + back_sy_floor;

			int c_back_yceil = Math_Clampl(back_yceil, ctop, cbot);
			int c_back_yfloor = Math_Clampl(back_yfloor, ctop, cbot);

			if(top_texture)
			{
				float ty_step = texheight * yl;
				float ty_pos = fabs(c_yceil - yceil) * ty_step;

				ty_pos += sidedef->y_offset;

				Scene_DrawWallCollumn(image, args->draw_args->depth_buffer, top_texture, x, c_yceil, c_back_yceil, depth, tx, ty_pos, ty_step, light, 127);
			}

			if (bot_texture)
			{
				float ty_step = (texheight) * yl;
				float ty_pos = fabs((c_back_yfloor + 1) - yceil) * ty_step;

				ty_pos += sidedef->y_offset;

				Scene_DrawWallCollumn(image, args->draw_args->depth_buffer, bot_texture, x, c_back_yfloor + 1, c_yfloor, depth, tx, ty_pos, ty_step, light, bot_texture->height_mask);
			}
			if (mid_texture)
			{
				float ty_step = (texheight)*yl;
				float ty_pos = (fabs(c_yceil - yceil)) * ty_step;

				ty_pos += sidedef->y_offset;

				Scene_StoreDrawCollumn(draw_args->drawcollumns, mid_texture, x, c_yceil, c_yfloor, tx, depth, ty_pos, ty_step);
			}


			draw_args->yclip_top[x] = Math_Clampl(max(c_yceil, c_back_yceil), ctop, image->height - 1);
			draw_args->yclip_bottom[x] = Math_Clampl(min(c_yfloor, c_back_yfloor), 0, cbot);
		}
		else
		{
			float ty_step = (texheight) * yl;
			float ty_pos = (fabs(c_yceil - yceil)) * ty_step;

			ty_pos += sidedef->y_offset;

			Scene_DrawWallCollumn(image, args->draw_args->depth_buffer, mid_texture, x, c_yceil, c_yfloor, depth, tx, ty_pos, ty_step, light, mid_height_mask);
		}

		x_pos++;
	}
}

void Scene_ClipAndDraw(ClipSegments* p_clip, int first, int last, bool solid, LineDrawArgs* args, Image* image)
{
	ClipSegments* clip = p_clip;
	Cliprange2* start = clip->solidsegs;

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
					Cliprange2* next = clip->newend;
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

	Cliprange2* next = start;
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
	float vx1 = line->x0 - args->view_x;
	float vy1 = line->y0 - args->view_y;

	float vx2 = line->x1 - args->view_x;
	float vy2 = line->y1 - args->view_y;

	if (vy1 * (vx1 - vx2) + vx1 * (vy2 - vy1) <= -MATH_EQUAL_EPSILON)
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

	float texwidth = line->width_scale * 2.0;

	float u0 = 0;
	float u1 = texwidth;


	if (tz1 <= 0 || tz2 <= 0)
	{
		Vertex org1 = { tx1,tz1 }, org2 = { tx2,tz2 };

		const float nearz = 1e-4f, farz = 0.1, nearside = 1e-5f, farside = 20.f;
		Vertex i1 = Math_IntersectLines(tx1, tz1, tx2, tz2, -nearside, nearz, -farside, farz);
		Vertex i2 = Math_IntersectLines(tx1, tz1, tx2, tz2, nearside, nearz, farside, farz);

		if (tz1 <= 0)
		{
			if (tz1 < nearz) { if (i1.y > 0) { tx1 = i1.x; tz1 = i1.y; } else { tx1 = i2.x; tz1 = i2.y; } }

		}
		if (tz2 <= 0)
		{
			if (tz2 < nearz) { if (i1.y > 0) { tx2 = i1.x; tz2 = i1.y; } else { tx2 = i2.x; tz2 = i2.y; } }
		}

		if (fabs(tx2 - tx1) > fabs(tz2 - tz1))
			u0 = fabs(tx1 - org1.x) * (texwidth) / fabs(org2.x - org1.x), u1 = fabs(tx2 - org1.x) * (texwidth) / fabs(org2.x - org1.x);
		else
			u0 = fabs(tz1 - org1.y) * (texwidth) / fabs(org2.y - org1.y), u1 = fabs(tz2 - org1.y) * (texwidth) / fabs(org2.y - org1.y);

	}

	float inv_tz1 = 1.0 / tz1;
	float inv_tz2 = 1.0 / tz2;

	float xscale1 = args->h_fov * inv_tz1;
	float xscale2 = args->h_fov * inv_tz2;

	int x1 = (image->half_width) - (int)(tx1 * xscale1);
	int x2 = (image->half_width) - (int)(tx2 * xscale2);

	//not visible
	if (x1 > x2 || x2 <= args->start_x || x1 >= args->end_x || x1 == x2)
	{
		return false;
	}

	float yscale1 = args->v_fov * inv_tz1;
	float yscale2 = args->v_fov * inv_tz2;

	float yceil = sector->ceil - args->view_z;
	float yfloor = sector->floor - args->view_z;

	int ceil_y1 = (image->half_height) - (int)((yceil) * yscale1);
	int ceil_y2 = (image->half_height) - (int)((yceil) * yscale2);

	int floor_y1 = (image->half_height) - (int)((yfloor) * yscale1);
	int floor_y2 = (image->half_height) - (int)((yfloor) * yscale2);

	int begin_x = max(x1, args->start_x);
	int end_x = min(x2, args->end_x);

	float xl = 1.0 / fabs(x2 - x1);

	Sector* backsector = NULL;

	if (line->back_sector >= 0)
	{
		backsector = &map->sectors[line->back_sector];
	}

	//setup line draw ags
	LineDrawArgs line_draw_args;
	line_draw_args.x1 = x1;
	line_draw_args.x2 = x2;
	line_draw_args.tz1 = tz1;
	line_draw_args.tz2 = tz2;
	line_draw_args.u0 = u0;
	line_draw_args.u1 = u1;
	line_draw_args.floorz = yfloor;
	line_draw_args.ceilz = yceil;
	line_draw_args.sy_ceil = ceil_y1;
	line_draw_args.sy_floor = floor_y1;
	line_draw_args.ceil_step = (ceil_y2 - ceil_y1) * xl;
	line_draw_args.floor_step = (floor_y2 - floor_y1) * xl;
	line_draw_args.depth_step = (tz2 - tz1) * xl;
	line_draw_args.line = line;
	line_draw_args.sector = sector;
	line_draw_args.draw_args = args;

	if (backsector)
	{
		float ybacksector_ceil = backsector->ceil - args->view_z;
		float ybacksector_floor = backsector->floor - args->view_z;

		int backceil_y1 = (image->half_height) - (int)((ybacksector_ceil) * yscale1);
		int backceil_y2 = (image->half_height) - (int)((ybacksector_ceil) * yscale2);

		int backfloor_y1 = (image->half_height) - (int)((ybacksector_floor) * yscale1);
		int backfloor_y2 = (image->half_height) - (int)((ybacksector_floor) * yscale2);

		line_draw_args.back_sy_ceil = backceil_y1;
		line_draw_args.back_sy_floor = backfloor_y1;
		line_draw_args.backceil_step = (backceil_y2 - backceil_y1) * xl;
		line_draw_args.backfloor_step = (backfloor_y2 - backfloor_y1) * xl;

		bool is_solid = false;

		Scene_ClipAndDraw(line_draw_args.draw_args->clipsegments, begin_x, end_x, false, &line_draw_args, image);
	}
	else
	{
		Scene_ClipAndDraw(line_draw_args.draw_args->clipsegments, begin_x, end_x, true, &line_draw_args, image);
	}


	return true;
}

void Scene_ProcessSubsector(Image* image, Map* map, Subsector* sub_sector, DrawingArgs* args)
{
	if (sub_sector->sector->sprite_add_index != Render_GetTicks())
	{
		Render_LockObjectMutex();

		//add object sprites to queue, to be drawn later on
		Object* obj = sub_sector->sector->object_list;

		while (obj)
		{
			Sprite* sprite = &obj->sprite;
			Render_AddSpriteToQueue(sprite);

			obj = obj->sector_next;
		}

		Render_UnlockObjectMutex();

		sub_sector->sector->sprite_add_index = Render_GetTicks();
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

	if(Scene_checkBBOX(args->clipsegments, image->width, image->height, args->view_x, args->view_y, args->view_sin, args->view_cos, node->bbox[side]))
	{
		total += Scene_ProcessBSPNode(image, map, node->children[side], args);
	}

	return total;
}

void Scene_DrawDrawCollumns(Image* image, DrawCollumns* collumns, float* depth_buffer)
{
	if (collumns->index == 0)
	{
		return;
	}

	for (int i = 0; i < collumns->index; i++)
	{
		DrawCollumn* c = &collumns->collumns[i];

		Scene_DrawWallCollumnDepth(image, c->texture, depth_buffer, c->x, c->y1, c->y2, c->depth, c->tx, c->ty_pos, c->ty_step, 0, 127);
	}

}

void Scene_Init()
{
	for (int i = 0; i < H; i++)
	{
		yslope[i] = ((H * vfov) / ((H / 2.0 - ((float)i)) - 0.0 * H * vfov));
	}
	
	//setup light lut
	for (int i = 0; i < 256; i++)
	{
		for (int k = 0; k < 256; k++)
		{
			float light = (float)k / 255.0f;

			int l = (float)i * light;

			if (l < 0) l = 0;
			else if (l > 255) l = 255;

			LIGHT_LUT2[i][k] = (unsigned char)l;
		}
	}

}
