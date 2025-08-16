#pragma once
#include <rack.hpp>
using namespace ::rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file

extern Model* modelBlank;
extern Model* modelPonyVCO;
extern Model* modelDrumVoice;
extern Model* modelDrumSequencer;

// Move UI components and helpers to dedicated headers. Left here for backward compatibility if other files include them.
