#ifndef loxj_value_h
#define loxj_value_h

#include <string.h>
#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define QNAN ((uint64_t)0x7ffc000000000000) // 0|11111111111|11......0  指数位全1表示NaN，静默NaN位和英特尔值位为1
#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define TAG_NIL 1   // 01
#define TAG_FALSE 2 // 10
#define TAG_TRUE 3  // 11

typedef uint64_t Value;
#define NUMBER_VAL(num) numToValue(num)
static inline Value numToValue(double num)
{ // 类型双关：大部分编译器都能识别这种模式，并完全优化掉 memcpy()
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}
#define AS_NUMBER(value) valueToNum(value)
static inline double valueToNum(Value value)
{
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
#define IS_NIL(value) ((value) == NIL_VAL)
#define IS_BOOL(value) (((value) | 1) == TRUE_VAL)
#define AS_BOOL(value) ((value) == TRUE_VAL)
#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define OBJ_VAL(obj) (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))
#define AS_OBJ(value) ((Obj *)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))
#define IS_OBJ(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#else

typedef enum
{
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct
{
    ValueType type; // 类型枚举，一般为 4 字节 (sizeof(ValueType) = 4)
    union
    {
        bool boolean;
        double number;
        Obj *obj;
    } as;
} Value;

#define OBJ_VAL(object) ((Value){.type = VAL_OBJ, .as = {.obj = (Obj *)object}})
#define IS_OBJ(value) ((value).type == VAL_OBJ)
#define AS_OBJ(value) ((value).as.obj)

#define BOOL_VAL(value) ((Value){.type = VAL_BOOL, .as = {.boolean = value}})
#define NIL_VAL ((Value){.type = VAL_NIL, .as = {.number = 0}})
#define NUMBER_VAL(value) ((Value){.type = VAL_NUMBER, .as = {.number = value}})

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#endif

typedef struct
{
    int capacity;
    int count;
    Value *values;
} ValueArray;

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);

bool isValuesEqual(Value a, Value b);
bool isFalsey(Value value);
void printValue(Value value);
const char *typeofValue(Value value);

#endif