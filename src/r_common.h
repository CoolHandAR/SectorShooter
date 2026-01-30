#ifndef R_COMMON_H
#define R_COMMON_H
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include "u_math.h"

#define MAX_IMAGE_MIPMAPS 8
#define DEPTH_CLEAR 9999999

#define BASE_RENDER_WIDTH 640
#define BASE_RENDER_HEIGHT 360

#define DEPTH_SHADING_SCALE 0.25
#define VIEW_FOV 90

#define MAX_LIGHT_VALUE 255 * 6

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
	//int num_mipmaps;
	//struct Image* mipmaps[MAX_IMAGE_MIPMAPS];
	//float x_scale;
	//float y_scale;

	bool is_collumn_stored;
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
	x = Math_Clampl(x, 0, img->width - 1);
	y = Math_Clampl(y, 0, img->height - 1);

	size_t index = 0;

	if (img->is_collumn_stored)
	{
		index = ((x * img->height) + y) * img->numChannels;
	}
	else
	{
		index = (x + y * img->width) * img->numChannels;
	}

	return &img->data[index];
}
inline unsigned char* Image_GetFast(Image* img, int x, int y)
{
	size_t index = 0;

	if (img->is_collumn_stored)
	{
		index = ((x * img->height) + y) * img->numChannels;
	}
	else
	{
		index = (x + y * img->width) * img->numChannels;
	}

	return &img->data[index];
}

void Image_Blur(Image* img, int size, float scale);
void Image_GenerateFrameInfo(Image* img);

FrameInfo* Image_GetFrameInfo(Image* img, int frame);
AlphaSpan* FrameInfo_GetAlphaSpan(FrameInfo* frame_info, int x);

typedef struct
{
	Image img;
	unsigned char name[10];
	int width_mask;
	int height_mask;
} Texture;

typedef struct
{
	union
	{
		Vec3_u16* data;
		Vec4* float_data; //hack! used only for lightmapping
	};
	
	int width;
	int height;
} Lightmap;

inline Vec3_u16* Lightmap_Get(Lightmap* lightmap, int x, int y)
{
	x = Math_Clampl(x, 0, lightmap->width - 1);
	y = Math_Clampl(y, 0, lightmap->height - 1);

	return &lightmap->data[x + y * lightmap->width];
}
inline Vec3_u16* Lightmap_GetFast(Lightmap* lightmap, int x, int y)
{
	return &lightmap->data[x + y * lightmap->width];
}
static void Lightmap_SampleWallLinearPoints(Lightmap* lightmap, float x, float y, float next_x, float x_frac, Vec4* r_lerp0, Vec4* r_lerp1)
{
	//only designed for wall collumn drawing
	y = Math_Clampl(y, 0, lightmap->height - 1);
	int next_y = y + 1;

	if (next_y >= lightmap->height) next_y = lightmap->height - 1;

	Vec3_u16 s0 = *Lightmap_GetFast(lightmap, x, y);
	Vec3_u16 s1 = *Lightmap_GetFast(lightmap, next_x, y);
	Vec3_u16 s2 = *Lightmap_GetFast(lightmap, x, next_y);
	Vec3_u16 s3 = *Lightmap_GetFast(lightmap, next_x, next_y);

	r_lerp0->r = Math_lerp(s0.r, s1.r, x_frac);
	r_lerp0->g = Math_lerp(s0.g, s1.g, x_frac);
	r_lerp0->b = Math_lerp(s0.b, s1.b, x_frac);

	r_lerp1->r = Math_lerp(s2.r, s3.r, x_frac);
	r_lerp1->g = Math_lerp(s2.g, s3.g, x_frac);
	r_lerp1->b = Math_lerp(s2.b, s3.b, x_frac);
}
inline void Lightmap_SamplePlaneLinearPoints(Lightmap* lightmap, float x, float y, Vec3_u16* r_s0, Vec3_u16* r_s1, Vec3_u16*r_s2, Vec3_u16* r_s3)
{
	//only designed for plane strip drawing
	x = Math_Clampl(x, 0, lightmap->width - 1);
	y = Math_Clampl(y, 0, lightmap->height - 1);
	int next_x = x + 1;
	int next_y = y + 1;

	if (next_x >= lightmap->width) next_x = lightmap->width - 1;
	if (next_y >= lightmap->height) next_y = lightmap->height - 1;

	*r_s0 = *Lightmap_GetFast(lightmap, x, y);
	*r_s1 = *Lightmap_GetFast(lightmap, next_x, y);
	*r_s2 = *Lightmap_GetFast(lightmap, x, next_y);
	*r_s3 = *Lightmap_GetFast(lightmap, next_x, next_y);
}

typedef struct
{
	float x, y, z;
	float offset_x, offset_y, offset_z;
	float scale_x, scale_y;
	bool flip_h, flip_v;

	//modifiers
	float transparency;
	Vec3_u16 light;

	//animation stuff
	float _anim_frame_progress;
	float anim_speed_scale;
	int frame_count;
	int frame;
	bool looping;
	bool playing;
	bool finished;
	bool action_frame_triggered;

	//for decals only
	int decal_line_index;

	int action_frame;

	int frame_offset_x;
	int frame_offset_y;

	//image data
	Image* img;
} Sprite;

void Sprite_UpdateAnimation(Sprite* sprite, float delta);
void Sprite_ResetAnimState(Sprite* sprite);
void Sprite_CheckForActionFunction(Sprite* sprite, int frame);

//Packed, slighty smaller version of sprite, only used for rendering
typedef struct
{
	//image data
	Image* img;

	float x, y, z;
	float scale_x, scale_y;

	short frame;

	short frame_offset_x;
	short frame_offset_y;

	short sprite_rect_width;
	short sprite_rect_height;

	//float transparency;

	Vec3_u16 light;

	int decal_line_index;

	bool flip_h;
	bool flip_v;
} DrawSprite;

typedef struct
{
	short first;
	short last;
} Cliprange;

typedef struct
{
	Cliprange* solidsegs;
	Cliprange* newend;

	//int num_allocated;
} ClipSegments;

typedef struct
{
	short* ytop;
	short* ybottom;

	Lightmap* lightmap;
	struct Texture* texture;
	float viewheight;
	int light;

	bool visible;
	bool is_sky;
} DrawPlane;

typedef struct
{
	float view_x, view_y, view_z;
	float view_sin, view_cos, view_angle;

	float tan_cos, tan_sin;
	float tangent;

	float focal_length_x;

	float h_fov, v_fov;
	int start_x, end_x;

	short* yclip_top;
	short* yclip_bottom;

	short* floor_plane_ytop;
	short* floor_plane_ybottom;

	short* ceil_plane_ytop;
	short* ceil_plane_ybottom;

	float* yslope;

	struct RenderData* render_data;

	float* depth_buffer;

	Vec3_u16 extra_light;
	int extra_light_max;
} DrawingArgs;

typedef struct
{
	bool is_both_sky;

	float plane_base_pos_x;
	float plane_base_pos_y;

	float plane_step_scale_x;
	float plane_step_scale_y;

	float plane_angle;
	float sky_plane_y_step;

	float u0, u1;
	int sy_floor;
	int sy_ceil;

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
	float lowest_ceilling;

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

typedef struct
{
	LineDrawArgs line_draw_args;
	short first, last;
	short light;
} DrawSeg;

#define MAX_DRAWSEGS 32

typedef struct
{
	DrawSeg segs[MAX_DRAWSEGS];
	int index;
} DrawSegList;

#define MAX_DRAWSPRITES 1024

typedef struct
{
	DrawSprite draw_sprites[MAX_DRAWSPRITES];
	int num_draw_sprites;

	ClipSegments clip_segs;
	DrawSegList draw_segs;

	uint64_t* visited_sectors_bitset;
	size_t bitset_size;

	DrawPlane floor_plane;
	DrawPlane ceil_plane;
	short* span_end;
} RenderData;

void Video_Setup();
bool Video_ClipLine(int xmin, int xmax, int ymin, int ymax, int* r_x0, int* r_y0, int* r_x1, int* r_y1);
void Video_DrawLine(Image* image, int x0, int y0, int x1, int y1, unsigned char* color);
void Video_DrawRectangle(Image* image, int p_x, int p_y, int p_w, int p_h, unsigned char* p_color);
void Video_DrawCircle(Image* image, int p_x, int p_y, float radius, unsigned char* p_color);
void Video_DrawBoxLines(Image* image, float box[2][2], unsigned char* color);
void Video_DrawBox(Image* image, float* depth_buffer, float box[2][3], float view_x, float view_y, float view_z, float view_cos, float view_sin, float tan_sin, float tan_cos, float v_fov, int x_start, int x_end, Vec3_u16* light);
void Video_DrawScreenTexture(Image* image, Image* texture, float p_x, float p_y, float p_scaleX, float p_scaleY);
void Video_DrawScreenSprite(Image* image, Sprite* sprite, int start_x, int end_x);
void Video_DrawSprite(Image* image, DrawingArgs* args, DrawSprite* sprite);
void Video_DrawDecalSprite(Image* image, DrawingArgs* args, DrawSprite* sprite);
void Video_DrawWallCollumn(Image* image, float* depth_buffer, struct Texture* texture, int x, int y1, int y2, float depth, int tx, float ty_pos, float ty_step, int lx, float ly_pos, Vec3_u16 light, int height_mask, Lightmap* lm);
void Video_DrawWallCollumnDepth(Image* image, struct Texture* texture, Lightmap* lm, float* depth_buffer, int x, int y1, int y2, float z, int tx, float ty_pos, float ty_step, int lx, float ly_pos, Vec3_u16 light, int height_mask);
void Video_DrawSkyPlaneStripe(Image* image, float* depth_buffer, struct Texture* texture, int x, int y1, int y2, LineDrawArgs* args);
void Video_DrawPlaneSpan(Image* image, DrawPlane* plane, LineDrawArgs* args, int y, int x1, int x2);
void Video_DrawThreadSlice(Image* image, int x1, int x2, Vec3_u16* color);

typedef void (*ShaderFun)(Image* image, int x, int y);
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
	TWT__DRAW_LEVEL,
	TWT__DRAW_HUD,
	TWT__EXIT
} ThreadWorkType;

typedef struct
{
	RenderData render_data;

	ThreadState state;

	HANDLE thread_handle;

	CRITICAL_SECTION mutex;
	HANDLE active_event;

	int x_start, x_end;
} RenderThread;

bool Render_Init(int width, int height, int scale);
void Render_ShutDown();
void Render_LockThreadsMutex();
void Render_UnlockThreadsMutex();
void Render_LockObjectMutex(bool writer);
void Render_UnlockObjectMutex(bool writer);
void Render_FinishAndStall();
void Render_Resume();
void Render_QueueFullscreenShader(ShaderFun shader_fun);

void Render_SetExtraLightFrame(Vec3_u16 extra_light);
void Render_ResizeWindow(int width, int height);
void Render_View(float x, float y, float z, float angle, float angleCos, float angleSin);
void Render_GetWindowSize(int* r_width, int* r_height);
void Render_GetRenderSize(int* r_width, int* r_height);
int Render_GetRenderScale();
void Render_SetRenderScale(int scale);
float Render_GetWindowAspect();
int Render_IsFullscreen();
void Render_ToggleFullscreen();
int Render_GetTicks();
void Render_Clear(int c);

void RenderUtl_ResetClip(ClipSegments* clip, short left, short right);
void RenderUtl_SetupRenderData(RenderData* data, int width, int x_start, int x_end);
void RenderUtl_Resize(RenderData* data, int width, int height, int x_start, int x_end);
void RenderUtl_DestroyRenderData(RenderData* data);
bool RenderUtl_CheckVisitedSectorBitset(RenderData* data, int sector);
void RenderUtl_SetVisitedSectorBitset(RenderData* data, int sector);
void RenderUtl_AddSpriteToQueue(RenderData* data, Sprite* sprite, int sector_light, Vec3_u16 extra_light, bool is_decal);

void Scene_DrawLineSeg(Image* image, int first, int last, LineDrawArgs* args);
void Scene_ClipAndDraw(ClipSegments* p_clip, int first, int last, bool solid, LineDrawArgs* args, Image* image);
bool Scene_RenderLine(Image* image, struct Map* map, struct Sector* sector, struct Line* line, DrawingArgs* args);
void Scene_ProcessSubsector(Image* image, struct Map* map, struct Subsector* sub_sector, DrawingArgs* args);
int Scene_ProcessBSPNode(Image* image, struct Map* map, int node_index, DrawingArgs* args);
void Scene_DrawDrawSegs(Image* image, DrawSegList* seg_list, float* depth_buffer, DrawingArgs* args);

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

void Shader_Hurt(Image* img, int x, int y);
void Shader_HurtSimple(Image* img, int x, int y);
void Shader_Dead(Image* img, int x, int y);
void Shader_Godmode(Image* img, int x, int y);
void Shader_Quad(Image* img, int x, int y);

#endif