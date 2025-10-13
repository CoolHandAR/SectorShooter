#include "r_common.h"

#include "utility.h"

#include <cjson/cJSON.h>
#include <stdlib.h>

#include "u_math.h"
#include <stdarg.h>

bool Text_LoadFont(const char* filename, const char* image_path, FontData* font_data)
{
	const unsigned char* filestr = File_Parse(filename, NULL);

	if (!filestr)
	{
		printf("ERROR::Failed to load font!\n");
		return false;
	}

	cJSON* json = cJSON_Parse(filestr);

	if (!json)
	{
		printf("ERROR::Failed to load font!\n");
		return false;
	}

	//we can free the raw char data
	free(filestr);
	filestr = NULL;

	font_data->atlas_data.distance_range = 2;

	//Parse atlas data
	const cJSON* atlas_name = cJSON_GetObjectItem(json, "atlas");
	if (atlas_name && atlas_name->child)
	{
		const cJSON* child = atlas_name->child;
		while (child)
		{
			if (!strcmp(child->string, "distanceRange"))
			{
				font_data->atlas_data.distance_range = cJSON_GetNumberValue(child);
			}
			else if (!strcmp(child->string, "size"))
			{
				font_data->atlas_data.size = cJSON_GetNumberValue(child);
			}
			else if (!strcmp(child->string, "width"))
			{
				font_data->atlas_data.width = cJSON_GetNumberValue(child);
			}
			else if (!strcmp(child->string, "height"))
			{
				font_data->atlas_data.height = cJSON_GetNumberValue(child);
			}

			child = child->next;
		}
	}
	//Parse metrics
	const cJSON* metrics = cJSON_GetObjectItem(json, "metrics");
	if (metrics && metrics->child)
	{
		const cJSON* child = metrics->child;
		while (child)
		{
			if (!strcmp(child->string, "emSize"))
			{
				font_data->metrics_data.em_size = cJSON_GetNumberValue(child);
			}
			else if (!strcmp(child->string, "lineHeight"))
			{
				font_data->metrics_data.line_height = cJSON_GetNumberValue(child);
			}
			else if (!strcmp(child->string, "ascender"))
			{
				font_data->metrics_data.ascender = cJSON_GetNumberValue(child);
			}
			else if (!strcmp(child->string, "descender"))
			{
				font_data->metrics_data.descender = cJSON_GetNumberValue(child);
			}
			else if (!strcmp(child->string, "underlineY"))
			{
				font_data->metrics_data.underline_y = cJSON_GetNumberValue(child);
			}
			else if (!strcmp(child->string, "underlineThickness"))
			{
				font_data->metrics_data.underline_thickness = cJSON_GetNumberValue(child);
			}

			child = child->next;
		}
	}
	//Parse glyphs
	const cJSON* glyphs = cJSON_GetObjectItem(json, "glyphs");
	if (glyphs && glyphs->child && cJSON_IsArray(glyphs))
	{
		int array_size = cJSON_GetArraySize(glyphs);

		for (int i = 0; i < array_size; i++)
		{
			const cJSON* array_child = cJSON_GetArrayItem(glyphs, i);

			if (!array_child)
				break;

			if (array_child->child)
				array_child = array_child->child;

			while (array_child)
			{
				if (!strcmp(array_child->string, "unicode"))
				{
					font_data->glyphs_data[i].unicode = cJSON_GetNumberValue(array_child);
				}
				else if (!strcmp(array_child->string, "advance"))
				{
					font_data->glyphs_data[i].advance = cJSON_GetNumberValue(array_child);
				}
				else if (!strcmp(array_child->string, "planeBounds"))
				{
					const cJSON* nested_child = array_child->child;

					for (int j = 0; j < 4; j++)
					{
						if (!nested_child)
							break;

						if (!strcmp(nested_child->string, "left"))
						{
							font_data->glyphs_data[i].plane_bounds.left = cJSON_GetNumberValue(nested_child);
						}
						else if (!strcmp(nested_child->string, "bottom"))
						{
							font_data->glyphs_data[i].plane_bounds.bottom = cJSON_GetNumberValue(nested_child);
						}
						else if (!strcmp(nested_child->string, "right"))
						{
							font_data->glyphs_data[i].plane_bounds.right = cJSON_GetNumberValue(nested_child);
						}
						else if (!strcmp(nested_child->string, "top"))
						{
							font_data->glyphs_data[i].plane_bounds.top = cJSON_GetNumberValue(nested_child);
						}

						nested_child = nested_child->next;

					}
				}
				else if (!strcmp(array_child->string, "atlasBounds"))
				{
					const cJSON* nested_child = array_child->child;

					for (int j = 0; j < 4; j++)
					{
						if (!nested_child)
							break;

						if (!strcmp(nested_child->string, "left"))
						{
							font_data->glyphs_data[i].atlas_bounds.left = cJSON_GetNumberValue(nested_child);
						}
						else if (!strcmp(nested_child->string, "bottom"))
						{
							font_data->glyphs_data[i].atlas_bounds.bottom = cJSON_GetNumberValue(nested_child);
						}
						else if (!strcmp(nested_child->string, "right"))
						{
							font_data->glyphs_data[i].atlas_bounds.right = cJSON_GetNumberValue(nested_child);
						}
						else if (!strcmp(nested_child->string, "top"))
						{
							font_data->glyphs_data[i].atlas_bounds.top = cJSON_GetNumberValue(nested_child);
						}

						nested_child = nested_child->next;
					}
				}
				array_child = array_child->next;
			}
		}
	}

	//cleanup
	cJSON_Delete(json);

	//load image
	return Image_CreateFromPath(&font_data->font_image, image_path);
}

FontGlyphData* FontData_GetGlyphData(const FontData* font_data, char ch)
{
	//glyphs start with unicode 32 and so that means that 0 index of the array is 32
	//so we subtract the unicode of the char with 32 to get the index of our array
	int glyph_index = (int)ch - 32;


	//glyph not found? return the glyph of a question mark 
	if (glyph_index < 0 || glyph_index >= MAX_FONT_GLYPHS)
	{
		char qstmark_ch = '?';
		return FontData_GetGlyphData(font_data, qstmark_ch);
	}

	return &font_data->glyphs_data[glyph_index];
}

void Text_DrawStr(Image* image, const FontData* font_data, float _x, float _y, float scale_x, float scale_y, int r, int g, int b, int a, const char* str)
{
	if (!str || scale_x <= 0 || scale_y <= 0)
	{
		return;
	}
	
	float scren_px = 4.5;

	int render_scale = Render_GetRenderScale();

	int pix_x = _x * image->width;
	int pix_y = _y * image->height;

	if (pix_x < 0)
	{
		pix_x = 0;
	}
	else if (pix_x >= image->width)
	{
		pix_x = image->width - 1;
	}
	if (pix_y < 0)
	{
		pix_y = 0;
	}
	else if (pix_y >= image->height)
	{
		pix_y = image->height - 1;
	}

	scale_x *= render_scale;
	scale_y *= render_scale;

	const float d_scale_x = 1.0 / scale_x;
	const float d_scale_y = 1.0 / scale_y;

	const float texel_width = 1.0 / font_data->font_image.width;
	const float texel_height = 1.0 / font_data->font_image.height;

	const float font_scale = 1.0 / (font_data->metrics_data.descender - font_data->metrics_data.ascender);

	const int len = strlen(str);

	const float space_advance = FontData_GetGlyphData(font_data, ' ')->advance;

	double x = pix_x;
	double y = pix_y;

	for (int i = 0; i < len; i++)
	{
		char ch = str[i];

		//handle special characters
		if (ch == '\r')
		{
			continue;
		}
		else if (ch == '\n')
		{
			x = pix_x;
			pix_y += font_scale * font_data->metrics_data.line_height + font_data->atlas_data.size;
			continue;
		}
		else if (ch == ' ')
		{
			x += (font_scale * space_advance + 12) * scale_x;
			continue;
		}
		
		const FontGlyphData* glyph_data = FontData_GetGlyphData(font_data, ch);

		const double x1 = glyph_data->atlas_bounds.left * scale_x;
		const double y1 = glyph_data->atlas_bounds.top * scale_y;

		const double x2 = glyph_data->atlas_bounds.right * scale_x;
		const double y2 = glyph_data->atlas_bounds.bottom * scale_y;

		float x_step = (glyph_data->atlas_bounds.right - glyph_data->atlas_bounds.left) * scale_x;
		float y_offset = (glyph_data->atlas_bounds.bottom - glyph_data->atlas_bounds.top) * scale_y;

		for (float tx = x1; tx < x2; tx += 1)
		{
			for (float ty = y1; ty < y2; ty += 1)
			{
				unsigned char* color = Image_Get(&font_data->font_image, tx * d_scale_x, ty * d_scale_y);

				unsigned char c = color[0];

				float sd = (float)c / 255.0f;
				float d = scren_px * (sd - 0.5);

				float opacity = Math_Clampd(d + 0.5, 0.0, 1.0);

				unsigned char* im = Image_Get(image, x, y);

				im[0] = Math_lerp(im[0], r, opacity);
				im[1] = Math_lerp(im[1], g, opacity);
				im[2] = Math_lerp(im[2], b, opacity);
				im[3] = a;

				y += font_data->metrics_data.em_size;
			}

			x += font_data->metrics_data.em_size;
			y = pix_y - y_offset;

			if (x < 0 || x > image->width)
			{
				break;
			}
			if (y < 0 || y > image->height)
			{
				break;
			}
		}

	}
}

void Text_Draw(Image* image, const FontData* font_data, float _x, float _y, float scale_x, float scale_y, const char* fmt, ...)
{
	char str[2048];
	memset(str, 0, sizeof(str));

	va_list args;
	va_start(args, fmt);

	vsprintf(str, fmt, args);

	va_end(args);

	Text_DrawStr(image, font_data, _x, _y, scale_x, scale_y, 255, 255, 255, 255, str);
}

void Text_DrawColor(Image* image, const FontData* font_data, float _x, float _y, float scale_x, float scale_y, int r, int g, int b, int a, const char* fmt, ...)
{
	char str[2048];
	memset(str, 0, sizeof(str));

	va_list args;
	va_start(args, fmt);

	vsprintf(str, fmt, args);

	va_end(args);

	Text_DrawStr(image, font_data, _x, _y, scale_x, scale_y, r, g, b, a, str);
}
