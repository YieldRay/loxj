#ifndef loxj_compiler_h
#define loxj_compiler_h

#include "chunk.h"
#include "object.h"

ObjFunction *compile(const char *sourceCode);
void markCompilerRoots();

#endif