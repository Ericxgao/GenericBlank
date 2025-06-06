RACK_DIR ?= ../..

# DaisySP configuration
DAISYSP_DIR = dep/DaisySP
FLAGS += -I$(DAISYSP_DIR)/Source
FLAGS += -I$(DAISYSP_DIR)/Source/Control
FLAGS += -I$(DAISYSP_DIR)/Source/Drums
FLAGS += -I$(DAISYSP_DIR)/Source/Dynamics
FLAGS += -I$(DAISYSP_DIR)/Source/Effects
FLAGS += -I$(DAISYSP_DIR)/Source/Filters
FLAGS += -I$(DAISYSP_DIR)/Source/Noise
FLAGS += -I$(DAISYSP_DIR)/Source/PhysicalModeling
FLAGS += -I$(DAISYSP_DIR)/Source/Sampling
FLAGS += -I$(DAISYSP_DIR)/Source/Synthesis
FLAGS += -I$(DAISYSP_DIR)/Source/Utility

SOURCES += src/plugin.cpp
SOURCES += src/Blank.cpp
SOURCES += src/DaisyOscillator.cpp
SOURCES += src/Grains.cpp

# Add all DaisySP source files
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Control/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Drums/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Dynamics/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Effects/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Filters/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Noise/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/PhysicalModeling/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Sampling/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Synthesis/*.cpp)
SOURCES += $(wildcard $(DAISYSP_DIR)/Source/Utility/*.cpp)

DISTRIBUTABLES += res
# DISTRIBUTABLES += presets
# DISTRIBUTABLES += selections

# Include the VCV Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk