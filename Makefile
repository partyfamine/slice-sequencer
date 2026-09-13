TARGET = slice-sequencer

# Sources
CPP_SOURCES = ./main.cpp ./sequence.cpp ./oled_ui.cpp ./main_menu.cpp ./edit_sequence.cpp ./cv_menu.cpp

# Library Locations
LIBDAISY_DIR = ./libDaisy
DAISYSP_DIR = ./daisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile