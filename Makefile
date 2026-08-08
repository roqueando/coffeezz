CC       ?= cc
GLFW_PREFIX = /opt/homebrew/opt/glfw

# ---- Compiler flags --------------------------------------------------
BASE_CFLAGS  = -std=c11 -Wall -Wextra -I$(GLFW_PREFIX)/include -I.
DEBUG_CFLAGS = $(BASE_CFLAGS) -g3 -O0
REL_CFLAGS   = $(BASE_CFLAGS) -O2 -DNDEBUG -Wno-unused-but-set-variable

# ---- Linker flags ----------------------------------------------------
APP_LDFLAGS  = -L$(GLFW_PREFIX)/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit
TEST_LDFLAGS = -lm

# ---- Directories -----------------------------------------------------
DEBUG_DIR = build/debug
REL_DIR   = build/release
DIST_D    = dist/debug
DIST_R    = dist/release

BINARY = coffeezz

# ---- Main app sources -------------------------------------------------
TOOL_SRCS = $(wildcard tools/*.c) $(wildcard tools/*/*.c)
SRCS      = main.c ui_infra.c $(TOOL_SRCS)

# Object files per variant
OBJS_D = $(SRCS:%.c=$(DEBUG_DIR)/%.o)
OBJS_R = $(SRCS:%.c=$(REL_DIR)/%.o)

# ---- Test sources -----------------------------------------------------
TEST_DIR        = tests
TEST_NK_IMPL    = $(TEST_DIR)/nuklear_impl.c
TEST_UI_INFRA   = $(TEST_DIR)/test_ui_infra.c
TEST_REGISTRY   = $(TEST_DIR)/test_tool_registry.c

TEST_OBJS = $(TEST_NK_IMPL:.c=.o) $(TEST_UI_INFRA:.c=.o) $(TEST_REGISTRY:.c=.o)

# ---- Targets ----------------------------------------------------------
.PHONY: all clean test debug release

all: debug

debug: $(DIST_D)/$(BINARY)

release: $(DIST_R)/$(BINARY)

# ---- Debug binary ----------------------------------------------------
$(DIST_D)/$(BINARY): $(OBJS_D)
	@mkdir -p $(DIST_D)
	$(CC) $(DEBUG_CFLAGS) -o $@ $^ $(APP_LDFLAGS)

$(DEBUG_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(DEBUG_CFLAGS) -c -o $@ $<

# ---- Release binary ---------------------------------------------------
$(DIST_R)/$(BINARY): $(OBJS_R)
	@mkdir -p $(DIST_R)
	$(CC) $(REL_CFLAGS) -o $@ $^ $(APP_LDFLAGS)

$(REL_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(REL_CFLAGS) -c -o $@ $<

# ---- Test binaries ----------------------------------------------------
test_ui_infra: $(DEBUG_DIR)/ui_infra.o $(TEST_NK_IMPL:.c=.o) $(TEST_UI_INFRA:.c=.o)
	$(CC) $(DEBUG_CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

test_tool_registry: $(DEBUG_DIR)/ui_infra.o $(DEBUG_DIR)/tools/tool_registry.o $(TEST_NK_IMPL:.c=.o) $(TEST_REGISTRY:.c=.o)
	$(CC) $(DEBUG_CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

# Test objects also go through the pattern rules below (they're compiled without NK_IMPLEMENTATION)
%.o: %.c
	$(CC) $(DEBUG_CFLAGS) -c -o $@ $<

# ---- Run tests --------------------------------------------------------
test: test_ui_infra test_tool_registry
	@echo "=== Running ui_infra tests ==="
	./test_ui_infra
	@echo ""
	@echo "=== Running tool_registry tests ==="
	./test_tool_registry
	@echo ""
	@echo "All test suites passed."

# ---- Clean ------------------------------------------------------------
clean:
	rm -rf $(DIST_D) $(DIST_R) $(DEBUG_DIR) $(REL_DIR) $(TEST_OBJS) test_ui_infra test_tool_registry
