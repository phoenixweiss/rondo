CXX ?= c++
PKG_CONFIG ?= pkg-config
BUILD ?= build
CPPFLAGS += -Isrc -Ivendor $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf)
CXXFLAGS ?= -O2 -g -std=c++17 -Wall -Wextra -Wpedantic
LDLIBS += $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf)
ifeq ($(shell uname),Linux)
LDLIBS += -ldl
endif
SOURCES = src/model.cpp src/platform.cpp src/ui.cpp src/core.cpp src/app.cpp src/wifi.cpp src/wifi_ui.cpp src/main.cpp
all: $(BUILD)/rondo
$(BUILD)/rondo: $(SOURCES) src/model.hpp src/ui.hpp src/core.hpp src/app.hpp src/platform.hpp src/wifi.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
$(BUILD)/model-tests: tests/model_tests.cpp src/model.cpp src/platform.cpp src/model.hpp src/platform.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/model_tests.cpp src/model.cpp src/platform.cpp $(LDFLAGS) $(LDLIBS) -o $@
test: $(BUILD)/model-tests
	$(BUILD)/model-tests assets
.PHONY: all test
$(BUILD)/core-tests: tests/core_tests.cpp src/model.cpp src/platform.cpp src/core.cpp src/core.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/core_tests.cpp src/model.cpp src/platform.cpp src/core.cpp $(LDFLAGS) $(LDLIBS) -o $@
$(BUILD)/device-tests: tests/device_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp src/core.cpp src/app.cpp src/wifi.cpp src/wifi_ui.cpp src/app.hpp src/wifi.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/device_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp src/core.cpp src/app.cpp src/wifi.cpp src/wifi_ui.cpp $(LDFLAGS) $(LDLIBS) -o $@
$(BUILD)/volume-ui-tests: tests/volume_ui_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp src/ui.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/volume_ui_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/timezone-tests: tests/timezone_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp src/core.cpp src/app.cpp src/wifi.cpp src/wifi_ui.cpp src/model.hpp src/app.hpp src/wifi.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/timezone_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp src/core.cpp src/app.cpp src/wifi.cpp src/wifi_ui.cpp $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/battery-tests: tests/battery_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp src/ui.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/battery_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/wifi-tests: tests/wifi_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp src/core.cpp src/app.cpp src/wifi.cpp src/wifi_ui.cpp src/wifi.hpp src/app.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/wifi_tests.cpp src/model.cpp src/platform.cpp src/ui.cpp src/core.cpp src/app.cpp src/wifi.cpp src/wifi_ui.cpp $(LDFLAGS) $(LDLIBS) -o $@
