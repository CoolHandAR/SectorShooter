#include "r_common.h"

#include "g_common.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "u_math.h"
#include <stdio.h>

static const float PI = 3.14159265359;
unsigned char LIGHT_LUT[256][256];
#define hfov (0.72f*1080 / 1920)  // Affects the horizontal field of vision
#define vfov (.2f)    // Affects the vertical field of vision
#define W 1920
#define H 1080
int ytop[W] = { 0 }, ybottom[W], renderedsectors[100];
int clamped_y_floor_arr[W];
int clamped_y_ceil_arr[W];
short depth_buf[W][H];
static bool overdraw_check[W][H];

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

void Video_DrawSprite(Image* image, Sprite* sprite, float* depth_buffer, float p_x, float p_y, float p_dirX, float p_dirY, float p_planeX, float p_planeY)
{
	//adapted parts from https://lodev.org/cgtutor/raycasting3.html

	if (!depth_buffer)
	{
		return;
	}

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

	int frame = sprite->frame;

	const int h_frames = sprite->img->h_frames;
	const int v_frames = sprite->img->v_frames;

	int sprite_offset_x = (h_frames > 0) ? frame % h_frames : 0;
	int sprite_offset_y = (v_frames > 0) ? frame / h_frames : 0;

	sprite_offset_x += sprite->frame_offset_x;
	sprite_offset_y += sprite->frame_offset_y;

	int sprite_rect_width = (h_frames > 0) ? sprite->img->width / h_frames : sprite->img->width;
	int sprite_rect_height = (v_frames > 0) ? sprite->img->height / v_frames : sprite->img->height;

	//translate sprite position to relative to camera
	float local_sprite_x = sprite->x - p_x;
	float local_sprite_y = sprite->y - p_y;

	float inv_det = 1.0 / (p_planeX * p_dirY - p_dirX * p_planeY);

	float transform_x = inv_det * (p_dirY * local_sprite_x - p_dirX * local_sprite_y);
	float transform_y = inv_det * (-p_planeY * local_sprite_x + p_planeX * local_sprite_y);

	if (transform_y <= 0)
	{
		return;
	}

	int sprite_screen_x = (int)((image->half_width) * (1 + transform_x / transform_y));

	int height_transform_y = fabs((int)(image->height / (transform_y)));

	int sprite_width = height_transform_y * (sprite->scale_x);
	int sprite_height = height_transform_y * (sprite->scale_y);

	if (sprite_height <= 0 || sprite_width <= 0)
	{
		return;
	}

	int v_move_screen = (int)((sprite->v_offset * image->half_height) / transform_y);
	
	int sprite_half_width = sprite_width / 2;
	int sprite_half_height = sprite_height / 2;

	//calculate lowest and highest pixel to fill in current stripe
	int draw_start_y = -sprite_height / 2 + image->height / 2 + v_move_screen;
	if (draw_start_y < 0) draw_start_y = 0;
	int draw_end_y = sprite_height / 2 + image->height / 2 + v_move_screen;
	if (draw_end_y >= image->height) draw_end_y = image->height - 1;

	if (draw_start_y >= image->height || draw_end_y <= 0)
	{
		return;
	}

	int draw_start_x = -sprite_width / 2 + sprite_screen_x;
	if (draw_start_x < 0) draw_start_x = 0;
	int draw_end_x = sprite_width / 2 + sprite_screen_x;
	if (draw_end_x > image->width) draw_end_x = image->width;

	if (draw_start_x >= image->width || draw_end_x <= 0)
	{
		return;
	}

	int safe_end_x = (draw_end_x >= image->width) ? draw_end_x - 1 : draw_end_x;
	int safe_end_y = (draw_end_y >= image->height) ? draw_end_y - 1 : draw_end_y;

	//all depth edges of a sprite rectangle
	float d0 = depth_buffer[draw_start_x + draw_start_y * image->width]; //top left edge
	float d1 = depth_buffer[safe_end_x + draw_start_y * image->width]; //top right edge
	float d2 = depth_buffer[draw_start_x + safe_end_y * image->width]; //bottom left edge
	float d3 = depth_buffer[safe_end_x + safe_end_y * image->width]; //bottom right edge

	float t0 = inv_det * (-p_planeY * (local_sprite_x - sprite_half_width) + p_planeX * (local_sprite_y - sprite_half_height));
	float t1 = inv_det * (-p_planeY * (local_sprite_x + sprite_half_width) + p_planeX * (local_sprite_y - sprite_half_height));
	float t2 = inv_det * (-p_planeY * (local_sprite_x - sprite_half_width) + p_planeX * (local_sprite_y + sprite_half_height));
	float t3 = inv_det * (-p_planeY * (local_sprite_x + sprite_half_width) + p_planeX * (local_sprite_y + sprite_half_height));

	//check if all edges are fully occluded
	if (t0 >= d0 && t1 >= d1 && t2 >= d2 && t3 >= d3)
	{
		//return;
	}

	int sprite_light = sprite->light * 255;

	int light = 255;

	light += sprite_light;
	light += 255;

	if (light > 255)
	{
		light = 255;
	}
	else if (light < 0)
	{
		light = 0;
	}

	FrameInfo* frame_info = Image_GetFrameInfo(sprite->img, frame + (sprite->frame_offset_x) + (sprite->frame_offset_y * sprite->img->h_frames));

	if (sprite->flip_h)
	{
		sprite_offset_x += 1;
	}

	float transparency = (sprite->transparency > 0) ? (1.0 / min(sprite->transparency, 1)) : 0;

	int min_x = (sprite->flip_h) ? ((sprite_rect_width)-(frame_info->max_real_x)) : frame_info->min_real_x;
	int max_x = (sprite->flip_h) ? ((sprite_rect_width)-(frame_info->min_real_x)) : frame_info->max_real_x;

	if (sprite->flip_h)
	{
		if (min_x > 0) min_x -= 1;
		max_x += 1;
	}

	float x_tex_step = (256 * (float)sprite_rect_width / (float)sprite_width) / 256.0;

	for (int stripe = draw_start_x; stripe < draw_end_x; stripe++)
	{
		float tex_pos_x = (256.0 * (stripe - (-sprite_half_width + sprite_screen_x)) * sprite_rect_width / sprite_width) / 256.0;
		int tex_x = tex_pos_x;

		if (tex_x < min_x)
		{
			continue;
		}
		else if (tex_x > max_x)
		{
			break;
		}

		int span_x = (sprite->flip_h) ? (sprite_rect_width - tex_x) : tex_x;

		AlphaSpan* span = FrameInfo_GetAlphaSpan(frame_info, span_x);

		if (span->min > span->max)
		{
			continue;
		}

		int x_steps = 0;
		float test_step_x = tex_pos_x;

		while ((int)test_step_x == tex_x && (stripe + x_steps) < draw_end_x)
		{
			test_step_x += x_tex_step;
			x_steps++;
		}

		int y_min = (sprite->flip_v) ? (sprite_rect_height - (span->max)) : span->min;
		int y_max = (sprite->flip_v) ? (sprite_rect_height - (span->min)) : span->max;

		if (sprite->flip_v)
		{
			if (y_min > 0) y_min -= 1;
			y_max += 1;
		}
		if (sprite->flip_h)
		{
			tex_x -= 1;
			tex_x = -tex_x;
		}

		bool next_tex_valid = false;
		int next_tex_y = -1;
		int next_d = -1;

		for (int y = draw_start_y; y < draw_end_y; y++)
		{
			int d = next_d;
			int tex_y = next_tex_y;

			if (!next_tex_valid)
			{
				d = (y - v_move_screen) * 256 - image->height * 128 + sprite_height * 128;
				tex_y = ((d * sprite_rect_height) / sprite_height) / 256;
			}

			next_tex_valid = false;

			if (tex_y < y_min)
			{
				continue;
			}
			else if (tex_y > y_max)
			{
				break;
			}

			unsigned char* tex_color = Image_Get(sprite->img, tex_x + (sprite_offset_x * sprite_rect_width), tex_y + (sprite_offset_y * sprite_rect_height));

			//alpha discard if possible
			if (sprite->img->numChannels >= 4)
			{
				if (tex_color[3] < 128)
				{
					continue;
				}
			}

			//apply transparency
			if (transparency > 1)
			{
				for (int l = 0; l < x_steps; l++)
				{
					unsigned char* old_color = Image_Get(image, stripe + l, y);

					//just do directly on the buffer pointer
					for (int k = 0; k < image->numChannels; k++)
					{
						old_color[k] = (old_color[k] / transparency) + (tex_color[k] / 2);
					}

					depth_buffer[(stripe + l) + y * image->width] = transform_y;
				}
			}
			else
			{
				unsigned char color[4] = { LIGHT_LUT[tex_color[0]][light], LIGHT_LUT[tex_color[1]][light], LIGHT_LUT[tex_color[2]][light], 255 };

				next_d = d;
				next_tex_y = tex_y;

				while (y < draw_end_y)
				{
					for (int l = 0; l < x_steps; l++)
					{
						int sl = (stripe + l);

						if (transform_y >= depth_buffer[sl + y * image->width])
						{
							//continue;
						}

						Image_Set2(image, sl, y, color);
						depth_buffer[sl + y * image->width] = transform_y;
					}

					next_d = ((y + 1) - v_move_screen) * 256 - image->height * 128 + sprite_height * 128;
					next_tex_y = ((next_d * sprite_rect_height) / sprite_height) / 256;

					if (next_tex_y != tex_y)
					{
						break;
					}

					y++;

				}

				next_tex_valid = true;
			}
		}

		stripe += x_steps - 1;
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

#define clamp(a, mi,ma)      min(max(a,mi),ma)         // clamp: Clamp value into set range.
static void vline(Image* image, int x, int y1, int y2, int top, int middle, int bottom)
{
	if (y2 == y1)
	{
		return;
	}

	int* pix = (int*)image->data;
	y1 = clamp(y1, 0, image->height - 1);
	y2 = clamp(y2, 0, image->height - 1);
	if (y2 == y1)
		pix[y1 * image->width + x] = middle;
	else if (y2 > y1)
	{
		pix[y1 * image->width + x] = top;
		for (int y = y1 + 1; y < y2; ++y)
		{
			pix[y * image->width + x] = middle;

			if (overdraw_check[x][y])
			{
				printf("overdraw, %i %I \n", x, y);
			}
			overdraw_check[x][y] = true;
		}
		pix[y2 * image->width + x] = bottom;
	}
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



void Video_DrawSprite2(Image* image, float* depth_buffer, Sprite* sprite, float p_x, float p_y, float p_z, float angle)
{
	float anglecos = cosf(angle);
	float anglesin = sinf(angle);

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

	int frame = sprite->frame;

	const int h_frames = sprite->img->h_frames;
	const int v_frames = sprite->img->v_frames;

	int sprite_offset_x = (h_frames > 0) ? frame % h_frames : 0;
	int sprite_offset_y = (v_frames > 0) ? frame / h_frames : 0;

	sprite_offset_x += sprite->frame_offset_x;
	sprite_offset_y += sprite->frame_offset_y;

	int sprite_rect_width = (h_frames > 0) ? sprite->img->width / h_frames : sprite->img->width;
	int sprite_rect_height = (v_frames > 0) ? sprite->img->height / v_frames : sprite->img->height;

	//translate sprite position to relative to camera
	float local_sprite_x = sprite->x - p_x;
	float local_sprite_y = sprite->y - p_y;
	float local_sprite_z = sprite->z - p_z;

	float transform_x = local_sprite_x * anglesin - local_sprite_y * anglecos;
	float transform_y = local_sprite_x * anglecos + local_sprite_y * anglesin;

	if (transform_y <= 0)
	{
		return;
	}
	
	float xscale = (W * hfov) / transform_y;
	float yscale = (H * vfov);

	int sprite_screen_x = (int)((image->half_width) - (transform_x * xscale));

	float height_transform_y = fabs((float)(image->height / (transform_y)));

	int sprite_width = height_transform_y * (sprite->scale_x);
	int sprite_height = height_transform_y * (sprite->scale_y);

	if (sprite_height <= 0 || sprite_width <= 0)
	{
		return;
	}

	int screen_offset_y = (int)((image->half_height) - (local_sprite_z * yscale));

	int v_move_screen = (int)(screen_offset_y / transform_y);

	int sprite_screen_y = (int)((image->half_height)) + v_move_screen;

	int sprite_half_width = sprite_width / 2;
	int sprite_half_height = sprite_height / 2;

	//calculate lowest and highest pixel to fill in current stripe
	int draw_start_y = -sprite_half_height + sprite_screen_y;
	if (draw_start_y < 0) draw_start_y = 0;
	int draw_end_y = sprite_half_height + sprite_screen_y;
	if (draw_end_y >= image->height) draw_end_y = image->height - 1;

	if (draw_start_y >= image->height || draw_end_y <= 0)
	{
		return;
	}

	int draw_start_x = -sprite_half_width + sprite_screen_x;
	if (draw_start_x < 0) draw_start_x = 0;
	int draw_end_x = sprite_half_width + sprite_screen_x;
	if (draw_end_x > image->width) draw_end_x = image->width;

	if (draw_start_x >= image->width || draw_end_x <= 0)
	{
		return;
	}


	int sprite_light = sprite->light * 255;


	int light = 255;

	light += sprite_light;
	light += 255;

	if (light > 255)
	{
		light = 255;
	}
	else if (light < 0)
	{
		light = 0;
	}

	light = 255;

	FrameInfo* frame_info = Image_GetFrameInfo(sprite->img, frame + (sprite->frame_offset_x) + (sprite->frame_offset_y * sprite->img->h_frames));

	if (sprite->flip_h)
	{
		sprite_offset_x += 1;
	}

	float transparency = (sprite->transparency > 0) ? (1.0 / min(sprite->transparency, 1)) : 0;

	int min_x = (sprite->flip_h) ? ((sprite_rect_width)-(frame_info->max_real_x)) : frame_info->min_real_x;
	int max_x = (sprite->flip_h) ? ((sprite_rect_width)-(frame_info->min_real_x)) : frame_info->max_real_x;

	if (sprite->flip_h)
	{
		if (min_x > 0) min_x -= 1;
		max_x += 1;
	}

	float x_tex_step = (256 * (float)sprite_rect_width / (float)sprite_width) / 256.0;

	for (int stripe = draw_start_x; stripe < draw_end_x; stripe++)
	{
		float tex_pos_x = (256.0 * (stripe - (-sprite_half_width + sprite_screen_x)) * sprite_rect_width / sprite_width) / 256.0;
		int tex_x = tex_pos_x;

		if (tex_x < min_x)
		{
			continue;
		}
		else if (tex_x > max_x)
		{
			break;
		}

		int span_x = (sprite->flip_h) ? (sprite_rect_width - tex_x) : tex_x;

		AlphaSpan* span = FrameInfo_GetAlphaSpan(frame_info, span_x);

		if (span->min > span->max)
		{
			continue;
		}

		int x_steps = 0;
		float test_step_x = tex_pos_x;

		while ((int)test_step_x == tex_x && (stripe + x_steps) < draw_end_x)
		{
			test_step_x += x_tex_step;
			x_steps++;
		}

		int y_min = (sprite->flip_v) ? (sprite_rect_height - (span->max)) : span->min;
		int y_max = (sprite->flip_v) ? (sprite_rect_height - (span->min)) : span->max;

		if (sprite->flip_v)
		{
			if (y_min > 0) y_min -= 1;
			y_max += 1;
		}
		if (sprite->flip_h)
		{
			tex_x -= 1;
			tex_x = -tex_x;
		}

		bool next_tex_valid = false;
		int next_tex_y = -1;
		int next_d = -1;

		int clamped_start_y = clamp(draw_start_y, ytop[stripe], ybottom[stripe]);
		int clamped_end_y = clamp(draw_end_y, ytop[stripe], ybottom[stripe]);

		clamped_start_y = draw_start_y;
		clamped_end_y = draw_end_y;

		for (int y = clamped_start_y; y < clamped_end_y; y++)
		{
			int d = next_d;
			int tex_y = next_tex_y;

			if (!next_tex_valid)
			{
				d = (y - v_move_screen) * 256 - image->height * 128 + sprite_height * 128;
				tex_y = ((d * sprite_rect_height) / sprite_height) / 256;
			}

			next_tex_valid = false;

			if (tex_y < y_min)
			{
				continue;
			}
			else if (tex_y > y_max)
			{
				break;
			}

			unsigned char* tex_color = Image_Get(sprite->img, tex_x + (sprite_offset_x * sprite_rect_width), tex_y + (sprite_offset_y * sprite_rect_height));

			//alpha discard if possible
			if (sprite->img->numChannels >= 4)
			{
				if (tex_color[3] < 128)
				{
					continue;
				}
			}

			//apply transparency
			if (transparency > 1)
			{
				for (int l = 0; l < x_steps; l++)
				{
					unsigned char* old_color = Image_Get(image, stripe + l, y);

					//just do directly on the buffer pointer
					for (int k = 0; k < image->numChannels; k++)
					{
						old_color[k] = (old_color[k] / transparency) + (tex_color[k] / 2);
					}

					//depth_buffer[(stripe + l) + y * image->width] = transform_y;
				}
			}
			else
			{
				unsigned char color[4] = { LIGHT_LUT[tex_color[0]][light], LIGHT_LUT[tex_color[1]][light], LIGHT_LUT[tex_color[2]][light], 255 };

				next_d = d;
				next_tex_y = tex_y;

				while (y < draw_end_y)
				{
					for (int l = 0; l < x_steps; l++)
					{
						int sl = (stripe + l);
						if (transform_y - 0.5 >= depth_buffer[sl + y * image->width])
						{
							continue;
						}

						depth_buffer[sl + y * image->width] = transform_y;
						Image_Set2(image, sl, y, color);
						
					}

					next_d = ((y + 1) - v_move_screen) * 256 - image->height * 128 + sprite_height * 128;
					next_tex_y = ((next_d * sprite_rect_height) / sprite_height) / 256;

					if (next_tex_y != tex_y)
					{
						break;
					}

					y++;

				}

				next_tex_valid = true;
			}
		}

		stripe += x_steps - 1;
	}

}

void Video_DrawSectorCollumn(Image* image, Image* texture, int x, int y1, int y2, int tx, float ty_start, float ty_scale)
{
	//nothing to draw
	if (y2 == y1)
	{
		return;
	}

	int* pixels = (int)image->data;

	y1 = clamp(y1, 0, image->height - 1);
	y2 = clamp(y2, 0, image->height - 1);

	for (int y = y1; y < y2; y++)
	{
		int ty = (int)(ty_start++ * ty_scale) & 63;

		int* data = Image_Get(texture, tx, ty);

		pixels[x + y * image->width] = *data;
	}
}

static int bspPointOnSide2(BSPNode* node, float p_x, float p_y)
{
	if (!node->line_dx)
	{
		if (p_x <= node->line_x)
		{
			return node->line_dy > 0;
		}

		return node->line_dy < 0;
	}
	if (!node->line_dy)
	{
		if (p_y <= node->line_y)
		{
			return node->line_dx < 0;
		}
		return node->line_dx > 0;
	}

	float dx = p_x - node->line_x;
	float dy = p_y - node->line_y;

	float left = node->line_dy * dx;
	float right = dy * node->line_dx;

	if (right < left)
	{
		return 0;
	}

	return 1;
}

typedef struct
{
	int first;
	int last;
} Cliprange2;

static Cliprange2 solidsegs[100];
static Cliprange2* newend;

static void ClearClipSegs()
{
	solidsegs[0].first = -0x7fffffff;
	solidsegs[0].last = -1;
	solidsegs[1].first = W;
	solidsegs[1].last = 0x7fffffff;
	newend = solidsegs + 2;
}


static float global_pcos = 0;
static float global_psin = 0;
static float global_floorz = 0;
static float global_ceilz = 0;
static float global_ya_step = 0;
static float global_yb_step = 0;
static float global_xs_start = 0;
static int global_x1 = 0;
static int global_x2 = 0;
static float global_u0 = 0;
static float global_u1 = 0;
static float global_tz1 = 0;
static float global_tz2 = 0;
static int global_is_backsector = 0;
static int global_ya1 = 0;
static int global_yb1 = 0;
static int global_ya2 = 0;
static int global_yb2 = 0;
static int global_nya1 = 0;
static int global_nyb1 = 0;
static int global_nya2 = 0;
static int global_nyb2 = 0;
static float global_xl = 0;
static float global_x = 0;
static float global_y = 0;
static Line* global_line = NULL;
static Sector* global_sector = NULL;
static float global_angle = 0;

DrawSpan draw_spans[2000];

typedef struct
{
	int top[W];
	int bottom[W];
	float height;
	int minx;
	int maxx;
	Image* texture;
} Visplane;

static Visplane visplanes[1000];
static int last_visplane;

static Visplane* global_visplane;
static int spanstart[10000];
static int spanend[10000];

static Visplane* FindVisPlane(float height, Texture* texture)
{
	Visplane* check = visplanes;

	for (int i = 0; i < last_visplane; i++)
	{
		check = &visplanes[i];

		if (check->height == height && check->texture == texture)
		{
			return check;
		}
	}

	check = &visplanes[last_visplane++];

	check->texture = texture;
	check->height = height;
	check->minx = W;
	check->maxx = -1;


	for (int i = 0; i < 1920; i++)
	{
		check->top[i] = 0x7fff;
	}
	for (int i = 0; i < 1920; i++)
	{
		check->bottom[i] = 0;
	}


	return check;
}
static Visplane* CheckVisPlane(Visplane* pl, int start, int stop)
{

	int		intrl;
	int		intrh;
	int		unionl;
	int		unionh;

	if (start < pl->minx)
	{
		intrl = pl->minx;
		unionl = start;
	}
	else
	{
		unionl = pl->minx;
		intrl = start;
	}

	if (stop > pl->maxx)
	{
		intrh = pl->maxx;
		unionh = stop;
	}
	else
	{
		unionh = pl->maxx;
		intrh = stop;
	}

	for (int x = intrl; x <= intrh; x++)
	{
		if (pl->top[x] != 0x7fff)
		{
			break;
		}

		if (x > intrh)
		{
			pl->minx = unionl;
			pl->maxx = unionh;

			return pl;
		}
	}

	Visplane* new_plane = &visplanes[last_visplane++];

	new_plane->height = pl->height;
	new_plane->texture = pl->texture;
	new_plane->minx = start;
	new_plane->maxx = stop;


	for (int i = 0; i < 1920; i++)
	{
		new_plane->top[i] = 0x7fff;
	}
	for (int i = 0; i < 1920; i++)
	{
		new_plane->bottom[i] = 0;
	}


	return new_plane;

}

static Image* global_image;
static Texture* global_texture;
static float global_height;
static float global_xscale;
static float global_yscale;

static void MapPlane(int y, int x1, int x2)
{
	float hei =  global_height;
	float mapx, mapz;
	float x_slope = (W / 2.0 - (float)x1) / (float)(W * hfov);
	mapz = hei * ((H * vfov) / ((H / 2.0 - ((float)y)) - 0.0 * H * vfov));
	mapx = mapz * x_slope;

	float pcos = global_pcos;
	float psin = global_psin;
	float p_x = global_x;
	float p_y = global_y;

	float rtx = mapz * pcos + mapx * psin;
	float rtz = mapz * psin - mapx * pcos;

	float z_pos = rtz + p_y;
	float x_pos = rtx + p_x;

	float mapx2 = mapz * (W / 2.0 - (float)x1 + 1) / (float)(W * hfov);

	float rtx2 = mapz * pcos + mapx2 * psin;
	float rtz2 = mapz * psin - mapx2 * pcos;

	rtz2 = rtz2 + p_y;
	rtx2 = rtx2 + p_x;

	float xstep = rtx2 - x_pos;
	float zstep = rtz2 - z_pos;

	int* data = global_image->data;
	for (int x = x1; x <= x2; x++)
	{	
		//x_slope = (W / 2.0 - (float)x) / (float)(W * hfov);
		mapx = mapz * x_slope;

		rtx = mapz * pcos + mapx * psin;
		rtz = mapz * psin - mapx * pcos;

		float fmapz = z_pos;
		float fmapx = x_pos;

		fmapz /= 64.0;
		fmapx /= 64.0;

		int tx = (int)(TILE_SIZE * (fmapx - (int)fmapx)) & (TILE_SIZE - 1);
		int ty = (int)(TILE_SIZE * (fmapz - (int)fmapz)) & (TILE_SIZE - 1);

		int* pix = Image_Get(&global_texture->img, ty, tx);

		data[x + y * global_image->width] = *pix;

		
		z_pos += (mapz * x_slope) / 64.0;
		x_pos += (mapz * x_slope) / 64.0;
	}
}

static void MakeSpans(int		x,
	int		t1,
	int		b1,
	int		t2,
	int		b2)
{
	while (t1 < t2 && t1 <= b1)
	{
		MapPlane(t1, spanstart[t1], x - 1);
		t1++;
	}
	while (b1 > b2 && b1 >= t1)
	{
		MapPlane(b1, spanstart[b1], x - 1);
		b1--;
	}

	while (t2 < t1 && t2 <= b2)
	{
		spanstart[t2] = x;
		t2++;
	}
	while (b2 > b1 && b2 >= t2)
	{
		spanstart[b2] = x;
		b2--;
	}
}

typedef struct
{
	Image* texture;
	int x;
	int y1, y2;
	int tx;
	float ypos;
	float ystep;
} MaskedCollumn;

static MaskedCollumn maskedcollumns[5000];
static int lastmaskedsegIndex = 0;

static void AddMaskedCollumn(Image* img, int x, int y1, int y2, int tx, float ypos, float ystep)
{
	MaskedCollumn* mc = &maskedcollumns[lastmaskedsegIndex++];

	mc->y1 = y1;
	mc->y2 = y2;
	mc->x = x;
	mc->tx = tx;
	mc->ypos = ypos;
	mc->ystep = ystep;
	mc->texture = img;
}

static void RenderLineSeg(Image* image, int first, int last, int p_ya, int p_yb)
{
	int* pix = image->data;

	float xs_start = fabs(first - global_x1);

	float xs = xs_start;
	GameAssets* assets = Game_GetAssets();

	int x1 = global_x1;
	int x2 = global_x2;
	int y1a = global_ya1;
	int y1b = global_yb1;
	int y2a = global_ya2;
	int y2b = global_yb2;

	Map* map = Map_GetMap();

	float tex_height = 128;

	int prev_cya = 0;
	int prev_cyb = 1080 - 1;

	int prev_top = 1080 - 1;
	int prev_bottom = 1080 - 1;

	float tz_STEP = global_tz2 - global_tz1;
	float xl = fabs(x2 - x1);

	tz_STEP /= xl;

	float yceil_pos = xs * global_ya_step;
	float yfloor_pos = xs * global_yb_step;
	float tz_pos = xs * tz_STEP;

	if (global_visplane)
	{
		global_visplane = CheckVisPlane(global_visplane, first, last);
	}
	

	for (int x = first; x < last; x++)
	{
		float xs1 = xs;
		int txtx = (global_u0 * (fabs(global_x2 - x) * global_tz2) + global_u1 * (fabs(x - global_x1) * global_tz1)) / (fabs(global_x2 - x) * global_tz2 + fabs(x - global_x1) * global_tz1);

		int ya = xs * global_ya_step + y1a, cya = clamp(ya, ytop[x], ybottom[x]); // top
		int yb = xs * global_yb_step + y1b, cyb = clamp(yb, ytop[x], ybottom[x]); // bottom

		int clamp_top = ytop[x];
		int clamp_bot = ybottom[x];

		int z = (tz_pos) + global_tz1;

		float x_slope = (W / 2.0 - (float)x) / (float)(W * hfov);
		if (global_sector->ceil_texture)
		{
			if (global_sector->is_sky)
			{		
				for (int y = ytop[x]; y <= cya - 1; y++)
				{
					pix[x + y * image->width] = 123132;

				}
			}
			else
			{
				for (int y = ytop[x]; y <= cya - 1; y++)
				{
					float hei = y < cya ? global_ceilz : global_floorz;
					float mapx, mapz;

					mapz = hei * ((H * vfov) / ((H / 2.0 - ((float)y)) - 0.0 * H * vfov));
					mapx = mapz * x_slope;

					float pcos = global_pcos;
					float psin = global_psin;
					float p_x = global_x;
					float p_y = global_y;

					float rtx = mapz * pcos + mapx * psin;
					float rtz = mapz * psin - mapx * pcos;

					mapz = rtz + p_y;
					mapx = rtx + p_x;

					mapz *= 0.05;
					mapx *= 0.05;

					if (overdraw_check[x][y])
					{
						printf("overdraw check %i %i \n", x, y);
					}

					overdraw_check[x][y] = true;

					// get the texture coordinate from the fractional part
					int tx = (int)(TILE_SIZE * (mapx - (int)mapx)) & (TILE_SIZE - 1);
					int ty = (int)(TILE_SIZE * (mapz - (int)mapz)) & (TILE_SIZE - 1);

					int* pix = (int*)image->data;
					int* texdata = Image_Get(&global_sector->ceil_texture->img, ty & 63, tx & 63);

					//pix[x + y * image->width] = *texdata;
					depth_buf[x][y] = min(depth_buf[x][y], z);

				}
			}
			
		}
	
		if (global_sector->floor_texture && global_visplane)
		{
			int top = cyb + 1;
			int bot = ybottom[x] - 1;

			if (top <= bot)
			{
				global_visplane->top[x] = top;
				global_visplane->bottom[x] = bot;
			}

			for (int y = cyb + 1; y <= ybottom[x]; y++)
			{
				break;
				float hei = y < cya ? global_ceilz : global_floorz;
				float mapx, mapz;

				mapz = hei * ((H * vfov) / ((H / 2.0 - ((float)y)) - 0.0 * H * vfov));
				mapx = mapz * x_slope;

				float pcos = global_pcos;
				float psin = global_psin;
				float p_x = global_x;
				float p_y = global_y;

				float rtx = mapz * pcos + mapx * psin;
				float rtz = mapz * psin - mapx * pcos;

				mapz = rtz + p_y;
				mapx = rtx + p_x;

				mapz *= 0.05;
				mapx *= 0.05;

				if (overdraw_check[x][y])
				{
					printf("overdraw check %i %i \n", x, y);
				}

				overdraw_check[x][y] = true;

				// get the texture coordinate from the fractional part
				int tx = (int)(TILE_SIZE * (mapx - (int)mapx)) & (TILE_SIZE - 1);
				int ty = (int)(TILE_SIZE * (mapz - (int)mapz)) & (TILE_SIZE - 1);

				int* pix = (int*)image->data;
				int* texdata = Image_Get(&global_sector->floor_texture->img, ty & 63, tx & 63);

				pix[x + y * image->width] = *texdata;
				depth_buf[x][y] = min(depth_buf[x][y], z);
			}
		}
		


		//vline(image, x, ytop[x], cya - 1, 0x111111, 12422, 0x111111);
		/* Render floor: everything below this sector's floor height. */
		//vline(image, x, cyb + 1, ybottom[x], 0x0000FF, 255123, 0x0000FF);

		if (global_is_backsector)
		{
			/* Same for _their_ floor and ceiling */
			int nya = fabs(x - x1) * (global_nya2 - global_nya1) / fabs(x2 - x1) + global_nya1, cnya = clamp(nya, ytop[x], ybottom[x]);
			int nyb = fabs(x - x1) * (global_nyb2 - global_nyb1) / fabs(x2 - x1) + global_nyb1, cnyb = clamp(nyb, ytop[x], ybottom[x]);
			/* If our ceiling is higher than their ceiling, render upper wall */
			unsigned r1 = 0x010101 * (255), r2 = 0x040007 * (31 / 8);

			int ys_start = fabs(cya - ya);
			float ys_step = (tex_height - 0) / fabs(yb - ya);

			if (global_line->sidedef->top_texture)
			{
				for (int y = cya + 1; y < cnya - 1; y++)
				{
					
					if (overdraw_check[x][y])
					{
						printf("overdraw, %i %i \n", x, y);
					}
					overdraw_check[x][y] = true;


					depth_buf[x][y] = min(depth_buf[x][y], z);

					int ty = (int)((float)ys_start++ * ys_step);

					int* data = Image_Get(&global_line->sidedef->top_texture->img, txtx & 63, ty & global_line->sidedef->top_texture->height_mask);

					pix[x + y * image->width] = *data;
				}
			}
			

			ys_start = fabs(cnyb + 1 - ya);
			ys_step = (tex_height - 0) / fabs(yb - ya);
			if (global_line->sidedef->bottom_texture)
			{
				for (int y = cnyb; y < cyb; y++)
				{
					
					if (overdraw_check[x][y])
					{
						//printf("overdraw, %i %i \n", x, y);
					}
					//overdraw_check[x][y] = true;


					depth_buf[x][y] = min(depth_buf[x][y], z);

					int ty = (int)((float)ys_start++ * ys_step);

					if (global_line->sidedef->bottom_texture)
					{
						int* data = Image_Get(&global_line->sidedef->bottom_texture->img, txtx & 63, ty & global_line->sidedef->bottom_texture->height_mask);

						pix[x + y * image->width] = *data;
					}


				}
			}
			

			//vline(image,x, cya, cnya - 1, 0, 255, 0); // Between our and their ceiling
			//vline3(image, &assets->wall_textures, x, cya, cnya - 1, (struct Scaler)Scaler_Init(ya, cya, yb, 0, 63), txtx, z);
			ytop[x] = clamp(max(cya, cnya), ytop[x], H - 1);   // Shrink the remaining window below these ceilings
			/* If our floor is lower than their floor, render bottom wall */
			//vline(image,x, cnyb, cyb, 0, 12555, 0); // Between their and our floor

			//vline3(image, &assets->wall_textures, x, cnyb + 1, cyb, (struct Scaler)Scaler_Init(ya, cnyb + 1, yb, 0, 63), txtx, z);
			ybottom[x] = clamp(min(cyb, cnyb), 0, ybottom[x]); // Shrink the remaining window above these floors

			if (global_line->sidedef->middle_texture)
			{
				float ys_start = fabs(cya - ya);
				float ys_step = (global_line->sidedef->middle_texture->img.height - 1 - 0) / fabs(yb - ya);

				AddMaskedCollumn(&global_line->sidedef->middle_texture->img, x, cya, cyb, txtx, ys_start, ys_step);
				
			}

		}
		else
		{

			if (global_line->sidedef->middle_texture)
			{
				Texture* texture = global_line->sidedef->middle_texture;

				txtx = txtx & (texture->img.width - 1);


				int texheightmask = (texture->img.height - 1);

				int ys_start = fabs(cya - ya);
				float ys_step = (texture->img.height - 1 - 0) / fabs(yb - ya);

				float ys = ys_start * ys_step;
				unsigned char* dest = image->data;
				for (int y = cya; y <= cyb; y++)
				{
					if (overdraw_check[x][y])
					{
						printf("overdraw, %i %i \n", x, y);
					}
					overdraw_check[x][y] = true;


					depth_buf[x][y] = min(depth_buf[x][y], z);

					int ty = (int)((float)ys);

					int* data = Image_Get(&global_line->sidedef->middle_texture->img, (txtx), (ty)&texheightmask);

				
					
					pix[x + y * image->width] = *data;

					ys += ys_step;
				}
			}
			
			
		}
		
		xs++;
		yceil_pos += global_ya_step;
		yfloor_pos += global_yb_step;
		tz_pos += tz_STEP;
	}
}

static void ClipAndDrawLine(Image* image, int first, int last, int ya, int yb, bool solid)
{
	Cliprange2* start = solidsegs;

	while (start->last < first)
	{
		start++;
	}

	if (first < start->first)
	{
		if (last <= start->first)
		{
			RenderLineSeg(image, first, last, ya, yb);

			if (solid)
			{
				if (last == start->first)
				{
					start->first = first;
				}
				else
				{
					Cliprange2* next = newend;
					newend++;

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

		RenderLineSeg(image, first, start->first, ya, yb);
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
		RenderLineSeg(image, next->last, (next + 1)->first, ya, yb);
		next++;

		if (last <= next->last)
		{
			last = next->last;
			goto crunch;
		}
	}

	RenderLineSeg(image, next->last, last, ya, yb);

crunch:

	if (solid)
	{
		start->last = last;

		if (next != start)
		{
			int i = 0;
			int j = 0;
			for (i = 1, j = (int)(newend - next); j > 0; i++, j--)
			{
				start[i] = next[i];
			}
			newend = start + i;
		}
	}

}


#define	NF_SUBSECTOR	0x8000

static void DrawSubsector(Image* image, float p_x, float p_y, float p_z, float p_yaw, float p_angle, int bsp_num, Map* map)
{
	float p_sin = sinf(p_angle);
	float p_cos = cosf(p_angle);
	global_angle = p_angle;
	Subsector* sub = &map->sub_sectors[bsp_num];
	Sector* sector = sub->sector;
	GameAssets* assets = Game_GetAssets();

	global_pcos = p_cos;
	global_psin = p_sin;

	global_x = p_x;
	global_y = p_y;

	for (int i = 0; i < sub->num_lines; i++)
	{
		Line* line = &map->line_segs[sub->line_offset + i];

		float vx1 = line->x0 - p_x;
		float vy1 = line->y0 - p_y;

		float vx2 = line->x1 - p_x;
		float vy2 = line->y1 - p_y;

		float tx1 = vx1 * p_sin - vy1 * p_cos;
		float tz1 = vx1 * p_cos + vy1 * p_sin;

		float tx2 = vx2 * p_sin - vy2 * p_cos;
		float tz2 = vx2 * p_cos + vy2 * p_sin;

		//completely behind the view plane
		if (tz1 <= 0 && tz2 <= 0)
		{
			continue;
		}

		float texwidth = 63;

		float wall_x = line->x1 - line->x0;
		float wall_y = line->y1 - line->y0;

		float wall_width = line->width_scale;

		texwidth *= wall_width * 0.025;
		
		float dist = sector->ceil - sector->floor;


		float u0 = 0;
		float u1 = texwidth;

		if (tz1 <= 0 || tz2 <= 0)
		{
			Vertex org1 = { tx1,tz1 }, org2 = { tx2,tz2 };

			float nearz = 1e-4f, farz = 0.1, nearside = 1e-5f, farside = 20.f;
			// Find an intersection between the wall and the approximate edges of player's view
			Vertex i1 = Intersect(tx1, tz1, tx2, tz2, -nearside, nearz, -farside, farz);
			Vertex i2 = Intersect(tx1, tz1, tx2, tz2, nearside, nearz, farside, farz);

			if (tz1 <= 0)
			{
				if (tz1 < nearz) { if (i1.y > 0) { tx1 = i1.x; tz1 = i1.y; } else { tx1 = i2.x; tz1 = i2.y; } }

			}
			if (tz2 <= 0)
			{
				if (tz2 < nearz) { if (i1.y > 0) { tx2 = i1.x; tz2 = i1.y; } else { tx2 = i2.x; tz2 = i2.y; } }
			}

			int s = texwidth;

			if (fabs(tx2 - tx1) > fabs(tz2 - tz1))
				u0 = fabs(tx1 - org1.x) * (s) / fabs(org2.x - org1.x), u1 = fabs(tx2 - org1.x) * (s) / fabs(org2.x - org1.x);
			else
				u0 = fabs(tz1 - org1.y) * (s) / fabs(org2.y - org1.y), u1 = fabs(tz2 - org1.y) * (s) / fabs(org2.y - org1.y);

		}

		float xscale1 = (W * hfov) / tz1;
		float xscale2 = (W * hfov) / tz2;

		float yscale1 = (H * vfov) / tz1;
		float yscale2 = (H * vfov) / tz2;

		int x1 = (image->width / 2) - (int)(tx1 * xscale1);
		int x2 = (image->width / 2) - (int)(tx2 * xscale2);

		if (x1 > x2 || x2 <= 0 || x1 >= image->width || x1 == x2)
		{
			continue;
		}

		float yceil = sector->ceil - p_z;
		float yfloor = sector->floor - p_z;

		int y1a = image->height / 2 - (int)((yceil)*yscale1);
		int y2a = image->height / 2 - (int)((yceil)*yscale2);
		int y1b = image->height / 2 - (int)((yfloor)*yscale1);
		int y2b = image->height / 2 - (int)((yfloor)*yscale2);

		int beginx = max(x1, 0), endx = min(x2, image->width- 1);

		float xs_start = fabs(beginx - x1);

		float xl = fabs(x2 - x1);
		float ya_diff = (y2a - y1a);
		float yb_diff = (y2b - y1b);

		float ya_step = (ya_diff / xl);
		float yb_step = (yb_diff / xl);

		int* pix = image->data;

		global_xs_start = xs_start;
		global_ya_step = ya_step;
		global_yb_step = yb_step;
		global_ya1 = y1a;
		global_ya2 = y2a;
		global_yb1 = y1b;
		global_yb2 = y2b;

		global_x1 = x1;
		global_x2 = x2;
		global_u0 = u0;
		global_u1 = u1;
		global_tz1 = tz1;
		global_tz2 = tz2;
		global_xl = xl;
		global_ceilz = yceil;
		global_floorz = yfloor;
		global_line = line;
		global_sector = sector;

		global_visplane = FindVisPlane(yfloor, sector->floor_texture);

		Sector* backsector = NULL;
		if (line->back_sector >= 0)
		{
			backsector = &map->sectors[line->back_sector];
		}

		bool clip_solid = true;

		global_is_backsector = 0;

		if (backsector)
		{
			if (backsector->ceil <= sector->floor || backsector->floor >= sector->ceil)
			{
				clip_solid = true;
			}
			else if(backsector->ceil != sector->ceil || backsector->floor != sector->floor)
			{
				clip_solid = false;
			}




			float neigh_ceil = backsector->ceil - p_z;
			float neigh_floor = backsector->floor - p_z;

			int ny_ceil1 = image->height / 2 - (int)((neigh_ceil)*yscale1);
			int ny_ceil2 = image->height / 2 - (int)((neigh_ceil)*yscale2);
			int ny_floor1 = image->height / 2 - (int)((neigh_floor)*yscale1);
			int ny_floor2 = image->height /2 - (int)((neigh_floor)*yscale2);
			
			
			global_nya1 = ny_ceil1;
			global_nya2 = ny_ceil2;
			global_nyb1 = ny_floor1;
			global_nyb2 = ny_floor2;
			global_is_backsector = true;

			
			ClipAndDrawLine(image, beginx, endx, y1a, y1b, false);

			continue;
			
		}
		
		//ClipSolidLine(image, beginx, endx, y1a, y1b);
		ClipAndDrawLine(image, beginx, endx, y1a, y1b, true);
		
		if (clip_solid)
		{
			//ClipSolidLine(image, beginx, endx, y1a, y1b);
		}
		else
		{
			//ClipPassLine(image, beginx, endx, y1a, y1b);
		}
		
	}
}
enum
{
	BOXTOP,
	BOXBOTTOM,
	BOXLEFT,
	BOXRIGHT
};	// bbox coordinates

static bool checkBBox(int width, int height ,float p_x, float p_y, float p_sin, float p_cos, float* bspcoord)
{
	return true;
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

	//if (y1 * (x1 - x2) + x1 * (y2 - y1) <= -CMP_EPSILON)
		//return true;

	float focal_tangent = tanf((133.0 * Math_PI / 180.0) / 2.0);

	float tancos = focal_tangent * p_cos;
	float tansin = focal_tangent * p_sin;

	float rx1 = x1 * p_sin - y1 * p_cos;
	float rx2 = x2 * p_sin - y2 * p_cos;
	float ry1 = x1 * p_cos + y1 * p_sin;
	float ry2 = x2 * p_cos + y2 * p_sin;

	ry1 = (W * hfov) / ry1;
	ry2 = (W * hfov) / ry2;


	int sx1 = 0;
	int sx2 = 0;
	int half_width = width / 2;
	int half_height = height / 2;

	sx1 = half_width - (int)(rx1 * ry1);
	sx2 = half_width - (int)(rx2 * ry2);

	if (rx1 >= -ry1)
	{
		if (rx1 > ry1)
		{
			return false;
		}
		if (ry1 == 0)
		{
			return false;
		}

		sx1 = (int)roundf(half_width + rx1 * half_width / ry1);
	}
	else
	{
		if (rx2 < -ry2)
		{
			return false;
		}
		if (rx1 - rx2 - ry2 + ry1 == 0)
		{
			return false;
		}
		sx1 = 0;
	}
	if (rx2 <= ry2)
	{
		if (rx2 < -ry2)
		{
			return false;
		}
		if (ry2 == 0)
		{
			return false;
		}
		sx2 = (int)roundf(half_width + rx2 * half_width / ry2);
	}
	else
	{
		if (rx1 > ry1)
		{
			return false;
		}
		if (ry2 - ry1 - rx2 + rx1 == 0)
		{
			return false;
		}
		sx2 = width;
	}

	if (sx2 <= sx1)
	{
		return false;
	}

	Cliprange2* start = solidsegs;
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

static void DrawRecursive(Image* image, float p_x, float p_y, float p_z, float p_yaw, float p_angle, int bsp_num, Map* map)
{
	if (bsp_num & NF_SUBSECTOR)
	{
		if (bsp_num == -1)
		{
			DrawSubsector(image, p_x, p_y, p_z, p_yaw, p_angle, 0, map);
		}
		else
		{
			DrawSubsector(image, p_x, p_y, p_z, p_yaw, p_angle, bsp_num & ~NF_SUBSECTOR, map);
		}

		return;
	}

	BSPNode* node = &map->bsp_nodes[bsp_num];

	int side = bspPointOnSide2(node, p_x, p_y);
	DrawRecursive(image, p_x, p_y, p_z, p_yaw, p_angle, node->children[side], map);

	side = side ^ 1;

	if (checkBBox(image->width, image->height, p_x, p_y, sinf(p_angle), cosf(p_angle), node->bbox[side]))
	{
		DrawRecursive(image, p_x, p_y, p_z, p_yaw, p_angle, node->children[side], map);
	}
}
static void DrawPlanes(Image* image, float p_x, float p_y, float p_z, float p_yaw, float p_angle)
{
	Visplane* pl = visplanes;

	global_image = image;

	for (int i = 0; i < last_visplane; i++)
	{
		pl = &visplanes[i];

		global_height = pl->height;
		global_texture = pl->texture;
	

		if (pl->minx > pl->maxx)
		{
			continue;
		}
		float xs = pl->maxx - image->half_width + 0.5;
		float rightxfrac = (global_pcos + xs * (1.0 / image->half_width));
		float rightyfrac = (global_psin + xs * (1.0 / image->half_width));

		xs = pl->minx - image->half_width + 0.5;
		float leftxfrac = (global_pcos + xs * (1.0 / image->half_width));
		float leftyfrac = (global_psin + xs * (1.0 / image->half_width));


		global_xscale = (rightxfrac - leftxfrac) / (pl->maxx - pl->minx);
		global_yscale = (rightyfrac - leftyfrac) / (pl->maxx - pl->minx);

		int x = pl->maxx - 1;
		int t2 = pl->top[x];
		int b2 = pl->bottom[x];

		if (b2 > t2)
		{
			for (int k = 0; k < (b2 - t2); k++)
			{
				spanend[t2 + k] = x;
			}
		}

		for (--x; x >= pl->minx; --x)
		{
			int t1 = pl->top[x];
			int b1 = pl->bottom[x];
			const int xr = x + 1;
			int stop;

			// Draw any spans that have just closed
			stop = min(t1, b2);

			while (t2 < stop)
			{
				int y = t2++;
				int x2 = spanend[y];
				MapPlane(y, xr, x2);
			}
			stop = max(b1, t2);
			while (b2 > stop)
			{
				int y = --b2;
				int x2 = spanend[y];
				MapPlane(y, xr, x2);
			}

			stop = min(t2, b1);
			while (t1 < stop)
			{
				spanend[t1++] = x;
			}
			stop = max(b2, t2);
			while (b1 > stop)
			{
				spanend[--b1] = x;
			}

			t2 = pl->top[x];
			b2 = pl->bottom[x];
		}
		while (t2 < b2)
		{
			int y = --b2;
			int x2 = spanend[y];
			MapPlane(y, pl->minx, x2);
		}
	}
}

void Video_DrawDoomSectors(Image* image, float p_x, float p_y, float p_z, float p_yaw, float p_angle)
{
	ClearClipSegs();
	lastmaskedsegIndex = 0;
	memset(overdraw_check, 0, sizeof(overdraw_check));
	memset(draw_spans, 0, sizeof(draw_spans));
	memset(spanstart, 0, sizeof(spanstart));
	memset(spanend, 0, sizeof(spanend));
	memset(visplanes, 0, sizeof(visplanes));
	last_visplane = 0;
	memset(spanstart, 0, sizeof(spanstart));
	for (unsigned x = 0; x < W; ++x) clamped_y_floor_arr[x] = 0;
	for (unsigned x = 0; x < W; ++x) clamped_y_ceil_arr[x] = 0;
	for (unsigned x = 0; x < W; ++x) ytop[x] = 0;
	for (unsigned x = 0; x < W; ++x) ybottom[x] = H - 1;

	for (unsigned x = 0; x < W; ++x)
	{
		for (int y = 0; y < H; y++)
		{
			depth_buf[x][y] = SHRT_MAX - 3;
		}
	}
	

	DrawRecursive(image, p_x, p_y, p_z, p_yaw, p_angle, Map_GetMap()->num_nodes - 1, Map_GetMap());



	int* pix = image->data;
	for (int i = 0; i < lastmaskedsegIndex; i++)
	{
		MaskedCollumn* mc = &maskedcollumns[i];

		float ys_start = mc->ypos;
		float ys_step = mc->ystep;
		for (int y = mc->y1; y <= mc->y2; y++)
		{
			int ty = (int)((float)ys_start++ * ys_step);
			unsigned char* data = Image_Get(mc->texture, mc->tx, ty);

			if (data[3] < 128)
			{
				continue;
			}

			pix[mc->x + y * image->width] = *(int*)data;
		}
	}

	DrawPlanes(image, p_x, p_y, p_z, p_yaw, p_angle);
}

void Video_DrawSprite3(Image* image, DrawingArgs* args, DrawSprite* sprite)
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

	//start_ty_pos += ty_add;

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
		float depth_step = xl;
		float depth = (x_pos * depth_step) + transform_y;

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
				dest[i + 0] = LIGHT_LUT[tex_data[0]][light];
				dest[i + 1] = LIGHT_LUT[tex_data[1]][light];
				dest[i + 2] = LIGHT_LUT[tex_data[2]][light];
			}

			ty_pos += ty_step;
			ty_local_pos += ty_step;
			dest_index += dest_index_step;
		}

		tx_pos += tx_step;
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
