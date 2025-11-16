#ifndef BVH_TREE_H
#define BVH_TREE_H
#pragma once

#include <stdbool.h>
#include "u_object_pool.h"

typedef int BVH_ID;

typedef struct
{
	Object_Pool* nodes;
	float thickness;
	int root;
} BVH_Tree;
typedef struct
{
	int data_index;

	int left;
	int right;
	int parent;
	int height;

	float bbox[2][2];
} BVH_Node;

BVH_Tree BVH_Tree_Create(float p_thickness);
void BVH_Tree_Destruct(BVH_Tree* const p_tree);
BVH_ID BVH_Tree_Insert(BVH_Tree* const p_tree, float p_bbox[2][2], int p_dataIndex);
void* BVH_Tree_Remove(BVH_Tree* const p_tree, BVH_ID p_bvhID);
void BVH_Tree_ClearAll(BVH_Tree* const p_tree);
bool BVH_Tree_UpdateBounds(BVH_Tree* const p_tree, BVH_ID p_bvhID, float p_bbox[2][2]);
int BVH_Tree_GetData(BVH_Tree* const p_tree, BVH_ID p_bvhID);
typedef void (*BVH_RegisterFun)(int _data_index, BVH_ID _index, int _hit_count);


int BVH_Tree_Cull_Box(BVH_Tree* const p_tree, float bbox[2][2], int p_maxHitCount, int* p_hits);
int BVH_Tree_Cull_Trace(BVH_Tree* const p_tree, float p_startX, float p_startY, float p_endX, float p_endY, int p_maxHitCount, int* p_hits);

#endif