TARGET = slice-sequencer

# Sources
CPP_SOURCES = \
./src/main.cpp \
./src/core/sequence.cpp \
./src/core/cv_mods.cpp \
./src/core/playback.cpp \
./src/core/patterns.cpp \
./src/ui/oled_ui.cpp \
./src/ui/main_menu.cpp \
./src/ui/edit_sequence.cpp \
./src/ui/cv_menu.cpp \
./src/ui/save_menu.cpp \
./src/ui/load_menu.cpp \
./src/ui/new_menu.cpp \
./src/ui/patterns_menu.cpp \
./src/ui/settings_menu.cpp \
./src/storage/storage_fs.cpp \
./src/storage/storage_seq.cpp

# Short includes: #include "sequence.h" etc. (not ../core/...)
C_INCLUDES += -Isrc/core -Isrc/ui -Isrc/storage

# Enable FatFS middleware for SD card save/load
USE_FATFS = 1

# Library Locations
LIBDAISY_DIR = ./libDaisy
DAISYSP_DIR = ./daisySP

# run from QSPI flash for additional space
APP_TYPE = BOOT_QSPI

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
