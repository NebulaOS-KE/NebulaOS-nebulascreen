#
# Makefile
#
ifdef CROSS_COMPILE
CC 	= $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
CPP = $(CC) -E
AS 	= $(CROSS_COMPILE)as
LD	= $(CROSS_COMPILE)ld
AR	= $(CROSS_COMPILE)ar
NM	= $(CROSS_COMPILE)nm
STRIP 	= $(CROSS_COMPILE)strip
endif

LVGL_DIR_NAME 	?= lvgl
LVGL_DIR 		?= .

WARNINGS		:= -Wall -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith \
					-fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess \
					-Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security -Wno-error=pedantic \
					-Wno-sign-compare -Wdouble-promotion -Wclobbered -Wempty-body -Wtype-limits -Wshift-negative-value \
					-Wno-unused-value -Wno-unused-parameter -Wno-missing-field-initializers -Wuninitialized -Wmaybe-uninitialized -Wall -Wextra -Wno-unused-parameter \
					-Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wpointer-arith -Wno-cast-qual \
					-Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security -Wno-sign-compare
CFLAGS 			?= -O3 -g0 -MD -MP -I$(LVGL_DIR)/ $(WARNINGS) 
LDFLAGS 		?= -static -lm -Llibhv/lib -Lspdlog/build -l:libhv.a -latomic -lpthread -lstdc++fs -l:libspdlog.a
BIN 			= nebulascreen
BUILD_DIR 		= ./build
BUILD_OBJ_DIR 	= $(BUILD_DIR)/obj
BUILD_BIN_DIR 	= $(BUILD_DIR)/bin
SPDLOG_DIR		= spdlog

prefix 			?= /usr
bindir 			?= $(prefix)/bin

#Collect the files to compile
MAINSRC = 		$(wildcard $(LVGL_DIR)/src/*.cpp)

include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

CSRCS 			+= $(wildcard $(LVGL_DIR)/assets/*.c)
CSRCS			+= $(wildcard $(LVGL_DIR)/lv_touch_calibration/*.c)

ASSET_DIR		= material
ifdef GUPPY_SMALL_SCREEN
ASSET_DIR		= material_46
DEFINES			+= -D GUPPY_SMALL_SCREEN
endif


ifdef GUPPY_ROTATE
DEFINES			+= -D GUPPY_ROTATE
endif


CSRCS 			+= $(wildcard $(LVGL_DIR)/assets/$(ASSET_DIR)/*.c)

# NebulaScreen (Run 15): NEBULASCREEN_VERSION; GUPPYSCREEN_VERSION is still
# accepted from older build scripts.
NEBULASCREEN_VERSION	?= $(GUPPYSCREEN_VERSION)
ifneq ($(NEBULASCREEN_VERSION),)
DEFINES			+= -D NEBULASCREEN_VERSION="\"${NEBULASCREEN_VERSION}\""
endif

OBJEXT 			?= .o

AOBJS 			= $(ASRCS:.S=$(OBJEXT))
COBJS 			= $(CSRCS:.c=$(OBJEXT))

MAINOBJ 		= $(MAINSRC:.cpp=$(OBJEXT))
DEPS                    = $(addprefix $(BUILD_OBJ_DIR)/, $(patsubst %.o, %.d, $(MAINOBJ)))

OBJS 			= $(AOBJS) $(COBJS) $(MAINOBJ)
TARGET 			= $(addprefix $(BUILD_OBJ_DIR)/, $(patsubst ./%, %, $(OBJS)))

INC 				:= -I./ -I./lvgl/ -I./lv_touch_calibration -I./spdlog/include -Ilibhv/include
LDLIBS	 			:= -lm

DEFINES				+= -D _GNU_SOURCE -DSPDLOG_COMPILED_LIB

ifdef EVDEV_CALIBRATE
DEFINES +=  -D EVDEV_CALIBRATE
endif

# SIMULATION is enabled by default, need CROSS_COMPILE variable to do MIPS build
ifndef CROSS_COMPILE
DEFINES +=  -D LV_BUILD_TEST=0 -D SIMULATOR
LDLIBS += -lSDL2
endif

COMPILE_CC				= $(CC) $(CFLAGS) $(INC) $(DEFINES)
COMPILE_CXX				= $(CC) $(CFLAGS) $(INC) $(DEFINES)

## MAINOBJ -> OBJFILES

all: default

libhv.a:
	$(MAKE) -C libhv -j$(nproc) libhv

libspdlog.a:
	@mkdir -p $(SPDLOG_DIR)/build
	@cmake -B $(SPDLOG_DIR)/build -S $(SPDLOG_DIR)/ -DCMAKE_CXX_COMPILER=$(CXX)
	$(MAKE) -C $(SPDLOG_DIR)/build -j$(nproc)

# Wi-Fi goes through NebulaOS (machine.nebulaos.wifi.*) since 2026-10-11; the
# wpa_supplicant client library is no longer linked. Kept as a no-op so older
# firmware build scripts that still call it keep working.
wpaclient:
	@true

## Offline, host-native pure-logic test binaries (2026-08-06) - no LVGL, no SDL2, no
## cross-compile: these always build with the plain host g++ regardless of CROSS_COMPILE,
## since they're meant to run right here, not on the printer. See tests/minitest.h and
## the matching tests/test_*.cpp.
.PHONY: test test-subscription-baseline-ordering test-config-theme-parse-safety test-plr-status test-nebulaos-display test-nebulaos-network test-nebulaos-macros test-nebulaos-calibration-flow test-nebulaos-system test-nebulaos-settings test-nebulaos-image

test: test-nebulascreen-paths test-subscription-baseline-ordering test-config-theme-parse-safety test-plr-status test-nebulaos-display test-nebulaos-network test-nebulaos-macros test-nebulaos-calibration-flow test-nebulaos-system test-nebulaos-settings test-nebulaos-image

# NebulaScreen rename (Run 15): device names and the legacy-config mapping.
test-nebulascreen-paths: libhv.a
	@mkdir -p build
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I. \
		src/nebulascreen_paths.cpp tests/test_nebulascreen_paths.cpp \
		-o build/test_nebulascreen_paths
	./build/test_nebulascreen_paths

# 2026-08-06 crash-fix regression test: Config::init()/ThemeConfig::init()
# must fall back to defaults, never crash, when a stat()-present file fails
# to actually parse (see tests/test_config_theme_parse_safety.cpp header).
test-config-theme-parse-safety: libhv.a libspdlog.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I./spdlog/include -I. -DSPDLOG_COMPILED_LIB \
		src/config.cpp src/nebulascreen_paths.cpp src/theme.cpp tests/test_config_theme_parse_safety.cpp \
		-Lspdlog/build -l:libspdlog.a -lstdc++fs \
		-o build/test_config_theme_parse_safety
	./build/test_config_theme_parse_safety

# NebulaOS power-loss recovery panel logic (src/plr_status.cpp): what the panel offers
# for each nebulaos_power_loss_recovery state, and the exact commands it sends.
test-plr-status: libhv.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I. \
		src/plr_status.cpp tests/test_plr_status.cpp \
		-o build/test_plr_status
	./build/test_plr_status

# NebulaOS display contract helpers (src/nebulaos_display.cpp): brightness levels and
# selection from machine.nebulaos.display.status, error messages.
test-nebulaos-display: libhv.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I. \
		src/nebulaos_display.cpp tests/test_nebulaos_display.cpp \
		-o build/test_nebulaos_display
	./build/test_nebulaos_display

# NebulaOS network contract helpers for the static IP panel (src/nebulaos_network.cpp).
test-nebulaos-network: libhv.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I. \
		src/nebulaos_network.cpp tests/test_nebulaos_network.cpp \
		-o build/test_nebulaos_network
	./build/test_nebulaos_network

# NebulaOS guided calibration panel logic (src/nebulaos_calibration_flow.cpp, D3-D5).
test-nebulaos-calibration-flow: libhv.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I. \
		src/nebulaos_calibration_flow.cpp tests/test_nebulaos_calibration_flow.cpp \
		-o build/test_nebulaos_calibration_flow
	./build/test_nebulaos_calibration_flow

# "Power & System" / Logs screen logic (src/nebulaos_system.cpp, D9).
test-nebulaos-system: libhv.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I. \
		src/nebulaos_system.cpp tests/test_nebulaos_system.cpp \
		-o build/test_nebulaos_system
	./build/test_nebulaos_system

# Connections, Wi-Fi and Updates screen logic (src/nebulaos_settings.cpp).
test-nebulaos-settings: libhv.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I. \
		src/nebulaos_settings.cpp tests/test_nebulaos_settings.cpp \
		-o build/test_nebulaos_settings
	./build/test_nebulaos_settings

# Settings > Updates > System image logic (src/nebulaos_image.cpp, 2026-10-14).
test-nebulaos-image: libhv.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I. \
		src/nebulaos_image.cpp tests/test_nebulaos_image.cpp \
		-o build/test_nebulaos_image
	./build/test_nebulaos_image

# GuppyScreen action -> NebulaOS macro mapping (src/nebulaos_macros.cpp).
test-nebulaos-macros:
	g++ -std=c++17 -Wall -Wextra -I. \
		src/nebulaos_macros.cpp tests/test_nebulaos_macros.cpp \
		-o build/test_nebulaos_macros
	./build/test_nebulaos_macros

# Part 3 deployment-verification regression: proves the subscription-baseline ordering
# guarantee against the REAL State class (src/state.cpp) - no LVGL runtime needed, see
# tests/test_subscription_baseline_ordering.cpp's own header comment.
test-subscription-baseline-ordering: libhv.a libspdlog.a
	g++ -std=c++17 -Wall -Wextra -I./libhv/include -I./spdlog/include -I. -I./lvgl \
		-DSPDLOG_COMPILED_LIB \
		src/state.cpp src/notify_consumer.cpp \
		tests/link_stubs_state_deps.cpp \
		tests/test_subscription_baseline_ordering.cpp \
		-Lspdlog/build -l:libspdlog.a \
		-o build/test_subscription_baseline_ordering
	./build/test_subscription_baseline_ordering

$(BUILD_OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(COMPILE_CXX) -std=c++17 $(CFLAGS) -c $< -o $@
	@echo "CXX $<"

$(BUILD_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(COMPILE_CC)  $(CFLAGS) -c $< -o $@
	@echo "CC $<"

default: $(TARGET)
	@mkdir -p $(dir $(BUILD_BIN_DIR)/)
	$(CXX) -o $(BUILD_BIN_DIR)/$(BIN) $(TARGET) $(LDFLAGS) $(LDLIBS)
	@echo "CXX $<"

spdlogclean:
	rm -rf $(SPDLOG_DIR)/build

libhvclean:
	$(MAKE) -C libhv clean

clean:
	rm -rf $(BUILD_DIR)

install:
	install -d $(DESTDIR)$(bindir)
	install $(BUILD_BIN_DIR)/$(BIN) $(DESTDIR)$(bindir)

uninstall:
	$(RM) -r $(addprefix $(DESTDIR)$(bindir)/,$(BIN))

build:
	$(MAKE) libhvclean
	$(MAKE) libhv.a
	$(MAKE) spdlogclean
	$(MAKE) libspdlog.a
	$(MAKE) clean
	$(MAKE) -j$(nproc)

-include			$(DEPS)
