#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "scanner.h"
#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include "table.h"

// TODO
// https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter23_jumping/1.md

// TODO
// 更好的IEEE754支持，包括 +-Infinity
// 实现 i++ i-- 操作符

typedef struct
{
    /** 指向当前词素起点 */
    const char *start;
    /** 指向当前字符 */
    const char *current;
    /** 行号 */
    int line;
} Scanner;

// 注意目前使用全局变量
Scanner scanner;

void initScanner(const char *source)
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

//
// 字符判断辅助函数
// 注意我们不使用 ctype.h 中的函数，因为它们与环境相关
//

static inline bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

/*
ALPHA = %x41-5A / %x61-7A / %x5F ; a-z A-Z _
*/
static inline bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

/*
DIGIT = %x30-39 ; 0-9
 */
static Token makeToken(TokenType type)
{
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

/** NULL 结尾 */
static inline bool isAtEnd()
{
    return *scanner.current == '\0';
}

/** 消费当前字符 */
static inline char advance()
{
    scanner.current++;
    return scanner.current[-1];
}

/** 仅查看当前字符，不消费 */
static inline char peek()
{
    return scanner.current[0];
}

/** 仅查看下一个字符，不消费 */
static char peekNext()
{
    if (isAtEnd())
        return '\0';
    return scanner.current[1];
}

/** 测试当前字符是否匹配，匹配时消费当前字符 */
static bool match(char expected)
{
    if (isAtEnd())
        return false;
    if (*scanner.current != expected)
        return false;
    scanner.current++;
    return true;
}

/** 跳过空白符 */
static void skipWhitespace()
{
    for (;;)
    {
        char c = peek(); // 没有消费，需要调用 advance() 消费

        switch (c)
        {
        case ' ':
        case '\r':
        case '\t':
            advance();
            break;
        case '\n':
            advance();
            scanner.line++;
            break;
        case '/':
            if (peekNext() == '/') // 跳过注释
                while (peek() != '\n' && !isAtEnd())
                    advance();
            else
                return;
            break;
        default:
            return;
        }
    }
}

//
// ABNF
// https://www.rfc-editor.org/rfc/rfc5234
//

/**
 * @param message 错误字符串存储于静态数据段，因此无需管理内存
 */
static Token errorToken(const char *message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

#ifdef LOXJ_OPTIONS_ESCAPE
static Token stringToken()
{
    int escapeCount = 0;
    const char *src = scanner.current;
    while (peek() != '"' && !isAtEnd())
    {
        if (peek() == '\n')
            scanner.line++;
        if (peek() == '\\' && !isAtEnd())
        {
            advance(); // skip the backslash
            switch (peek())
            {
            case 'n':
            case 't':
            case '\\':
            case '"':
                escapeCount++;
                advance(); // skip
                break;
            default:
                return errorToken("Unsupported escape sequences.");
            }
        }
        else
        {
            advance();
        }
    }

    if (isAtEnd())
        return errorToken("Unterminated string.");

    advance(); // Consume the closing quote

    int srcLen = (int)(scanner.current - scanner.start) - 2;
    int len = srcLen - escapeCount;
    char *unescaped = (char *)malloc(len + 2); // 请注意这里分配了内存
    char *dst = unescaped;
    unescaped[len] = '\0';

    for (int i = 0; i < srcLen; i++)
    {
        if (src[i] == '\\')
        {
            switch (src[++i])
            {
            case 't':
                *dst = '\t';
                break;
            case 'n':
                *dst = '\n';
                break;
            case '"':
                *dst = '"';
                break;
            case '\\':
                *dst = '\\';
                break;
            default:
                break; // unreachable
            }
        }
        else
        {
            *dst = src[i];
        }
        dst++;
    }

    Token token;
    token.type = TOKEN_STRING; // scanner 保证 TOKEN_STRING 标识符
    token.start = unescaped;   // 其 start 指向的字符串
    token.length = len;        // 所有权移交给调用方
    token.line = scanner.line; // 因此调用方必须负责管理其内存
    return token;              // 参见 takeStringFromToken 辅助函数
}
#else
static Token stringToken()
{
    while (peek() != '"' && !isAtEnd())
    {
        if (peek() == '\n')
            scanner.line++;
        advance();
    }

    if (isAtEnd())
        return errorToken("Unterminated string.");

    advance();
    return makeToken(TOKEN_STRING);
}
#endif

/*
number         = integer [fraction] ; 整数部分后可选小数部分
integer        = 1*DIGIT            ; 一个或多个数字
fraction       = "." 1*DIGIT        ; 小数点后跟一个或多个数字
*/
static Token numberToken()
{
    while (isDigit(peek()))
        advance();

    if (peek() == '.' && isDigit(peekNext()))
    {
        advance();

        while (isDigit(peek()))
            advance();
    }

    return makeToken(TOKEN_NUMBER);
}

/**
 * 返回自定义关键字 TOKEN_IDENTIFIER 或 保留关键字 TOKEN_*
 */
static TokenType identifierType()
{
#define CHECK_KEYWORD(start_, length, rest, token_type)               \
    ((scanner.current - scanner.start) == ((start_) + (length))) &&   \
            (memcmp(scanner.start + (start_), (rest), (length)) == 0) \
        ? (token_type)                                                \
        : TOKEN_IDENTIFIER
    // 注意：现在 scanner.current 已指向当前词素的 **下一个字符**
    // scanner.start 指向当前词素的起点

    switch (scanner.start[0])
    {
    case 'a':
        return CHECK_KEYWORD(1, 2, "nd", TOKEN_AND); // and
    case 'b':
        return CHECK_KEYWORD(1, 4, "reak", TOKEN_AND); // break
    case 'c':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'l':
                return CHECK_KEYWORD(2, 3, "ass", TOKEN_CLASS); // class
            case 'o':
                return CHECK_KEYWORD(2, 6, "ntinue", TOKEN_CONTINUE); // continue
            }
        }
    }
    case 'e':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'l':
                return CHECK_KEYWORD(2, 2, "se", TOKEN_ELSE); // else
            case 'x':
                return CHECK_KEYWORD(2, 5, "tends", TOKEN_EXTENDS); // extends
            }
        }
    }
    case 'f':
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'a':
                return CHECK_KEYWORD(2, 3, "lse", TOKEN_FALSE); // false
            case 'o':
                return CHECK_KEYWORD(2, 1, "r", TOKEN_FOR); // for
            case 'u':
                return CHECK_KEYWORD(2, 1, "n", TOKEN_FUN) || CHECK_KEYWORD(2, 6, "nction", TOKEN_FUN); // function
            }
        }
        break;
    case 'i':
        return CHECK_KEYWORD(1, 1, "f", TOKEN_IF);
    case 'n':
        return CHECK_KEYWORD(1, 2, "il", TOKEN_NIL);
    case 'o':
        return CHECK_KEYWORD(1, 1, "r", TOKEN_OR);
    case 'p':
        return CHECK_KEYWORD(1, 4, "rint", TOKEN_PRINT);
    case 'r':
        return CHECK_KEYWORD(1, 5, "eturn", TOKEN_RETURN);
    case 's':
        return CHECK_KEYWORD(1, 4, "uper", TOKEN_SUPER);
    case 't':
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'h':
                return CHECK_KEYWORD(2, 2, "is", TOKEN_THIS); // this
            case 'r':
                return CHECK_KEYWORD(2, 2, "ue", TOKEN_TRUE); // true
            case 'y':
                return CHECK_KEYWORD(2, 4, "peof", TOKEN_TYPEOF); // typeof
            }
        }
        break;
    case 'v':
        return CHECK_KEYWORD(1, 2, "ar", TOKEN_VAR);
    case 'w':
        return CHECK_KEYWORD(1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;

#undef CHECK_KEYWORD
}

/*
identifier = ALPHA *(ALPHA / DIGIT)
 */
static Token identifierToken()
{
    while (isAlpha(peek()) || isDigit(peek()))
        advance();

    return makeToken(identifierType());
}

Token scanToken()
{
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd())
        return makeToken(TOKEN_EOF);

    char c = advance();
    if (isAlpha(c))
        return identifierToken();
    if (isDigit(c))
        return numberToken();

    switch (c)
    {
    case '(':
        return makeToken(TOKEN_LEFT_PAREN);
    case ')':
        return makeToken(TOKEN_RIGHT_PAREN);
    case '{':
        return makeToken(TOKEN_LEFT_BRACE);
    case '}':
        return makeToken(TOKEN_RIGHT_BRACE);
    case ';':
        return makeToken(TOKEN_SEMICOLON);
    case ',':
        return makeToken(TOKEN_COMMA);
    case '.':
        return makeToken(TOKEN_DOT);
    case '-':
        return makeToken(TOKEN_MINUS);
    case '+':
        return makeToken(TOKEN_PLUS);
    case '/':
        return makeToken(TOKEN_SLASH);
    case '*':
        return makeToken(TOKEN_STAR);
    //
    case '!':
        return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
        return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
    {
        if (match('='))
            return makeToken(TOKEN_LESS_EQUAL);
        else if (match('<'))
            return makeToken(match('<') ? TOKEN_UNSIGNED_LEFT_SHIFT : TOKEN_LEFT_SHIFT);
        else
            return makeToken(TOKEN_LESS);
        // 注意：我们这里添加了 <<< 无符号左移操作符
        // 虽然这是无意义的，因为 << 和 <<< 没有任何区别
        // 实现它只是为了对称
    }
    case '>':
    {
        if (match('='))
            return makeToken(TOKEN_GREATER_EQUAL);
        else if (match('>'))
            return makeToken(match('>') ? TOKEN_UNSIGNED_RIGHT_SHIFT : TOKEN_RIGHT_SHIFT);
        else
            return makeToken(TOKEN_LESS);
    }
    case '"':
        return stringToken();
    case '&':
        return makeToken(match('&') ? TOKEN_AND : TOKEN_BITWISE_AND);
    case '|':
        return makeToken(match('|') ? TOKEN_OR : TOKEN_BITWISE_OR);
    case '~':
        return makeToken(TOKEN_BITWISE_NOT);
    case '^':
        return makeToken(TOKEN_BITWISE_XOR);
    case '%':
        return makeToken(TOKEN_REMAINDER);
    }
    return errorToken("Unexpected character.");
}

Token syntheticToken(const char *text)
{
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}