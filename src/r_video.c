#include "r_common.h"

#include "g_common.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "u_math.h"
#include <stdio.h>

//#define WHITE_TEXTURES

static const float PI = 3.14159265359;
unsigned char LIGHT_LUT[256][MAX_LIGHT_VALUE];

const int INSIDE = 0b0000;
const int LEFT = 0b0001;
const int RIGHT = 0b0010;
const int BOTTOM = 0b0100;
const int TOP = 0b1000;

static void SWAP_INT(int* a, int* b)
{
	int temp = *a;

	*a = *b;
	*b = temp;
}

static float Video_FastLerp(float v, float from, float to)
{

}

void Video_Setup()
{
	//setup light lut
	for (int i = 0; i < 256; i++)
	{
		for (int k = 0; k < MAX_LIGHT_VALUE; k++)
		{
			float light = (float)k / 255.0f;

#ifdef WHITE_TEXTURES
			int l = (float)255.0 * light;
#else
			int l = (float)i * light;
#endif // WHITE_TEXTURES

			if (l < 0) l = 0;
			else if (l > 255) l = 255;

			LIGHT_LUT[i][k] = (unsigned char)l;		
		}
	}

	float s = 0;
}


static int ComputeOutCode(int xmin, int xmax, int ymin, int ymax, float x, float y)
{
	int code = INSIDE;

	if (x < xmin)           // to the left of clip window
		code |= LEFT;
	else if (x > xmax)      // to the right of clip window
		code |= RIGHT;
	if (y < ymin)           // below the clip window
		code |= BOTTOM;
	else if (y > ymax)      // above the clip window
		code |= TOP;

	return code;
}

bool Video_ClipLine(int xmin, int xmax, int ymin, int ymax, int* r_x0, int* r_y0, int* r_x1, int* r_y1)
{
	//src https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm

	float x0 = *r_x0;
	float y0 = *r_y0;
	float x1 = *r_x1;
	float y1 = *r_y1;

	int outcode0 = ComputeOutCode(xmin, xmax, ymin, ymax, x0, y0);
	int outcode1 = ComputeOutCode(xmin, xmax, ymin, ymax, x1, y1);

	while (true)
	{
		if (!(outcode0 | outcode1))
		{
			*r_x0 = x0;
			*r_y0 = y0;
			*r_x1 = x1;
			*r_y1 = y1;

			return true;
		}
		if (outcode0 & outcode1)
		{
			return false;
		}
		float x = 0, y = 0;

		int outcodeout = outcode1 > outcode0 ? outcode1 : outcode0;

		if (outcodeout & TOP)
		{
			x = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0);
			y = ymax;
		}
		else if (outcodeout & BOTTOM)
		{
			x = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0);
			y = ymin;
		}
		else if (outcodeout & RIGHT)
		{
			y = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0);
			x = xmax;
		}
		else if (outcodeout & LEFT)
		{
			y = y0 + (y1 - y0) * (xmin - x0) / (x1 - x0);
			x = xmin;
		}

		if(outcodeout == outcode0)
		{
			x0 = x;
			y0 = y;
			outcode0 = ComputeOutCode(xmin, xmax, ymin, ymax, x0, y0);
		}
		else
		{
			x1 = x;
			y1 = y;
			outcode1 = ComputeOutCode(xmin, xmax, ymin, ymax, x1, y1);
		}
	}

	return false;
}

void Video_DrawLine(Image* image, int x0, int y0, int x1, int y1, unsigned char* color)
{
	if (!Video_ClipLine(0, image->width, 0, image->height, &x0, &y0, &x1, &y1))
	{
		return;
	}
	
	int steep = 0;

	if (abs(x0 - x1) < abs(y0 - y1))
	{
		SWAP_INT(&x0, &y0);
		SWAP_INT(&x1, &y1);

		steep = 1;
	}
	if (x0 > x1)
	{
		SWAP_INT(&x0, &x1);
		SWAP_INT(&y0, &y1);
	}

	int dx = x1 - x0;
	int dy = y1 - y0;

	int derror = abs(dy) * 2;
	int error = 0;

	int y = y0;
	int y_add_sign = (y1 > y0) ? 1 : -1;
	for (int x = x0; x <= x1; x++)
	{
		if (steep == 1)
		{
			Image_Set2(image, y, x, color);
		}
		else
		{
			Image_Set2(image, x, y, color);
		}

		error += derror;

		if (error > dx)
		{
			y += y_add_sign;
			error -= dx * 2;
		}
	}
}

void Video_DrawRectangle(Image* image, int p_x, int p_y, int p_w, int p_h, unsigned char* p_color)
{
	if (p_x < 0)
	{
		p_x = 0;
	}
	if (p_x >= image->width)
	{
		p_x = image->width - 1;
	}
	if (p_y < 0)
	{
		p_y = 0;
	}
	if (p_y >= image->height)
	{
		p_y = image->height - 1;
	}

	for (int x = 0; x < p_w; x++)
	{
		for (int y = 0; y < p_h; y++)
		{
			Image_Set2(image, x + p_x, y + p_y, p_color);
		}
	}
}

void Video_DrawCircle(Image* image, int p_x, int p_y, float radius, unsigned char* p_color)
{
	if (p_x < 0)
	{
		p_x = 0;
	}
	if (p_x >= image->width)
	{
		p_x = image->width - 1;
	}
	if (p_y < 0)
	{
		p_y = 0;
	}
	if (p_y >= image->height)
	{
		p_y = image->height - 1;
	}

	int min_x = p_x - radius;
	int min_y = p_y - radius;

	int max_x = p_x + radius;
	int max_y = p_y + radius;

	for (int x = min_x; x <= max_x; x++)
	{
		for (int y = min_y; y <= max_y; y++)
		{
			if (Math_PointInsideCircle(x, y, p_x, p_y, radius))
			{
				Image_Set2(image, x, y, p_color);
			}
		}
	}
}

void Video_DrawBoxLines(Image* image, float box[2][2], unsigned char* color)
{	
	//top line
	Video_DrawLine(image, box[0][0], box[0][1], box[1][0], box[0][1], color);

	//bottom line
	Video_DrawLine(image, box[0][0], box[1][1], box[1][0], box[1][1], color);

	//left line
	Video_DrawLine(image, box[0][0], box[0][1], box[0][0], box[1][1], color);

	//right line
	Video_DrawLine(image, box[1][0], box[0][1], box[1][0], box[1][1], color);
}

void Video_DrawBox(Image* image, float box[2][3], float view_x, float view_y, float view_z, float view_cos, float view_sin, float tan_sin, float tan_cos, float v_fov, int x_start, int x_end)
{
	float box_size_x = box[1][0] - box[0][0];
	float box_size_y = box[1][1] - box[0][1];
	float box_size_z = box[1][2] - box[0][2];

	float box_center_x = box[0][0] + (box_size_x * 0.5);
	float box_center_y = box[0][1] + (box_size_y * 0.5);
	float box_center_z = box[0][2] + (box_size_z);

	float local_box_x = box_center_x - view_x;
	float local_box_y = box_center_y - view_y;
	float local_box_z = box_center_z - view_z;

	float transform_x = local_box_x * view_sin - local_box_y * view_cos;
	float transform_y = local_box_x * view_cos + local_box_y * view_sin;

	if (transform_y <= 0)
	{
		return;
	}

	float transform_y_tan = local_box_x * tan_cos + local_box_y * tan_sin;

	if (transform_x >= -transform_y_tan && transform_x > transform_y_tan) return;
	if (transform_y_tan == 0) return;

	float fsx1 = image->half_width + transform_x * image->half_width / transform_y_tan;

	float yscale = v_fov;

	int screen_x = (int)floor(fsx1 + 0.5);

	float height_transform_y = fabs((float)(image->height / (transform_y)));

	int box_width = height_transform_y * box_size_x;
	int box_height = height_transform_y * (box_size_z / 2);

	if (box_height <= 0 || box_width <= 0)
	{
		return;
	}

	int box_half_width = box_width / 2;
	int box_half_height = box_height / 2;


	int draw_start_x = -box_half_width + screen_x;
	if (draw_start_x < x_start) draw_start_x = x_start;

	if (draw_start_x >= x_end)
	{
		return;
	}


	int draw_end_x = box_half_width + screen_x;
	if (draw_end_x > x_end) draw_end_x = x_end;

	if (draw_end_x <= x_start)
	{
		return;
	}

	int screen_offset_y = (int)((image->half_height) - (local_box_z * yscale));
	int v_move_screen = (int)(screen_offset_y / transform_y);
	int box_screen_y = (int)((image->half_height)) + v_move_screen;

	int draw_start_y = -box_half_height + box_screen_y;
	if (draw_start_y < 0) draw_start_y = 0;
	int draw_end_y = box_half_height + box_screen_y;
	if (draw_end_y >= image->height) draw_end_y = image->height - 1;

	if (draw_start_y >= image->height || draw_end_y <= 0)
	{
		return;
	}

	unsigned char top_color[4] = { 255, 0, 255, 255 };
	unsigned char bottom_color[4] = { 0, 255, 255, 255 };
	unsigned char left_color[4] = { 255, 255, 0, 255 };
	unsigned char right_color[4] = { 128, 255, 128, 255 };

	//top line
	for (int x = draw_start_x; x < draw_end_x; x++) Image_Set2(image, x, draw_start_y, top_color);

	//bottom line
	for (int x = draw_start_x; x < draw_end_x; x++) Image_Set2(image, x, draw_end_y, bottom_color);

	//left line
	for (int y = draw_start_y; y < draw_end_y; y++) Image_Set2(image, draw_start_x, y, left_color);

	//right line
	for (int y = draw_start_y; y < draw_end_y; y++) Image_Set2(image, draw_end_x, y, right_color);

}

void Video_DrawScreenTexture(Image* image, Image* texture, float p_x, float p_y, float p_scaleX, float p_scaleY)
{
	if (p_scaleX <= 0 || p_scaleY <= 0)
	{
		return;
	}

	//out of bounds
	if (p_x < 0 || p_x > image->width || p_y < 0 || p_y > image->height)
	{
		return;
	}

	int render_scale = Render_GetRenderScale();

	p_scaleX *= render_scale;
	p_scaleY *= render_scale;

	float d_scale_x = 1.0 / p_scaleX;
	float d_scale_y = 1.0 / p_scaleY;

	const int rect_width = texture->width * p_scaleX;
	const int rect_height = texture->height * p_scaleY;

	for (int x = 0; x < rect_width; x++)
	{
		for (int y = 0; y < rect_height; y++)
		{
			unsigned char* color = Image_Get(texture, x * d_scale_x, y * d_scale_y);

			//perform alpha discarding if there is an alpha channel
			if (texture->numChannels >= 4)
			{
				if (color[3] < 128)
				{
					continue;
				}
			}
		
			Image_Set2(image, x + p_x, y + p_y, color);	
		}
	}	
}

void Video_DrawScreenSprite(Image* image, Sprite* sprite, int start_x, int end_x)
{
	if (sprite->scale_x <= 0 || sprite->scale_y <= 0)
	{
		return;
	}

	//completely transparent
	if (sprite->transparency >= 1)
	{
		return;
	}

	int render_scale = Render_GetRenderScale();

	int pix_x = sprite->x * image->width;
	int pix_y = sprite->y * image->height;

	//out of bounds
	if (pix_x < 0 || pix_x >= end_x || pix_y < 0 || pix_y >= image->height)
	{
		return;
	}

	Vec3_u16 light = sprite->light;

	int frame = sprite->frame;

	const int h_frames = sprite->img->h_frames;
	const int v_frames = sprite->img->v_frames;

	int offset_x = (h_frames > 0) ? frame % h_frames : 0;
	int offset_y = (v_frames > 0) ? frame / h_frames : 0;

	int rect_width = (h_frames > 0) ? sprite->img->width / h_frames : sprite->img->width;
	int rect_height = (v_frames > 0) ? sprite->img->height / v_frames : sprite->img->height;

	float scale_x = sprite->scale_x * render_scale;
	float scale_y = sprite->scale_y * render_scale;

	float d_scale_x = 1.0 / scale_x;
	float d_scale_y = 1.0 / scale_y;

	rect_width *= scale_x;
	rect_height *= scale_y;

	if (sprite->flip_h)
	{
		offset_x += 1;
	}
	if (sprite->flip_v)
	{
		offset_y += 1;
	}

	float transparency = (sprite->transparency > 0) ? (1.0 / min(sprite->transparency, 1)) : 0;

	FrameInfo* frame_info = Image_GetFrameInfo(sprite->img, frame + (sprite->frame_offset_x) + (sprite->frame_offset_y * sprite->img->h_frames));

	int min_x = (sprite->flip_h) ? (rect_width - (frame_info->max_real_x * scale_x)) : frame_info->min_real_x * scale_x;
	int max_x = (sprite->flip_h) ? (rect_width - (frame_info->min_real_x * scale_x)) : frame_info->max_real_x * scale_x;

	if (min_x + pix_x >= end_x)
	{
		return;
	}

	if (sprite->flip_h)
	{
		if (min_x > 0) min_x -= 1;
		max_x += 1;
	}

	float x_tex_step = 0;
	float x_tex_pos = 0;
	
	for (int x = min_x; x <= max_x; x++)
	{
		if (x + pix_x < start_x)
		{
			continue;
		}
		else if (x + pix_x >= end_x)
		{
			break;
		}

		if (sprite->flip_h)
		{
			x_tex_pos = (offset_x * rect_width - x - 1) * d_scale_x;
			x_tex_step = -d_scale_x;
		}
		else
		{
			x_tex_pos = (x + offset_x * rect_width) * d_scale_x;
			x_tex_step = d_scale_x;
		}

		int tx = (int)x_tex_pos;
		int span_x = (sprite->flip_h) ? (rect_width - x - 1) : x;

		AlphaSpan* span = FrameInfo_GetAlphaSpan(frame_info, span_x * d_scale_x);

		if (span->min > span->max)
		{
			continue;
		}

		int x_steps = 0;

		float test_pos_x = x_tex_pos;
		//calculate how many steps we can take
		while ((int)test_pos_x == tx && (x + x_steps) <= max_x)
		{
			if (x + x_steps + pix_x >= end_x)
			{
				break;
			}

			test_pos_x += x_tex_step;
			x_steps++;
		}

		int y_min = (sprite->flip_v) ? (rect_height - (span->max * scale_y)) : span->min * scale_y;
		int y_max = (sprite->flip_v) ? (rect_height - (span->min * scale_y)) : span->max * scale_y;

		if (sprite->flip_v)
		{
			if (y_min > 0) y_min -= 1;
			y_max += 1;
		}

		for (int y = y_min; y < y_max; y++)
		{
			float y_tex_pos = 0;
			float y_tex_step = 0;

			if (sprite->flip_v)
			{
				y_tex_pos = (offset_y * rect_height - y - 1) * d_scale_y;
				y_tex_step = -d_scale_y;
			}
			else
			{
				y_tex_pos = (y + offset_y * rect_height) * d_scale_y;
				y_tex_step = d_scale_y;
			}

			int ty = (int)y_tex_pos;

			unsigned char* color = Image_Get(sprite->img, tx, ty);

			//perform alpha discarding if there is 4 color channels
			//most pixels should be discarded with alpha spans
			if (sprite->img->numChannels >= 4)
			{
				if (color[3] < 128)
				{
					continue;
				}
			}

			unsigned char clr[4] = { LIGHT_LUT[color[0]][light.r], LIGHT_LUT[color[1]][light.g], LIGHT_LUT[color[2]][light.b], 255 };

			//apply transparency
			if (transparency > 1)
			{
				
			}
			else
			{
				//try to set as many pixels in a row as possible
				while (y < y_max)
				{
					for (int l = 0; l < x_steps; l++)
					{
						Image_Set2(image, (x + pix_x + l), y + pix_y, clr);
					}

					y_tex_pos += y_tex_step;

					if ((int)y_tex_pos != ty)
					{
						break;
					}

					y++;
				}
			}		
		}
		x += x_steps - 1;
	}
}

void Video_DrawSprite(Image* image, DrawingArgs* args, DrawSprite* sprite)
{
	//invalid scale
	if (sprite->scale_x <= 0 || sprite->scale_y <= 0)
	{
		return;
	}
	//completely transparent
	if (sprite->transparency >= 1)
	{
		return;
	}

	//translate sprite position to relative to camera
	float local_sprite_x = sprite->x - args->view_x;
	float local_sprite_y = sprite->y - args->view_y;
	float local_sprite_z = sprite->z - args->view_z;

	float transform_x = local_sprite_x * args->view_sin - local_sprite_y * args->view_cos;
	float transform_y = local_sprite_x * args->view_cos + local_sprite_y * args->view_sin;

	if (transform_y <= 0)
	{
		return;
	}

	float transform_y_tan = local_sprite_x * args->tan_cos + local_sprite_y * args->tan_sin;

	//if (transform_x >= -transform_y_tan && transform_x > transform_y_tan) return;
	if (transform_y_tan == 0) return;

	float fsx1 = image->half_width + transform_x * image->half_width / transform_y_tan;

	float xscale = args->h_fov / transform_y;
	float yscale = args->v_fov;

	int sprite_screen_x = (int)floor(fsx1 + 0.5);

	float height_transform_y = fabs((float)(image->height / (transform_y)));

	int sprite_width = height_transform_y * (sprite->scale_x);
	int sprite_height = height_transform_y * (sprite->scale_y);

	if (sprite_height <= 0 || sprite_width <= 0)
	{
		return;
	}

	int sprite_half_width = sprite_width / 2;
	int sprite_half_height = sprite_height / 2;

	int frame = sprite->frame;

	int sprite_offset_x = sprite->frame_offset_x;
	int sprite_offset_y = sprite->frame_offset_y;

	int sprite_rect_width = sprite->sprite_rect_width;
	int sprite_rect_height = sprite->sprite_rect_height;

	FrameInfo* frame_info = Image_GetFrameInfo(sprite->img, frame);

	if (!frame_info)
	{
		return;
	}

	int min_x = (sprite->flip_h) ? ((sprite_rect_width)-(frame_info->max_real_x)) : frame_info->min_real_x;
	int max_x = (sprite->flip_h) ? ((sprite_rect_width)-(frame_info->min_real_x)) : frame_info->max_real_x;
	
	int draw_start_x = -sprite_half_width + sprite_screen_x;
	if (draw_start_x < args->start_x) draw_start_x = args->start_x;

	if (draw_start_x >= args->end_x)
	{
		return;
	}

	int draw_end_x = sprite_half_width + sprite_screen_x;
	if (draw_end_x > args->end_x) draw_end_x = args->end_x;

	if (draw_end_x <= args->start_x)
	{
		return;
	}

	if (draw_start_x >= draw_end_x)
	{
		return;
	}

	int screen_offset_y = (int)((image->half_height) - (local_sprite_z * yscale));
	int v_move_screen = (int)(screen_offset_y / transform_y);
	int sprite_screen_y = (int)((image->half_height)) + v_move_screen;

	int draw_start_y = -sprite_half_height + sprite_screen_y;
	if (draw_start_y < 0) draw_start_y = 0;
	int draw_end_y = sprite_half_height + sprite_screen_y;
	if (draw_end_y >= image->height) draw_end_y = image->height - 1;

	if (draw_start_y >= image->height || draw_end_y <= 0)
	{
		return;
	}

	Vec3_u16 light = sprite->light;

	float tx_pos = 0;
	float tx_step = 256.0 * ((float)sprite_rect_width / (float)sprite_width) / 256.0;

	if (sprite->flip_h)
	{
		tx_pos = (256.0 * ((-sprite_half_width + sprite_screen_x) - draw_start_x - 1) * sprite_rect_width / sprite_width) / 256.0;
		tx_step = -tx_step;
		sprite_offset_x += 1;
	}
	else
	{
		tx_pos = (256.0 * (draw_start_x - (-sprite_half_width + sprite_screen_x)) * sprite_rect_width / sprite_width) / 256.0;
	}

	float tx_add = ((sprite_offset_x)*sprite_rect_width);
	tx_pos += tx_add;
	
	float d = (draw_start_y - v_move_screen) * 256 - image->height * 128 + sprite_height * 128;
	float start_ty_pos = ((d * sprite_rect_height) / sprite_height) / 256;
	float ty_step = 256.0 * ((float)sprite_rect_height / (float)sprite_height) / 256.0;

	float ty_add = (sprite_offset_y * sprite_rect_height);


	unsigned char* dest = image->data;
	size_t dest_index_step = (size_t)image->width;
	
	float xl = 1.0 / fabs(draw_end_x - draw_start_x);

	float depth_scale = (transform_y * DEPTH_SHADING_SCALE);

	int light_r = Math_Clampl(light.r - depth_scale, 0, MAX_LIGHT_VALUE - 1);
	int light_g = Math_Clampl(light.g - depth_scale, 0, MAX_LIGHT_VALUE - 1);
	int light_b = Math_Clampl(light.b - depth_scale, 0, MAX_LIGHT_VALUE - 1);

	//try to make this loop as fast as possible
	for (int x = draw_start_x; x < draw_end_x; x++)
	{
		size_t dest_index = (size_t)x + (size_t)(draw_start_y) * (size_t)image->width;

		int tx = tx_pos;
		int local_tx = max(tx, tx_add) - min(tx, tx_add);

		if (local_tx < min_x)
		{
			tx_pos += tx_step;
			continue;
		}
		else if (local_tx > max_x)
		{
			break;
		}
	
		int span_x = (sprite->flip_h) ? (sprite_rect_width - local_tx) : local_tx;

		AlphaSpan* span = FrameInfo_GetAlphaSpan(frame_info, span_x);

		if (span->min > span->max)
		{
			tx_pos += tx_step;
			continue;
		}

		float x_pos = fabs(x - draw_start_x);
		float depth = (x_pos * xl) + transform_y;

		int min_y = (sprite->flip_v) ? (sprite_rect_height - (span->max)) : span->min;
		int max_y = (sprite->flip_v) ? (sprite_rect_height - (span->min)) : span->max;

		float ty_pos = start_ty_pos + ty_add;
		float ty_local_pos = start_ty_pos;
		for (int y = draw_start_y; y < draw_end_y; y++)
		{
			int ty = ty_pos;
			int local_ty = ty_local_pos;

			if (local_ty < min_y)
			{
				ty_pos += ty_step;
				ty_local_pos += ty_step;
				dest_index += dest_index_step;
				continue;
			}
			else if (local_ty > max_y)
			{
				break;
			}

			unsigned char* tex_data = Image_Get(sprite->img, tx, ty);

			if (tex_data[3] > 128 && transform_y_tan < args->depth_buffer[dest_index])
			{
				args->depth_buffer[dest_index] = transform_y_tan;

				size_t i = dest_index * 4;

				//avoid loops, this is faster
				dest[i + 0] = LIGHT_LUT[tex_data[0]][light_r];
				dest[i + 1] = LIGHT_LUT[tex_data[1]][light_g];
				dest[i + 2] = LIGHT_LUT[tex_data[2]][light_b];
			}

			ty_pos += ty_step;
			ty_local_pos += ty_step;
			dest_index += dest_index_step;
		}

		tx_pos += tx_step;
	}

}

void Video_DrawDecalSprite(Image* image, DrawingArgs* args, DrawSprite* sprite)
{
	if (sprite->decal_line_index < 0)
	{
		return;
	}

	Linedef* line = Map_GetLineDef(sprite->decal_line_index);

	if (!line)
	{
		return;
	}
	Linedef* decal_line = line;

	float half_sprite_width = sprite->sprite_rect_width / 2;
	float half_sprite_height = sprite->sprite_rect_height / 2;

	float decal_rect_left = half_sprite_width / 4;
	float decal_rect_right = half_sprite_width / 4;

	float line_normal_x = decal_line->dx;
	float line_normal_y = decal_line->dy;
	Math_XY_Normalize(&line_normal_x, &line_normal_y);

	float line_vx2 = decal_line->x0 - args->view_x;
	float line_vy2 = decal_line->y0 - args->view_y;

	float line_vx1 = decal_line->x1 - args->view_x;
	float line_vy1 = decal_line->y1 - args->view_y;
	
	float line_min_x = min(line_vx1, line_vx2);
	float line_min_y = min(line_vy1, line_vy2);

	float line_max_x = max(line_vx1, line_vx2);
	float line_max_y = max(line_vy1, line_vy2);

	float frac = 0;
	if (fabs(decal_line->dx) > fabs(decal_line->dy))
	{	
		frac = (sprite->x - decal_line->x0) / decal_line->dx;
	}
	else if(decal_line->dy != 0)
	{
		frac = (sprite->y - decal_line->y0) / decal_line->dy;
	}
	
	float decal_x = decal_line->x0 + frac * decal_line->dx;
	float decal_y = decal_line->y0 + frac * decal_line->dy;

	float vx2 = decal_x - decal_rect_left * line_normal_x - args->view_x;
	float vy2 = decal_y - decal_rect_left * line_normal_y - args->view_y;

	float vx1 = decal_x + decal_rect_right * line_normal_x - args->view_x;
	float vy1 = decal_y + decal_rect_right * line_normal_y - args->view_y;

	//probaly should use line's screen space bounds
	//but works okay for now
	vx1 = Math_Clamp(vx1, line_min_x, line_max_x);
	vy1 = Math_Clamp(vy1, line_min_y, line_max_y);

	vx2 = Math_Clamp(vx2, line_min_x, line_max_x);
	vy2 = Math_Clamp(vy2, line_min_y, line_max_y);

	float tx1 = vx1 * args->view_sin - vy1 * args->view_cos;
	float tz1 = vx1 * args->view_cos + vy1 * args->view_sin;

	float tx2 = vx2 * args->view_sin - vy2 * args->view_cos;
	float tz2 = vx2 * args->view_cos + vy2 * args->view_sin;

	//completely behind the view plane
	if (tz1 <= 0 && tz2 <= 0)
	{
		return;
	}

	float tanZ1 = vx1 * args->tan_cos + vy1 * args->tan_sin;
	float tanZ2 = vx2 * args->tan_cos + vy2 * args->tan_sin;

	float texwidth = sprite->sprite_rect_width;

	float u0 = 0;
	float u1 = 1;

	float fsx1, fsz1, fsx2, fsz2;

	if (tx1 >= -tanZ1)
	{
		if (tx1 > tanZ1) return;
		if (tanZ1 == 0) return;
		fsx1 = image->half_width + tx1 * image->half_width / tanZ1;
		fsz1 = tanZ1;
		u0 = 0.0f;
	}
	else
	{
		if (tx2 < -tanZ2) return;
		float den = tx1 - tx2 - tanZ2 + tanZ1;
		if (den == 0) return;
		fsx1 = 0;
		u0 = (tx1 + tanZ1) / den;
		fsz1 = tanZ1 + (tanZ2 - tanZ1) * u0;
	}

	if (fsz1 <= 0)
	{
		return;
	}
	if (tx2 <= tanZ2)
	{
		if (tx2 < -tanZ2) return;
		if (tanZ2 == 0) return;
		fsx2 = image->half_width + tx2 * image->half_width / tanZ2;
		fsz2 = tanZ2;
		u1 = 1.0f;
	}
	else
	{
		if (tx1 > tanZ1) return;
		float den = tanZ2 - tanZ1 - tx2 + tx1;
		if (den == 0) return;
		fsx2 = image->width;
		u1 = (tx1 - tanZ1) / den;
		fsz2 = tanZ1 + (tanZ2 - tanZ1) * u1;
	}

	if (fsz2 <= 0)
	{
		return;
	}

	int x1 = (int)floor(fsx1 + 0.5);
	int x2 = (int)floor(fsx2 + 0.5);

	//not visible
	if (x1 >= x2 || x2 <= args->start_x || x1 >= args->end_x)
	{
		return;
	}

	FrameInfo* frame_info = Image_GetFrameInfo(sprite->img, sprite->frame);

	if (!frame_info)
	{
		return;
	}
	int min_x = (sprite->flip_h) ? ((sprite->sprite_rect_width)-(frame_info->max_real_x)) : frame_info->min_real_x;
	int max_x = (sprite->flip_h) ? ((sprite->sprite_rect_width)-(frame_info->min_real_x)) : frame_info->max_real_x;

	u0 = fabs(u0);
	u1 = fabs(u1);

	u0 *= texwidth;
	u1 *= texwidth;

	Sector* frontsector = Map_GetSector(decal_line->front_sector);
	Sector* backsector = NULL;

	if (decal_line->back_sector >= 0)
	{
		backsector = Map_GetSector(decal_line->back_sector);
	}

	float yscale1 = args->v_fov * (1.0 / fsz1);
	float yscale2 = args->v_fov * (1.0 / fsz2);

	float front_ytop = frontsector->r_ceil - args->view_z;
	float front_ybottom = frontsector->r_floor - args->view_z;

	int front_top_y1 = image->half_height - (int)(front_ytop * yscale1);
	int front_top_y2 = image->half_height - (int)(front_ytop * yscale2);

	int front_bottom_y1 = image->half_height - (int)(front_ybottom * yscale1);
	int front_bottom_y2 = image->half_height - (int)(front_ybottom * yscale2);

	float back_ytop = 0;
	float back_ybottom = 0;

	int back_y_top1 = 0;
	int back_y_top2 = 0;

	int back_y_bottom1 = 0;
	int back_y_bottom2 = 0;

	if (backsector)
	{
		back_ytop = backsector->r_ceil - args->view_z;
		back_ybottom = backsector->r_floor - args->view_z;

		back_y_top1 = image->half_height - (int)(back_ytop * yscale1);
		back_y_top2 = image->half_height - (int)(back_ytop * yscale2);

		back_y_bottom1 = image->half_height - (int)(back_ybottom * yscale1);
		back_y_bottom2 = image->half_height - (int)(back_ybottom * yscale2);
	}

	float ytop = (sprite->z + half_sprite_height) - args->view_z;
	float ybottom = (sprite->z - half_sprite_height) - args->view_z;

	int top_y1 = (image->half_height) - (int)((ytop)*yscale1);
	int top_y2 = (image->half_height) - (int)((ytop)*yscale2);

	int bottom_y1 = (image->half_height) - (int)((ybottom)*yscale1);
	int bottom_y2 = (image->half_height) - (int)((ybottom)*yscale2);

	int begin_x = max(x1, args->start_x);
	int end_x = min(x2, args->end_x);

	tz1 = tanZ1;
	tz2 = tanZ2;

	float xl = 1.0 / fabs(x2 - x1);
	float x_pos = fabs(begin_x - x1);
	float x_pos2 = fabs(x2 - begin_x);
	float ceil_step = (top_y2 - top_y1) * xl;
	float floor_step = (bottom_y2 - bottom_y1) * xl;
	float depth_step = (tz2 - tz1)* xl;

	float front_ceil_step = (front_top_y2 - front_top_y1) * xl;
	float front_bottom_step = (front_bottom_y2 - front_bottom_y1) * xl;

	float back_ceil_step = (back_y_top2 - back_y_top2) * xl;
	float back_bottom_step = (back_y_bottom2 - back_y_bottom1) * xl;

	float texheight = sprite->sprite_rect_height;

	for (int x = begin_x; x < end_x; x++)
	{
		int tx = (u0 * (x_pos2 * tz2) + u1 * (x_pos * tz1)) / (x_pos2 * tz2 + x_pos * tz1);

		if (tx < min_x)
		{
			x_pos++;
			x_pos2--;
			continue;
		}
		else if (tx > max_x)
		{
			//break;
		}
		int span_x = (sprite->flip_h) ? (sprite->sprite_rect_width - tx) : tx;

		AlphaSpan* span = FrameInfo_GetAlphaSpan(frame_info, span_x);

		if (span->min > span->max)
		{
			x_pos++;
			x_pos2--;
			continue;
		}

		int min_y = (sprite->flip_v) ? (sprite->sprite_rect_height - (span->max)) : span->min;
		int max_y = (sprite->flip_v) ? (sprite->sprite_rect_height - (span->min)) : span->max;

		tx += sprite->frame_offset_x * sprite->sprite_rect_width;

		float depth = (int)(x_pos * depth_step) + tz1;
		depth *= 0.95;
		int depth_light = 255;//Math_Clampl(sprite->light - (depth * DEPTH_SHADING_SCALE), 0, 255);

		float yceil = (int)(x_pos * ceil_step) + top_y1;
		float yfloor = (int)(x_pos * floor_step) + bottom_y1;

		int c_yceil = 0;
		int c_yfloor = 0;

		float front_ytop = (int)(x_pos * front_ceil_step) + front_top_y1;
		float front_ybottom = (int)(x_pos * front_bottom_step) + front_bottom_y1;

		int c_front_ytop = Math_Clampl(front_ytop, 0, image->height - 1);
		int c_front_ybottom = Math_Clampl(front_ybottom, 0, image->height - 1);

		c_yceil = Math_Clampl(yceil, c_front_ytop, c_front_ybottom);
		c_yfloor = Math_Clampl(yfloor, c_front_ytop, c_front_ybottom);

		if (backsector)
		{
			float back_ytop = (int)(x_pos * back_ceil_step) + back_y_top1;
			float back_ybottom = (int)(x_pos * back_bottom_step) + back_y_bottom1;

			int c_back_ytop = Math_Clampl(back_ytop, 0, image->height - 1);
			int c_back_ybottom = Math_Clampl(back_ybottom, 0, image->height - 1);

			c_yceil = Math_Clampl(c_yceil, c_back_ybottom, c_front_ybottom);
			c_yfloor = Math_Clampl(c_yfloor, c_back_ybottom, c_front_ybottom);
		}

		float yl = 1.0 / max(fabs(yfloor - yceil), 0.001);
		float ty_step = texheight * yl;

		float ty_pos = (fabs(c_yceil - yceil)) * ty_step;
		float ty_local_pos = ty_pos;
		ty_pos += sprite->frame_offset_y * sprite->sprite_rect_height;

		for (int y = c_yceil; y < c_yfloor; y++)
		{
			if ((int)ty_local_pos < min_y)
			{
				ty_pos += ty_step;
				ty_local_pos += ty_step;
				continue;
			}
			else if ((int)ty_local_pos > max_y)
			{
				//break;
			}

			if (depth <= args->depth_buffer[x + y * image->width])
			{
				int ty = (int)ty_pos;
				unsigned char* tex_color = Image_Get(sprite->img, tx, ty);

				if (tex_color[3] > 128)
				{
					size_t index = ((size_t)x + (size_t)y * (size_t)image->width) * (size_t)image->numChannels;

					//avoid loops
					image->data[index + 0] = LIGHT_LUT[image->data[index + 0]][128] + LIGHT_LUT[LIGHT_LUT[tex_color[0]][depth_light]][2];
					image->data[index + 1] = LIGHT_LUT[image->data[index + 1]][128] + LIGHT_LUT[LIGHT_LUT[tex_color[1]][depth_light]][2];
					image->data[index + 2] = LIGHT_LUT[image->data[index + 2]][128] + LIGHT_LUT[LIGHT_LUT[tex_color[2]][depth_light]][2];
				}
			}
			
			ty_pos += ty_step;
			ty_local_pos += ty_step;
		}

		x_pos++;
		x_pos2--;
	}
}

void Video_DrawWallCollumn(Image* image, float* depth_buffer, Texture* texture, int x, int y1, int y2, float depth, int tx, float ty_pos, float ty_step, int lx, float ly_pos, Vec3_u16 light, int height_mask, Lightmap* lm)
{
	unsigned char* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1) * (size_t)image->width;

	tx &= texture->width_mask;

	//optimized lightmap only loop
	if (lm && lm->data)
	{
		float flx = Math_Clamp((float)lx * LIGHTMAP_INV_LUXEL_SIZE, 0, lm->width - 1);
		float flx_frac = flx - (int)flx;

		float next_flx = Math_Clampl(flx + 1, 0, lm->width - 1);

		float fly = (ly_pos * LIGHTMAP_INV_LUXEL_SIZE);
		int prev_ly = -1;

		float fly_step = ty_step * LIGHTMAP_INV_LUXEL_SIZE;

		Vec4 lerp_light0 = Vec4_Zero();
		Vec4 lerp_light1 = Vec4_Zero();
		Vec4 lerp_dx = Vec4_Zero();

		for (int y = y1; y < y2; y++)
		{
			int ty = (int)ty_pos & height_mask;

			unsigned char* data = Image_Get(&texture->img, tx, ty);

			int ly = (int)fly;
			float fly_frac = fly - ly;

			//sample 2 points between this and the next luxel
			if (ly != prev_ly)
			{
				Lightmap_SampleWallLinearPoints(lm, flx, fly, next_flx, flx_frac, &lerp_light0, &lerp_light1);

				lerp_dx.r = lerp_light1.r - lerp_light0.r;
				lerp_dx.g = lerp_light1.g - lerp_light0.g;
				lerp_dx.b = lerp_light1.b - lerp_light0.b;

				lerp_light0.r = lerp_light0.r + lerp_dx.r * fly_frac;
				lerp_light0.g = lerp_light0.g + lerp_dx.g * fly_frac;
				lerp_light0.b = lerp_light0.b + lerp_dx.b * fly_frac;

				lerp_dx.r *= fly_step;
				lerp_dx.g *= fly_step;
				lerp_dx.b *= fly_step;

				prev_ly = ly;
			}

			//light step
			lerp_light0.r += lerp_dx.r;
			lerp_light0.g += lerp_dx.g;
			lerp_light0.b += lerp_dx.b;

			int current_light_r = lerp_light0.r;
			int current_light_g = lerp_light0.g;
			int current_light_b = lerp_light0.b;

			//clamp
			current_light_r = Math_Clampl(current_light_r + light.r, 0, 255);
			current_light_g = Math_Clampl(current_light_g + light.g, 0, 255);
			current_light_b = Math_Clampl(current_light_b + light.b, 0, 255);

			size_t i = index * 4;

			//avoid loops
			dest[i + 0] = LIGHT_LUT[data[0]][current_light_r];
			dest[i + 1] = LIGHT_LUT[data[1]][current_light_g];
			dest[i + 2] = LIGHT_LUT[data[2]][current_light_b];

			depth_buffer[index] = depth;

			ty_pos += ty_step;
			fly += fly_step;
			index += image->width;
		}
	}
	else
	{
		int max_light = max(light.r, max(light.g, light.b));

		if (max_light > 0)
		{
			for (int y = y1; y < y2; y++)
			{
				int ty = (int)ty_pos & height_mask;

				unsigned char* data = Image_Get(&texture->img, tx, ty);

				size_t i = index * 4;

				//avoid loops
				dest[i + 0] = LIGHT_LUT[data[0]][light.r];
				dest[i + 1] = LIGHT_LUT[data[1]][light.g];
				dest[i + 2] = LIGHT_LUT[data[2]][light.b];

				depth_buffer[index] = depth;

				ty_pos += ty_step;
				index += image->width;
			}
		}
		else
		{
			//set depth and also set image to black
			for (int y = y1; y < y2; y++)
			{
				size_t i = index * 4;

				//avoid loops
				dest[i + 0] = 0;
				dest[i + 1] = 0;
				dest[i + 2] = 0;

				depth_buffer[index] = depth;

				index += image->width;
			}
		}	
	}

}

void Video_DrawWallCollumnDepth(Image* image, Texture* texture, Lightmap* lm, float* depth_buffer, int x, int y1, int y2, float z, int tx, float ty_pos, float ty_step, int light, int height_mask)
{
	unsigned char* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1 + 1) * (size_t)image->width;

	//optimized lightmap only loop
	if (lm && lm->data)
	{
		float flx = Math_Clamp((float)tx * LIGHTMAP_INV_LUXEL_SIZE, 0, lm->width - 1);
		float flx_frac = flx - (int)flx;

		float next_flx = Math_Clampl(flx + 1, 0, lm->width - 1);

		float fly = (ty_pos * LIGHTMAP_INV_LUXEL_SIZE);
		int prev_ly = -1;

		float fly_step = ty_step * LIGHTMAP_INV_LUXEL_SIZE;

		Vec4 lerp_light0 = Vec4_Zero();
		Vec4 lerp_light1 = Vec4_Zero();
		Vec4 lerp_dx = Vec4_Zero();

		tx &= texture->width_mask;

		for (int y = y1; y < y2; y++)
		{
			int ty = (int)ty_pos & height_mask;

			unsigned char* data = Image_Get(&texture->img, tx, ty);

			int ly = (int)fly;
			float fly_frac = fly - ly;

			//sample 2 points between this and the next luxel
			if (ly != prev_ly)
			{
				Lightmap_SampleWallLinearPoints(lm, flx, fly, next_flx, flx_frac, &lerp_light0, &lerp_light1);

				lerp_dx.r = lerp_light1.r - lerp_light0.r;
				lerp_dx.g = lerp_light1.g - lerp_light0.g;
				lerp_dx.b = lerp_light1.b - lerp_light0.b;

				lerp_light0.r = lerp_light0.r + lerp_dx.r * fly_frac;
				lerp_light0.g = lerp_light0.g + lerp_dx.g * fly_frac;
				lerp_light0.b = lerp_light0.b + lerp_dx.b * fly_frac;

				lerp_dx.r *= fly_step;
				lerp_dx.g *= fly_step;
				lerp_dx.b *= fly_step;

				prev_ly = ly;
			}

			//light step
			lerp_light0.r += lerp_dx.r;
			lerp_light0.g += lerp_dx.g;
			lerp_light0.b += lerp_dx.b;

			if (data[3] > 128 && z <= depth_buffer[index])
			{
				int current_light_r = lerp_light0.r;
				int current_light_g = lerp_light0.g;
				int current_light_b = lerp_light0.b;

				//clamp
				current_light_r = Math_Clampl(current_light_r, 0, 255);
				current_light_g = Math_Clampl(current_light_g, 0, 255);
				current_light_b = Math_Clampl(current_light_b, 0, 255);

				size_t i = index * 4;

				//avoid loops
				dest[i + 0] = LIGHT_LUT[data[0]][current_light_r];
				dest[i + 1] = LIGHT_LUT[data[1]][current_light_g];
				dest[i + 2] = LIGHT_LUT[data[2]][current_light_b];

				depth_buffer[index] = z;
			}

			ty_pos += ty_step;
			fly += fly_step;
			index += image->width;
		}
	}
	else if (light >= 0)
	{
		tx &= texture->width_mask;
		for (int y = y1 + 1; y < y2; y++)
		{
			int ty = (int)ty_pos & height_mask;

			unsigned char* data = Image_Get(&texture->img, tx, ty);

			if (data[3] > 128 && z <= depth_buffer[index])
			{
				size_t i = index * 4;

				//avoid loops
				dest[i + 0] = LIGHT_LUT[data[0]][light];
				dest[i + 1] = LIGHT_LUT[data[1]][light];
				dest[i + 2] = LIGHT_LUT[data[2]][light];

				depth_buffer[index] = z;
			}

			ty_pos += ty_step;
			index += image->width;
		}
	}
}

void Video_DrawSkyPlaneStripe(Image* image, float* depth_buffer, Texture* texture, int x, int y1, int y2, LineDrawArgs* args)
{
	if (y2 <= y1)
	{
		return;
	}

	const float TEX_HEIGHT_SCALE = 0.35;
	const float TEX_Y_OFFSET = 28.0 * 0.5;

	unsigned char* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1) * (size_t)image->width;

	float tex_cylinder = texture->img.width;
	float tex_height = texture->img.height * TEX_HEIGHT_SCALE;

	float x_angle = (0.5 - x / (float)image->width) * args->draw_args->tangent * Math_DegToRad(90);
	float angle = (args->plane_angle + x_angle) * tex_cylinder;

	angle = fabs(angle);

	float tex_y_step = args->sky_plane_y_step * tex_height;
	float tex_y_pos = (float)y1 * tex_y_step;

	tex_y_pos += TEX_Y_OFFSET;

	int tex_x = (int)angle & texture->width_mask;

	for (int y = y1; y < y2; y++)
	{
		unsigned char* data = Image_Get(&texture->img, tex_x, (int)tex_y_pos & texture->img.height - 1);

		size_t i = index * 4;

		//avoid loops
		dest[i + 0] = data[0];
		dest[i + 1] = data[1];
		dest[i + 2] = data[2];

		depth_buffer[index] = DEPTH_CLEAR;

		tex_y_pos += tex_y_step;
		index += image->width;
	}
}
void Video_DrawPlaneSpan(Image* image, DrawPlane* plane, LineDrawArgs* args, int y, int x1, int x2)
{
	//return;
	Texture* texture = plane->texture;

	float* depth_buffer = args->draw_args->depth_buffer;

	unsigned char* dest = image->data;
	size_t index = (size_t)x1 + (size_t)(y) * (size_t)image->width;

	//setup
	float distance = fabs(plane->viewheight * args->draw_args->yslope[y]);

	if (isnan(distance))
	{
		return;
	}

	float x_step = distance * args->plane_step_scale_x;
	float y_step = distance * args->plane_step_scale_y;

	float x_pos = args->plane_base_pos_x + args->plane_step_scale_x * (x1 - args->x1);
	float y_pos = args->plane_base_pos_y + args->plane_step_scale_y * (x1 - args->x1);

	x_pos = distance * x_pos + args->draw_args->view_x;
	y_pos = distance * y_pos + -args->draw_args->view_y;

	x_pos *= 2;
	x_step *= 2;

	y_pos *= 2;
	y_step *= 2;

	Vec3_u16 extra_light = args->draw_args->extra_light;
	float depth_scale = (distance * DEPTH_SHADING_SCALE);

	//optimized lightmap only loop
	if (plane->lightmap && plane->lightmap->data)
	{
		if (args->draw_args->extra_light_max > 0)
		{
			extra_light.r = Math_Clampl(extra_light.r - depth_scale, 0, MAX_LIGHT_VALUE - 1);
			extra_light.g = Math_Clampl(extra_light.g - depth_scale, 0, MAX_LIGHT_VALUE - 1);
			extra_light.b = Math_Clampl(extra_light.b - depth_scale, 0, MAX_LIGHT_VALUE - 1);
		}

		Sector* sector = args->sector;

		float sector_size_x = sector->bbox[1][0] * 2;
		float sector_size_y = sector->bbox[1][1] * 2;

		Vec3_u16 s0 = Vec3_u16_Zero();
		Vec3_u16 s1 = Vec3_u16_Zero();
		Vec3_u16 s2 = Vec3_u16_Zero();
		Vec3_u16 s3 = Vec3_u16_Zero();

		Vec4 lerp0 = Vec4_Zero();
		Vec4 lerp1 = Vec4_Zero();

		Vec4 sdx0 = Vec4_Zero();
		Vec4 sdx1 = Vec4_Zero();
		
		float flx_pos = ((x_pos - (sector_size_x)) * LIGHTMAP_INV_LUXEL_SIZE);
		float flx_step = x_step * LIGHTMAP_INV_LUXEL_SIZE;

		float fly_pos = ((y_pos + (sector_size_y)) * LIGHTMAP_INV_LUXEL_SIZE);
		float fly_step = y_step * LIGHTMAP_INV_LUXEL_SIZE;

		int prev_lx = -1;
		int prev_ly = -1;

		for (int x = x1; x <= x2; x++)
		{
			float flx = flx_pos + plane->lightmap->width;
			float fly = fly_pos;

			int lx = (int)flx;
			int ly = (int)fly;

			float lx_frac = flx - (int)flx;
			float ly_frac = fly - (int)fly;

			if (lx != prev_lx || ly != prev_ly)
			{
				Lightmap_SamplePlaneLinearPoints(plane->lightmap, flx, fly, &s0, &s1, &s2, &s3);

				//hyper optimization lunacy
				sdx0.r = (s1.r - s0.r);
				sdx0.g = (s1.g - s0.g);
				sdx0.b = (s1.b - s0.b);
				
				sdx1.r = (s3.r - s2.r);
				sdx1.g = (s3.g - s2.g);
				sdx1.b = (s3.b - s2.b);

				lerp0.r = s0.r + sdx0.r * lx_frac;
				lerp0.g = s0.g + sdx0.g * lx_frac;
				lerp0.b = s0.b + sdx0.b * lx_frac;

				lerp1.r = s2.r + sdx1.r * lx_frac;
				lerp1.g = s2.g + sdx1.g * lx_frac;
				lerp1.b = s2.b + sdx1.b * lx_frac;

				//calc step size
				sdx0.r *= flx_step;
				sdx0.g *= flx_step;
				sdx0.b *= flx_step;

				sdx1.r *= flx_step;
				sdx1.g *= flx_step;
				sdx1.b *= flx_step;

				prev_lx = lx;
				prev_ly = ly;
			}

			lerp0.r += sdx0.r;
			lerp0.g += sdx0.g;
			lerp0.b += sdx0.b;
					
			lerp1.r += sdx1.r;
			lerp1.g += sdx1.g;
			lerp1.b += sdx1.b;

			int light_r = Math_lerp(lerp0.r, lerp1.r, ly_frac);
			int light_g = Math_lerp(lerp0.g, lerp1.g, ly_frac);
			int light_b = Math_lerp(lerp0.b, lerp1.b, ly_frac);

			light_r = Math_Clampl(light_r + extra_light.r, 0, MAX_LIGHT_VALUE - 1);
			light_g = Math_Clampl(light_g + extra_light.g, 0, MAX_LIGHT_VALUE - 1);
			light_b = Math_Clampl(light_b + extra_light.b, 0, MAX_LIGHT_VALUE - 1);;

			unsigned char* data = Image_GetFast(&texture->img, (int)x_pos & 63, (int)y_pos & 63);

			index = ((size_t)x + (size_t)y * (size_t)image->width);
			size_t i = index * 4;

			//avoid loops
			dest[i + 0] = LIGHT_LUT[data[0]][light_r];
			dest[i + 1] = LIGHT_LUT[data[1]][light_g];
			dest[i + 2] = LIGHT_LUT[data[2]][light_b];

			depth_buffer[index] = distance;

			x_pos += x_step;
			y_pos += y_step;
			flx_pos += flx_step;
			fly_pos += fly_step;
		}
	}
	else
	{
		int light_r = Math_Clampl((plane->light + extra_light.r) - depth_scale, 0, MAX_LIGHT_VALUE - 1);
		int light_g = Math_Clampl((plane->light + extra_light.g) - depth_scale, 0, MAX_LIGHT_VALUE - 1);
		int light_b = Math_Clampl((plane->light + extra_light.b) - depth_scale, 0, MAX_LIGHT_VALUE - 1);

		for (int x = x1; x <= x2; x++)
		{
			unsigned char* data = Image_GetFast(&texture->img, (int)x_pos & 63, (int)y_pos & 63);

			index = ((size_t)x + (size_t)y * (size_t)image->width);
			size_t i = index * 4;

			//avoid loops
			dest[i + 0] = LIGHT_LUT[data[0]][light_r];
			dest[i + 1] = LIGHT_LUT[data[1]][light_g];
			dest[i + 2] = LIGHT_LUT[data[2]][light_b];

			depth_buffer[index] = distance;

			x_pos += x_step;
			y_pos += y_step;
		}
	}
	


	
}

void Video_Shade(Image* image, ShaderFun shader_fun, int x0, int y0, int x1, int y1)
{
	if (!shader_fun)
	{
		return;
	}

	//clamp all the values
	x0 = Math_Clampl(x0, 0, image->width);
	y0 = Math_Clampl(y0, 0, image->height);

	x1 = Math_Clampl(x1, 0, image->width);
	y1 = Math_Clampl(y1, 0, image->height);

	for (int x = x0; x < x1; x++)
	{
		for (int y = y0; y < y1; y++)
		{
			shader_fun(image, x, y, 0, 0);
		}
	}

}
