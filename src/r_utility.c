#include "r_common.h"

#include "g_common.h"

void RenderUtl_ResetClip(ClipSegments* clip, short left, short right)
{
	clip->solidsegs[0].first = -0x7fff;
	clip->solidsegs[0].last = left;
	clip->solidsegs[1].first = right;
	clip->solidsegs[1].last = 0x7fff;

	clip->newend = clip->solidsegs + 2;
}

void RenderUtl_SetupRenderData(RenderData* data, int width, int x_start, int x_end)
{
	Map* map = Map_GetMap();

	int num_sectors = map->num_sectors;
	
	if (!data->visited_sectors_bitset)
	{
		size_t num_bitsets = ceilf((float)num_sectors / 64.0) + 1;

		data->visited_sectors_bitset = calloc(num_bitsets, sizeof(uint64_t));

		if (!data->visited_sectors_bitset)
		{
			return;
		}

		data->bitset_size = num_bitsets;
	}
	else
	{
		memset(data->visited_sectors_bitset, 0, sizeof(uint64_t) * data->bitset_size);
	}

	data->num_draw_sprites = 0;
	data->draw_collums.index = 0;
	
	RenderUtl_ResetClip(&data->clip_segs, 0, width);

	if (x_start > 0)
	{
		Scene_ClipAndDraw(&data->clip_segs, 0, x_start, true, NULL, NULL);
	}
	if (x_end < width)
	{
		Scene_ClipAndDraw(&data->clip_segs, x_end, width, true, NULL, NULL);
	}
}

void RenderUtl_Resize(RenderData* data, int width, int height, int x_start, int x_end)
{
	//resize the collumns
	if (data->draw_collums.collumns)
	{
		free(data->draw_collums.collumns);
	}

	int num_collumns = ((x_end - x_start)) * 4;
	data->draw_collums.collumns = calloc(num_collumns, sizeof(DrawCollumn));
	data->draw_collums.size = num_collumns;

	//resize the span ends array
	if (data->span_end)
	{
		free(data->span_end);
	}

	data->span_end = calloc(height + 2, sizeof(short));
}

void RenderUtl_DestroyRenderData(RenderData* data)
{
	if (data->visited_sectors_bitset) free(data->visited_sectors_bitset);
	if (data->draw_collums.collumns) free(data->draw_collums.collumns);
	if (data->span_end) free(data->span_end);

	data->visited_sectors_bitset = NULL;
}

bool RenderUtl_CheckVisitedSectorBitset(RenderData* data, int sector)
{
	if (!data->visited_sectors_bitset)
	{
		return false;
	}

	const uint64_t i = sector;
	const uint64_t mask_index = i / 64;
	const uint64_t local_index = i % 64;
	const uint64_t mask = ((uint64_t)1 << local_index);

	return data->visited_sectors_bitset[mask_index] & mask;
}

void RenderUtl_SetVisitedSectorBitset(RenderData* data, int sector)
{
	if (!data->visited_sectors_bitset)
	{
		return;
	}

	const uint64_t i = sector;
	const uint64_t mask_index = i / 64;
	const uint64_t local_index = i % 64;
	const uint64_t mask = ((uint64_t)1 << local_index);

	data->visited_sectors_bitset[mask_index] |= mask;
}

void RenderUtl_AddSpriteToQueue(RenderData* data, Sprite* sprite, int sector_light, bool is_decal)
{
	if (data->num_draw_sprites >= MAX_DRAWSPRITES)
	{
		return;
	}

	const int h_frames = sprite->img->h_frames;
	const int v_frames = sprite->img->v_frames;

	int sprite_offset_x = (h_frames > 0) ? sprite->frame % h_frames : 0;
	int sprite_offset_y = (v_frames > 0) ? sprite->frame / h_frames : 0;

	sprite_offset_x += sprite->frame_offset_x;
	sprite_offset_y += sprite->frame_offset_y;

	int sprite_rect_width = (h_frames > 0) ? sprite->img->width / h_frames : sprite->img->width;
	int sprite_rect_height = (v_frames > 0) ? sprite->img->height / v_frames : sprite->img->height;

	Vec3_u16 light = sprite->light;

#ifdef DISABLE_LIGHTMAPS
	light.r = sector_light;
	light.g = sector_light;
	light.b = sector_light;
#endif

	//Convert to draw sprite
	DrawSprite* draw_sprite = &data->draw_sprites[data->num_draw_sprites++];
	memset(draw_sprite, 0, sizeof(DrawSprite));

	draw_sprite->frame = sprite->frame + (sprite->frame_offset_x) + (sprite->frame_offset_y * sprite->img->h_frames);
	draw_sprite->img = sprite->img;
	draw_sprite->x = sprite->x + sprite->offset_x;
	draw_sprite->y = sprite->y + sprite->offset_y;
	draw_sprite->z = sprite->z + sprite->offset_z;
	draw_sprite->scale_x = sprite->scale_x;
	draw_sprite->scale_y = sprite->scale_y;
	draw_sprite->frame_offset_x = sprite_offset_x;
	draw_sprite->frame_offset_y = sprite_offset_y;
	draw_sprite->sprite_rect_width = sprite_rect_width;
	draw_sprite->sprite_rect_height = sprite_rect_height;
	draw_sprite->light = light;
	draw_sprite->flip_h = sprite->flip_h;
	draw_sprite->flip_v = sprite->flip_v;
	draw_sprite->decal_line_index = (is_decal) ? sprite->decal_line_index : -1;
}
