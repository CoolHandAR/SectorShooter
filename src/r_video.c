#include "r_common.h"

#include "g_common.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "u_math.h"
#include <stdio.h>

static const float PI = 3.14159265359;
unsigned char LIGHT_LUT[256][256];

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

void Video_Setup()
{
	//setup light lut
	for (int i = 0; i < 256; i++)
	{
		for (int k = 0; k < 256; k++)
		{
			float light = (float)k / 255.0f;
			
			int l = (float)i * light;

			if (l < 0) l = 0;
			else if (l > 255) l = 255;

			LIGHT_LUT[i][k] = (unsigned char)l;		
		}
	}
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

void Video_DrawBox(Image* image, float box[2][3], float view_x, float view_y, float view_z, float view_cos, float view_sin, float h_fov, float v_fov)
{
	float vx1 = box[0][0] - view_x;
	float vy1 = box[0][1] - view_y;

	float vx2 = box[1][0] - view_x;
	float vy2 = box[1][1] - view_y;

	if (vy1 * (vx1 - vx2) + vx1 * (vy2 - vy1) <= -MATH_EQUAL_EPSILON)
	{
		return;
	}

	float tx1 = vx1 * view_sin - vy1 * view_cos;
	float tz1 = vx1 * view_cos + vy1 * view_sin;

	float tx2 = vx2 * view_sin - vy2 * view_cos;
	float tz2 = vx2 * view_cos + vy2 * view_sin;

	//completely behind the view plane
	if (tz1 <= 0 && tz2 <= 0)
	{
		return;
	}

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
	}

	float inv_tz1 = 1.0 / tz1;
	float inv_tz2 = 1.0 / tz2;

	float xscale1 = h_fov * inv_tz1;
	float xscale2 = h_fov * inv_tz2;

	int x1 = (image->half_width) - (int)(tx1 * xscale1);
	int x2 = (image->half_width) - (int)(tx2 * xscale2);

	//not visible
	if (x1 >= x2 || x2 <= 0 || x1 >= image->width)
	{
		return;
	}
	float yscale1 = v_fov * inv_tz1;
	float yscale2 = v_fov * inv_tz2;

	float yceil = box[1][2] - view_z;
	float yfloor = box[0][2] - view_z;

	int ceil_y1 = (image->half_height) - (int)((yceil)*yscale1);
	int ceil_y2 = (image->half_height) - (int)((yceil)*yscale2);

	int floor_y1 = (image->half_height) - (int)((yfloor)*yscale1);
	int floor_y2 = (image->half_height) - (int)((yfloor)*yscale2);

	int begin_x = max(x1, 0);
	int end_x = min(x2, image->width - 1);

	float xl = 1.0 / fabs(x2 - x1);
	float ceil_step = (ceil_y2 - ceil_y1) * xl;
	float floor_step = (floor_y2 - floor_y1) * xl;

	float x_pos = fabs(begin_x - x1);

	for (int x = begin_x; x <= end_x; x++)
	{
		float yceil = (int)(x_pos * ceil_step) + ceil_y1;
		float yfloor = (int)(x_pos * floor_step) + floor_y1;

		//clamped
		int c_yceil = Math_Clamp(yceil, 0, image->height - 1);
		int c_yfloor = Math_Clamp(yfloor, 0, image->height - 1);

		for (int y = yceil; y <= yfloor; y++)
		{
			unsigned char color[4] = { 255, 255, 255, 255 };
			Image_Set2(image, x, y, color);
		}

		x_pos++;
	}

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

	int light = sprite->light * 255.0;

	if (light < 0) light = 0;
	else if (light > 255) light = 255;

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

			unsigned char clr[4] = { LIGHT_LUT[color[0]][light], LIGHT_LUT[color[1]][light], LIGHT_LUT[color[2]][light], 255 };

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

	if (transform_x >= -transform_y_tan && transform_x > transform_y_tan) return;
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
	
	float pixel_size = (1.0 / (float)(sprite_rect_width)) * sprite->scale_x;
	float pixel_height_size = (1.0 / (float)sprite_rect_height) * sprite->scale_y;

	int sprite_screen_start_x = (int)((image->half_width) - ((transform_x) * xscale));
	sprite_screen_start_x = sprite_screen_x;

	int draw_start_x = -sprite_half_width + sprite_screen_start_x;
	if (draw_start_x < args->start_x) draw_start_x = args->start_x;

	if (draw_start_x >= args->end_x)
	{
		return;
	}

	int sprite_screen_end_x = (int)((image->half_width) - ((transform_x) * xscale));
	sprite_screen_end_x = sprite_screen_x;

	int draw_end_x = sprite_half_width + sprite_screen_end_x;
	if (draw_end_x > args->end_x) draw_end_x = args->end_x;

	if (draw_end_x <= args->start_x)
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

	int light = sprite->light;

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
		int depth_light = Math_Clampl(light - (depth * DEPTH_SHADING_SCALE), 0, 255);

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

			if (tex_data[3] > 128 && depth < args->depth_buffer[dest_index])
			{
				args->depth_buffer[dest_index] = depth;

				size_t i = dest_index * 4;

				//avoid loops, this is faster
				dest[i + 0] = LIGHT_LUT[tex_data[0]][depth_light];
				dest[i + 1] = LIGHT_LUT[tex_data[1]][depth_light];
				dest[i + 2] = LIGHT_LUT[tex_data[2]][depth_light];
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

	Line* decal_line = Map_GetLine(sprite->decal_line_index);

	float half_sprite_width = sprite->sprite_rect_width / 2;
	float half_sprite_height = sprite->sprite_rect_height / 2;

	float decal_rect_left = half_sprite_width / 4;
	float decal_rect_right = half_sprite_width / 4;

	float line_normal_x = decal_line->dx;
	float line_normal_y = decal_line->dy;
	Math_XY_Normalize(&line_normal_x, &line_normal_y);

	float left = 0;
	if (fabs(decal_line->side_dx) > fabs(decal_line->side_dy))
	{	
		left = (sprite->x - decal_line->x0) / decal_line->side_dx;
	}
	else if(decal_line->side_dy != 0)
	{
		left = (sprite->y - decal_line->y0) / decal_line->side_dy;
	}
	float decal_x = decal_line->x0 + left * decal_line->side_dx;
	float decal_y = decal_line->y0 + left * decal_line->side_dy;

	float vx2 = decal_x - decal_rect_left * line_normal_x - args->view_x;
	float vy2 = decal_y - decal_rect_left * line_normal_y - args->view_y;

	float vx1 = decal_x + decal_rect_right * line_normal_x - args->view_x;
	float vy1 = decal_y + decal_rect_right * line_normal_y - args->view_y;

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
	{
		float t1 = 0, t2 = 0;
		if (fabs(decal_line->side_dx) > fabs(decal_line->side_dy))
		{
			t1 = (decal_line->x1 - decal_line->side_x1) / decal_line->side_dx;
			t2 = (decal_line->x0 - decal_line->side_x1) / decal_line->side_dx;
		}
		else
		{
			t1 = (decal_line->y1 - decal_line->side_y1) / decal_line->side_dy;
			t2 = (decal_line->y0 - decal_line->side_y1) / decal_line->side_dy;
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

	float ytop = (sprite->z + half_sprite_height) - args->view_z;
	float ybottom = (sprite->z - half_sprite_height) - args->view_z;

	int top_y1 = (image->half_height) - (int)((ytop)*yscale1);
	int top_y2 = (image->half_height) - (int)((ytop)*yscale2);

	int bottom_y1 = (image->half_height) - (int)((ybottom)*yscale1);
	int bottom_y2 = (image->half_height) - (int)((ybottom)*yscale2);

	float yheight = sprite->z - args->view_z;

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
			break;
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
		int depth_light = Math_Clampl(sprite->light - (depth * DEPTH_SHADING_SCALE), 0, 255);

		float yceil = (int)(x_pos * ceil_step) + top_y1;
		float yfloor = (int)(x_pos * floor_step) + bottom_y1;

		int c_yceil = Math_Clamp(yceil, 0, image->height - 1);
		int c_yfloor = Math_Clamp(yfloor, 0, image->height - 1);

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
				break;
			}

			if (depth <= args->depth_buffer[x + y * image->width])
			{
				int ty = (int)ty_pos;
				unsigned char* tex_color = Image_Get(sprite->img, tx, ty);

				if (tex_color[3] > 128)
				{
					size_t index = ((size_t)x + (size_t)y * (size_t)image->width) * (size_t)image->numChannels;

					image->data[index + 0] = LIGHT_LUT[tex_color[0]][depth_light];
					image->data[index + 1] = LIGHT_LUT[tex_color[1]][depth_light];
					image->data[index + 2] = LIGHT_LUT[tex_color[2]][depth_light];
				}
			}
			
			ty_pos += ty_step;
			ty_local_pos += ty_step;
		}

		x_pos++;
		x_pos2--;
	}
}

void Video_DrawWallCollumn(Image* image, float* depth_buffer, Texture* texture, int x, int y1, int y2, float depth, int tx, float ty_pos, float ty_step, int light, int height_mask)
{
	tx &= texture->width_mask;

	unsigned char* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1) * (size_t)image->width;

	if (light > 0)
	{
		for (int y = y1; y < y2; y++)
		{
			int ty = (int)ty_pos & height_mask;

			unsigned char* data = Image_Get(&texture->img, tx, ty);

			size_t i = index * 4;

			//avoid loops
			dest[i + 0] = LIGHT_LUT[data[0]][light];
			dest[i + 1] = LIGHT_LUT[data[1]][light];
			dest[i + 2] = LIGHT_LUT[data[2]][light];

			depth_buffer[index] = depth;

			ty_pos += ty_step;
			index += image->width;
		}
	}
	else
	{
		//only set depth
		for (int y = y1; y < y2; y++)
		{
			depth_buffer[index += image->width] = depth;
		}
	}
}

void Video_DrawWallCollumnDepth(Image* image, Texture* texture, float* depth_buffer, int x, int y1, int y2, float z, int tx, float ty_pos, float ty_step, int light, int height_mask)
{
	tx &= texture->width_mask;

	unsigned char* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1 + 1) * (size_t)image->width;

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

void Video_DrawSkyPlaneStripe(Image* image, float* depth_buffer, Texture* texture, int x, int y1, int y2, LineDrawArgs* args)
{
	if (y2 <= y1)
	{
		return;
	}

	unsigned char* dest = image->data;
	size_t index = (size_t)x + (size_t)(y1) * (size_t)image->width;

	float tex_cylinder = texture->img.width;
	float tex_height = texture->img.height * 0.35;

	float x_angle = (0.5 - x / (float)image->width) * args->draw_args->tangent * Math_DegToRad(90);
	float angle = (args->plane_angle + x_angle) * tex_cylinder;

	angle = fabs(angle);

	float tex_y_step = args->sky_plane_y_step * tex_height;
	float tex_y_pos = (float)y1 * tex_y_step;

	tex_y_pos += 28 * 0.5;

	int tex_x = (int)angle & texture->width_mask;

	for (int y = y1; y < y2; y++)
	{
		unsigned char* data = Image_Get(&texture->img, tex_x, (int)tex_y_pos & texture->img.height - 1);

		size_t i = index * 4;

		//avoid loops
		dest[i + 0] = data[0];
		dest[i + 1] = data[1];
		dest[i + 2] = data[2];

		tex_y_pos += tex_y_step;
		index += image->width;
	}
}
void Video_DrawPlaneSpan(Image* image, DrawPlane* plane, LineDrawArgs* args, int y, int x1, int x2)
{
	Texture* texture = plane->texture;

	float* depth_buffer = args->draw_args->depth_buffer;

	unsigned char* dest = image->data;
	size_t index = (size_t)x1 + (size_t)(y) * (size_t)image->width;

	//setup
	float distance = fabs(plane->viewheight * args->draw_args->yslope[y]);

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

	int light = Math_Clampl(plane->light - (distance * DEPTH_SHADING_SCALE), 0, 255);

	for (int x = x1; x <= x2; x++)
	{
		unsigned char* data = Image_GetFast(&texture->img, (int)y_pos & 63, (int)x_pos & 63);

		index = ((size_t)x + (size_t)y * (size_t)image->width);
		size_t i = index * 4;

		//avoid loops
		dest[i + 0] = LIGHT_LUT[data[0]][light];
		dest[i + 1] = LIGHT_LUT[data[1]][light];
		dest[i + 2] = LIGHT_LUT[data[2]][light];

		depth_buffer[index] = distance;

		x_pos += x_step;
		y_pos += y_step;
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
