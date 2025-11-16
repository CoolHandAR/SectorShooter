#include "BVH_Tree2D.h"

#include <assert.h>
#include "u_math.h"
#include <stdlib.h>
#include "dynamic_array.h"

//inspired by https://web.archive.org/web/20240328144640/https://www.azurefromthetrenches.com/introductory-guide-to-aabb-tree-collision-detection/
//and https://github.com/RandyGaul/qu3e/blob/master/src/broadphase/q3DynamicAABBTree.cpp#L240

#define BVH_NODE_NULL_INDEX -1
#define BVH_STACK_HELPER_ALLOC_SIZE 128

typedef struct
{
	int index;
	bool fully_within;
} BVH_StackItem;

typedef struct
{
	BVH_StackItem items[BVH_STACK_HELPER_ALLOC_SIZE];
	int index;

	dynamic_array* fallback_stack;
} BVH_StackHelper;

static BVH_StackHelper BVH_StackHelper_Create()
{
	BVH_StackHelper sh;
	memset(&sh, 0, sizeof(sh));
	
	return sh;
}

static inline void BVH_StackHelper_Push(BVH_StackHelper* stack, int p_index, bool p_fully_within)
{
	if (stack->index >= BVH_STACK_HELPER_ALLOC_SIZE)
	{
		if (!stack->fallback_stack)
		{
			stack->fallback_stack = dA_INIT(BVH_StackItem, BVH_STACK_HELPER_ALLOC_SIZE);
		}

		BVH_StackItem* item = dA_emplaceBack(stack->fallback_stack);

		item->index = p_index;
		item->fully_within = p_fully_within;
		return;
	}

	stack->items[stack->index].index = p_index;
	stack->items[stack->index].fully_within = p_fully_within;

	stack->index++;
}

static inline void BVH_StackHelper_Pop(BVH_StackHelper* stack)
{
	if (stack->index <= 0)
	{
		stack->items[0].index = -1;
		stack->items[0].fully_within = false;
		return;
	}
	if (stack->fallback_stack)
	{
		if (dA_size(stack->fallback_stack) > 0)
		{
			dA_popBack(stack->fallback_stack);
			return;
		}
	}

	stack->items[stack->index - 1].index = -1;
	stack->index--;
}

static inline BVH_StackItem BVH_StackHelper_GetTop(BVH_StackHelper* stack)
{
	if (stack->index <= 0)
	{
		return stack->items[0];
	}
	if (stack->fallback_stack)
	{
		if (dA_size(stack->fallback_stack) > 0)
		{
			return *(BVH_StackItem*)dA_getLast(stack->fallback_stack);
		}
	}
	return stack->items[stack->index - 1];
}

static inline void BVH_StackHelper_Exit(BVH_StackHelper* stack)
{
	if (stack->fallback_stack)
	{
		dA_Destruct(stack->fallback_stack);
	}
}

static float BVH_calcBoxArea(float box[2][2])
{
	float box_size_x = box[1][0] - box[0][0];
	float box_size_y = box[1][1] - box[0][1];

	return box_size_x * box_size_y;
}

static void BVH_ReleaseNode(BVH_Tree* const _tree, int _index)
{
	BVH_Node* node = dA_at(_tree->nodes->pool, _index);

	memset(node, 0, sizeof(BVH_Node));

	node->parent = BVH_NODE_NULL_INDEX;
	node->left = BVH_NODE_NULL_INDEX;
	node->right = BVH_NODE_NULL_INDEX;
	node->data_index = 0;
	node->height = BVH_NODE_NULL_INDEX;

	Object_Pool_Free(_tree->nodes, _index, false);
}

static void BVH_FattenBox(BVH_Tree* const _tree, float box_src[2][2], float box_dest[2][2])
{
	box_dest[0][0] = box_src[0][0] - _tree->thickness;
	box_dest[0][1] = box_src[0][1] - _tree->thickness;

	box_dest[1][0] = box_src[1][0] + _tree->thickness;
	box_dest[1][1] = box_src[1][1] + _tree->thickness;
}

static int BVH_Balance(BVH_Tree* const _tree, int _index)
{
	int A = _index;
	BVH_Node* node_a = dA_at(_tree->nodes->pool, A);

	if (node_a->left == BVH_NODE_NULL_INDEX || node_a->height == 1)
	{
		return _index;
	}

	/*      A
		  /   \
		 B     C
		/ \   / \
	   D   E F   G
	*/

	int B = node_a->left;
	int C = node_a->right;

	BVH_Node* node_b = dA_at(_tree->nodes->pool, B);
	BVH_Node* node_c = dA_at(_tree->nodes->pool, C);

	int balance = node_c->height - node_b->height;

	if (balance > 1)
	{
		int F = node_c->left;
		int G = node_c->right;

		BVH_Node* node_f = dA_at(_tree->nodes->pool, F);
		BVH_Node* node_g = dA_at(_tree->nodes->pool, G);

		if (node_a->parent != BVH_NODE_NULL_INDEX)
		{
			BVH_Node* parent = dA_at(_tree->nodes->pool, node_a->parent);

			if (parent->left == A)
			{
				parent->left = C;
			}
			else
			{
				parent->right = C;
			}
		}
		else
		{
			_tree->root = C;
		}

		node_c->left = A;
		node_c->parent = node_a->parent;
		node_a->parent = C;

		if (node_f->height > node_g->height)
		{
			node_c->right = F;
			node_a->right = G;
			node_g->parent = A;

			Math_BoxMerge(node_b->bbox, node_g->bbox, node_a->bbox);
			Math_BoxMerge(node_a->bbox, node_f->bbox, node_c->bbox);

			node_a->height = 1 + max(node_b->height, node_g->height);
			node_c->height = 1 + max(node_a->height, node_f->height);
		}
		else
		{
			node_c->right = G;
			node_a->right = F;
			node_f->parent = A;

			Math_BoxMerge(node_b->bbox, node_f->bbox, node_a->bbox);
			Math_BoxMerge(node_a->bbox, node_g->bbox, node_c->bbox);

			node_a->height = 1 + max(node_b->height, node_f->height);
			node_c->height = 1 + max(node_a->height, node_g->height);
		}

		return C;
	}
	else if (balance < -1)
	{
		int D = node_b->left;
		int E = node_b->right;

		BVH_Node* node_d = dA_at(_tree->nodes->pool, D);
		BVH_Node* node_e = dA_at(_tree->nodes->pool, E);

		if (node_a->parent != BVH_NODE_NULL_INDEX)
		{
			BVH_Node* parent = dA_at(_tree->nodes->pool, node_a->parent);

			if (parent->left == A)
			{
				parent->left = B;
			}
			else
			{
				parent->right = B;
			}
		}
		else
		{
			_tree->root = B;
		}

		node_b->right = A;
		node_b->parent = node_a->parent;
		node_a->parent = B;

		if (node_d->height > node_e->height)
		{
			node_b->left = D;
			node_a->left = E;
			node_e->parent = A;

			Math_BoxMerge(node_c->bbox, node_e->bbox, node_a->bbox);
			Math_BoxMerge(node_a->bbox, node_d->bbox, node_b->bbox);

			node_a->height = 1 + max(node_c->height, node_e->height);
			node_b->height = 1 + max(node_a->height, node_d->height);
		}
		else
		{
			node_b->left = E;
			node_a->left = D;
			node_d->parent = A;

			Math_BoxMerge(node_c->bbox, node_d->bbox, node_a->bbox);
			Math_BoxMerge(node_a->bbox, node_e->bbox, node_b->bbox);

			node_a->height = 1 + max(node_c->height, node_d->height);
			node_b->height = 1 + max(node_a->height, node_e->height);
		}

		return B;
	}

	return A;
}

static void BVH_FixUpwards(BVH_Tree* const _tree, int _index)
{
	int index = _index;

	while (index != BVH_NODE_NULL_INDEX)
	{
		index = BVH_Balance(_tree, index);

		BVH_Node* node = dA_at(_tree->nodes->pool, index);

		assert(node->left != BVH_NODE_NULL_INDEX && node->right != BVH_NODE_NULL_INDEX);

		BVH_Node* left_node = dA_at(_tree->nodes->pool, node->left);
		BVH_Node* right_node = dA_at(_tree->nodes->pool, node->right);

		node->height = 1 + max(left_node->height, right_node->height);
		Math_BoxMerge(left_node->bbox, right_node->bbox, node->bbox);

		index = node->parent;
	}
}

static void BVH_InsertLeaf(BVH_Tree* const _tree, int _index)
{
	//is our root index empty?
	if (_tree->root == BVH_NODE_NULL_INDEX)
	{
		//insert the element into root
		_tree->root = _index;
		return;
	}

	BVH_Node* node_array = dA_getFront(_tree->nodes->pool);
	BVH_Node* leaf_node = dA_at(_tree->nodes->pool, _index);

	assert(leaf_node->left == BVH_NODE_NULL_INDEX);
	assert(leaf_node->right == BVH_NODE_NULL_INDEX);
	assert(leaf_node->parent == BVH_NODE_NULL_INDEX);

	int itr = _tree->root;
	//iterate from root
	while (node_array[itr].left != BVH_NODE_NULL_INDEX)
	{
		BVH_Node* itr_node = dA_at(_tree->nodes->pool, itr);
		BVH_Node* left_node = dA_at(_tree->nodes->pool, itr_node->left);
		BVH_Node* right_node = dA_at(_tree->nodes->pool, itr_node->right);

		float box_merged[2][2];
		Math_BoxMerge(itr_node->bbox, leaf_node->bbox, box_merged);

		const float merged_box_area = BVH_calcBoxArea(box_merged);

		float new_parent_cost = 2.0f * merged_box_area;
		float min_push_down_cost = 2.0f * merged_box_area - BVH_calcBoxArea(itr_node->bbox);

		float cost_left = 0;
		float cost_right = 0;

		float box_left_merged[2][2];
		Math_BoxMerge(leaf_node->bbox, left_node->bbox, box_left_merged);
		const float box_left_area = BVH_calcBoxArea(box_left_merged);

		float box_right_merged[2][2];
		Math_BoxMerge(leaf_node->bbox, right_node->bbox, box_right_merged);
		const float box_right_area = BVH_calcBoxArea(box_right_merged);

		//Evalue the costs
		if (left_node->left == BVH_NODE_NULL_INDEX)
		{
			cost_left = box_left_area + min_push_down_cost;
		}
		else
		{
			cost_left = (box_left_area - BVH_calcBoxArea(left_node->bbox)) + min_push_down_cost;
		}
		if (right_node->left == BVH_NODE_NULL_INDEX)
		{
			cost_right = box_right_area + min_push_down_cost;
		}
		else
		{
			cost_left = (box_right_area - BVH_calcBoxArea(right_node->bbox)) + min_push_down_cost;
		}

		if (new_parent_cost < cost_left && new_parent_cost < cost_right)
		{
			break;
		}

		if (cost_left < cost_right)
		{
			itr = itr_node->left;
		}
		else
		{
			itr = itr_node->right;
		}
	}

	int sibling_index = itr;
	BVH_Node* sibling_node = dA_at(_tree->nodes->pool, sibling_index);

	int old_parent_index = sibling_node->parent;
	int new_parent_index = Object_Pool_Request(_tree->nodes);

	//Update the pointers after a possible realloc
	sibling_node = dA_at(_tree->nodes->pool, sibling_index);
	leaf_node = dA_at(_tree->nodes->pool, _index);

	BVH_Node* new_parent_node = dA_at(_tree->nodes->pool, new_parent_index);
	Math_BoxMerge(leaf_node->bbox, sibling_node->bbox, new_parent_node->bbox);

	new_parent_node->parent = old_parent_index;
	new_parent_node->left = sibling_index;
	new_parent_node->right = _index;
	new_parent_node->height = sibling_node->height + 1;

	leaf_node->parent = new_parent_index;
	sibling_node->parent = new_parent_index;

	if (old_parent_index == BVH_NODE_NULL_INDEX)
	{
		_tree->root = new_parent_index;
	}
	else
	{
		BVH_Node* old_parent = dA_at(_tree->nodes->pool, old_parent_index);
		if (old_parent->left == sibling_index)
		{
			old_parent->left = new_parent_index;
		}
		else
		{
			old_parent->right = new_parent_index;
		}
	}

	BVH_FixUpwards(_tree, leaf_node->parent);
}

static void BVH_RemoveLeaf(BVH_Tree* const _tree, int _index)
{
	if (_tree->root == _index)
	{
		_tree->root = BVH_NODE_NULL_INDEX;
		return;
	}

	BVH_Node* node = dA_at(_tree->nodes->pool, _index);

	int parent_index = node->parent;

	BVH_Node* parent_node = dA_at(_tree->nodes->pool, parent_index);

	int grandparent_index = parent_node->parent;
	int sibling_node_index = parent_node->left == _index ? parent_node->right : parent_node->left;

	assert(sibling_node_index != BVH_NODE_NULL_INDEX);

	BVH_Node* sibling_node = dA_at(_tree->nodes->pool, sibling_node_index);

	if (grandparent_index != BVH_NODE_NULL_INDEX) 
	{
		BVH_Node* grandparent_node = dA_at(_tree->nodes->pool, grandparent_index);

		if (grandparent_node->left == parent_index)
		{
			grandparent_node->left = sibling_node_index;
		}
		else
		{
			grandparent_node->right = sibling_node_index;
		}

		sibling_node->parent = grandparent_index;
	}
	else
	{
		_tree->root = sibling_node_index;
		sibling_node->parent = BVH_NODE_NULL_INDEX;
	}

	BVH_ReleaseNode(_tree, parent_index);
	BVH_FixUpwards(_tree, grandparent_index);
}

BVH_Tree BVH_Tree_Create(float p_thickness)
{
	BVH_Tree tree;
	memset(&tree, 0, sizeof(tree));

	tree.nodes = Object_Pool_INIT(BVH_Node, 0);
	tree.root = BVH_NODE_NULL_INDEX;
	tree.thickness = fabsf(p_thickness);

	return tree;
}

void BVH_Tree_Destruct(BVH_Tree* const p_tree)
{
	if (p_tree->nodes)
	{
		Object_Pool_Destruct(p_tree->nodes);
	}
}

BVH_ID BVH_Tree_Insert(BVH_Tree* const p_tree, float p_bbox[2][2], int p_dataIndex)
{
	int request_index = Object_Pool_Request(p_tree->nodes);
	BVH_Node* node_array = p_tree->nodes->pool->data;

	BVH_FattenBox(p_tree, p_bbox, node_array[request_index].bbox);

	node_array[request_index].left = BVH_NODE_NULL_INDEX;
	node_array[request_index].right = BVH_NODE_NULL_INDEX;
	node_array[request_index].parent = BVH_NODE_NULL_INDEX;
	node_array[request_index].data_index = p_dataIndex;
	node_array[request_index].height = 0;

	BVH_InsertLeaf(p_tree, request_index);

	return request_index;
}

void* BVH_Tree_Remove(BVH_Tree* const p_tree, BVH_ID p_bvhID)
{
	BVH_Node* node = dA_at(p_tree->nodes->pool, p_bvhID);

	void* data = node->data_index;

	BVH_RemoveLeaf(p_tree, p_bvhID);

	return data;
}

void BVH_Tree_ClearAll(BVH_Tree* const p_tree)
{
	p_tree->root = BVH_NODE_NULL_INDEX;
	
	Object_Pool_ClearAll(p_tree->nodes);
}

bool BVH_Tree_UpdateBounds(BVH_Tree* const p_tree, BVH_ID p_bvhID, float p_bbox[2][2])
{
	BVH_Node* node = dA_at(p_tree->nodes->pool, p_bvhID);

	if(Math_BoxContainsBox(node->bbox, p_bbox))
	{
		return true;
	}
	
	BVH_RemoveLeaf(p_tree, p_bvhID);

	node = dA_at(p_tree->nodes->pool, p_bvhID);
	node->parent = BVH_NODE_NULL_INDEX;
	node->height = BVH_NODE_NULL_INDEX;

	BVH_FattenBox(p_tree, p_bbox, node->bbox);

	BVH_InsertLeaf(p_tree, p_bvhID);

	return true;
}

int BVH_Tree_GetData(BVH_Tree* const p_tree, BVH_ID p_bvhID)
{
	BVH_Node* node = dA_at(p_tree->nodes->pool, p_bvhID);

	return node->data_index;
}

int BVH_Tree_Cull_Box(BVH_Tree* const p_tree, float bbox[2][2], int p_maxHitCount, int* p_hits)
{
	if (p_tree->root == BVH_NODE_NULL_INDEX)
	{
		return 0;
	}

	BVH_StackHelper stack = BVH_StackHelper_Create();

	BVH_StackHelper_Push(&stack, p_tree->root, false);

	int hit_count = 0;

	while (stack.index > 0)
	{
		if (hit_count >= p_maxHitCount)
		{
			BVH_StackHelper_Exit(&stack);
			return hit_count;
		}

		BVH_StackItem stack_item = BVH_StackHelper_GetTop(&stack);
		BVH_StackHelper_Pop(&stack);

		int node_index = stack_item.index;

		if (node_index == BVH_NODE_NULL_INDEX)
			continue;

		BVH_Node* node = dA_at(p_tree->nodes->pool, node_index);

		//Perform Test if not fully within
		if (stack_item.fully_within == false)
		{
			//failed the test, continue to the next item in the stack
			if (!Math_BoxIntersectsBox(node->bbox, bbox))
			{
				continue;
			}
			//stack_item.fully_within = Math_BoxContainsBox(node->bbox, bbox);
		}

		//Is the node leaf?
		if (node->left == BVH_NODE_NULL_INDEX)
		{
			p_hits[hit_count] = node->data_index;

			hit_count++;
		}
		else
		{
			//is parent so insert children to the stack
			BVH_StackHelper_Push(&stack, node->left, stack_item.fully_within);

			if (node->right != BVH_NODE_NULL_INDEX)
			{
				BVH_StackHelper_Push(&stack, node->right, stack_item.fully_within);
			}
		}

	}
	BVH_StackHelper_Exit(&stack);

	return hit_count;
}

int BVH_Tree_Cull_Trace(BVH_Tree* const p_tree, float p_startX, float p_startY, float p_endX, float p_endY, int p_maxHitCount, int* p_hits)
{
	if (p_tree->root == BVH_NODE_NULL_INDEX)
	{
		return 0;
	}

	BVH_StackHelper stack = BVH_StackHelper_Create();

	BVH_StackHelper_Push(&stack, p_tree->root, false);

	int hit_count = 0;

	while (stack.index > 0)
	{
		if (hit_count >= p_maxHitCount)
		{
			BVH_StackHelper_Exit(&stack);
			return hit_count;
		}

		BVH_StackItem stack_item = BVH_StackHelper_GetTop(&stack);
		BVH_StackHelper_Pop(&stack);

		int node_index = stack_item.index;

		if (node_index == BVH_NODE_NULL_INDEX)
			continue;

		BVH_Node* node = dA_at(p_tree->nodes->pool, node_index);

		//failed the test, continue to the next item in the stack
		if (!Math_TraceLineVsBox2(p_startX, p_startY, p_endX, p_endY, node->bbox, NULL, NULL, NULL))
		{
			continue;
		}

		//Is the node leaf?
		if (node->left == BVH_NODE_NULL_INDEX)
		{
			p_hits[hit_count] = node->data_index;

			hit_count++;
		}
		else
		{
			//is parent so insert children to the stack
			BVH_StackHelper_Push(&stack, node->left, stack_item.fully_within);

			if (node->right != BVH_NODE_NULL_INDEX)
			{
				BVH_StackHelper_Push(&stack, node->right, stack_item.fully_within);
			}
		}

	}
	BVH_StackHelper_Exit(&stack);

	return hit_count;
}

int BVH_Tree_GetAllNodes(BVH_Tree* const p_tree, int p_maxNodes, BVH_Node** r_nodes)
{
	if (p_tree->root == BVH_NODE_NULL_INDEX)
	{
		return 0;
	}

	BVH_StackHelper stack = BVH_StackHelper_Create();

	BVH_StackHelper_Push(&stack, p_tree->root, false);

	int hit_count = 0;

	while (stack.index > 0)
	{
		if (hit_count >= p_maxNodes)
		{
			BVH_StackHelper_Exit(&stack);
			return hit_count;
		}

		BVH_StackItem stack_item = BVH_StackHelper_GetTop(&stack);
		BVH_StackHelper_Pop(&stack);

		int node_index = stack_item.index;

		if (node_index == BVH_NODE_NULL_INDEX)
			continue;

		BVH_Node* node = dA_at(p_tree->nodes->pool, node_index);

		r_nodes[hit_count] = node;
		hit_count++;

		//Is the node leaf?
		if (node->left == BVH_NODE_NULL_INDEX)
		{
			
		}
		else
		{
			//is parent so insert children to the stack
			BVH_StackHelper_Push(&stack, node->left, false);

			if (node->right != BVH_NODE_NULL_INDEX)
			{
				BVH_StackHelper_Push(&stack, node->right, false);
			}
		}

	}

	BVH_StackHelper_Exit(&stack);

	return hit_count;
}

