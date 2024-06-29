#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include "memory.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// 单遍编译：同时解析AST和执行AST

typedef struct
{
    Token current;  // 当前标记
    Token previous; // 前一个标记
    bool hadError;  // 是否出错
    bool panicMode; // 防止级联错误。即，是否已经出错了
} Parser;

// 优先级，从低到高排序
typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
} Precedence;

Parser parser = {.hadError = false, .panicMode = false};

typedef void (*ParseFn)(bool canAssign);
typedef struct
{
    ParseFn prefix;        // 编译以该类型标识为起点的前缀表达式的函数
    ParseFn infix;         // 编译一个左操作数后跟该类型标识的中缀表达式的函数
    Precedence precedence; // 该标识作为操作符的中缀表达式的优先级
} ParseRule;

// Pratt 解析器函数声明
static void unary(bool canAssign);
static void binary(bool canAssign);
static void grouping(bool canAssign);
static void number(bool canAssign);
static void literal(bool canAssign);
static void string(bool canAssign);
static void variable(bool canAssign);
static void and_(bool canAssign);
static void or_(bool canAssign);
static void call(bool canAssign);
static void this_(bool canAssign);
static void dot(bool canAssign);
static void super_(bool canAssign);
static void parsePrecedence(Precedence precedence);

// 自顶向下算符优先解析
// 解析规则数组，每种标记类型对应的解析规则
ParseRule const rules[] = {
    /** [TokenType] [prefix-parser] [infix-parser] [precedence]  */
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_EXTENDS] = {NULL, binary, PREC_COMPARISON}, // same as TOKEN_LESS
    [TOKEN_TYPEOF] = {unary, NULL, PREC_COMPARISON},   // typeof
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {super_, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

// 根据标记类型获取解析规则
static inline const ParseRule *getRule(TokenType type)
{
    return &rules[type];
}

// 局部变量，运行时值存于栈上，这里只需追踪其深度做编译时判定即可（无解释期开销）
typedef struct
{
    Token name;
    /**
     * 与 scopeDepth 的深度保持一致
     * -1 表示未初始化，防止循环引用
     */
    int depth;
    bool isCaptured;
} Local;

typedef struct
{
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_METHOD,
    TYPE_INITIALIZER, // constructor
    TYPE_SCRIPT
} FunctionType;

// 在这个实现中，编译器是针对函数的
typedef struct Compiler
{
    struct Compiler *enclosing; // 指向父函数的编译器

    ObjFunction *function;
    FunctionType type; // 区分是顶层函数还是普通函数，顶层函数使用全局变量分配在堆上

    /** 编译器追踪的局部变量信息，非运行时值，运行时只需根据堆栈效应即可 */
    Local locals[UINT8_MAX + 1]; // TODO：增加最大局部变量槽数，需要改字节码
    /** 局部变量数量，-1 得到顶部索引 */
    int localCount;
    /** 作用域深度 */
    int scopeDepth;
    /** 上值数组 */
    Upvalue upvalues[UINT8_MAX + 1];
} Compiler;

// 当前编译字节块
Compiler *currentCompiler = NULL;
static Chunk *compilingChunk() { return &currentCompiler->function->chunk; }

typedef struct ClassCompiler
{
    struct ClassCompiler *enclosing;
    bool hasSuperclass;
} ClassCompiler;
ClassCompiler *currentClass = NULL;

static void initCompiler(Compiler *compiler, FunctionType type)
{
    compiler->enclosing = currentCompiler;
    compiler->function = NULL;
    compiler->type = type;

    compiler->localCount = 0;
    compiler->scopeDepth = 0;

    compiler->function = newFunction();
    currentCompiler = compiler;

    if (type != TYPE_SCRIPT)
        currentCompiler->function->name = copyString(parser.previous.start, parser.previous.length);

    // 隐式槽，this
    Local *local = &currentCompiler->locals[currentCompiler->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION)
    {
        local->name.start = "this";
        local->name.length = 4;
    }
    else
    {
        local->name.start = "";
        local->name.length = 0;
    }
}

// 错误处理函数（打印错误消息而已）
static void errorAt(Token *token, const char *message)
{
    if (parser.panicMode)
        return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // Nothing.
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    fflush(stderr);
    parser.hadError = true;
}

static inline void error(const char *message)
{
    errorAt(&parser.previous, message);
}

static inline void errorAtCurrent(const char *message)
{
    errorAt(&parser.current, message);
}

static inline void emitByte(uint8_t byte)
{
    writeChunk(compilingChunk(), byte, parser.previous.line);
}

static void emitReturn()
{
    if (currentCompiler->type == TYPE_INITIALIZER)
    { // return this
        emitByte(OP_GET_LOCAL);
        emitByte(0);
    }
    else
    {
        emitByte(OP_NIL);
    }

    emitByte(OP_RETURN);
}

static ObjFunction *endCompiler()
{
    emitReturn(); // 隐式 return
    // 函数编译完后把这个函数返回，使之成为**运行时值**
    ObjFunction *function = currentCompiler->function;

#ifdef DEBUG_PRINT_CODE
    // if (!parser.hadError)
    disassembleChunk(compilingChunk(), function->name != NULL ? function->name->chars : "<script>");
#endif

    currentCompiler = currentCompiler->enclosing;
    return function;
}

/** emitJump MUST pair with patchJump to work */
static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    // 跳转地址占2字节，大端
    emitByte(0xff); // <- return this offset
    emitByte(0xff);
    return compilingChunk()->count - 2;
}

static void patchJump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    // offset 字节码占 2 字节
    // jump 为向后跳转的指令（字节）数
    int jump = compilingChunk()->count - offset - 2;

    if (jump > UINT16_MAX)
        error("Too much code to jump over.");

    // big endian
    compilingChunk()->code[offset] = (jump >> 8) & 0xff;
    compilingChunk()->code[offset + 1] = jump & 0xff;
}

static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);
    // 往回跳转的字节数
    int offset = compilingChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

/** 将value加入常量数组，返回单字节，表示索引 */
static uint8_t makeConstant(Value value)
{
    int constantIndex = addConstant(compilingChunk(), value);
    if (constantIndex > UINT8_MAX)
    { // OP_CONSTANT <single_byte>, so we can only store UINT8_MAX
        error("Too many constants in one chunk.");
        // TODO：更大的常量池
        return 0;
    }
    // 因为暂时只支持单字节索引
    return (uint8_t)constantIndex;
}

static void emitConstant(Value value)
{
    emitByte(OP_CONSTANT);
    emitByte(makeConstant(value));
}

/** 步进到下一个标记 */
static void advance()
{
    parser.previous = parser.current;

    for (;;)
    {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(parser.current.start);
    }
}

/**
 * 检查当前词素是否匹配
 */
static inline bool check(TokenType type)
{
    return parser.current.type == type;
}

/**
 * 消费当前标记，如果类型不匹配则报错
 */
static inline void consume(TokenType type, const char *message)
{
    if (check(type))
    {
        advance();
        return;
    }

    errorAtCurrent(message);
}

/**
 * 消费当前标记，前提是类型匹配
 */
static inline bool match(TokenType type)
{
    if (check(type))
    {
        advance();
        return true;
    }

    return false;
}

static inline bool identifiersEqual(Token *a, Token *b)
{
    return a->length == b->length ? memcmp(a->start, b->start, a->length) == 0 : false;
}

/**
 * 局部变量，编译器实际上不需要变量名，只需要根据堆栈效应知道数量即可。
 * 不过当前实现为了比较变量名，仍保存变量名
 */
static void addLocal(Token name)
{
    if (currentCompiler->localCount >= UINT8_MAX + 1) // exceed limit
    {
        error("Too many local variables in function.");
        // TODO
        return;
    }

    Local *local = &currentCompiler->locals[currentCompiler->localCount++];
    local->name = name;
    local->isCaptured = false;
    local->depth = -1; //! 未初始化状态，防止循环引用 currentCompiler->scopeDepth;

#ifdef DEBUG_TRACE_EXECUTION
    fprintf(stderr, "[[DEBUG_TRACE_EXECUTION]]  addLocal:  index=%d  ", currentCompiler->localCount - 1);
    printToken(&local->name);
    fprintf(stderr, "\n");
#endif
}

/** 仅对当前作用域函数添加上值
 * @return 上值在函数的上值列表中的索引
 */
static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal)
{
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++)
    { // 无需重复捕获相同的上值
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal)
            return i;
    }

    if (upvalueCount >= UINT8_MAX + 1)
    {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

/**
 * 搜索本函数局部变量（不是闭包）
 * @return 局部变量索引，-1 表示未找到
 */
static int resolveLocal(Compiler *compiler, Token *name)
{
#ifdef DEBUG_TRACE_EXECUTION
    printf("[[DEBUG_TRACE_EXECUTION]]  Resolve Local  ");
    printToken(name);
    fputc('\n', stderr);
#endif

    // TODO：local 是所有局部变量，即嵌套作用域的局部变量也存在一起，通过 Local.depth 区分
    // 这样需要扫描整个数组，并且 identifiersEqual 比较Token字符串，需要优化
    // 反向遍历：自内层向外层扫描
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];

        if (identifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
                error("Can't read local variable in its own initializer.");
            else
                return i;
        }
    }

    return -1;
}

/**
 * 参见 namedVariable 函数说明
 */
static int resolveUpvalue(Compiler *compiler, Token *name)
{
    if (compiler->enclosing == NULL)
        return -1;

    int local = resolveLocal(compiler->enclosing, name); // 在上层函数中找

    if (local != -1)
    { // 若是上层函数的局部变量，则标记被捕获
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
        // true 表示是上层函数的局部变量，不过这个信息保存在当前函数中
        // 注意：只有不是本函数的局部变量，才会解析上值，因此上值总是与上层函数相关
        // 对于解释器来说，由于闭包指令在函数指令的最后，因此解析闭包指令时
        // 当前函数已解析完毕，作用域自然就是上层函数
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1)
    { // 为搜索上值的函数和每层中间函数都添加上值，达到穿透的效果
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }
    return -1;
}

//
// 语法生成式
//

// 解析一个表达式
static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

/**
 * 将标识符加入常量池
 * @return 标识符在常量数组中的索引
 */
static uint8_t identifierConstant(Token *name)
{
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

/**
 * 声明局部变量
 */
static inline void declareVariable()
{
    if (currentCompiler->scopeDepth == 0)
        return; // 无需处理全局变量

    Token *name = &parser.previous;

#ifdef DEBUG_TRACE_EXECUTION
    fprintf(stderr, "[[DEBUG_TRACE_EXECUTION]]  declareVariable:  ");
    printToken(name);
    fprintf(stderr, "\n");
#endif

    for (int i = currentCompiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &currentCompiler->locals[i];
        if (local->depth != -1 && local->depth < currentCompiler->scopeDepth)
            break; // 已经遍历到上层词法作用域，无需继续

        if (identifiersEqual(name, &local->name))
            error("Already a variable with this name in this scope.");
    }

    addLocal(*name);
}

static void markInitialized()
{
    if (currentCompiler->scopeDepth == 0)
        return; // 全局变量无需绑定

    currentCompiler->locals[currentCompiler->localCount - 1].depth = currentCompiler->scopeDepth;
}

/**
 * （声明之后）定义变量
 */
static inline void defineVariable(uint8_t global)
{
#ifdef DEBUG_TRACE_EXECUTION
    fprintf(stderr, "[[DEBUG_TRACE_EXECUTION]]  defineVariable:   index=%d", global);
    fprintf(stderr, "\n");
#endif

    if (currentCompiler->scopeDepth > 0)
    {
        markInitialized();
        return; // 局部变量，无需索引
    }
    else
    { // 全局变量
        emitByte(OP_DEFINE_GLOBAL);
        emitByte(global);
    }
}

/** 实际上是解析解析变量名，不生成任何指令 */
static uint8_t parseVariable(const char *expectMessage)
{
    consume(TOKEN_IDENTIFIER, expectMessage);

    declareVariable();

    // 局部变量名
    if (currentCompiler->scopeDepth > 0)
        return 0;

    // 全局变量名
    return identifierConstant(&parser.previous);
}

/**
 * 调用者保证现在 var 标识符已被消费，当前词素为 IDENTIFIER
```
varDecl ::= "var" IDENTIFIER [ "=" expression ] ";"
```
 */
static void varDeclaration()
{
    uint8_t global = parseVariable("Expect variable name."); // 变量名

    if (match(TOKEN_EQUAL))
        expression(); // 变量值
    else
        emitByte(OP_NIL);

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

static void printStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void beginScope()
{
    currentCompiler->scopeDepth++;
}

/** 释放局部变量 */
static void endScope()
{
    currentCompiler->scopeDepth--;

    // 当我们弹出一个作用域时，后向遍历局部变量数组，查找在刚刚离开的作用域深度上声明的所有变量。通过简单地递减数组长度来丢弃它们
    while (currentCompiler->localCount > 0 &&
           currentCompiler->locals[currentCompiler->localCount - 1].depth > currentCompiler->scopeDepth)
    {
        if (currentCompiler->locals[currentCompiler->localCount - 1].isCaptured)
        { // 已捕获表示是上值，要求解释器提取到堆中
            emitByte(OP_CLOSE_UPVALUE);
        }
        else
        {
            emitByte(OP_POP);
        }

        currentCompiler->localCount--;
    }
}

/** 生成字节码，将指定标识符加载到栈顶 */
static void namedVariable(Token name, bool canAssign)
{

#ifdef DEBUG_TRACE_EXECUTION
    fprintf(stderr, "[[DEBUG_TRACE_EXECUTION]]  namedVariable:  ");
    printToken(&name);
    fprintf(stderr, "\n");
#endif

    uint8_t getOp, setOp;
    // 十分小心类型转换！
    int arg = resolveLocal(currentCompiler, &name);
    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = resolveUpvalue(currentCompiler, &name)) != -1)
    { // 闭包中的变量，如果不是本作用域的局部变量，则需要捕获
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitByte(setOp);
        emitByte((uint8_t)arg);
    }
    else
    {
        emitByte(getOp);
        emitByte((uint8_t)arg);
    }
}

static void declaration();
static void block()
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        declaration();

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void statement();
static void ifStatement()
{
    // if(expression)
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    // statement
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    statement();
    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    // else statement
    if (match(TOKEN_ELSE))
        statement();

    patchJump(elseJump);

    /** (Example)
0000     | OP_CONSTANT
0002     | OP_JUMP_IF_FALSE  -----
0005     | OP_POP                |
0006     | OP_CONSTANT           |
0008     | OP_PRINT              |
0009     | OP_JUMP -------       |
0012     | OP_POP   -----|--------
0013     | OP_CONSTANT   |
0015     | OP_PRINT      |
0016     | OP_RETURN -----
     */
}

// TODO：支持 continue 和 break
static void whileStatement()
{
    int loopStart = compilingChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

// TODO: break is not implemented yet
// 支持 continue 和 break
int innermostLoopStart = -1;     // the point that a continue statement should jump to
int innermostLoopScopeDepth = 0; // the scope of the variables declared inside the loop
static void forStatement()
{
    beginScope(); // {

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    if (match(TOKEN_SEMICOLON))
        ; // No initializer.
    else if (match(TOKEN_VAR))
        varDeclaration();
    else
        expressionStatement();

    int surroundingLoopStart = innermostLoopStart;
    int surroundingLoopScopeDepth = innermostLoopScopeDepth;
    innermostLoopStart = compilingChunk()->count;
    innermostLoopScopeDepth = currentCompiler->scopeDepth;

    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON))
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.
    }

    if (!match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = compilingChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(innermostLoopStart);
        innermostLoopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();
    emitLoop(innermostLoopStart);

    if (exitJump != -1)
    {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    innermostLoopStart = surroundingLoopStart;
    innermostLoopScopeDepth = surroundingLoopScopeDepth;

    endScope(); // }
}

static void continueStatement()
{
    if (innermostLoopStart == -1)
        error("Can't use 'continue' outside of a loop.");
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    // Discard any locals created inside the loop.
    for (int i = currentCompiler->localCount - 1;
         i >= 0 && currentCompiler->locals[i].depth > innermostLoopScopeDepth;
         i--)
    {
        emitByte(OP_POP);
    }

    // Jump to top of current innermost loop.
    emitLoop(innermostLoopStart);
}

static void method();
static void classDeclaration()
{
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitByte(OP_CLASS);
    emitByte(nameConstant); // 子类字节码
    defineVariable(nameConstant);

    // 保持类编译器链栈
    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (match(TOKEN_EXTENDS) || match(TOKEN_LESS))
    { // 继承
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);
        if (identifiersEqual(&className, &parser.previous))
        {
            error("A class can't inherit from itself.");
        }
        // super
        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);

        namedVariable(className, false); // 父类字节码
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false); // 将类加载到栈顶

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        method();
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

    emitByte(OP_POP); // 将类弹出

    if (classCompiler.hasSuperclass)
        endScope();

    currentClass = currentClass->enclosing;
}

static void function(FunctionType type)
{
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            int count = ++currentCompiler->function->arity;
            if (count > 255)
                errorAtCurrent("Can't have more than 255 parameters.");

            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    // compiler 进入 函数体块编译并解释，当函数解释完后，整个调用帧被丢弃
    // 无需 endScope()

    ObjFunction *function = endCompiler();
    // 函数编译完成的最后生成一系列闭包指令，令解释器正确处理上值
    emitByte(OP_CLOSURE);
    emitByte(makeConstant(OBJ_VAL(function)));

    // 这里无需上值数量的字节码，因为单遍编译并同时解释字节码
    // 编译器针对函数/闭包编译，此信息已保存在编译器中
    // 换句话说，vm 是能够访问 compiler 的，compiler 并非只有字节码
    for (int i = 0; i < function->upvalueCount; i++)
    {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method()
{
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(&parser.previous);
    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == LOXJ_OPTIONS_INIT_LENGTH &&
        memcmp(parser.previous.start, LOXJ_OPTIONS_INIT, LOXJ_OPTIONS_INIT_LENGTH) == 0)
    {
        type = TYPE_INITIALIZER;
    }
    function(type);
    emitByte(OP_METHOD);
    emitByte(constant);
}

/**
```
funDecl ::= "fun" function
```
 */
static void funDeclaration()
{
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void returnStatement()
{
    if (currentCompiler->type == TYPE_SCRIPT)
        error("Illegal return statement in the top-level.");

    if (match(TOKEN_SEMICOLON))
    {
        emitReturn();
    }
    else
    {
        if (currentCompiler->type == TYPE_INITIALIZER)
        { // constructor
            error("Can't return a value from an initializer.");
        }

        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void statement()
{
    if (match(TOKEN_PRINT))
        printStatement();
    else if (match(TOKEN_FOR))
        forStatement();
    else if (match(TOKEN_IF))
        ifStatement();
    else if (match(TOKEN_WHILE))
        whileStatement();
    else if (match(TOKEN_CONTINUE))
        continueStatement();
    else if (match(TOKEN_RETURN))
        returnStatement();
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else
        expressionStatement();
}

static void synchronize();
/**
```
declaration ::= classDecl
              | funDecl
              | varDecl
              | statement
```
 */
static void declaration()
{
    if (match(TOKEN_CLASS))
        classDeclaration();
    else if (match(TOKEN_FUN))
        funDeclaration();
    else if (match(TOKEN_VAR))
        varDeclaration();
    // TODO：增加 const 初始化
    else
        statement();

    if (parser.panicMode)
        synchronize();
}

static void variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

static void this_(bool canAssign)
{
    if (currentClass == NULL)
    {
        error("Can't use 'this' outside of a class.");
        return;
    }

    variable(false);
}

static uint8_t argumentList();
static void super_(bool canAssign)
{
    if (currentClass == NULL)
        error("Can't use 'super' outside of a class.");
    else if (!currentClass->hasSuperclass)
        error("Can't use 'super' in a class without superclass.");

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);

    if (match(TOKEN_LEFT_PAREN))
    {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false);
        emitByte(OP_SUPER_INVOKE);
        emitByte(name);
        emitByte(argCount);
    }
    else
    {
        namedVariable(syntheticToken("super"), false);
        emitByte(OP_GET_SUPER);
        emitByte(name);
    }
}

// 解析一个数字
static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

// 解析其它字面量
static void literal(bool canAssign)
{
    switch (parser.previous.type)
    {
    case TOKEN_FALSE:
        emitByte(OP_FALSE);
        break;
    case TOKEN_NIL:
        emitByte(OP_NIL);
        break;
    case TOKEN_TRUE:
        emitByte(OP_TRUE);
        break;
    default:
        return; // Unreachable.
    }
}

// 解析字符串
static void string(bool canAssign)
{
#ifdef LOXJ_OPTIONS_ESCAPE
    // 必须与 scanner 的 stringToken 保持一致
    emitConstant(OBJ_VAL(takeStringFromToken((char *)parser.previous.start, parser.previous.length)));
#else
    emitConstant(
        OBJ_VAL(
            // 去除开头结尾的引号
            copyString(parser.previous.start + 1, parser.previous.length - 2)));
#endif
}

// 解析一个分组表达式
static void grouping(bool canAssign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

// 解析一个一元操作符
static void unary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;

    // 编译操作数
    parsePrecedence(PREC_UNARY);

    // 生成操作符指令
    switch (operatorType)
    {
    case TOKEN_BANG:
        emitByte(OP_NOT);
        break;
    case TOKEN_MINUS:
        emitByte(OP_NEGATE);
        break;
    case TOKEN_TYPEOF:
        emitByte(OP_TYPEOF);
        break;

    default:
        return; // 不可达
    }
}

// 解析一个二元操作符
static void binary(bool canAssign)
{
    //! 当解析中缀时，其左侧已经解析，因此只需获得操作符并（递归）解析右侧
    TokenType operatorType = parser.previous.type; // 调用者保证这是操作符
    ParseRule const *rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    // 生成操作符指令
    switch (operatorType)
    {
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;

    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    case TOKEN_BANG_EQUAL:
        emitByte(OP_EQUAL);
        emitByte(OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitByte(OP_LESS);
        emitByte(OP_NOT);
        break;
    case TOKEN_LESS:
        emitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitByte(OP_GREATER);
        emitByte(OP_NOT);
        break;
    default:
        return; // 不可达
    }
}

static void and_(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
    /*
left-statement
OP_JUMP_IF_FALSE
OP_POP 短路丢弃左边值
right-statement
next-statement
*/
}

static void or_(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
    /* TODO：需要3条指令，优化为2条
left-expression
OP_JUMP_IF_FALSE
OP_JUMP
OP_POP
right-expession
next-statement
     */
}

static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression();

            if (argCount == 255)
                error("Can't have more than 255 arguments.");

            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void call(bool canAssign)
{
    uint8_t argCount = argumentList();
    emitByte(OP_CALL);
    emitByte(argCount);
}

static void dot(bool canAssign)
{
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitByte(OP_SET_PROPERTY);
        emitByte(name);
    }
    else if (match(TOKEN_LEFT_PAREN))
    { // instance.method()
        uint8_t argCount = argumentList();
        emitByte(OP_INVOKE);
        emitByte(name);
        emitByte(argCount);
    }
    else
    {
        emitByte(OP_GET_PROPERTY);
        emitByte(name);
    }
}

/**
 * 递归下降解析 优先级 >= precedence 的所有词素
 * @param precedence 优先级枚举
 */
static void parsePrecedence(Precedence precedence)
{
    //! 从左到右，第一个标记必定为前缀表达式（这当然是由我们的语法决定的，源代码由语句构成）
    advance(); // 移动到下一个标记（token）

    // 获取当前标记的前缀解析规则
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    // 如果没有前缀解析器，那么这个标记一定是语法错误
    if (prefixRule == NULL)
    {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign); // 调用前缀解析

    // 当当前标记的优先级大于等于传入的优先级时，继续解析
    while (getRule(parser.current.type)->precedence >= precedence)
    {
        advance(); // 当前标记是操作符，现在移动到下一个标记（token）

        // 解析完前缀后就解析中缀
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    // 如果 = 没有作为表达式的一部分被消耗，那么表明左侧无法被赋值
    if (canAssign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target.");
    }
}

/**
 * 编译源代码到字节码块
```
program ::= { declaration } EOF
```
 */
ObjFunction *compile(const char *sourceCode)
{
    initScanner(sourceCode); // 初始化扫描器

    // TODO: 下面的定义有生命期问题，最好要优化
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance(); // 获取第一个标记

    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    ObjFunction *function = endCompiler();
    return parser.hadError ? NULL : function; // NULL 表示编译错误
}

// 尝试消除恐慌模式。即跳过可能是级联错误的语素，尝试解析下一个错误（hasError 还是 true）
static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;

        switch (parser.current.type)
        {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;

        default:; // Do nothing.
        }

        advance();
    }
}

void markCompilerRoots()
{
    Compiler *compiler = currentCompiler;
    while (compiler != NULL)
    {
        markObject((Obj *)compiler->function);
        compiler = compiler->enclosing;
    }
}