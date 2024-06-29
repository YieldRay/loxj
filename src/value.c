#include <stdio.h>
#include <string.h>
#include <math.h>

#include "value.h"
#include "object.h"
#include "memory.h"

/**
 * 必须使用此函数初始化值数组
 * 此函数亦可用作清理，因为不分配内存
 */
void initValueArray(ValueArray *array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

/**
 * 尾追加
 * push back
 */
void writeValueArray(ValueArray *array, Value value)
{
    if (array->capacity < array->count + 1)
    {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray *array)
{
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

/**
 * 返回静态段内存字符串，无需分配和回收内存
 */
const char *typeofValue(Value value)
{
#ifdef NAN_BOXING
    if (IS_BOOL(value))
        return "boolean";
    else if (IS_NIL(value))
        return "nil";
    else if (IS_NUMBER(value))
        return "number";
    else if (IS_OBJ(value))
    {
        switch (AS_OBJ(value)->type)
        {
        case OBJ_CLASS:
            return "class";
        case OBJ_INSTANCE:
            return "object";
        case OBJ_BOUND_METHOD:
        case OBJ_CLOSURE:
        case OBJ_FUNCTION:
        case OBJ_NATIVE:
            return "function";
        case OBJ_STRING:
            return "string";
        case OBJ_UPVALUE: // unreachable
            return "upvalue";
        }
    }
#else
    switch (value.type)
    {
    case VAL_BOOL:
        return "boolean";
    case VAL_NIL:
        return "nil";
    case VAL_NUMBER:
        return "number";
    case VAL_OBJ:
    {
        switch (AS_OBJ(value)->type)
        {
        case OBJ_CLASS:
            return "class";
        case OBJ_INSTANCE:
            return "object";
        case OBJ_BOUND_METHOD:
        case OBJ_CLOSURE:
        case OBJ_FUNCTION:
        case OBJ_NATIVE:
            return "function";
        case OBJ_STRING:
            return "string";
        case OBJ_UPVALUE: // unreachable
            return "upvalue";
        }
    }
    }
#endif
    return "unknown"; // unreachable
}

bool isValuesEqual(Value a, Value b)
{
#ifdef NAN_BOXING
    if (IS_NUMBER(a) && IS_NUMBER(b))
        return AS_NUMBER(a) == AS_NUMBER(b); // handle NaN
    return a == b;
#else

    if (a.type != b.type)
        return false;
    switch (a.type)
    {
    case VAL_BOOL:
        return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:
        return true;
    case VAL_NUMBER:
        if (isnan(AS_NUMBER(a)) || isnan(AS_NUMBER(b)))
            return false; // NaN != NaN
        return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ:
        return AS_OBJ(a) == AS_OBJ(b); // 对象为不变值，且字符串驻留，因此只需比较地址
    default:
        return false; // Unreachable.
    }
#endif
}

inline bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)) || (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

static void printFunction(ObjFunction *function)
{
    if (function->name == NULL)
    {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

static void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJ_CLASS:
        printf("<class %s>", AS_CLASS(value)->name->chars);
        break;
    case OBJ_INSTANCE:
        printf("<instance %s>", AS_INSTANCE(value)->klass->name->chars);
        break;
    case OBJ_BOUND_METHOD:
        printFunction(AS_BOUND_METHOD(value)->method->function);
        break;
    case OBJ_CLOSURE:
        printFunction(AS_CLOSURE(value)->function);
        break;
    case OBJ_FUNCTION:
        printFunction(AS_FUNCTION(value));
        break;
    case OBJ_NATIVE:
        printf("<native fn>");
        break;
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    case OBJ_UPVALUE: // Unreachable.
        printf("<upvalue>");
        break;
    }
}

// 仅供内部打印使用
inline void printValue(Value value)
{
#ifdef NAN_BOXING
    if (IS_BOOL(value))
    {
        printf(AS_BOOL(value) ? "true" : "false");
    }
    else if (IS_NIL(value))
    {
        printf("nil");
    }
    else if (IS_NUMBER(value))
    {
        printf("%g", AS_NUMBER(value));
    }
    else if (IS_OBJ(value))
    {
        printObject(value);
    }
#else
    switch (value.type)
    {
    case VAL_BOOL:
        printf(AS_BOOL(value) ? "true" : "false");
        break;
    case VAL_NIL:
        printf("<nil>");
        break;
    case VAL_NUMBER:
        printf("%g", AS_NUMBER(value));
        break;
    case VAL_OBJ:
        printObject(value);
        break;
    }
#endif
}
