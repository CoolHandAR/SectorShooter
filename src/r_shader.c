#include "r_common.h"

void Shader_Hurt(Image* img, int x, int y)
{
	int center_x = abs(x - img->half_width);
	int center_y = abs(y - img->half_height);

	float strenght_x = (float)center_x / (float)img->half_width;
	float strenght_y = (float)center_y / (float)img->half_height;

	float lerp = Math_lerp(0, strenght_x, strenght_y);

	unsigned char* sample = Image_Get(img, x, y);

	sample[0] = Math_lerp(sample[0], 255, lerp);
}
void Shader_HurtSimple(Image* img, int x, int y)
{
	unsigned char* sample = Image_Get(img, x, y);

	int r = sample[0];

	r *= 2;

	if (r > 255)
	{
		r = 255;
	}

	sample[0] = r;
}

void Shader_Dead(Image* img, int x, int y)
{
	int center_x = abs(x - img->half_width);
	int center_y = abs(y - img->half_height);

	float strenght_x = (float)center_x / (float)img->half_width;
	float strenght_y = (float)center_y / (float)img->half_height;

	unsigned char* sample = Image_Get(img, x, y);

	Image_Set2(img, x + (rand() % 2), y - (rand() % 2), sample);
}

void Shader_Godmode(Image* img, int x, int y)
{
	unsigned char* sample = Image_Get(img, x, y);

	sample[0] ^= sample[1];
}
void Shader_Quad(Image* img, int x, int y)
{
	unsigned char* sample = Image_Get(img, x, y);

	sample[2] = Math_Clampl((int)sample[0] + 12, 0, 255);
}