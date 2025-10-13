#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H
#pragma once

/*
   Usage define on atleast one .c file
   #define DYNAMIC_ARRAY_IMPLEMENTATION
   #include "dynamic_array.h"
*/

#include <stdlib.h>
#include <stdbool.h>

typedef struct dynamic_array
{
    size_t capacity; // The number of items that the array can hold
    size_t elements_size; // The amount of elements currently held
    size_t alloc_size; // The allocation byte size
    void* data; //The raw array data
} dynamic_array;

#ifdef __cplusplus
extern "C" {
#endif
/**
* Init the dynamic array
\param T The type of item that will be stores
\param RESERVE_SIZE The initial amount of items that can be stored.

\return Pointer to the dynamic array struct
*/
#define dA_INIT(T, RESERVE_SIZE) _dA_Init(sizeof(T), RESERVE_SIZE)

/**
* Init the dynamic array
\param Byte size of the item that will be stored as the value. sizeof(...)
\param RESERVE_SIZE The initial amount of items that can be stored.

\return Pointer to the dynamic array struct
*/
#define dA_INIT2(ALLOC_SIZE, RESERVE_SIZE) _dA_Init(ALLOC_SIZE, RESERVE_SIZE);
    extern void* dA_getLast(dynamic_array* const p_dA);
    extern void* dA_getFront(dynamic_array* const p_dA);
    extern void* dA_at(dynamic_array* const p_dA, size_t p_index);
    extern bool dA_resize(dynamic_array* p_dA, size_t p_newSize);
    extern bool dA_reserve(dynamic_array* p_dA, size_t p_toReserve);
    extern bool dA_shrinkToFit(dynamic_array* p_dA);
    extern void* dA_emplaceBack(dynamic_array* const p_dA);
    extern void* dA_emplaceBackMultiple(dynamic_array* p_dA, size_t p_size);
    extern bool dA_emplaceBackMultipleData(dynamic_array* p_dA, size_t p_size, const void* p_data);
    extern bool dA_popBack(dynamic_array* p_dA);
    extern bool dA_popBackMultiple(dynamic_array* p_dA, size_t p_size);
    extern void dA_memcpy(const dynamic_array* p_dA, size_t p_destIndex, const void* p_data);
    extern void* dA_insert(dynamic_array* p_dA, size_t p_pos, size_t p_amount);
    extern bool dA_erase(dynamic_array* p_dA, size_t p_pos, size_t p_amount);
    extern void dA_clear(dynamic_array* p_dA);
    extern size_t dA_getItemsByteSize(const dynamic_array* p_dA);
    extern size_t dA_getTotalByteCapacitySize(const dynamic_array* p_dA);
    extern size_t dA_size(const dynamic_array* p_dA);
    extern size_t dA_capacity(const dynamic_array* p_dA);
    extern bool dA_isEmpty(const dynamic_array* p_dA);
    extern void dA_Destruct(dynamic_array* p_dA);
#ifdef __cplusplus
}
#endif
#include <assert.h>
/**
_Internal: DO NOT USE!
*/
static dynamic_array* _dA_Init(size_t p_allocSize, size_t p_initReserveSize)
{
    assert(p_allocSize > 0 && "Alloc size must be higher than 0");

    dynamic_array* dA_ptr = malloc(sizeof(dynamic_array));

    //we failed to alloc the dynamic array
    if (dA_ptr == NULL)
        return NULL;

    dA_ptr->data = NULL;

    //alloc the raw data
    if (p_initReserveSize > 0)
    {
        dA_ptr->data = calloc(p_initReserveSize, p_allocSize);

        //we failed to alloc the raw data
        if (dA_ptr->data == NULL)
        {
            //clean up
            free(dA_ptr);
            return NULL;
        }
    }

    //SUCCESS
    dA_ptr->alloc_size = p_allocSize;
    dA_ptr->capacity = p_initReserveSize;
    dA_ptr->elements_size = 0;

    return dA_ptr;
}

/*
This is for preventing greying out of the implementation section.
*/
#if defined(Q_CREATOR_RUN) || defined(__INTELLISENSE__) || defined(__CDT_PARSER__)
#define DYNAMIC_ARRAY_IMPLEMENTATION
#endif

#if defined(DYNAMIC_ARRAY_IMPLEMENTATION)
#ifndef DYNAMIC_ARRAY_C
#define DYNAMIC_ARRAY_C

#include <string.h>

#define __dA_ZERO_MEMORY(DEST, SIZEOFDATA) memset(DEST, 0, SIZEOFDATA)


/**
_Internal: DO NOT USE!
*/
void _dA_assertSetData(const dynamic_array* p_dA)
{
    assert(p_dA != NULL && "The dynamic array data is NULL");
    //assert(p_dA->data != NULL && "Data is not initizialized with dA_init()");
    assert(p_dA->alloc_size > 0 && "Alloc size can't be equal or lower than 0");
}

/**
_Internal: DO NOT USE!
*/
bool _dA_safeRealloc(dynamic_array* p_dA, size_t p_size)
{
    void* prev = p_dA->data;

    if (prev == NULL)
    {
        void* data = malloc(p_size);

        if (data)
        {
            p_dA->data = data;
        }
        return true;
    }

    p_dA->data = realloc(prev, p_size);

    //successfull realloc
    if (p_dA->data)
    {
        return true;
    }

    //Free the previous data if we failed to realloc
    free(prev);

    return false;
}

/**
_Internal: DO NOT USE!
*/
bool _dA_handleAlloc(dynamic_array* p_dA, size_t p_size)
{
    const size_t next_element_alloc_size = p_dA->alloc_size * (p_dA->elements_size + p_size);
    const size_t capacity_byte_size = p_dA->capacity * p_dA->alloc_size;

    //realloc if our capacity is lower than the new element size
    if (next_element_alloc_size > capacity_byte_size)
    {
        if (_dA_safeRealloc(p_dA, next_element_alloc_size))
        {
            //get the pointer to the last element after address
            void* last_element_after_address = (char*)p_dA->data + (p_dA->elements_size * p_dA->alloc_size);
            //init the items
            __dA_ZERO_MEMORY(last_element_after_address, p_dA->alloc_size * p_size);

            p_dA->elements_size += p_size;
            p_dA->capacity = p_dA->elements_size;
            return true;
        }
    }
    //there is enough capacity for the new elements
    else
    {
        //get the pointer to the last element after address
        void* last_element_after_address = (char*)p_dA->data + (p_dA->elements_size * p_dA->alloc_size);
        //init the items
        __dA_ZERO_MEMORY(last_element_after_address, p_dA->alloc_size * p_size);
        p_dA->elements_size += p_size;
        return true;
    }

    return false;
}

/**
Returns the pointer to last element of the array

\param p_dA Pointer to the dynamic array
*/
void* dA_getLast(dynamic_array* const p_dA)
{
    _dA_assertSetData(p_dA);

    void* ptr = (char*)p_dA->data + ((p_dA->elements_size - 1) * p_dA->alloc_size);

    return ptr;
}
/**
Returns the pointer to the first element of the array

\param p_dA Pointer to the dynamic array
*/
void* dA_getFront(dynamic_array* const p_dA)
{
    _dA_assertSetData(p_dA);

    return p_dA->data;
}
/**
Returns the pointer to the item specified by the index

\param p_dA Pointer to the dynamic array
\param p_index Index of the item
*/
void* dA_at(dynamic_array* const p_dA, size_t p_index)
{
    _dA_assertSetData(p_dA);

    assert(p_index < p_dA->elements_size && "Index out of bounds");

    void* ptr = (char*)p_dA->data + (p_index * p_dA->alloc_size);

    return ptr;
}

/**
Resize the array.

\param p_dA Pointer to the dynamic array
\param p_newSize The new size of the array. NOTE: Nothing happens if size is same as the new one

\return True if success
*/
bool dA_resize(dynamic_array* p_dA, size_t p_newSize)
{
    _dA_assertSetData(p_dA);

    //do nothing if the sizes are the same
    if (p_dA->elements_size == p_newSize)
        return true;

    //are we increasing the size or decreasing it?
    if (p_newSize > p_dA->elements_size)
    {
        size_t diff = p_newSize - p_dA->elements_size;

        if (_dA_handleAlloc(p_dA, diff))
        {
            return true;
        }
    }
    else
    {
        //we just decrease the element size
        p_dA->elements_size = p_newSize;
        return true;
    }

    return false;
}

/**
Reallocates more capacity.

\param p_dA Pointer to the dynamic array
\param p_toReserve How much to increase the capacity. NOTE: Does nothing if 0

\return True on success
*/
bool dA_reserve(dynamic_array* p_dA, size_t p_toReserve)
{
    _dA_assertSetData(p_dA);

    if (p_toReserve == 0)
        return false;

    const size_t new_allocation_size = (p_dA->capacity + p_toReserve) * p_dA->alloc_size;

    if (_dA_safeRealloc(p_dA, new_allocation_size))
    {
        p_dA->capacity += p_toReserve;
        return true;
    }

    return false;
}

/**
Shrinks the capacity to the size of the elements

\param p_dA Pointer to the dynamic array

\return True on success
*/
bool dA_shrinkToFit(dynamic_array* p_dA)
{
    _dA_assertSetData(p_dA);

    if (p_dA->capacity == p_dA->elements_size)
    {
        return true;
    }

    const size_t total_byte_size = p_dA->elements_size * p_dA->alloc_size;

    if (_dA_safeRealloc(p_dA, total_byte_size))
    {
        p_dA->capacity = p_dA->elements_size;
        return true;
    }

    return false;
}

/**
Emplaces an item at the back of the array

\param p_dA Pointer to the dynamic array

\return Pointer to the last element
*/
void* dA_emplaceBack(dynamic_array* const p_dA)
{
    _dA_assertSetData(p_dA);

    if (!_dA_handleAlloc(p_dA, 1))
        return NULL;

    return dA_getLast(p_dA);
}

/**
Emplaces an item at the back of the array

\param p_dA Pointer to the dynamic array

\return Pointer to the last element
*/
void* dA_emplaceBackData(dynamic_array* const p_dA, const void* p_data)
{
    _dA_assertSetData(p_dA);

    if (!_dA_handleAlloc(p_dA, 1))
        return NULL;

    void* last = dA_getLast(p_dA);

    if (p_data)
    {
        memcpy(last, p_data, p_dA->alloc_size);
    }

    return last;
}

void* dA_emplaceBackMultiple(dynamic_array* p_dA, size_t p_size)
{
    _dA_assertSetData(p_dA);

    if (p_size == 0)
        return NULL;

    size_t prev_size = p_dA->elements_size;

    if (!_dA_handleAlloc(p_dA, p_size))
    {
        return false;
    }

    void* at = dA_at(p_dA, prev_size);

    return at;
}

/**
Emplaces multiple items at the back of the array

\param p_dA Pointer to the dynamic array
\param p_size How many items to emplace. NOTE: Does nothing if 0.

\return True on success
*/
bool dA_emplaceBackMultipleData(dynamic_array* p_dA, size_t p_size, const void* p_data)
{
    _dA_assertSetData(p_dA);

    if (p_size == 0)
        return false;


    size_t prev_size = p_dA->elements_size;

    if (!_dA_handleAlloc(p_dA, p_size))
    {
        return false;
    }

    void* at = dA_at(p_dA, prev_size);

    if (p_data)
    {
        memcpy(at, p_data, p_dA->alloc_size * p_size);
    }

    return true;
}

/**
Removes an item from the back of the array

\param p_dA Pointer to the dynamic array

\return True on success
*/
bool dA_popBack(dynamic_array* p_dA)
{
    _dA_assertSetData(p_dA);

    if (p_dA->elements_size <= 0)
        return false;

    p_dA->elements_size--;

    return true;
}

/**
Removes multiple items from the back of the array

\param p_dA Pointer to the dynamic array
\param p_size Number of items to remove. NOTE: Does nothing if size is 0

\return True on success
*/
bool dA_popBackMultiple(dynamic_array* p_dA, size_t p_size)
{
    _dA_assertSetData(p_dA);

    if (p_size == 0)
        return false;

    const int next_elements_size = p_dA->elements_size - p_size;
    assert(next_elements_size >= 0 && "Tried to remove more elements than possible");

    p_dA->elements_size -= p_size;

    return true;
}
/**
Copies data to the specified index

\param p_dA Pointer to the dynamic array
\param p_destIndex Index of the element to copy to
\param p_data The data to copy
*/
void dA_memcpy(const dynamic_array* p_dA, size_t p_destIndex, const void* p_data)
{
    _dA_assertSetData(p_dA);
    
    void* ptr = dA_at(p_dA, p_destIndex);

    memcpy(ptr, p_data, p_dA->alloc_size);
}

/**
Inserts items at the specified position in the array.

\param p_dA Pointer to the dynamic array
\param p_pos Position to insert the items NOTE: Position can't be more than the amount of elements.
\param p_amount Amount of items to insert NOTE: Does nothing if amount is 0

\return Pointer to the start of the inserted element
*/
void* dA_insert(dynamic_array* p_dA, size_t p_pos, size_t p_amount)
{
    _dA_assertSetData(p_dA);
    assert(p_pos < p_dA->elements_size && "dA out of bounds");

    if (p_amount == 0)
        return false;

    const size_t before_element_size = p_dA->elements_size;

    //failed to alloc for some reason
    if (!_dA_handleAlloc(p_dA, p_amount))
    {
        return NULL;
    }

    //address where we want to insert the data
    void* pos_address = (char*)p_dA->data + (p_pos * p_dA->alloc_size);

    //address after the placed items
    void* next_address = (char*)pos_address + (p_amount * p_dA->alloc_size);

    //byte size of the items we are moving forward
    //Note: the elements size is not yet increased by p_amount
    const size_t byte_size = ((before_element_size)-(p_pos)) * p_dA->alloc_size;

    //copy memory from old address to new one;
    memcpy(next_address, pos_address, byte_size);

    //reset the data on the newly added blocks
    __dA_ZERO_MEMORY(pos_address, p_amount * p_dA->alloc_size);

    return pos_address;
}

/**
Erases items at the specified position in the array.

\param p_dA Pointer to the dynamic array
\param p_pos Position from where to erase the items NOTE: Position can't be more than the amount of elements.
\param p_amount Amount of items to delete NOTE: Does nothing if amount is 0

\return True on success
*/
bool dA_erase(dynamic_array* p_dA, size_t p_pos, size_t p_amount)
{
    _dA_assertSetData(p_dA);
    assert(p_pos < p_dA->elements_size && "dA out of bounds");
    assert(p_amount <= (p_dA->elements_size - p_pos) && "Deleting more items possible");

    if (p_amount == 0)
        return false;

    //const int next_elements_size = p_dA->elements_size - p_amount;

   // assert(next_elements_size >= 0 && "Deleting more items than the array holds");

    //address where we want to remove the data from
    void* read_address = (char*)p_dA->data + ((p_pos + p_amount) * p_dA->alloc_size);

    //address after the deleted items
    void* write_address = (char*)p_dA->data + (p_pos * p_dA->alloc_size);

    //byte size of the items we are moving back
    const size_t move_elements = p_dA->elements_size - (p_pos + p_amount);

    const size_t byte_size = move_elements * p_dA->alloc_size;
    p_dA->elements_size -= p_amount;

    //memcpy(pos_address, next_address, byte_size);

    memmove(write_address, read_address, byte_size);

 
    return true;
}

/**
Clears all items of the array to 0

\param p_dA Pointer to the dynamic array
*/
void dA_clear(dynamic_array* p_dA)
{
    _dA_assertSetData(p_dA);

    p_dA->elements_size = 0;
}
/**
Returns the total byte size of all elements

\param p_dA Pointer to the dynamic array
*/
size_t dA_getItemsByteSize(const dynamic_array* p_dA)
{
    _dA_assertSetData(p_dA);

    return p_dA->elements_size * p_dA->alloc_size;
}
/**
Returns the total byte occupied byte size of the array

\param p_dA Pointer to the dynamic array
*/
size_t dA_getTotalByteCapacitySize(const dynamic_array* p_dA)
{
    _dA_assertSetData(p_dA);

    return p_dA->capacity * p_dA->alloc_size;
}
/**
Returns the element size of the array

\param p_dA Pointer to the dynamic array
*/
size_t dA_size(const dynamic_array* p_dA)
{   
    _dA_assertSetData(p_dA);

    return p_dA->elements_size;
}
size_t dA_capacity(const dynamic_array* p_dA)
{
    _dA_assertSetData(p_dA);

    return p_dA->capacity;
}
/**
Returns true if elements size is 0

\param p_dA Pointer to the dynamic array
*/
bool dA_isEmpty(const dynamic_array* p_dA)
{
    _dA_assertSetData(p_dA);

    return p_dA->elements_size == 0;
}
/**
Destroys the dynamic array and all its data

\param p_dA Pointer to the dynamic array
*/
void dA_Destruct(dynamic_array* p_dA)
{
    assert(p_dA != NULL && "The dynamic array is NULL");

    if (p_dA->data)
    {
        free(p_dA->data);
    }
    free(p_dA);

    p_dA = NULL;
}


#endif
#endif
#endif //DYNAMIC_ARRAY_H