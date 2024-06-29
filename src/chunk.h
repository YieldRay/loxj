#ifndef loxj_chunk_h
#define loxj_chunk_h

#include "common.h"
#include "value.h"

/**
 * 字节码（大端）
 * 低地址存放高字节，高地址（堆顶）存放低字节
 */
typedef enum
{
    // value
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    // stack
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    // logic
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    // jump
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    // math
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    // func
    OP_PRINT,
    OP_CALL,
    OP_CLOSURE,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_TYPEOF,
    // class
    OP_CLASS,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_METHOD,
    OP_INVOKE,
    OP_INHERIT,
    OP_GET_SUPER,
    OP_SUPER_INVOKE,
} OpCode;

// 指令动态数组
typedef struct
{
    int count;
    int capacity;
    /** 字节码数组 */
    uint8_t *code;
    /**
     * 整数数组。数组中的每个数字都是字节码中对应字节所在的行号
     * （优化？）https://craftinginterpreters.488848.xyz/chunks-of-bytecode.html#challenges
     */
    int *lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);

int addConstant(Chunk *chunk, Value value);

#endif