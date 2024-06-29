#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void repl()
{
    size_t bufferSize = 1024;
    char *line = malloc(bufferSize);
    if (!line)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (;;)
    {
        printf("> ");
        fflush(stdout);

        size_t len = 0;

        while (fgets(line + len, bufferSize - len, stdin))
        {
            len += strlen(line + len);

            // Check if the newline character is present
            if (line[len - 1] == '\n')
            {
                line[len - 1] = '\0'; // Remove the newline character
                break;
            }

            // If the buffer is full, we need to resize it
            if (len == bufferSize - 1)
            {
                bufferSize *= 2;
                char *newLine = realloc(line, bufferSize);
                if (!newLine)
                {
                    perror("realloc");
                    free(line);
                    exit(EXIT_FAILURE);
                }
                line = newLine;
            }
        }

        // If fgets returns NULL, check for end-of-file or error
        if (ferror(stdin))
        {
            perror("Error reading line");
            free(line);
            exit(EXIT_FAILURE);
        }
        if (feof(stdin))
        {
            printf("\n");
            break;
        }

        interpret(line);
    }

    free(line);
}

/**
 * Remember to call free()
 */
static char *readFile(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "fopen(%s", path);
        perror(")");
        exit(74);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0)
    {
        perror("fseek");
        fclose(file);
        exit(74);
        return NULL;
    }

    long fileSize = ftell(file);
    if (fileSize == -1)
    {
        perror("ftell");
        fclose(file);
        exit(74);
        return NULL;
    }

    rewind(file);

    char *buffer = (char *)malloc(fileSize + 1);
    if (buffer == NULL)
    {
        perror("malloc");
        fclose(file);
        exit(74);
        return NULL;
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize && ferror(file))
    {
        fprintf(stderr, "fread(%s", path);
        perror(")");
        free(buffer);
        fclose(file);
        exit(74);
        return NULL;
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const char *path)
{
    char *sourceCode = readFile(path);
    InterpretResult result = interpret(sourceCode);
    free(sourceCode);

    if (result == INTERPRET_COMPILE_ERROR)
        exit(65);
    if (result == INTERPRET_RUNTIME_ERROR)
        exit(70);
}

int main(int argc, const char *argv[])
{
    initVM();

    if (argc == 1)
    {
        repl();
    }
    else if (argc == 2)
    {
        runFile(argv[1]);
    }
    else
    {
        fprintf(stderr, "Usage: %s [path]\n", argv[0]);
        exit(64);
    }

    freeVM();
    return EXIT_SUCCESS;
}

InterpretResult try_loxj(char *code)
{
    initVM();
    InterpretResult result = interpret(code);
    freeVM();
    return result;
}