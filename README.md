# LoxJ

> Lox in JavaScript like grammer.

LoxJ 是 [lox](https://github.com/munificent/craftinginterpreters) 的方言，支持部分 JavaScript 风格的关键字。  
基于 clox 实现（MIT LICENSE），clox 是仅依赖 C 标准库实现的单遍编译器+字节码虚拟机。

lox 是 C 风格的动态类型语言，支持闭包、基于类的面向对象编程。

```
; 语法
<program>        ::= <declaration>* <EOF>

<declaration>    ::= <classDecl>
                   | <funDecl>
                   | <varDecl>
                   | <statement>

<classDecl>      ::= "class" <IDENTIFIER> ( "<" <IDENTIFIER> )?
                     "{" <function>* "}"
<funDecl>        ::= "fun" <function>
<varDecl>        ::= "var" <IDENTIFIER> ( "=" <expression> )? ";"

<statement>      ::= <exprStmt>
                   | <forStmt>
                   | <ifStmt>
                   | <printStmt>
                   | <returnStmt>
                   | <whileStmt>
                   | <block>

<exprStmt>       ::= <expression> ";"
<forStmt>        ::= "for" "(" ( <varDecl> | <exprStmt> | ";" )
                               <expression>? ";"
                               <expression>? ")" <statement>
<ifStmt>         ::= "if" "(" <expression> ")" <statement>
                     ( "else" <statement> )?
<printStmt>      ::= "print" <expression> ";"
<returnStmt>     ::= "return" <expression>? ";"
<whileStmt>      ::= "while" "(" <expression> ")" <statement>
<block>          ::= "{" <declaration>* "}"

<expression>     ::= <assignment>

<assignment>     ::= ( <call> "." )? <IDENTIFIER> "=" <assignment>
                   | <logic_or>

<logic_or>       ::= <logic_and> ( "or" <logic_and> )*
<logic_and>      ::= <equality> ( "and" <equality> )*
<equality>       ::= <comparison> ( ( "!=" | "==" ) <comparison> )*
<comparison>     ::= <term> ( ( ">" | ">=" | "<" | "<=" ) <term> )*
<term>           ::= <factor> ( ( "-" | "+" ) <factor> )*
<factor>         ::= <unary> ( ( "/" | "*" ) <unary> )*

<unary>          ::= ( "!" | "-" ) <unary> | <call>
<call>           ::= <primary> ( "(" <arguments>? ")" | "." <IDENTIFIER> )*
<primary>        ::= "true" | "false" | "nil" | "this"
                   | <NUMBER> | <STRING> | <IDENTIFIER> | "(" <expression> ")"
                   | "super" "." <IDENTIFIER>

<function>       ::= <IDENTIFIER> "(" <parameters>? ")" <block>
<parameters>     ::= <IDENTIFIER> ( "," <IDENTIFIER> )*
<arguments>      ::= <expression> ( "," <expression> )*

; 词法
<NUMBER>         ::= <DIGIT>+ ( "." <DIGIT>+ )?
<STRING>         ::= "\"" <any char except "\">* "\""
<IDENTIFIER>     ::= <ALPHA> ( <ALPHA> | <DIGIT> )*
<ALPHA>          ::= "a" ... "z" | "A" ... "Z" | "_"
<DIGIT>          ::= "0" ... "9"
```

# 编译

| 宏                  | 描述                              |
| ------------------- | --------------------------------- |
| LOXJ_OPTIONS_ESCAPE | 启用字符串字面量转义              |
| LOXJ_OPTIONS_SLEEP  | 启用跨平台内置函数 sleep(seconds) |

```
$ make
```

下面仅说明 WASM 编译目标。

## [emscripten](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html)

使用 emscripten 将 `InterpretResult try_loxj(char *code)` 函数编译为 WASM。

```sh
emcc src/*.c -o bin/index.js -sEXPORTED_FUNCTIONS=_try_loxj -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
```

```js
Module.onRuntimeInitialized = () => {
    const try_loxj = Module.cwrap("try_loxj", "number", ["string"]); // 将函数包装为 JavaScript 函数

    globalThis.interpret = try_loxj; // 把这个函数放到别处使用
};

// 对于输入输出，需调整 emscripten 生成的 Module 对象
button.onclick = () => {
    interpret(`print "Hello, LoxJ!";`);
};
```

## WASI

支持使用 [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) 编译为 WASI。

```sh
$CC src/*.c -o bin/loxj.wasm
```

需要在任意 [WASI 运行时](https://wasi.dev/#how-to-get-started) 中运行。

JavaScript 环境，如 [Node.js](https://nodejs.org/api/wasi.html)/Bun/[Deno](https://deno.land/std@0.206.0/wasi) 内置 WASI 支持；  
亦可通过其它库来运行，如：[@wasmer/wasi](https://www.npmjs.com/package/@wasmer/wasi)

# Others

For a real scripting language, you may prefer [wren-lang](https://github.com/wren-lang/wren).
