#include "g_common.h"

#include "u_math.h"

#define MAX_ITEMS 2000
#define MANUAL_MOVE_SPEED 500
#define MAX_ZOOM_LEVEL 4
#define ZOOM_STEP 0.25
#define WORLD_CLAMP_BIAS 32
#define PLAYER_ARROW_BELL_SIZE 2
#define PLAYER_ARROW_LENGTH 24
#define PLAYER_ARROW_HEAD_LENGTH 8

static const unsigned char PLAYER_COLOR[3] = { 255, 128, 0 };
static const unsigned char WALL_COLOR[3] = { 255, 255, 0 };
static const unsigned char FLOOR_MISMATCH_COLOR[3] = { 255, 0, 0 };
static const unsigned char CEIL_MISMATCH_COLOR[3] = { 0, 0, 255 };
static const unsigned char SPECIAL_WALL_COLOR[3] = { 0, 255, 255 };

typedef struct
{
	float player_x, player_y, player_angle;
	float center_x, center_y;
	float angle;
	int zoom_level;
	
	int w_offset;
	int h_offset;

	bool rotate;
	bool player_follow_mode;

	int mode;

	float bbox[2][2];
	float input_timer;
	float pulse;
	int pulse_dir;

	int result_traces[MAX_ITEMS];
} VisualMap;

static VisualMap s_visualMap;

static void Register_Result(int _data_index, BVH_ID _index, int _hit_count)
{
	s_visualMap.result_traces[_hit_count] = _data_index;
}

static void VisualMap_ProcessInput(GLFWwindow* window, float delta)
{
	if (s_visualMap.input_timer > 0)
	{
		return;
	}

	if (s_visualMap.mode > 0)
	{
		if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
		{
			s_visualMap.player_follow_mode = !s_visualMap.player_follow_mode;
			s_visualMap.input_timer = 1;
		}
		else if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
		{
			s_visualMap.zoom_level += 1;
			s_visualMap.input_timer = 0.25;
		}
		else if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
		{
			s_visualMap.rotate = !s_visualMap.rotate;
			s_visualMap.input_timer = 0.25;
		}
		if (!s_visualMap.player_follow_mode)
		{
			float move_x = 0;
			float move_y = 0;

			if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
			{
				move_x = -1;
			}
			else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
			{
				move_x = 1;
			}
			if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
			{
				move_y = -1;
			}
			else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
			{
				move_y = 1;
			}

			s_visualMap.center_x += (move_x * MANUAL_MOVE_SPEED) * delta;
			s_visualMap.center_y += (move_y * MANUAL_MOVE_SPEED) * delta;
		}
	}
	if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
	{
		s_visualMap.mode = (s_visualMap.mode + 1) % 3;
		s_visualMap.input_timer = 0.5;
	}
}

void VisualMap_Init()
{
	memset(&s_visualMap, 0, sizeof(s_visualMap));
	s_visualMap.zoom_level = 1;
	s_visualMap.pulse_dir = 1;
	s_visualMap.rotate = true;
	s_visualMap.player_follow_mode = true;
}

void VisualMap_Update(GLFWwindow* window, double delta)
{
	VisualMap_ProcessInput(window, delta);

	if (s_visualMap.input_timer > 0)
	{
		s_visualMap.input_timer -= delta;
	}
	if (s_visualMap.mode == 0)
	{
		return;
	}

	Render_LockObjectMutex(true);

	//update some timers and make sure limits are set
	if (s_visualMap.zoom_level <= 0 || s_visualMap.zoom_level > MAX_ZOOM_LEVEL)
	{
		s_visualMap.zoom_level = 1;
	}
	if (s_visualMap.pulse >= 1)
	{
		s_visualMap.pulse_dir = -1;
	}
	else if (s_visualMap.pulse <= 0)
	{
		s_visualMap.pulse_dir = 1;
	}
	if (!s_visualMap.player_follow_mode)
	{
		s_visualMap.rotate = false;
	}

	s_visualMap.pulse = Math_Clamp(s_visualMap.pulse + (delta * 1) * s_visualMap.pulse_dir, 0, 1);
	
	Map* map = Map_GetMap();

	int width = 0, height = 0;
	Render_GetRenderSize(&width, &height);
	
	float scaled_zoom_level = ((float)s_visualMap.zoom_level * (float)ZOOM_STEP) * (float)Render_GetRenderScale();

	width = ((float)width / 2.0) / scaled_zoom_level;
	height = ((float)height / 2.0) / scaled_zoom_level;

	Player_GetView(&s_visualMap.player_x, &s_visualMap.player_y, NULL, NULL, &s_visualMap.player_angle);

	if (s_visualMap.player_follow_mode)
	{
		s_visualMap.center_x = s_visualMap.player_x;
		s_visualMap.center_y = s_visualMap.player_y;
		s_visualMap.angle = s_visualMap.player_angle;
	}
		
	if (s_visualMap.rotate)
	{
		//not corrent but, good enough for now
		s_visualMap.bbox[0][0] = (s_visualMap.center_x - (width * 2));
		s_visualMap.bbox[0][1] = (s_visualMap.center_y - (height * 2));

		s_visualMap.bbox[1][0] = (s_visualMap.center_x + (width * 2));
		s_visualMap.bbox[1][1] = (s_visualMap.center_y + (height * 2));
	}
	else
	{
		//s_visualMap.center_x = Math_Clamp(s_visualMap.center_x, (map->world_bounds[0][0] - WORLD_CLAMP_BIAS) + width, (map->world_bounds[1][0] + WORLD_CLAMP_BIAS) - width);
		//s_visualMap.center_y = Math_Clamp(s_visualMap.center_y, (map->world_bounds[0][1] - WORLD_CLAMP_BIAS) + height, (map->world_bounds[1][1] + WORLD_CLAMP_BIAS) - height);

		s_visualMap.bbox[0][0] = (s_visualMap.center_x - width);
		s_visualMap.bbox[0][1] = (s_visualMap.center_y - height);

		s_visualMap.bbox[1][0] = (s_visualMap.center_x + width);
		s_visualMap.bbox[1][1] = (s_visualMap.center_y + height);
	}

	if (s_visualMap.rotate)
	{
		s_visualMap.angle += Math_DegToRad(90);
		//s_visualMap.angle = -s_visualMap.angle;

		Math_XY_Rotate(&s_visualMap.center_x, &s_visualMap.center_y, cosf(s_visualMap.angle), sinf(s_visualMap.angle));
	}

	s_visualMap.w_offset = width;
	s_visualMap.h_offset = height;

	Render_UnlockObjectMutex(true);
}

void VisualMap_Draw(Image* image, FontData* font)
{
	//visual map is closed
	if (s_visualMap.mode == 0)
	{
		return;
	}

	Render_LockObjectMutex(false);

	//keep everything on the stack, so that the mutex would not last so long
	bool rotate = s_visualMap.rotate;
	int mode = s_visualMap.mode;

	float bbox[2][2];
	memcpy(bbox, s_visualMap.bbox, sizeof(bbox));

	Map* map = Map_GetMap();

	float angle = s_visualMap.angle;
	float cos_angle = cosf(angle);
	float sin_angle = sinf(angle);

	float player_angle = s_visualMap.player_angle;
	float player_x = s_visualMap.player_x;
	float player_y = s_visualMap.player_y;

	float center_x = s_visualMap.center_x - s_visualMap.w_offset;
	float center_y = s_visualMap.center_y - s_visualMap.h_offset;

	float pulse = s_visualMap.pulse;
	float zoom_level = ((float)s_visualMap.zoom_level * (float)ZOOM_STEP) * (float)Render_GetRenderScale();

	Render_UnlockObjectMutex(false);

	if (mode == 1)
	{
		Image_Clear(image, 0);
	}

	int num_hits = BVH_Tree_Cull_Box(Map_GetSpatialTree(), bbox, MAX_ITEMS, Register_Result);

	for (int i = 0; i < num_hits; i++)
	{
		int index = s_visualMap.result_traces[i];

		//Is a line
		if (index < 0)
		{
			index = -(index + 1);
			Line* line = &map->line_segs[index];

			if (!(line->flags & MF__LINE_MAPPED) || (line->flags & MF__LINE_DONT_DRAW))
			{
				continue;
			}

			Sector* frontsector = Map_GetSector(line->front_sector);
			Sector* backsector = NULL;
			if (line->back_sector >= 0)
			{
				backsector = Map_GetSector(line->back_sector);
			}

			unsigned char color[4] = { WALL_COLOR[0], WALL_COLOR[1], WALL_COLOR[2], 255};

			if (backsector && !(line->flags & MF__LINE_SECRET))
			{
				if (line->special == 1 || line->special == 62)
				{
					for (int k = 0; k < 3; k++)
					{
						color[k] = SPECIAL_WALL_COLOR[k] * pulse;
					}
				}
				else if (frontsector->floor != backsector->floor)
				{
					color[0] = FLOOR_MISMATCH_COLOR[0]; color[1] = FLOOR_MISMATCH_COLOR[1]; color[2] = FLOOR_MISMATCH_COLOR[2];
				}
				else if (frontsector->ceil != backsector->ceil)
				{
					color[0] = CEIL_MISMATCH_COLOR[0]; color[1] = CEIL_MISMATCH_COLOR[1]; color[2] = CEIL_MISMATCH_COLOR[2];
				}
			}

			float line_x0 = line->x0;
			float line_y0 = line->y0;

			float line_x1 = line->x1;
			float line_y1 = line->y1;

			if (rotate)
			{
				Math_XY_Rotate(&line_x0, &line_y0, cos_angle, sin_angle);
				Math_XY_Rotate(&line_x1, &line_y1, cos_angle, sin_angle);
			}

			int x0 = (line_x0 - center_x) * zoom_level;
			int y0 = (line_y0 - center_y) * zoom_level;
		    int x1 = (line_x1 - center_x) * zoom_level;
			int y1 = (line_y1 - center_y) * zoom_level;

			Video_DrawLine(image, x0, y0, x1, y1, color);
		}
		else
		{
			
		}
	}


	//draw player arrow
	unsigned char color[4] = { PLAYER_COLOR[0], PLAYER_COLOR[1], PLAYER_COLOR[2], 255};

	float arrow_length = PLAYER_ARROW_LENGTH;
	float head_length = PLAYER_ARROW_HEAD_LENGTH;

	float pcos = cosf(player_angle);
	float psin = sinf(player_angle);

	float px0 = (player_x);
	float py0 = (player_y);
	float px1 = (player_x + (pcos * arrow_length));
	float py1 = (player_y + (psin * arrow_length));

	if (rotate)
	{
		Math_XY_Rotate(&px0, &py0, cos_angle, sin_angle);
		Math_XY_Rotate(&px1, &py1, cos_angle, sin_angle);
	}
	px0 = (px0 - center_x) * zoom_level;
	py0 = (py0 - center_y) * zoom_level;
	px1 = (px1 - center_x) * zoom_level;
	py1 = (py1 - center_y) * zoom_level;

	//draw bell
	Video_DrawCircle(image, px0, py0, PLAYER_ARROW_BELL_SIZE * zoom_level, color);
	//draw line
	Video_DrawLine(image, px0, py0, px1, py1, color);

	//draw the head ends
	float head_end_x = (player_x + (pcos * arrow_length)) - (head_length * cos(player_angle + Math_PI / 7));
	float head_end_y = (player_y + (psin * arrow_length)) - (head_length * sin(player_angle + Math_PI / 7));

	if (rotate)
	{
		Math_XY_Rotate(&head_end_x, &head_end_y, cos_angle, sin_angle);
	}

	head_end_x = (head_end_x - center_x) * zoom_level;
	head_end_y = (head_end_y - center_y) * zoom_level;

	Video_DrawLine(image, px1, py1, head_end_x, head_end_y, color);

	head_end_x = (player_x + (pcos * arrow_length)) - (head_length * cos(player_angle - Math_PI / 7));
	head_end_y = (player_y + (psin * arrow_length)) - (head_length * sin(player_angle - Math_PI / 7));

	if (rotate)
	{
		Math_XY_Rotate(&head_end_x, &head_end_y, cos_angle, sin_angle);
	}

	head_end_x = (head_end_x - center_x) * zoom_level;
	head_end_y = (head_end_y - center_y) * zoom_level;

	Video_DrawLine(image, px1, py1, head_end_x, head_end_y, color);

	//draw input tooltips
	Text_DrawColor(image, font, 0.1, 0.9, 0.3, 0.3, 0, image->width, 255, 255, 255, 255, "F = FOLLOW PLAYER");
	Text_DrawColor(image, font, 0.3, 0.9, 0.3, 0.3, 0, image->width, 255, 255, 255, 255, "R = ROTATE");
	if (!s_visualMap.player_follow_mode)
	{
		Text_DrawColor(image, font, 0.45, 0.9, 0.3, 0.3, 0, image->width, 255, 255, 255, 255, "ARROW_KEYS = MOVE POSITION");
	}
}

int VisualMap_GetMode()
{
	return s_visualMap.mode;
}
