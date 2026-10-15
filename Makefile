CXX ?= c++
BUILD ?= build
CPPFLAGS += -Isrc -Ivendor
CXXFLAGS ?= -O2 -g -std=c++17 -Wall -Wextra -Wpedantic
LDLIBS += -pthread
ifeq ($(shell uname),Linux)
LDLIBS += -ldl
endif
COMMON = src/model.cpp src/platform.cpp
all: $(BUILD)/model-tests
$(BUILD)/model-tests: tests/model_tests.cpp $(COMMON) src/model.hpp src/platform.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/model_tests.cpp $(COMMON) $(LDFLAGS) $(LDLIBS) -o $@
test: all
	$(BUILD)/model-tests assets
.PHONY: all test ui-test
PKG_CONFIG ?= pkg-config
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf)
LDLIBS += $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf)
all: $(BUILD)/volume-ui-tests
$(BUILD)/volume-ui-tests: tests/volume_ui_tests.cpp $(COMMON) src/ui.cpp src/ui.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/volume_ui_tests.cpp $(COMMON) src/ui.cpp $(LDFLAGS) $(LDLIBS) -o $@
ui-test: all
	env -u RONDO_DEVICE_INPUT -u RONDO_BOOTLOGO_TOOL SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(BUILD)/volume-ui-tests assets $(BUILD)/volume-renders
