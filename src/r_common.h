#ifndef R_COMMON_H
#define R_COMMON_H
#pragma once

#include "engine_common.h"
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <windows.h>

#define MAX_IMAGE_MIPMAPS 8
#define DEPTH_CLEAR 9999999

#define BASE_RENDER_WIDTH 640
#define BASE_RENDER_HEIGHT 360

#define LIGHT_LOW 0
#define LIGHT_HIGH 1

typedef struct
{
	uint8_t light;
	uint8_t hit_tile;
	int pointer_index;
	int width;
	int tex_x;
	float wall_dist;
	float floor_x;
	float floor_y;

	float cyb, cya;
} DrawSpan;

typedef struct
{
	int min, max;
} AlphaSpan;

typedef struct
{
	int min_real_x;
	int max_real_x;
	int width;
	AlphaSpan* alpha_spans;
} FrameInfo;

typedef struct
{
	int half_width;
	int half_height;

	int numChannels;
	int width;
	int height;
	unsigned char* data;

	int h_frames;
	int v_frames;

	FrameInfo* frame_info;

	//mipmap stuff
	int num_mipmaps;
	struct Image* mipmaps[MAX_IMAGE_MIPMAPS];
	float x_scale;
	float y_scale;
} Image;

bool Image_Create(Image* img, int p_width, int p_height, int p_numChannels);
bool Image_CreateFromPath(Image* img, const char* path);
bool Image_SaveToPath(Image* img, const char* filename);
void Image_Resize(Image* img, int p_width, int p_height);
void Image_Destruct(Image* img);
void Image_Clear(Image* img, int c);
void Image_Copy(Image* dest, Image* src);
void Image_Blit(Image* dest, Image* src, int dstX0, int dstY0, int dstX1, int dstY1, int srcX0, int srcY0, int srcX1, int srcY1);
void Image_Set(Image* img, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

inline void Image_Set2(Image* img, int x, int y, unsigned char* color)
{
	if (!img->data || x < 0 || y < 0 || x >= img->width || y >= img->height)
	{
		return;
	}

	unsigned char* d = img->data + (x + y * img->width) * img->numChannels;

	memcpy(d, color, img->numChannels);
}
inline void Image_Set3(Image* img, int x, int y, unsigned char* color, int numChannels)
{
	if (!img->data || x < 0 || y < 0 || x >= img->width || y >= img->height)
	{
		return;
	}

	unsigned char* d = img->data + (x + y * img->width) * img->numChannels;

	memcpy(d, color, numChannels);
}
inline void Image_SetFast(Image* img, int x, int y, unsigned char* color)
{
	unsigned char* d = img->data + (x + y * img->width) * img->numChannels;

	memcpy(d, color, img->numChannels);
}
inline void Image_SetScaled(Image* img, int x, int y, float scale, unsigned char* color)
{
	unsigned char* d = img->data + (x + y * img->width) * img->numChannels;

	//no scale, so set directly
	if (scale == 1)
	{
		memcpy(d, color, img->numChannels);
	}
	//completely black
	else if (scale <= 0)
	{
		unsigned char clr[4] = { 0, 0, 0, 255 };

		memcpy(d, clr, img->numChannels);
	}
	//scale the color
	else
	{
		unsigned char clr[4];

		for (int i = 0; i < img->numChannels; i++)
		{
			//skip alpha component
			if (i == 3)
			{
				clr[i] == 255;
				continue;
			}

			int c = (float)color[i] * scale;

			if (c > 255)
			{
				c = 255;
			}

			clr[i] = c;
		}

		memcpy(d, clr, img->numChannels);
	}
	
}

inline unsigned char* Image_Get(Image* img, int x, int y)
{
	if (x < 0 || x >= img->width)
	{
		x &= img->width - 1;
	}
	if (y < 0 || y >= img->height)
	{
		y &= img->height - 1;
	}

	return img->data + (x + y * img->width) * img->numChannels;
}
inline unsigned char* Image_Get2(Image* img, int x, int y)
{
	return img->data + (x + y * img->width) * img->numChannels;
}

//unfinished
inline unsigned char* Image_GetMipmapped(Image* img, int x, int y, float dist)
{
	dist = fabs(dist);

	int mipmap_index = (int)dist;

	if (dist >= img->num_mipmaps - 1)
	{
		mipmap_index = img->num_mipmaps - 1;
	}

	if (mipmap_index == 0)
	{
		return Image_Get(img, x, y);
	}

	Image* mip_map = img->mipmaps[mipmap_index];

	x *= mip_map->x_scale;
	y *= mip_map->y_scale;

	if (x < 0)
	{
		x = 0;
	}
	else if (x >= mip_map->width)
	{
		x = mip_map->width - 1;
	}
	if (y < 0)
	{
		y = 0;
	}
	else if (y >= mip_map->height)
	{
		y = mip_map->height - 1;
	}

	return mip_map->data + (x + y * mip_map->width) * mip_map->numChannels;
}

void Image_Blur(Image* img, int size, float scale);
void Image_GenerateMipmaps(Image* img);
void Image_GenerateFrameInfo(Image* img);

FrameInfo* Image_GetFrameInfo(Image* img, int frame);
AlphaSpan* FrameInfo_GetAlphaSpan(FrameInfo* frame_info, int x);

typedef struct
{
	size_t offset;
	size_t count;
} CollumnData;

typedef struct
{
	int half_width;
	int half_height;

	int numChannels;
	int width;
	int height;
	CollumnData* collumn_data;
	unsigned int* data;

	int h_frames;
	int v_frames;
} CollumnImage;

bool CollumnImage_Create(CollumnImage* col_image, int p_width, int p_height, int p_numChannels);
void CollumnImage_Destruct(CollumnImage* col_image);

typedef struct
{
	float x, y, z;
	float offset_x, offset_y;
	float scale_x, scale_y;
	float dist;
	float v_offset;
	bool flip_h, flip_v;

	//modifiers
	float transparency;
	float light;

	//animation stuff
	float _anim_frame_progress;
	float anim_speed_scale;
	int frame_count;
	int frame;
	bool looping;
	bool playing;

	int loops;
	int action_loop;

	int frame_offset_x;
	int frame_offset_y;

	//image data
	Image* img;
} Sprite;

//Packed, slighty smaller version of sprite, only used for rendering
typedef struct
{
	//image data
	Image* img;

	float x, y, z;
	float scale_x, scale_y;
	float dist;

	short frame;

	short frame_offset_x;
	short frame_offset_y;

	short sprite_rect_width;
	short sprite_rect_height;

	float transparency;

	short light;

	bool flip_h;
	bool flip_v;
} DrawSprite;

void Sprite_UpdateAnimation(Sprite* sprite, float delta);
void Sprite_ResetAnimState(Sprite* sprite);

#define MAX_CLIPSEGMENTS 2000

typedef struct
{
	short first;
	short last;
} Cliprange;

typedef struct
{
	Cliprange solidsegs[MAX_CLIPSEGMENTS];
	Cliprange* newend;
} ClipSegments;

#define MAX_DRAW_COLLUMNS 2000
typedef struct
{
	unsigned char light;
	short x;
	short y1;
	short y2;
	short tx;
	int depth;
	float ty_pos;
	float ty_step;
	struct Texture* texture;
} DrawCollumn;

typedef struct
{
	DrawCollumn collumns[MAX_DRAW_COLLUMNS];
	int index;
} DrawCollumns;

#define MAX_DRAWSPRITES 1024

typedef struct
{
	DrawSprite draw_sprites[MAX_DRAWSPRITES];
	int num_draw_sprites;

	ClipSegments clip_segs;
	DrawCollumns draw_collums;

	uint64_t* visited_sectors_bitset;
	size_t bitset_size;
} RenderData;

typedef struct
{
	float view_x, view_y, view_z;
	float view_sin, view_cos;
	float h_fov, v_fov;
	int start_x, end_x;

	short* yclip_top;
	short* yclip_bottom;

	RenderData* render_data;

	float* depth_buffer;
} DrawingArgs;

typedef struct
{
	float u0, u1;
	int sy_floor;
	int sy_ceil;

	float floorz;
	float ceilz;

	float ceil_step;
	float floor_step;
	float depth_step;

	float tz1, tz2;

	int back_sy_floor;
	int back_sy_ceil;

	float backceil_step;
	float backfloor_step;

	float sector_height;

	float backsector_floor_height;
	float backsector_ceil_height;

	float highest_floor;

	float world_low;
	float world_high;
	float world_top;
	float world_bottom;

	int x1;
	int x2;

	struct Line* line;
	struct Sector* sector;

	DrawingArgs* draw_args;
} LineDrawArgs;

void Video_Setup();
bool Video_ClipLine(int xmin, int xmax, int ymin, int ymax, int* r_x0, int* r_y0, int* r_x1, int* r_y1);
void Video_DrawLine(Image* image, int x0, int y0, int x1, int y1, unsigned char* color);
void Video_DrawRectangle(Image* image, int p_x, int p_y, int p_w, int p_h, unsigned char* p_color);
void Video_DrawCircle(Image* image, int p_x, int p_y, float radius, unsigned char* p_color);
void Video_DrawBoxLines(Image* image, float box[2][2], unsigned char* color);

void Video_DrawBox(Image* image, float box[2][3], float view_x, float view_y, float view_z, float view_cos, float view_sin, float h_fov, float v_fov);
void Video_DrawSprite(Image* image, Sprite* sprite, float* depth_buffer, float p_x, float p_y, float p_dirX, float p_dirY, float p_planeX, float p_planeY);
void Video_DrawScreenTexture(Image* image, Image* texture, float p_x, float p_y, float p_scaleX, float p_scaleY);
void Video_DrawScreenSprite(Image* image, Sprite* sprite, int start_x, int end_x);

void Video_DrawSprite2(Image* image, float* depth_buffer, Sprite* sprite, float p_x, float p_y, float p_z, float angle);

void Video_DrawSectorCollumn(Image* image, Image* texture, int x, int y1, int y2, int tx, float ty_start, float ty_scale);
void Video_DrawDoomSectors(Image* image, float p_x, float p_y, float p_z, float p_yaw, float p_angle);

void Video_DrawSprite3(Image* image, DrawingArgs* args, DrawSprite* sprite);

typedef void (*ShaderFun)(Image* image, int x, int y, int tx, int ty);
void Video_Shade(Image* image, ShaderFun shader_fun, int x0, int y0, int x1, int y1);

typedef enum
{
	TS__NONE,
	TS__SLEEPING,
	TS__WORKING,
} ThreadState;

typedef enum
{
	TWT__NONE,

	TWT__SHADER,
	TWT__DRAW_LEVEL
} ThreadWorkType;

typedef struct
{
	RenderData render_data;

	ThreadState state;
	ThreadWorkType work_type;

	HANDLE thread_handle;

	CRITICAL_SECTION mutex;
	HANDLE start_work_event;
	HANDLE active_event;
	HANDLE finished_work_event;

	int x_start, x_end;
	bool shutdown;
} RenderThread;

bool Render_Init(int width, int height);
void Render_ShutDown();
void Render_LockThreadsMutex();
void Render_UnlockThreadsMutex();
void Render_LockObjectMutex(bool writer);
void Render_UnlockObjectMutex(bool writer);
void Render_FinishAndStall();
void Render_Resume();
void Render_AddScreenSpriteToQueue(Sprite* sprite);
void Render_QueueFullscreenShader(ShaderFun shader_fun);

void Render_ResizeWindow(int width, int height);
void Render_View(float x, float y, float z, float angleCos, float angleSin);
void Render_GetWindowSize(int* r_width, int* r_height);
void Render_GetRenderSize(int* r_width, int* r_height);
int Render_GetRenderScale();
void Render_SetRenderScale(int scale);
float Render_GetWindowAspect();
int Render_IsFullscreen();
void Render_ToggleFullscreen();
int Render_GetTicks();

void RenderUtl_ResetClip(ClipSegments* clip, short left, short right);
void RenderUtl_SetupRenderData(RenderData* data, int width, int x_start, int x_end);
void RenderUtl_DestroyRenderData(RenderData* data);
bool RenderUtl_CheckVisitedSectorBitset(RenderData* data, int sector);
void RenderUtl_SetVisitedSectorBitset(RenderData* data, int sector);
void RenderUtl_AddSpriteToQueue(RenderData* data, Sprite* sprite, int sector_light);

void Scene_ResetClip(ClipSegments* clip, short left, short right);
void Scene_DrawLineSeg(Image* image, int first, int last, LineDrawArgs* args);
void Scene_ClipAndDraw(ClipSegments* p_clip, int first, int last, bool solid, LineDrawArgs* args, Image* image);
bool Scene_RenderLine(Image* image, struct Map* map, struct Sector* sector, struct Line* line, DrawingArgs* args);
void Scene_ProcessSubsector(Image* image, struct Map* map, struct Subsector* sub_sector, DrawingArgs* args);
int Scene_ProcessBSPNode(Image* image, struct Map* map, int node_index, DrawingArgs* args);
void Scene_DrawDrawCollumns(Image* image, DrawCollumns* collumns, float* depth_buffer);
void Scene_Setup(int width, int height, float h_fov, float v_fov);

#define MAX_FONT_GLYPHS 100

typedef struct
{
	int distance_range;
	float size;
	int width, height;
} FontAtlasData;
typedef struct
{
	int em_size;
	double line_height;
	double ascender;
	double descender;
	double underline_y;
	double underline_thickness;
}  FontMetricData;

typedef struct
{
	double left;
	double bottom;
	double right;
	double top;
} FontBounds;

typedef struct
{
	unsigned unicode;
	double advance;
	FontBounds plane_bounds;
	FontBounds atlas_bounds;
} FontGlyphData;

typedef struct
{
	Image font_image;
	FontAtlasData atlas_data;
	FontMetricData metrics_data;
	FontGlyphData glyphs_data[MAX_FONT_GLYPHS];
} FontData;


bool Text_LoadFont(const char* filename, const char* image_path, FontData* font_data);
FontGlyphData* FontData_GetGlyphData(const FontData* font_data, char ch);
void Text_DrawStr(Image* image, const FontData* font_data, float _x, float _y, float scale_x, float scale_y, int start_x, int end_x, int r, int g, int b, int a, const char* str);
void Text_Draw(Image* image, const FontData* font_data, float _x, float _y, float scale_x, float scale_y, int start_x, int end_x, const char* fmt, ...);
void Text_DrawColor(Image* image, const FontData* font_data, float _x, float _y, float scale_x, float scale_y, int start_x, int end_x, int r, int g, int b, int a, const char* fmt, ...);

#endif