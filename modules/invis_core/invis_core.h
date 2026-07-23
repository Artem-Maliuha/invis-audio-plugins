/*
  BEGIN_JUCE_MODULE_DECLARATION

  ID:               invis_core
  vendor:           Invis Audio
  version:          1.0.0
  name:             Invis Audio Core Module
  description:      Shared UI Atoms, Functional Modules, Design System, and DSP Primitives
  license:          Proprietary
  dependencies:     juce_audio_basics juce_audio_processors juce_audio_utils juce_core juce_data_structures juce_events juce_graphics juce_gui_basics juce_gui_extra juce_dsp

  END_JUCE_MODULE_DECLARATION
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Design System
#include "design_system/InvisTheme.h"
#include "design_system/InvisThemeSupplier.h"

// UI Atoms
#include "ui_atoms/InvisCellSelector.h"
#include "ui_atoms/InvisKnob.h"
#include "ui_atoms/InvisLED.h"
#include "ui_atoms/InvisLEDMeter.h"

// Functional Modules
#include "functional_modules/input_filter/InputFilterDSP.h"
#include "functional_modules/input_filter/InputFilterUI.h"
#include "functional_modules/oversampling/OversamplingDSP.h"
#include "functional_modules/midi_learn/MidiLearnModule.h"
