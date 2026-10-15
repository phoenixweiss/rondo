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
.PHONY: all test
