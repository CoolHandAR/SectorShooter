#include "u_object_pool.h"

#include <assert.h>

Object_Pool* _objPoolInit(size_t alloc_size, unsigned init_size)
{
	assert(alloc_size > 0 && "Invalid alloc size \n");

	Object_Pool* pool = malloc(sizeof(Object_Pool));

	if (!pool)
	{
		assert(false);
	}

	pool->pool = dA_INIT2(alloc_size, init_size);
	pool->free_list = dA_INIT(unsigned, 0);
	pool->used_pool_size = init_size;


	return pool;
}

unsigned Object_Pool_Request(Object_Pool* const p_pool)
{
	unsigned r_index = 0;

	p_pool->used_pool_size++;

	//check if we have anything in the free list
	if (p_pool->free_list->elements_size > 0)
	{
		unsigned new_free_list_size = p_pool->free_list->elements_size - 1;
		//give the index to the last free index
		unsigned* free_list_array = p_pool->free_list->data;
		r_index = free_list_array[new_free_list_size];
		
		dA_resize(p_pool->free_list, new_free_list_size);

		return r_index;
	}

	unsigned given_index = p_pool->pool->elements_size;
	r_index = given_index;

	//we resize, so we can give the promised space in the array
	dA_resize(p_pool->pool, given_index + 1);

	return r_index;
}

size_t Object_Pool_Size(Object_Pool* const p_pool)
{
	return dA_size(p_pool->pool);
}

unsigned Object_Pool_UsedSize(Object_Pool* const p_pool)
{
	return p_pool->used_pool_size;
}

void* Object_Pool_At(Object_Pool* const p_pool, unsigned p_index)
{
	return dA_at(p_pool->pool, p_index);
}

void Object_Pool_Free(Object_Pool* const p_pool, unsigned p_index, bool p_zeroMem)
{
	//bounds check
	assert((p_index >= 0 && p_index < p_pool->pool->elements_size) && "Can't free object with an invalid index\n");

	//memset zero the value?
	if (p_zeroMem)
	{
		void* value = dA_at(p_pool->pool, p_index);
		memset(value, 0, p_pool->pool->alloc_size);
	}
	//add the index id to the free list
	unsigned* emplaced = dA_emplaceBack(p_pool->free_list);
	*emplaced = p_index;

	p_pool->used_pool_size--;
}

void Object_Pool_ClearAll(Object_Pool* const p_pool)
{
	dA_clear(p_pool->pool);
	dA_clear(p_pool->free_list);
	p_pool->used_pool_size = 0;
}

void Object_Pool_Destruct(Object_Pool* p_pool)
{
	dA_Destruct(p_pool->pool);
	dA_Destruct(p_pool->free_list);
	free(p_pool);
}
