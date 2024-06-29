#ifndef loxj_memory_h
#define loxj_memory_h

#include "common.h"
#include "object.h"
#include "compiler.h"

#define CAPACITY_MIN 8       // 最小负载
#define CAPACITY_GROW_RATE 2 // 增长系数

#define GROW_CAPACITY(capacity) \
    ((capacity) < CAPACITY_MIN ? CAPACITY_MIN : (capacity) * CAPACITY_GROW_RATE)

#define GROW_ARRAY(TYPE, pointer, oldCount, newCount) \
    (TYPE *)reallocate(pointer, sizeof(TYPE) * (oldCount), sizeof(TYPE) * (newCount))

#define FREE_ARRAY(TYPE, pointer, oldCount) reallocate(pointer, sizeof(TYPE) * (oldCount), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#define ALLOCATE(TYPE, count) (TYPE *)reallocate(NULL, 0, sizeof(TYPE) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

void freeObjects();

void markValue(Value value);
void markObject(Obj *object);
void collectGarbage();

#endif