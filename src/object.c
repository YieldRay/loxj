#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(TYPE, objectType) (TYPE *)allocateObject(sizeof(TYPE), objectType)

static Obj *allocateObject(size_t size, ObjType type)
{
    Obj *object = (Obj *)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    // 头插
    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif

    return object;
}

/**
 * chars 所有权被转移到此函数
 * 注意此函数分配的是 obj，而非其内部的 chars
 */
static ObjString *allocateString(char *chars, int length, uint32_t hash)
{
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NIL_VAL); // 驻留字符串，以便重用
    pop();

    return string;
}

static uint32_t hashString(const char *key, int length);

// 调用方无 chars 所有权
ObjString *copyString(const char *chars, int length)
{
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned; // 若有驻留值，返回驻留值，无需重复分配内存

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

// 调用方需具有 chars 所有权
ObjString *takeString(char *chars, int length)
{
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1); // NULL 结尾，size = length + 1
        return interned;
        // 如果字符串已经驻留了，传入的 chars 就是多余的。所以需要释放其内存
    }

    return allocateString(chars, length, hash);
}

/**
 * 由于 scanner 内存不归 vm 管理，因此需要做特殊处理
 */
ObjString *takeStringFromToken(char *chars, int length)
{
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
    {
        free(chars); // 使用普通 free
        return interned;
    }
    // 移交所有权
    return allocateString(chars, length, hash);
}

/**
 * FNV-1a (32-bit) 算法
 * http://www.isthe.com/chongo/tech/comp/fnv/
 */
static uint32_t hashString(const char *key, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjFunction *newFunction()
{
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    function->upvalueCount = 0;
    return function;
}

ObjNative *newNative(NativeFn function)
{
    ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

ObjClosure *newClosure(ObjFunction *function)
{
    ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++)
    {
        upvalues[i] = NULL;
    }

    ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjUpvalue *newUpvalue(Value *slot)
{
    ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->next = NULL;
    upvalue->closed = NIL_VAL;
    return upvalue;
}

ObjClass *newClass(ObjString *name)
{
    ObjClass *klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjInstance *newInstance(ObjClass *klass)
{
    ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method)
{
    ObjBoundMethod *bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}