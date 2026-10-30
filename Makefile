CXX ?= c++
PKG_CONFIG ?= pkg-config
BUILD ?= build
CPPFLAGS += -Isrc -Ivendor $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf)
CXXFLAGS ?= -O2 -g -std=c++17 -Wall -Wextra -Wpedantic
LDLIBS += $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf)
ifeq ($(shell uname),Linux)
LDLIBS += -ldl
endif
H700_SYSTEM_LIBS ?= -ldl -lpthread -lstdc++fs

BASE = src/preferences.cpp src/model.cpp src/platform.cpp src/psx.cpp src/state.cpp src/rumble.cpp src/restore.cpp
UI = $(BASE) src/artwork.cpp src/ui.cpp
APP = $(UI) src/backup.cpp src/backup_ui.cpp src/progress.cpp src/progress_ui.cpp src/library_ui.cpp src/navigation.cpp src/game_statistics.cpp src/screenshots.cpp src/screenshot_ui.cpp src/core.cpp src/app.cpp src/preferences_ui.cpp src/wizard_ui.cpp src/input_test_ui.cpp src/clock.cpp src/clock_ui.cpp src/wifi.cpp src/wifi_ui.cpp src/battery_log.cpp src/bluetooth.cpp src/bluetooth_ui.cpp src/system_info.cpp src/system_info_ui.cpp
NATIVE_TESTS = progress backup backup-ui statistics library screenshot preferences artwork input rumble state psx system-info md snes gba gb bluetooth battery-log model core device volume-ui timezone battery wifi clock
H700_TESTS = progress backup backup-ui statistics library screenshot preferences model artwork input rumble state psx system-info md snes gba gb bluetooth battery-log h700-device device timezone battery wifi h700-wifi clock h700-clock

all: $(BUILD)/rondo
native-programs: all $(addprefix $(BUILD)/,$(addsuffix -tests,$(NATIVE_TESTS)))
h700-programs: all $(addprefix $(BUILD)/,$(addsuffix -tests,$(H700_TESTS)))
test: $(BUILD)/model-tests
	$(BUILD)/model-tests assets

# One object per translation unit and compatible compile configuration. Separate
# BUILD directories keep cross/native and independent reproducibility runs apart.
# Compare the full command configuration, including a switch back to old flags.
quote = '$(subst ','"'"',$(1))'
$(BUILD)/.compile-config: FORCE
	@mkdir -p $(BUILD)
	@printf '%s\n' $(call quote,$(CXX)) $(call quote,$(CPPFLAGS)) $(call quote,$(CXXFLAGS)) > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
$(BUILD)/.link-config: FORCE
	@mkdir -p $(BUILD)
	@printf '%s\n' $(call quote,$(CXX)) $(call quote,$(CXXFLAGS)) $(call quote,$(LDFLAGS)) $(call quote,$(LDLIBS)) $(call quote,$(H700_SYSTEM_LIBS)) > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

$(BUILD)/obj/%.o: %.cpp $(BUILD)/.compile-config Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Keep source ordering and adapter-specific --wrap flags at link time.
define program
$(BUILD)/$(1): $(addprefix $(BUILD)/obj/,$(2:.cpp=.o)) $(BUILD)/.link-config Makefile
	$$(CXX) $$(CXXFLAGS) $(addprefix $(BUILD)/obj/,$(2:.cpp=.o)) $$(LDFLAGS) $(3) -o $$@
OBJECTS += $(addprefix $(BUILD)/obj/,$(2:.cpp=.o))
endef
$(eval $(call program,backup-tests,tests/backup_tests.cpp $(BASE) src/backup.cpp src/progress.cpp src/game_statistics.cpp,$$(LDLIBS)))
$(eval $(call program,backup-ui-tests,tests/backup_ui_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,progress-tests,tests/progress_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,battery-log-tests,tests/battery_log_tests.cpp $(BASE) src/battery_log.cpp,$$(LDLIBS)))
$(eval $(call program,input-tests,tests/input_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,psx-tests,tests/psx_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,system-info-tests,tests/system_info_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,md-tests,tests/md_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,snes-tests,tests/snes_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,gba-tests,tests/gba_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,gb-tests,tests/gb_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,rondo,$(APP) src/main.cpp,$$(LDLIBS)))
$(eval $(call program,model-tests,tests/model_tests.cpp $(BASE),$$(LDLIBS)))
$(eval $(call program,core-tests,tests/core_tests.cpp $(BASE) src/core.cpp,$$(LDLIBS)))
$(eval $(call program,device-tests,tests/device_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,volume-ui-tests,tests/volume_ui_tests.cpp $(UI),$$(LDLIBS)))
$(eval $(call program,timezone-tests,tests/timezone_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,battery-tests,tests/battery_tests.cpp $(UI),$$(LDLIBS)))
$(eval $(call program,wifi-tests,tests/wifi_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,bluetooth-tests,tests/bluetooth_tests.cpp $(APP),$$(LDLIBS)))
$(eval $(call program,clock-tests,tests/clock_tests.cpp $(APP),$$(LDLIBS)))
# Commas in linker options must not split the make function arguments.
comma := ,
$(eval $(call program,h700-device-tests,tests/h700_device_tests.cpp $(BASE),$$(H700_SYSTEM_LIBS) -Wl$$(comma)--wrap=ioctl$$(comma)--wrap=write))
$(eval $(call program,h700-wifi-tests,tests/h700_wifi_tests.cpp $(BASE) src/wifi.cpp,$$(H700_SYSTEM_LIBS) -Wl$$(comma)--wrap=posix_spawn))
$(eval $(call program,h700-clock-tests,tests/h700_clock_tests.cpp $(BASE) src/clock.cpp,$$(H700_SYSTEM_LIBS) -Wl$$(comma)--wrap=adjtimex$$(comma)--wrap=clock_settime$$(comma)--wrap=clock_gettime$$(comma)--wrap=ioctl))


$(eval $(call program,artwork-tests,tests/artwork_tests.cpp $(UI),$$(LDLIBS)))
$(eval $(call program,artwork-benchmark,tests/artwork_performance.cpp $(UI),$$(LDLIBS)))
$(eval $(call program,state-tests,tests/state_tests.cpp $(BASE),$$(LDLIBS)))
$(eval $(call program,rumble-tests,tests/rumble_tests.cpp $(BASE) src/core.cpp,$$(LDLIBS)))
$(BUILD)/rumble-fixture.so: tests/rumble_fixture.cpp $(BUILD)/.compile-config $(BUILD)/.link-config
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -shared -fPIC $< $(LDFLAGS) -o $@
$(BUILD)/rumble-tests: $(BUILD)/rumble-fixture.so
$(eval $(call program,psx-benchmark,tests/psx_performance.cpp $(BASE) src/core.cpp,$$(LDLIBS)))

$(eval $(call program,state-benchmark,$(APP) tests/state_performance.cpp,$$(LDLIBS)))

.DELETE_ON_ERROR:
.PHONY: all native-programs h700-programs test FORCE
FORCE:

$(eval $(call program,preferences-tests,tests/preferences_tests.cpp $(APP),$$(LDLIBS)))

$(eval $(call program,screenshot-tests,tests/screenshot_tests.cpp $(APP),$$(LDLIBS)))
$(BUILD)/screenshot-tests: $(BUILD)/rumble-fixture.so

$(eval $(call program,library-tests,tests/library_tests.cpp $(APP),$$(LDLIBS)))

$(eval $(call program,statistics-tests,tests/statistics_tests.cpp $(BASE) src/game_statistics.cpp,$$(LDLIBS)))

# Include dependencies only after every program has registered its objects.
-include $(sort $(OBJECTS:.o=.d))
