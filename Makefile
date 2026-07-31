CC       ?= cc
GLFW_PREFIX = /opt/homebrew/opt/glfw
CFLAGS   += -std=c11 -Wall -Wextra -O2 -I$(GLFW_PREFIX)/include
LDFLAGS  += -L$(GLFW_PREFIX)/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit

TARGET    = nuklear_app
SRCS      = main.c ui_infra.c
OBJS      = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c nuklear.h nuklear_glfw_gl3.h ui_infra.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
