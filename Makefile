TARGET = slice-sequencer

# Sources
CPP_SOURCES = ./main.cpp ./sequence.cpp ./cv_mods.cpp ./playback.cpp ./oled_ui.cpp ./main_menu.cpp ./edit_sequence.cpp ./cv_menu.cpp ./storage_fs.cpp ./storage_seq.cpp ./save_menu.cpp ./load_menu.cpp ./new_menu.cpp ./patterns.cpp ./patterns_menu.cpp ./settings_menu.cpp

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
