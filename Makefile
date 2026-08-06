CC       ?= cc
GLFW_PREFIX = /opt/homebrew/opt/glfw
CFLAGS   += -std=c11 -Wall -Wextra -I$(GLFW_PREFIX)/include -I.
CFLAGS 	+= -g3 -O0

# Linker flags for the main GUI app (needs GLFW, OpenGL, etc.)
APP_LDFLAGS = -L$(GLFW_PREFIX)/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit

# Linker flags for headless test binaries (no graphics frameworks)
TEST_LDFLAGS = -lm

TARGET    = coffeez

# ---- Main app sources ----
TOOL_SRCS = $(wildcard tools/*.c) $(wildcard tools/*/*.c)
SRCS      = main.c ui_infra.c $(TOOL_SRCS)
OBJS      = $(SRCS:.c=.o)

# ---- Test sources ----
TEST_DIR        = tests
TEST_NK_IMPL    = $(TEST_DIR)/nuklear_impl.c
TEST_UI_INFRA   = $(TEST_DIR)/test_ui_infra.c
TEST_REGISTRY   = $(TEST_DIR)/test_tool_registry.c

TEST_OBJS = $(TEST_NK_IMPL:.c=.o) $(TEST_UI_INFRA:.c=.o) $(TEST_REGISTRY:.c=.o)

.PHONY: all clean test

all: $(TARGET)

# ---- Main binary ----
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(APP_LDFLAGS)

# ---- Test binaries ----
test_ui_infra: ui_infra.o $(TEST_NK_IMPL:.c=.o) $(TEST_UI_INFRA:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

test_tool_registry: ui_infra.o tools/tool_registry.o $(TEST_NK_IMPL:.c=.o) $(TEST_REGISTRY:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

# ---- Run tests ----
test: test_ui_infra test_tool_registry
	@echo "=== Running ui_infra tests ==="
	./test_ui_infra
	@echo ""
	@echo "=== Running tool_registry tests ==="
	./test_tool_registry
	@echo ""
	@echo "All test suites passed."

# ---- Compile rules ----
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- Clean ----
clean:
	rm -f $(TARGET) $(OBJS) $(TEST_OBJS) test_ui_infra test_tool_registry
