#include "r_common.h"

#include <string.h>
#include <assert.h>
#include <stb_image/stb_image.h>

bool Image_Create(Image* img, int p_width, int p_height, int p_numChannels)
{	
	memset(img, 0, sizeof(Image));

	if (p_width < 1)
	{
		p_width = 1;
	}
	if (p_height < 1)
	{
		p_height = 1;
	}

	img->width = p_width;
	img->height = p_height;
	img->numChannels = p_numChannels;

	size_t size = img->width * img->height * img->numChannels;

	//allocate a buffer
	unsigned char* data = malloc(size);

	if (!data)
	{
		printf("ERROR:: Failed to load texture");
		return false;
	}

	memset(data, 0, size);

	img->data = data;


	img->half_width = img->width / 2;
	img->half_height = img->height / 2;

	return true;
}
bool Image_CreateFromPath(Image* img, const char* path)
{
	memset(img, 0, sizeof(Image));

	//load from file
	stbi_uc* data = stbi_load(path, &img->width, &img->height, &img->numChannels, 0);

	//failed to load
	if (!data)
	{
		printf("ERROR:: %s", stbi_failure_reason());
		return false;
	}

	img->data = data;
	

	img->half_width = img->width / 2;
	img->half_height = img->height / 2;

	return true;
}

bool Image_SaveToPath(Image* img, const char* filename)
{
	if (!img->data)
	{
		return false;
	}


	return false;
}

void Image_Resize(Image* img, int p_width, int p_height)
{
	if (p_width <= 0 || p_height <= 0)
	{
		return;
	}

	assert(img->data);

	free(img->data);
	img->data = NULL;

	size_t size = p_width * p_height * img->numChannels;

	img->data = malloc(size);

	if (!img->data)
	{
		return;
	}

	memset(img->data, 0, size);

	img->width = p_width;
	img->height = p_height;

	img->half_width = img->width / 2;
	img->half_height = img->height / 2;
}

void Image_Destruct(Image* img)
{
	if (!img->data)
	{
		return;
	}

	free(img->data);

	if (img->frame_info)
	{
		int total_frames = img->h_frames * img->v_frames;

		//only single frame
		if (total_frames == 0)
		{
			total_frames = 1;
		}

		for (int i = 0; i < total_frames; i++)
		{
			FrameInfo* f_info = &img->frame_info[i];

			if (f_info->alpha_spans)
			{
				free(f_info->alpha_spans);
			}
		}

		free(img->frame_info);
	}
}

void Image_Clear(Image* img, int c)
{
	memset(img->data, (int)c, sizeof(unsigned char) * img->width * img->height * img->numChannels);
}

void Image_Set(Image* img, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	if (!img->data || x < 0 || y < 0 || x >= img->width || y >= img->height)
	{
		return;
	}

	unsigned char* d = img->data + (x + y * img->width) * img->numChannels;

	if (img->numChannels >= 1) d[0] = r;
	if (img->numChannels >= 2) d[1] = g;
	if (img->numChannels >= 3) d[2] = b;
	if (img->numChannels >= 4) d[3] = a;
}

Vec4 Image_CalcAverageColor(Image* img, int min_avg, int max_avg)
{
	if (!img->data)
	{
		return Vec4_Zero();
	}

	Vec4 total_color = Vec4_Zero();
	int samples = 0;

	for (int x = 0; x < img->width; x++)
	{
		for (int y = 0; y < img->height; y++)
		{
			unsigned char* data = Image_GetFast(img, x, y);

			if (!data)
			{
				continue;
			}

			total_color.r += (float)data[0];
			total_color.g += (float)data[1];
			total_color.b += (float)data[2];

			samples++;
		}
	}

	if (samples > 0)
	{
		total_color.r = Math_Clamp((float)total_color.r / (float)samples, min_avg, max_avg);
		total_color.g = Math_Clamp((float)total_color.g / (float)samples, min_avg, max_avg);
		total_color.b = Math_Clamp((float)total_color.b / (float)samples, min_avg, max_avg);
	}

	return total_color;
}

void Image_Blur(Image* img, int size, float scale)
{
	if (!img->data || size <= 0 || scale <= 0)
	{
		return;
	}

	Image blur_image;
	
	if (!Image_Create(&blur_image, img->width, img->height, img->numChannels))
	{
		return;
	}
	

	int dir_x = 1;
	int dir_y = 0;

	int half_size = size / 2;

	for (int x = 0; x < img->width; x++)
	{
		for (int y = 0; y < img->height; y++)
		{
			int result[4];

			memset(result, 0, sizeof(result));

			for (int i = -half_size; i < half_size; i++)
			{
				if (i == 0)
				{
					continue;
				}

				float radius = (float)i * scale;

				int tx = x + dir_x * radius;
				int ty = y + dir_y * radius;

				unsigned char* sample = Image_Get(img, tx, ty);
				
				for (int k = 0; k < img->numChannels; k++)
				{
					result[k] += sample[k];
				}
			}

			unsigned char* color = Image_Get(&blur_image, x, y);


			for (int k = 0; k < img->numChannels; k++)
			{
				result[k] /= size - 1;

				if (result[k] < 0)
				{
					result[k] = 0;
				}
				else if (result[k] > 255)
				{
					result[k] = 255;
				}

				color[k] = (unsigned char)result[k];

				if (k == 3)
				{
					color[k] = 255;
				}
			}

		}
	}

	Image_Copy(img, &blur_image);

	Image_Destruct(&blur_image);

}

void Image_GenerateFrameInfo(Image* img)
{
	const int h_frames = img->h_frames;
	const int v_frames = img->v_frames;

	int total_frames = h_frames * v_frames;

	//only single frame
	if (total_frames == 0)
	{
		total_frames = 1;
	}

	FrameInfo* frame_infos = malloc(sizeof(FrameInfo) * total_frames);

	if (!frame_infos)
	{
		return;
	}

	int w = (h_frames > 0) ? img->width / h_frames : img->width;

	for (int i = 0; i < total_frames; i++)
	{
		FrameInfo* f_info = &frame_infos[i];
		f_info->alpha_spans = malloc(sizeof(AlphaSpan) * w);

		if (!f_info->alpha_spans)
		{
			return;
		}

		memset(f_info->alpha_spans, 0, sizeof(AlphaSpan) * w);

		f_info->width = w;

		bool min_x_set = false;
		int min_x = 0;
		int max_x = 0;

		int sprite_offset_x = (h_frames > 0) ? i % h_frames : 0;
		int sprite_offset_y = (v_frames > 0) ? i / h_frames : 0;

		int sprite_rect_width = (h_frames > 0) ? img->width / h_frames : img->width;
		int sprite_rect_height = (v_frames > 0) ? img->height / v_frames : img->height;

		for (int x = 0; x < sprite_rect_width; x++)
		{
			int tx = x + (sprite_offset_x * sprite_rect_width);

			AlphaSpan* sp = &f_info->alpha_spans[x];

			bool min_y_set = false;
			int min_y = 0;
			int max_y = 0;

			for (int y = 0; y < sprite_rect_height; y++)
			{
				int ty = y + (sprite_offset_y * sprite_rect_height);

				unsigned char* color = Image_Get(img, tx, ty);

				if (!color)
				{
					continue;
				}

				//valid pixel
				if (color[3] > 128)
				{
					if (!min_y_set)
					{
						min_y = y;
						min_y_set = true;
					}
					if (!min_x_set)
					{
						min_x = x;
						min_x_set = true;
					}
					max_x = x;
					max_y = y;
				}
				else
				{
					if (!min_y_set)
					{
						min_y = y;
					}
					if (!min_x_set)
					{
						min_x = x;
					}
				}
			}

			sp->min = (min_y > 0) ? min_y - 1 : 0;
			sp->max = max_y;
		}

		f_info->min_real_x = (min_x > 0) ? min_x - 1 : 0;
		f_info->max_real_x = max_x;
	}

	img->frame_info = frame_infos;
}

FrameInfo* Image_GetFrameInfo(Image* img, int frame)
{
	int total_frames = img->h_frames * img->v_frames;

	//only single frame
	if (total_frames == 0)
	{
		total_frames = 1;
	}

	if (!img->frame_info || frame < 0 || frame >= total_frames)
	{
		return NULL;
	}


	return &img->frame_info[frame];
}

AlphaSpan* FrameInfo_GetAlphaSpan(FrameInfo* frame_info, int x)
{
	if (!frame_info->alpha_spans)
	{
		return NULL;
	}

	if (x < 0)
	{
		x = 0;
	}
	else if (x >= frame_info->width)
	{
		x = frame_info->width - 1;
	}

	return &frame_info->alpha_spans[x];
}


void Image_Copy(Image* dest, Image* src)
{
	assert(dest->numChannels == src->numChannels);
	
	memcpy(dest->data, src->data, sizeof(unsigned char) * dest->width * dest->height * dest->numChannels);
}

void Image_Blit(Image* dest, Image* src, int dstX0, int dstY0, int dstX1, int dstY1, int srcX0, int srcY0, int srcX1, int srcY1)
{
	assert(dest->numChannels == src->numChannels);

	int dx = dstX0;
	int dy = dstY0;

	for (int x = srcX0; x < srcX1; x++)
	{
		dy = dstY0;

		for (int y = srcY0; y < srcY1; y++)
		{

			unsigned char* sample = Image_Get(src, x, y);

			if (sample[3] <= 128)
			{
				continue;
			}

			Image_Set2(dest, dx, dy, sample);

			dy++;

			if (dy >= dstY1)
			{
				break;
			}
		}
		dx++;

		if (dx >= dstX1)
		{
			break;
		}
	}
}

void Sprite_UpdateAnimation(Sprite* sprite, float delta)
{
	if (!sprite->playing)
	{
		return;
	}
	if (sprite->anim_speed_scale == 0)
	{
		return;
	}

	int old_frame = sprite->frame;
	int frame = sprite->frame;

	float remaining = delta;

	int i = 0;

	while (remaining >= 0)
	{
		float speed = 12 * sprite->anim_speed_scale;
		float abs_speed = abs(speed);

		if (speed == 0)
		{
			break;
		}

		int frame_count = sprite->frame_count;

		int last_frame = frame_count - 1;

		if (speed > 0)
		{
			if (sprite->_anim_frame_progress >= 1.0)
			{
				//anim restart
				if (frame >= last_frame)
				{
					if (sprite->looping)
					{
						sprite->finished = true;
						frame = 0;
					}
					else
					{
						frame = last_frame;
						sprite->playing = false;
						sprite->finished = true;
						break;
					}
				}
				else
				{
					sprite->finished = false;
					Sprite_CheckForActionFunction(sprite, frame);
					frame++;
				}
				sprite->_anim_frame_progress = 0.0;
			}

			float to_process = min((1.0 - sprite->_anim_frame_progress) / abs_speed, remaining);
			sprite->_anim_frame_progress += to_process * abs_speed;
			remaining -= to_process;
		}

		i++;
		if (i > frame_count)
		{
			break;
		}
	}

	sprite->frame = frame;
}

void Sprite_ResetAnimState(Sprite* sprite)
{
	sprite->playing = false;
	sprite->frame = 0;
	sprite->_anim_frame_progress = 0;
}

void Sprite_CheckForActionFunction(Sprite* sprite, int frame)
{
	if (sprite->action_frame == frame)
	{
		sprite->action_frame_triggered = true;
	}
}

