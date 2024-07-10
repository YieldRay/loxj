#ifndef loxj_scanner_h
#define loxj_scanner_h

typedef enum
{
    // Single-character tokens
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,
    // One or two character tokens
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    // Literals
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    // Keywords
    TOKEN_CLASS,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NIL,
    TOKEN_REMAINDER,            // %
    TOKEN_BITWISE_AND,          // &
    TOKEN_BITWISE_OR,           // |
    TOKEN_BITWISE_XOR,          // ^
    TOKEN_BITWISE_NOT,          // ~
    TOKEN_LEFT_SHIFT,           // <<
    TOKEN_UNSIGNED_LEFT_SHIFT,  // <<<
    TOKEN_RIGHT_SHIFT,          // >>
    TOKEN_UNSIGNED_RIGHT_SHIFT, // >>>
    TOKEN_PRINT,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,
    TOKEN_CONTINUE,
    TOKEN_BREAK,
    TOKEN_EXTENDS,
    TOKEN_TYPEOF,
    // Others
    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

/** 词素 */
typedef struct
{
    TokenType type;
    const char *start;
    int length;
    int line;
} Token;

void initScanner(const char *sourceCode);
Token scanToken();
Token syntheticToken(const char *text);

#endif