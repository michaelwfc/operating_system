# Building the Project
# To build the project, open a terminal in the project directory and run: 
# make
# This will compile the source files into object files and link them to create the executable my_program.


# Cleaning the Project
# To clean the project (remove object files and the executable), run:
# make clean
# This removes the compiled files, allowing you to start fresh if needed.


# Compiler flags  
# -Wall: Enables most compiler warnings to help identify potential issues in the code.
# -Wextra: Enables additional warnings that are not included with -Wall.
# -std=c11: Specifies that the compiler should use the C11 standard.
# -I.: Adds the current directory to the list of directories to be searched for header files.
# -Iinclude: Adds the include directory to the list of directories to be searched for header files.
# -Isrc/utils: Adds the src/utils directory to the list of directories to be searched for header files.
# The CFLAGS variable is updated to include the current directory (-I.). This tells the compiler to look for header files in the current directory.
# The -I option specifies an additional include directory. In this case, . refers to the current directory.

# Compiler to use
CC = gcc

CFLAGS = -g -Wall -Wextra -std=c11 -I../../src/utils

# Target executable name
TARGET = main

# Source files
SRCS  = main.c ../../src/utils/arrayUtils.c

# Object files: List of object files derived from the source files
OBJS = $(SRCS:.c=.o)

# Default target to build the executable
all: $(TARGET)

# Rule to link object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile each source file into an object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target to remove object files and the executable
clean:
	rm -f $(OBJS) $(TARGET)

# $(TARGET): $(SOURCES)
#     $(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# clean:
#     rm -f $(TARGET)