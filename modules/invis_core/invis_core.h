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
#include "design_system/InvisLayout.h"
#include "design_system/InvisMotion.h"
#include "design_system/InvisFonts.h"
#include "design_system/InvisEffectPalette.h"
#include "design_system/InvisPopupLookAndFeel.h"

// Framework (shared machine-driven behaviour)
#include "framework/InvisAutoRoutine.h"

// UI Atoms
#include "ui_atoms/InvisButton.h"
#include "ui_atoms/InvisCellSelector.h"
#include "ui_atoms/InvisKnob.h"
#include "ui_atoms/InvisLED.h"
#include "ui_atoms/InvisLevelLamp.h"
#include "ui_atoms/InvisLEDMeter.h"
#include "ui_atoms/InvisConstellation.h"
#include "ui_atoms/InvisSeparator.h"
#include "ui_atoms/InvisStepperField.h"

// Functional Modules
#include "functional_modules/InvisLoudnessAnalyser.h"
#include "functional_modules/InvisPresetTree.h"
#include "functional_modules/InvisSidebarLayout.h"
#include "functional_modules/input_filter/InputFilterDSP.h"
#include "functional_modules/input_filter/InputFilterUI.h"
#include "functional_modules/input_sidebar/InputSidebarDSP.h"
#include "functional_modules/input_sidebar/InputSidebarUI.h"
#include "functional_modules/output_sidebar/OutputSidebarDSP.h"
#include "functional_modules/output_sidebar/OutputSidebarUI.h"
#include "functional_modules/top_sidebar/TopSidebarDSP.h"
#include "functional_modules/top_sidebar/TopSidebarUI.h"

// The chassis: the frame every plugin wears, wired. Include LAST - it composes the sidebars above.
// Effects: the algorithms, the block around them, and the engine that runs the chart.
#include "effects/InvisEffect.h"
#include "effects/InvisAlgorithms.h"
#include "effects/InvisEffectSlot.h"
#include "effects/InvisConstellationEngine.h"

#include "functional_modules/constellation/ConstellationWorkspace.h"

#include "chassis/InvisChassisDSP.h"
#include "chassis/InvisChassisUI.h"
#include "functional_modules/oversampling/OversamplingDSP.h"
#include "functional_modules/midi_learn/MidiLearnModule.h"
