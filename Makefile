UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    CC := /mnt/c/w64devkit/bin/gcc.exe
    RAYLIB_INCLUDE := C:/raylib/include
    RAYLIB_LIB := C:/raylib/lib
else
    CC := gcc
    RAYLIB_INCLUDE := C:/raylib/include
    RAYLIB_LIB := C:/raylib/lib
endif

CFLAGS  := -std=c17 -Wall -I $(RAYLIB_INCLUDE)
LDFLAGS := -L $(RAYLIB_LIB) -lraylib -lopengl32 -lgdi32 -lwinmm

SRCS := $(shell find src -name "*.c")
OBJS := $(patsubst %.c,%.o,$(SRCS))
TARGET := build/game.exe

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf build
	rm -f $(OBJS)
	rm -f *.exe
