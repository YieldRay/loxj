# Tutorial
# https://riife.github.io/how-to-write-makefile/
# https://sourceforge.net/projects/ezwinports/

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -std=c99 -lm
# -Wall -Wextra -Werror

# Directories
SRC_DIR = ./src
OBJ_DIR = ./obj
BIN_DIR = ./bin

# Executable name
TARGET = $(BIN_DIR)/loxj

# Find all source files
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Generate object file names from source files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@

# Compile each source file into an object file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create the bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Create the obj directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean up the build
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Phony targets
.PHONY: all clean
