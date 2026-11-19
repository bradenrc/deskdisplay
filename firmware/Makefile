# Desktop simulator for the desk display UI.
#
#   make deps    fetch LVGL and ArduinoJson into .deps (once)
#   make         build build/deskdisplay-sim
#   make run     open the window with live data from http://localhost:8080
#   make demo    open the window with the canned data in fixtures/
#   make shots   render every page to build/shots/*.png
#
# Needs: a C/C++ compiler, SDL2 (macOS: brew install sdl2), libcurl.

LVGL_TAG   := v9.2.2
JSON_TAG   := v7.2.1
SERVER     ?= http://localhost:8080

BUILD      := build
BIN        := $(BUILD)/deskdisplay-sim
LVGL       := .deps/lvgl
JSON       := .deps/ArduinoJson

SDL_PREFIX := $(shell sdl2-config --prefix 2>/dev/null)
INCLUDES   := -I. -Iui -Itank -I$(LVGL) -I$(JSON)/src -I$(SDL_PREFIX)/include
DEFINES    := -DLV_CONF_INCLUDE_SIMPLE -DDESK_SIM
CFLAGS     := -O2 -g $(DEFINES) $(INCLUDES) -Wno-unused-parameter
CXXFLAGS   := -std=c++17 $(CFLAGS) -Wall
LDLIBS     := $(shell sdl2-config --libs 2>/dev/null) -lcurl -lm

LVGL_SRC   := $(shell find $(LVGL)/src -name '*.c' 2>/dev/null)
FONT_SRC   := $(wildcard ui/fonts/*.c) $(wildcard ui/*.c)
UI_SRC     := $(wildcard ui/*.cpp) sim/main.cpp
OBJS       := $(LVGL_SRC:%.c=$(BUILD)/%.o) $(FONT_SRC:%.c=$(BUILD)/%.o) $(UI_SRC:%.cpp=$(BUILD)/%.o)

.PHONY: all deps run demo shots clean
all: $(BIN)

deps:
	@mkdir -p .deps
	@[ -d $(LVGL) ] || git clone -q --depth 1 --branch $(LVGL_TAG) https://github.com/lvgl/lvgl.git $(LVGL)
	@[ -d $(JSON) ] || git clone -q --depth 1 --branch $(JSON_TAG) https://github.com/bblanchon/ArduinoJson.git $(JSON)
	@echo "dependencies ready"

$(BIN): $(OBJS)
	@$(CXX) -o $@ $(OBJS) $(LDLIBS)
	@echo "built $@"

$(BUILD)/%.o: %.c lv_conf.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp lv_conf.h $(wildcard ui/*.h) $(wildcard tank/*.h)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	$(BIN) --server $(SERVER)

demo: $(BIN)
	$(BIN) --fixtures fixtures

# Renders with canned data by default; `make shots LIVE=1` uses the server instead.
shots: $(BIN)
	@rm -rf $(BUILD)/shots
	@$(BIN) $(if $(LIVE),--server $(SERVER),--fixtures fixtures) --screenshot $(BUILD)/shots
	@for f in $(BUILD)/shots/*.bmp; do sips -s format png "$$f" --out "$${f%.bmp}.png" >/dev/null && rm "$$f"; done
	@ls $(BUILD)/shots

clean:
	rm -rf $(BUILD)
