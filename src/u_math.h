#ifndef UTILITY_MATH
#define UTILITY_MATH
#pragma once

#include <float.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define Math_PI 3.1415926535897932384626433833
#define CMP_EPSILON 0.00001
#define MATH_EQUAL_EPSILON (1/65536.)

static void Math_SwapFloat(float* a, float* b)
{
	float temp = *a;

	*a = *b;
	*b = temp;
}
static inline uint32_t Math_rand()
{
	return rand();
}

static inline float Math_randf()
{
	return (float)Math_rand() / (float)RAND_MAX;
}

static inline double Math_randd()
{
	return (double)Math_rand() / (double)RAND_MAX;
}
static inline float Math_randfb()
{
	return (Math_rand() & 0x7fff) / (float)0x7fff;
}
inline bool Math_IsZeroApprox(float s)
{
	return fabs(s) < (float)CMP_EPSILON;
}
inline bool Math_IsEqualApprox(float left, float right)
{
	if (left == right)
	{
		return true;
	}

	float thr = CMP_EPSILON * fabs(left);

	if (thr < CMP_EPSILON)
	{
		thr = CMP_EPSILON;
	}

	return fabs(left - right) < thr;
}
inline int Math_RoundToInt(double real)
{
	return floor(real + 0.5);
}
inline float Math_Clamp(float v, float min_v, float max_v)
{
	return v < min_v ? min_v : (v > max_v ? max_v : v);
}
inline double Math_Clampd(double v, double min_v, double max_v)
{
	return v < min_v ? min_v : (v > max_v ? max_v : v);
}
inline long Math_Clampl(long v, long min_v, long max_v)
{
	return v < min_v ? min_v : (v > max_v ? max_v : v);
}

inline float Math_DegToRad(float deg) 
{
	return deg * Math_PI / 180.0f;
}
inline float Math_RadToDeg(float rad)
{
	return rad * 180.0f / Math_PI;
}
inline int Math_signf(float x)
{
	return (x < 0) ? -1 : 1;
}
inline float Math_sign_float(float x)
{
	return x > 0 ? +1.0f : (x < 0 ? -1.0f : 0.0f);
}

inline float Math_Smooth(float t) 
{
	return t * t * (3.0f - 2.0f * t);
}

inline float Math_move_towardf(float from, float to, float delta)
{
	return fabsf(to - from) <= delta ? to : from + Math_sign_float(to - from) * delta;
}

inline float Math_lerp(float from, float to, float t) 
{
	return from + (to - from) * t;
}
inline float Math_lerpClamped(float from, float to, float t)
{
	t = Math_Clamp(t, 0, 1);

	return from + (to - from) * t;
}
inline float Math_lerpFraction(float from, float to, float lerp)
{
	return to * lerp + from * (1.0 - lerp);
}
inline float Math_SmoothInterp(float from, float to, float t)
{
	return from + Math_Smooth(t) * (to - from);
}

inline long double Math_fract2(long double x)
{
	return x - floor(x);
}

inline int Math_step(float edge, float x)
{
	return x < edge ? 0 : 1;
}

inline float Math_XY_Distance(float x1, float y1, float x2, float y2)
{
	float dist = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);

	return sqrtf(dist);
}
inline float Math_XY_DistanceSquared(float x1, float y1, float x2, float y2)
{
	float dist = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);

	return dist;
}

inline float Math_XY_Dot(float x1, float y1, float x2, float y2)
{
	return x1 * x2 + y1 * y2;
}

inline float Math_XY_Length(float x, float y)
{
	float dot = Math_XY_Dot(x, y, x, y);

	return sqrtf(dot);
}

inline void Math_XY_Normalize(float* x, float* y)
{
	float x_local = *x;
	float y_local = *y;

	float len = Math_XY_Length(x_local, y_local);

	if (len == 0.0f) 
	{
		*x = 0;
		*y = 0;
		return;
	}
	
	float n = 1.0 / len;

	x_local = x_local * n;
	y_local = y_local * n;

	*x = x_local;
	*y = y_local;
}
inline void Math_XY_Reflect(float x, float y, float nx, float ny, float* r_x, float* r_y)
{
	float normal_dot = Math_XY_Dot(nx, ny, x, y);

	*r_x = 2.0 * nx * normal_dot - x;
	*r_y = 2.0 * ny * normal_dot - y;
}

inline void Math_XY_Bounce(float x, float y, float nx, float ny, float* r_x, float* r_y)
{
	float t_x = *r_x;
	float t_y = *r_y;

	Math_XY_Reflect(x, y, nx, ny, &t_x, &t_y);

	*r_x = -t_x;
	*r_y = -t_y;
}
inline float Math_XY_Cross(float x1, float y1, float x2, float y2)
{
	return x1 * y2 - y1 - x2;
}
inline float Math_XY_Angle(float x, float y)
{
	return atan2(y, x);
}
inline float Math_XY_AngleToPoint(float x1, float y1, float x2, float y2)
{
	float a = Math_XY_Cross(x1, y1, x2, y2);
	float d = Math_XY_Dot(x1, y1, x2, y2);

	return atan2(a, d);
}

inline void Math_XY_Rotate(float* x, float* y, float p_cos, float p_sin)
{
	float t_x = *x;
	float l_y = *y;

	*x = t_x * -p_cos - l_y * p_sin;
	*y = t_x * -p_sin + l_y * p_cos;
}
inline void Math_XY_RotateAngle(float* x, float* y, float angle)
{
	float cos = cosf(angle);
	float sin = sinf(angle);

	Math_XY_Rotate(x, y, cos, sin);
}
inline void Math_XYZ_Cross(float x1, float y1, float z1, float x2, float y2, float z2, float* r_x, float* r_y, float* r_z)
{
	*r_x = y1 * z2 - z1 * y2;
	*r_y = z1 * x2 - x1 * z2;
	*r_z = x1 * y2 - y1 * x2;
}
inline float Math_XYZ_Dot(float x1, float y1, float z1, float x2, float y2, float z2)
{
	return x1 * x2 + y1 * y2 + z1 * z2;
}
inline float Math_XYZ_Length(float x, float y, float z)
{
	float dot = Math_XYZ_Dot(x, y, z, x, y, z);

	return sqrtf(dot);
}
inline void Math_XYZ_Normalize(float* x, float* y, float* z)
{
	float x_local = *x;
	float y_local = *y;
	float z_local = *z;

	float len = Math_XYZ_Length(x_local, y_local, z_local);

	if (len == 0.0f)
	{
		*x = 0;
		*y = 0;
		*z = 0;
		return;
	}

	float n = 1.0 / len;

	x_local = x_local * n;
	y_local = y_local * n;
	z_local = z_local * n;

	*x = x_local;
	*y = y_local;
	*z = z_local;
}
inline float Math_XYZ_Distance(float x1, float y1, float z1, float x2, float y2, float z2)
{
	float dist = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2) + (z1 - z2) * (z1 - z2);

	return sqrtf(dist);
}
inline void Math_XYZ_Swap(float* x1, float* y1, float* z1, float* x2, float* y2, float* z2)
{
	Math_SwapFloat(x1, x2);
	Math_SwapFloat(y1, y2);
	Math_SwapFloat(z1, z2);
}
inline bool Math_TraceLineVsBox(float p_x, float p_y, float p_endX, float p_endY, float box_x, float box_y, float size, float* r_interX, float* r_interY, float* r_dist)
{
	float minDistance = 0;
	float maxDistance = 1;
	int axis = 0;
	float sign = 0;

	float box[2][2];

	box[0][0] = box_x - size;
	box[0][1] = box_y - size;

	box[1][0] = box_x + size;
	box[1][1] = box_y + size;

	for (int i = 0; i < 2; i++)
	{
		float box_begin = box[0][i];
		float box_end = box[1][i];
		float trace_start = (i == 0) ? p_x : p_y;
		float trace_end = (i == 0) ? p_endX : p_endY;

		float c_min = 0;
		float c_max = 0;
		float c_sign = 0;

		if (trace_start < trace_end)
		{
			if (trace_start > box_end || trace_end < box_begin)
			{
				return false;
			}

			float trace_length = trace_end - trace_start;
			c_min = (trace_start < box_begin) ? ((box_begin - trace_start) / trace_length) : 0;
			c_max = (trace_end > box_end) ? ((box_end - trace_start) / trace_length) : 1;
			c_sign = -1.0;
		}
		else
		{
			if (trace_end > box_end || trace_start < box_begin)
			{
				return false;
			}

			float trace_length = trace_end - trace_start;
			c_min = (trace_start > box_end) ? ((box_end - trace_start) / trace_length) : 0;
			c_max = (trace_end < box_begin) ? ((box_begin - trace_start) / trace_length) : 1;
			c_sign = 1.0;
		}

		if (c_min > minDistance)
		{
			minDistance = c_min;
			axis = i;
			sign = c_sign;
		}
		if (c_max < maxDistance)
		{
			maxDistance = c_max;
		}
		if (maxDistance < minDistance)
		{
			return false;
		}
	}

	if (r_interX) *r_interX = p_x + (p_endX - p_x) * minDistance;
	if (r_interY) *r_interY = p_y + (p_endY - p_y) * minDistance;

	if (r_dist)
	{
		*r_dist = minDistance;
	}

	return true;
}
inline bool Math_TraceLineVsBox2(float p_x, float p_y, float p_endX, float p_endY, float bbox[2][2], float* r_interX, float* r_interY, float* r_dist)
{
	float minDistance = 0;
	float maxDistance = 1;
	int axis = 0;
	float sign = 0;

	for (int i = 0; i < 2; i++)
	{
		float box_begin = bbox[0][i];
		float box_end = bbox[1][i];
		float trace_start = (i == 0) ? p_x : p_y;
		float trace_end = (i == 0) ? p_endX : p_endY;

		float c_min = 0;
		float c_max = 0;
		float c_sign = 0;

		if (trace_start < trace_end)
		{
			if (trace_start > box_end || trace_end < box_begin)
			{
				return false;
			}

			float trace_length = trace_end - trace_start;
			c_min = (trace_start < box_begin) ? ((box_begin - trace_start) / trace_length) : 0;
			c_max = (trace_end > box_end) ? ((box_end - trace_start) / trace_length) : 1;
			c_sign = -1.0;
		}
		else
		{
			if (trace_end > box_end || trace_start < box_begin)
			{
				return false;
			}

			float trace_length = trace_end - trace_start;
			c_min = (trace_start > box_end) ? ((box_end - trace_start) / trace_length) : 0;
			c_max = (trace_end < box_begin) ? ((box_begin - trace_start) / trace_length) : 1;
			c_sign = 1.0;
		}

		if (c_min > minDistance)
		{
			minDistance = c_min;
			axis = i;
			sign = c_sign;
		}
		if (c_max < maxDistance)
		{
			maxDistance = c_max;
		}
		if (maxDistance < minDistance)
		{
			return false;
		}
	}

	if (r_interX) *r_interX = p_x + (p_endX - p_x) * minDistance;
	if (r_interY) *r_interY = p_y + (p_endY - p_y) * minDistance;

	if (r_dist)
	{
		*r_dist = minDistance;
	}

	return true;
}
inline bool Math_TraceLineVsBox3D(float p_x, float p_y, float p_z, float p_endX, float p_endY, float p_endZ, float bbox[2][3], float* r_interX, float* r_interY, float* r_interZ, float* r_dist)
{
	float minDistance = 0;
	float maxDistance = 1;
	int axis = 0;
	float sign = 0;

	for (int i = 0; i < 3; i++)
	{
		float box_begin = bbox[0][i];
		float box_end = bbox[1][i];
		float trace_start = (i == 0) ? p_x : p_y;
		float trace_end = (i == 0) ? p_endX : p_endY;

		float c_min = 0;
		float c_max = 0;
		float c_sign = 0;

		if (trace_start < trace_end)
		{
			if (trace_start > box_end || trace_end < box_begin)
			{
				return false;
			}

			float trace_length = trace_end - trace_start;
			c_min = (trace_start < box_begin) ? ((box_begin - trace_start) / trace_length) : 0;
			c_max = (trace_end > box_end) ? ((box_end - trace_start) / trace_length) : 1;
			c_sign = -1.0;
		}
		else
		{
			if (trace_end > box_end || trace_start < box_begin)
			{
				return false;
			}

			float trace_length = trace_end - trace_start;
			c_min = (trace_start > box_end) ? ((box_end - trace_start) / trace_length) : 0;
			c_max = (trace_end < box_begin) ? ((box_begin - trace_start) / trace_length) : 1;
			c_sign = 1.0;
		}

		if (c_min > minDistance)
		{
			minDistance = c_min;
			axis = i;
			sign = c_sign;
		}
		if (c_max < maxDistance)
		{
			maxDistance = c_max;
		}
		if (maxDistance < minDistance)
		{
			return false;
		}
	}

	if (r_interX) *r_interX = p_x + (p_endX - p_x) * minDistance;
	if (r_interY) *r_interY = p_y + (p_endY - p_y) * minDistance;
	if (r_interZ) *r_interZ = p_z + (p_endZ - p_z) * minDistance;

	if (r_dist)
	{
		*r_dist = minDistance;
	}

	return true;
}

inline bool Math_BoxIntersectsBox(float aabb[2][2], float other[2][2])
{
	return (aabb[0][0] <= other[1][0] && aabb[1][0] >= other[0][0])
		&& (aabb[0][1] <= other[1][1] && aabb[1][1] >= other[0][1]);
}

inline bool Math_BoxContainsBox(float aabb[2][2], float other[2][2])
{
	return (aabb[0][0] <= other[0][0] && aabb[1][0] >= other[1][0])
		&& (aabb[0][1] <= other[0][1] && aabb[1][1] >= other[1][1]);
}

inline bool Math_BoxInRangeBox(float aabb[2][2], float other[2][2])
{
	return (aabb[0][0] < other[1][0] || aabb[1][0] > other[0][0])
		|| (aabb[0][1] < other[1][1] || aabb[1][1] > other[0][1]);
}
inline bool Math_BoxContainsPoint(float aabb[2][2], float point[2])
{
	return (point[0] >= aabb[0][0] && point[0] <= aabb[1][0] &&
		point[1] >= aabb[0][1] && point[1] <= aabb[1][1]);
}

inline bool Math_BoxIntersectsCircle(float aabb[2][2], float circle_x, float circle_y, float circle_radius)
{
	float a = (circle_x < aabb[0][0]) + (circle_x > aabb[1][0]);
	float b = (circle_y < aabb[0][1]) + (circle_y > aabb[1][1]);

	float dmin = pow((circle_x - aabb[!(a - 1)][0]) * (a != 0), 2)
		+ pow((circle_y - aabb[!(b - 1)][1]) * (b != 0), 2);

	return dmin <= pow(circle_radius, 2);
}
inline void Math_BoxMerge(float box1[2][2], float box2[2][2], float dest[2][2])
{
	dest[0][0] = min(box1[0][0], box2[0][0]);
	dest[0][1] = min(box1[0][1], box2[0][1]);

	dest[1][0] = max(box1[1][0], box2[1][0]);
	dest[1][1] = max(box1[1][1], box2[1][1]);
}

inline void Math_SizeToBbox(float x, float y, float size, float dest[2][2])
{
	dest[0][0] = x - size;
	dest[0][1] = y - size;

	dest[1][0] = x + size;
	dest[1][1] = y + size;
}
inline void Math_GetBoxCenter(float box[2][2], float* r_centerX, float* r_centerY)
{
	*r_centerX = box[0][0] + ((box[1][0] - box[0][0]) * 0.5);
	*r_centerY = box[0][1] + ((box[1][1] - box[0][1]) * 0.5);
}
inline void Math_GetBoxSize(float box[2][2], float* r_sizeX, float* r_sizeY)
{
	*r_sizeX = box[1][0] - box[0][0];
	*r_sizeY = box[1][1] - box[0][1];
}
inline void Math_BoxRotatedBounds(float box[2][2], float cos, float sin)
{
	float box_width = 0, box_height = 0;
	Math_GetBoxSize(box, &box_width, &box_height);

	float as = fabs(sin);
	float cs = fabs(cos);

	float W = box_width * cs + box_height * as;
	float H = box_width * as + box_height * cs;

	//box[0][0] += (W * 0.5);
	//box[0][1] += (H * 0.5);

	box[1][0] = box[0][0] + W;
	box[1][1] = box[0][1] + H;
}

inline bool Math_PointInsideCircle(float x, float y, float circle_x, float circle_y, float circle_radius)
{
	float sq_dist = Math_XY_DistanceSquared(x, y, circle_x, circle_y);

	return sq_dist <= circle_radius * circle_radius;
}

inline bool Math_RayIntersectsPlane(float x, float y, float ray_x, float ray_y, float normal_x, float normal_y, float d)
{
	float den = Math_XY_Dot(normal_x, normal_y, ray_x, ray_y);

	if (den == 0)
	{
		return false;
	}

	float dist = (Math_XY_Dot(normal_x, normal_y, x, y) - d) / den;

	if (dist > CMP_EPSILON)
	{
		return false;
	}

	dist = -dist;

	return true;
}
inline bool Math_RayIntersectsPlane3D(float x, float y, float z, float ray_x, float ray_y, float ray_z, float normal_x, float normal_y, float normal_z, float d
, float* r_hitX, float* r_hitY, float* r_hitZ)
{
	float den = Math_XYZ_Dot(normal_x, normal_y, normal_z, ray_x, ray_y, ray_z);

	if (den == 0)
	{
		return false;
	}

	float dist = (Math_XYZ_Dot(normal_x, normal_y, normal_z, x, y, z) - d) / den;

	if (dist > CMP_EPSILON)
	{
		return false;
	}

	dist = -dist;
	
	if (r_hitX) *r_hitX = x + ray_x * dist;
	if (r_hitY) *r_hitY = y + ray_y * dist;
	if (r_hitZ) *r_hitZ = z + ray_z * dist;

	return true;
}

inline void Math_GenHemisphereVector(float* r_x, float* r_y, float* r_z)
{
	float u = (float)rand() / (float)RAND_MAX;
	float v = (float)rand() / (float)RAND_MAX;

	float theta = 2.0 * Math_PI * u;
	float phi = acos(2.0 * v - 1.0);
	//*r_x = cos(theta) * sin(phi);
	//*r_y = cos(phi);
	//*r_z = sin(theta) * sin(phi);

	*r_x = cos(theta) * sin(phi);
	*r_y = sin(theta) * sin(phi);
	*r_z = cos(phi);
}


typedef struct
{
	float r, g, b, a;
} Vec4;

inline Vec4 Vec4_Zero()
{
	Vec4 vec;
	vec.r = 0;
	vec.g = 0;
	vec.b = 0;
	vec.a = 0;

	return vec;
}

inline void Vec4_Add(Vec4* a, Vec4 b)
{
	a->r += b.r;
	a->g += b.g;
	a->b += b.b;
	a->a += b.a;
}
inline void Vec4_DivScalar(Vec4* a, float scalar)
{
	a->r /= scalar;
	a->g /= scalar;
	a->b /= scalar;
	a->a /= scalar;
}
inline void Vec4_ScaleScalar(Vec4* a, float scalar)
{
	a->r *= scalar;
	a->g *= scalar;
	a->b *= scalar;
	a->a *= scalar;
}

inline void Vec4_Clamp(Vec4* a, float min, float max)
{
	a->r = Math_Clamp(a->r, min, max);
	a->g = Math_Clamp(a->g, min, max);
	a->b = Math_Clamp(a->b, min, max);
	a->a = Math_Clamp(a->a, min, max);
}

typedef struct
{
	unsigned char r, g, b, a;
} Vec4_u8;

inline Vec4_u8 Vec4_u8_Zero()
{
	Vec4_u8 vec;
	vec.r = 0;
	vec.g = 0;
	vec.b = 0;
	vec.a = 0;

	return vec;
}

typedef struct
{
	unsigned char r, g, b;
} Vec3_u8;

typedef struct
{
	unsigned short r, g, b;
} Vec3_u16;

inline Vec3_u16 Vec3_u16_Zero()
{
	Vec3_u16 vec;
	vec.r = 0;
	vec.g = 0;
	vec.b = 0;

	return vec;
}

typedef struct
{
	unsigned short r, g, b, a;
} Vec4_u16;

#endif // !UTILITY_MATH
