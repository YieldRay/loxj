#include <stdlib.h>

#include "chunk.h"
#include "vm.h"
#include "memory.h"

#ifdef DEBUG_TRACE_EXECUTION
#include <stdio.h>
#endif

/**
 * 必须使用此函数指令
 * 此函数亦可用作清理，因为不分配内存
 */
void initChunk(Chunk *chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

/**
 * @param byte Byte Code
 * @param line Line Number
 */
void writeChunk(Chunk *chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1)
    {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }
    // count = last_index + 1 = current_index
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;

    chunk->count++;
}

/**
 * add contant value to chunk
 * @return index of `value` within all constants in chunk
 */
int addConstant(Chunk *chunk, Value value)
{
    int index = chunk->constants.count;

    push(value);
    writeValueArray(&chunk->constants, value);
    pop();

#ifdef DEBUG_TRACE_EXECUTION
    printf("[[DEBUG_TRACE_EXECUTION]]  addConstant:  constantIndex=%d  ", index);
    printf("constantValue=");
    printValue(value);
    putchar('\n');
#endif

    return index; // last count is the same as current index
}