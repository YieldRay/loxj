#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "value.h"
#include "object.h"

//
// helper functions for disassembling single binary instruction
//

static int simpleInstruction(const char *name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}
static int constantInstruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t constantIndex = chunk->code[offset + 1];

    printf("%-16s ", name);
    printf("constantIndex=%-4d ", constantIndex);
    printf("constantValue=");
    printValue(chunk->constants.values[constantIndex]);
    putchar('\n');
    // OP_CONSTANT         0 '1.2'
    return offset + 2;
}
static int byteInstruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s ", name);
    printf("%d", slot);
    putchar('\n');
    return offset + 2;
}
/** @param sign forth or back */
static int jumpInstruction(const char *name, int8_t sign, Chunk *chunk, int offset)
{
    uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2]; // 大端
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}
static int invokeInstruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s constantIndex=%d ", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("(%d args)\n", argCount);
    return offset + 3;
}

//
// 字节码块调试
//

/**
 * Disassemble the entire chunk, and print it
 */
void disassembleChunk(Chunk *chunk, const char *name)
{
    printf("\n\n== begin %s ==\n", name);

    printf("Index Line %-16s ExtraInfo\n", "ByteCode");
    for (int offset = 0; offset < chunk->count;)
    {
        offset = disassembleInstruction(chunk, offset);
    }
    printf("== end %s ==\n\n", name);
}

/**
 * Disassemble and print current instuction by offset, returns next offset
 * @param offset 当前字节码偏移量
 * @return 下一个字节码偏移量
 */
int disassembleInstruction(Chunk *chunk, int offset)
{
    printf("%04d  ", offset);

    // 打印行号
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
    { // 表示当前指令的源码位置和上一行处于同一行
        printf("   | ");
    }
    else
    { // 当前行
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
    case OP_CONSTANT:
        return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NIL:
        return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
        return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
        return simpleInstruction("OP_FALSE", offset);
    case OP_POP:
        return simpleInstruction("OP_POP", offset);
    case OP_GET_PROPERTY:
        return constantInstruction("OP_GET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY:
        return constantInstruction("OP_SET_PROPERTY", chunk, offset);
    case OP_EQUAL:
        return simpleInstruction("OP_EQUAL", offset);
    case OP_GET_GLOBAL:
        return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
        return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
        return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_UPVALUE:
        return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
        return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_GET_LOCAL:
        return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
        return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_GREATER:
        return simpleInstruction("OP_GREATER", offset);
    case OP_LESS:
        return simpleInstruction("OP_LESS", offset);
    case OP_ADD:
        return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
        return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
        return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return simpleInstruction("OP_DIVIDE", offset);
    case OP_NOT:
        return simpleInstruction("OP_NOT", offset);
    case OP_BITWISE_NOT:
        return simpleInstruction("OP_BITWISE_NOT", offset);
    case OP_BITWISE_XOR:
        return simpleInstruction("OP_BITWISE_XOR", offset);
    case OP_BITWISE_AND:
        return simpleInstruction("OP_BITWISE_AND", offset);
    case OP_BITWISE_OR:
        return simpleInstruction("OP_BITWISE_OR", offset);
    case OP_LEFT_SHIFT:
        return simpleInstruction("OP_LEFT_SHIFT", offset);
    case OP_RIGHT_SHIFT:
        return simpleInstruction("OP_RIGHT_SHIFT", offset);
    case OP_UNSIGNED_LEFT_SHIFT:
        return simpleInstruction("OP_UNSIGNED_LEFT_SHIFT", offset);
    case OP_UNSIGNED_RIGHT_SHIFT:
        return simpleInstruction("OP_UNSIGNED_RIGHT_SHIFT", offset);
    case OP_NEGATE:
        return simpleInstruction("OP_NEGATE", offset);
    case OP_PRINT:
        return simpleInstruction("OP_PRINT", offset);
    case OP_JUMP:
        return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
        return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
        return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
        return byteInstruction("OP_CALL", chunk, offset);
    case OP_CLOSURE:
    {
        offset++;
        uint8_t constant = chunk->code[offset++];
        printf("%-16s %4d ", "OP_CLOSURE", constant);
        printValue(chunk->constants.values[constant]);
        printf("\n");

        ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
        for (int j = 0; j < function->upvalueCount; j++)
        {
            int isLocal = chunk->code[offset++];
            int index = chunk->code[offset++];
            printf("%04d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
        }
        return offset;
    }
    case OP_CLOSE_UPVALUE:
        return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
        return simpleInstruction("OP_RETURN", offset);
    case OP_CLASS:
        return constantInstruction("OP_CLASS", chunk, offset);
    case OP_METHOD:
        return constantInstruction("OP_METHOD", chunk, offset);
    case OP_INVOKE:
        return invokeInstruction("OP_INVOKE", chunk, offset);
    case OP_INHERIT:
        return simpleInstruction("OP_INHERIT", offset);
    case OP_GET_SUPER:
        return constantInstruction("OP_GET_SUPER", chunk, offset);
    case OP_SUPER_INVOKE:
        return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
    case OP_TYPEOF:
        return simpleInstruction("OP_TYPEOF", offset);
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}

void printToken(Token *token)
{
    char *name = (char *)malloc(token->length + 1);
    memcpy(name, token->start, token->length);
    name[token->length] = '\0';

    fprintf(stderr, "Token(type=%d, name=%s, L%d)", token->type, name, token->line);
    free(name);
}