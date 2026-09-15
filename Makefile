TARGET = slice-sequencer

# Sources
CPP_SOURCES = ./main.cpp ./sequence.cpp ./oled_ui.cpp ./main_menu.cpp ./edit_sequence.cpp ./cv_menu.cpp ./storage.cpp ./save_menu.cpp ./load_menu.cpp ./new_menu.cpp

# Enable FatFS middleware for SD card save/load
USE_FATFS = 1

# Library Locations
LIBDAISY_DIR = ./libDaisy
DAISYSP_DIR = ./daisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
