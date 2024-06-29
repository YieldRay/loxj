#ifndef loxj_object_h
#define loxj_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

//
// objects
//

typedef enum
{
    OBJ_CLASS,
    OBJ_BOUND_METHOD,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

struct Obj
{
    ObjType type;
    bool isMarked;
    struct Obj *next; // 作链表用，跟踪所有对象
};

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

static inline bool isObjType(Value value, ObjType type)
{ // value 出现两次，因此不能使用宏实现
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

struct ObjString
{
    Obj obj;
    int length;
    uint32_t hash; // Jay字符串是不可变值，故其哈希不变。字符串存储哈希以免多次计算
    char *chars;   // TODO：使用灵活数组成员改写：https://ray.deno.dev/posts/clang-flexible-array-member
};

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

ObjString *copyString(const char *chars, int length);
ObjString *takeString(char *chars, int length);
ObjString *takeStringFromToken(char *chars, int length);

typedef struct
{
    Obj obj;
    ObjString *name;
    int arity;
    /** 每个函数有自己的字节码块 */
    Chunk chunk; // TODO：也可以令函数的字节码块嵌入整个块
    int upvalueCount;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNative;

ObjFunction *newFunction();
ObjNative *newNative(NativeFn function);

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)

// 上值充当中间层，以便在被捕获的局部变量离开堆栈后能继续找到它
typedef struct ObjUpvalue
{
    Obj obj;
    /** 上值一律按引用捕获，因此是指针 */
    Value *location;
    /** 上值关闭时使用 */
    Value closed;
    /** 链表 */
    struct ObjUpvalue *next;
} ObjUpvalue;

// TODO：let关键字每次为for重新创建变量
// https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter25_closures/2.md

ObjUpvalue *newUpvalue(Value *slot);

typedef struct
{
    Obj obj;
    ObjFunction *function;
    ObjUpvalue **upvalues;
    int upvalueCount;
} ObjClosure;

ObjClosure *newClosure(ObjFunction *function);
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))

typedef struct
{
    Obj obj;
    ObjString *name;
    Table methods;
    // https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter28_methods/1.md
} ObjClass;

ObjClass *newClass(ObjString *name);
#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define AS_CLASS(value) ((ObjClass *)AS_OBJ(value))

typedef struct
{
    Obj obj;
    ObjClass *klass;
    Table fields;
} ObjInstance;

ObjInstance *newInstance(ObjClass *klass);
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define AS_INSTANCE(value) ((ObjInstance *)AS_OBJ(value))

typedef struct
{
    Obj obj;
    Value receiver;
    ObjClosure *method;
} ObjBoundMethod;

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method);
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define AS_BOUND_METHOD(value) ((ObjBoundMethod *)AS_OBJ(value))

#endif