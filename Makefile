CC      := gcc
CFLAGS  := -std=c17 -Wall -I C:/raylib/include
LDFLAGS := -L C:/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm

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
