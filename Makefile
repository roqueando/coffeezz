CC       ?= cc
GLFW_PREFIX = /opt/homebrew/opt/glfw
CFLAGS   += -std=c11 -Wall -Wextra -I$(GLFW_PREFIX)/include -I.
CFLAGS 	+= -g3 -O0
LDFLAGS  += -L$(GLFW_PREFIX)/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit

TARGET    = nuklear_app

# Auto-discover tool sources: infrastructure at tools/ root, then each tool subdirectory.
TOOL_SRCS = $(wildcard tools/*.c) $(wildcard tools/*/*.c)
SRCS      = main.c ui_infra.c $(TOOL_SRCS)
OBJS      = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
