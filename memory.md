# Invis Audio Plugins - Project Memory & Architecture Log

## Project Goal
Design and build a modular C++ (strictly JUCE 9) audio plugin monorepo for AU/VST3/CLAP/Standalone plugins, focusing on reusable UI components, functional audio-UI modules, and a flexible cascading design system.

---

## Mandatory Universal Requirements for All Plugins

1. **Fully Resizable UI & Aspect Ratio Preservation**:
   - Every plugin window MUST be resizable (`setResizable(true, true)`).
   - UI elements, modules, knobs, borders, and ALL text labels (titles, values, tick marks) MUST scale proportionally with component bounds during window resizing.
   - NEVER use hardcoded font sizes (e.g. `11.0f`) or static pixel offsets in `paint()` or `resized()`. Always compute font sizes dynamically relative to bounds height/width.
   - **CRITICAL**: NEVER hardcode static pixel bounding boxes for text labels (e.g. `Rectangle(..., 28, 14)`) or static radial offsets (e.g. `radius + 8.0f`). Compute `labelRect` width/height dynamically based on font size (`fontSize * 3.5f`) and radial distance relative to `radius` to prevent text truncation or label overlapping during UI scaling.

2. **Dual Oversampling Engine (Online vs Offline)**:
   - Every plugin MUST include an oversampling module supporting independent settings for **Online** (realtime playback) and **Offline** (DAW bounce/export).
   - Default setting is **Off (1x)**.
   - Automatically switches quality based on `AudioProcessor::isNonRealtime()`.

3. **Full DAW Automation Support**:
   - Complete integration with DAW automation modes (Read, Write, Touch, Latch).
   - All UI interactions must trigger `beginChangeGesture()` and `endChangeGesture()` on parameters to allow proper DAW gesture recording.

4. **Assignable MIDI Learn**:
   - Built-in parameter-level MIDI Learn manager in every plugin for CC mapping.

---

## Key Architectural Decisions

### 1. Framework Standard
- **STRICT REQUIREMENT**: Strictly **JUCE 9** framework standard across all plugins and modules.

### 2. Three-Tier Component Architecture
- **Tier 1: Primitives & Atoms (`ui_atoms`, `dsp_atoms`)**
  - **Stateless Presentational UI**: Pure UI controls (e.g., `InvisKnob`, `InvisSwitch`, `InvisMeter`) that do not know about JUCE `AudioProcessor`, `APVTS`, or DSP logic. They receive values via setters (`setValue`) and emit user events via C++ lambdas (`std::function<void(float)>`).
  - **`InvisKnob` Core Features**:
    - Procedural 3D metallic geometry: 32 directional knurled grip teeth, CNC lathe concentric micro-grooves, recessed shadow moat, and multi-pass halo LED capsule indicator slot.
    - Full range limits (e.g. 20 Hz to 20 kHz) are 100% active and reachable. `OFF` position operates as a dedicated detent beyond active limits or via explicit state.
    - Optional angle sweep range in degrees: `setAngleRange(float startDegrees, float endDegrees)` (default: 220° to 500°).
    - Scale ticks and labels around the knob passed via `std::vector<ScaleTick>` or `juce::StringArray`.
    - Scale curve mapping support: `Linear`, `Logarithmic`, and `InverseLogarithmic`.
    - Double-click resets knob to `defaultValue`.
    - Clicking value text opens inline keyboard text editor (`juce::TextEditor`) for numeric / "OFF" entry.
  - **DSP Primitives**: Mathematical algorithms, filters (BiQuad, IIR), parameter scaling (logarithmic/linear skew ranges).

- **Tier 2: Functional Modules (`functional_modules`)**
  - Combines DSP processing and UI containers into a single reusable unit (e.g., `InputFilterModule`, `OversamplingModule`, `MidiLearnModule`).
  - **DSP Component** (e.g., `InputFilterDSP`): Processes audio buffers (`processBlock`), defines parameter layouts (`addParameters`), handles log/lin scale configs.
  - **UI Component** (e.g., `InputFilterUI`): Container that embeds UI atoms (e.g., 2 `InvisKnob`s for HPF/LPF), binds them to `APVTS` parameters via attachments, and renders response graphs if applicable.
  - **Plug-and-play**: Can be plugged into any plugin target with ~3 lines of code.

- **Tier 3: Plugin Targets (`plugins/*`)**
  - Concrete plugins (e.g., `InvisSaturator`, `InvisCompressor`) that compose multiple functional modules together.

---

### 3. Cascading Design System & Theme Overrides

**Hierarchy Cascade**:
$$\text{DesignSystem} \longrightarrow \text{UI Atoms} \longrightarrow \text{Functional Module (1)} \longrightarrow \text{Nested Module (N)} \longrightarrow \text{Plugin Target}$$

- **`InvisTheme` Tokens**: Contains colors, geometry, font definitions, and track dimensions.
- **Theme Overrides**:
  - Global defaults provided by `DesignSystem`.
  - Plugins, Modules (Level 1..N), or individual UI Atoms can override any subset of tokens via `InvisTheme::withAccent(...)`, `setThemeOverride(...)`, or `setModuleTheme(...)`.
  - Effective theme resolution cascades upwards to the nearest parent container or falls back to global `DesignSystem` defaults.

---

### 4. Repository Directory Layout

```
invis-audio-plugins/
├── CMakeLists.txt                 # Master CMake configuration (Strict JUCE 9 setup)
├── memory.md                      # Persistent project memory and architecture decisions
├── agents.md                      # AI agent guidelines & workflow rules
├── modules/
│   └── invis_core/                # Shared C++/JUCE module
│       ├── invis_core.h
│       ├── invis_core.cpp
│       ├── design_system/         # InvisTheme, InvisLookAndFeel, Design Tokens
│       ├── ui_atoms/              # Stateless controls (InvisKnob, InvisSwitch, InvisMeter)
│       ├── dsp_atoms/             # Math & filter primitives
│       └── functional_modules/    # Reusable DSP+UI modules (InputFilter, Oversampling, MidiLearn)
└── plugins/                       # Individual plugin targets
    ├── InvisSaturator/
    └── ...
```

---

## Log of Completed Architectural Alignments
- [x] Added 4 mandatory universal requirements (Resizable UI, Online/Offline Oversampling, DAW Automation gestures, Assignable MIDI Learn).
- [x] Fixed strict requirement: JUCE 9 ONLY.
- [x] Defined Stateless vs Container component separation.
- [x] Defined Functional Modules concept combining DSP engine + UI container.
- [x] Defined Design System cascade hierarchy (`DesignSystem -> UI Atoms -> Modules(1..N) -> Plugin`).
- [x] Created `memory.md` and `agents.md` for project memory persistence.
