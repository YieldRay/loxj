#ifndef loxj_vm_h
#define loxj_vm_h

#include "chunk.h"
#include "value.h"
#include "table.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * (UINT8_MAX + 1))

// 调用帧
typedef struct
{
    /** 注意函数持有字节码块 */
    ObjClosure *closure;
    /** 函数自身的指令指针 */
    uint8_t *ip;
    /** 指向 vm 值栈中该函数可以使用的第一个槽 */
    Value *slots;
} CallFrame;
// https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter25_closures/1.md

typedef struct
{
    Chunk *chunk;
    /** instruction pointer */
    uint8_t *ip;
    /** 栈是静态定长数组，故其生命周期由 VM 结构体管理 */
    Value stack[STACK_MAX];
    /** 指向下一个栈顶元素，见 push/pop 实现 */
    Value *stackTop;
    /** 跟踪所有对象的链表 */
    Obj *objects;
    /** 驻留字符串常量值（哈希表作集合用） */
    Table strings;
    /** 堆全局变量 */
    Table globals; // TODO：优化为符号表

    /** 调用帧 */
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    /** 开放上值链表 */
    ObjUpvalue *openUpvalues;

    /** constructor */
    ObjString *initString;

    // 灰色对象工作列表
    int grayCount;
    int grayCapacity;
    Obj **grayStack;

    // 决定GC调度时机
    size_t bytesAllocated;
    size_t nextGC;
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;
void initVM();
void freeVM();

InterpretResult interpret(const char *sourceCode);
void push(Value value);
Value pop();
void defineNative(const char *name, NativeFn function);

#endif