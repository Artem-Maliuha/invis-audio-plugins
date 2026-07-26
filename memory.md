# Invis Audio Plugins - Project Memory & Architecture Log

## Project Goal
Design and build a modular C++ (strictly JUCE 9) audio plugin monorepo for AU/VST3/CLAP/Standalone plugins, focusing on reusable UI components, functional audio-UI modules, and a flexible cascading design system.

---

## Mandatory Universal Requirements for All Plugins

1. **Resizable UI & Proportional Layout Scaling Standard**:
   - Always call `setResizable(true, true)` in `PluginEditor` and maintain a fixed aspect ratio (`getConstrainer()->setFixedAspectRatio(...)`).
   - All visual elements, UI controls, module panels, borders, and ALL text labels (titles, values, tick marks) MUST scale proportionally with component bounds during resizing.
   - **NEVER hardcode static font sizes** (e.g. `Font(11.0f)`) or static pixel offsets in `paint()` or `resized()`. Compute all font sizes dynamically relative to bounds (e.g. `bounds.getHeight() * ratio` or `radius * ratio`).
   - **NEVER hardcode static pixel bounds for text labels** (e.g. `Rectangle(x, y, 28, 14)`). Bounding box dimensions MUST scale dynamically with the active font size (e.g. `rectWidth = fontSize * 3.8f`, `rectHeight = fontSize * 1.4f`) to prevent text truncation ("OFF" -> "O") or overlaps.
   - **Proportional Spacing & Proximity Standard (Стандарт відступів та пропорцій)**:
     - Header Title Area: $12\%$ of component height (`titleHeight = bounds.getHeight() * 0.12f`, font size $65\%$).
     - Value Text Field Area: Compact $10\%$ of component height (`valueHeight = std::clamp(bounds.getHeight() * 0.10f, 12.0f, 20.0f)`).
     - Control Body Diameter: $58\%$ of remaining knob bounds (`diameter = minArea * 0.58f`).
     - Value Label Proximity: Value text MUST sit directly below the control body with minimal gap ($\le 3\text{px}$) to eliminate dead space and keep controls tightly grouped.
   - **Centered Responsive Layout**: Sub-modules and container panels MUST scale dynamically with window bounds and stay centered (`withSizeKeepingCentre()`) or fill proportional grid cells without leaving unwanted asymmetric gaps.
   - **Strict Proportionality Integrity Policy for Base UI Atoms (Заборона спотворення пропорцій базових елементів)**:
     - NEVER scale, stretch, compress, or squish base UI atoms (`InvisKnob`, `InvisLEDMeter`, `InvisSwitch`, `InvisLED`) into unnatural or arbitrary aspect ratios to fit cramped layout spaces.
     - UI atoms MUST ALWAYS strictly maintain their predefined design system proportions and size presets (`InvisKnobSize::XS`, `S`, `M`, `L`, `XL`). If layout space is tight, expand container bounds or reorganize component positioning, NEVER deform base UI atoms.

2. **Dual Oversampling Engine (Online vs Offline)**:
   - Every plugin MUST include an oversampling module supporting independent settings for **Online** (realtime playback) and **Offline** (DAW bounce/export).
   - Default setting is **Off (1x)**.
   - Automatically switches quality based on `AudioProcessor::isNonRealtime()`.

3. **Full DAW Automation Support**:
   - Complete integration with DAW automation modes (Read, Write, Touch, Latch).
   - All UI interactions must trigger `beginChangeGesture()` and `endChangeGesture()` on parameters to allow proper DAW gesture recording.

4. **Assignable MIDI Learn**:
   - Built-in parameter-level MIDI Learn manager in every plugin for CC mapping.

5. **Volumetric Light & Emissive Surface Interaction Standard**:
   - Any light-emitting UI element (LED slots, active track arcs, meters, status indicators, power LEDs) MUST NOT look flatly drawn on top of surfaces.
   - All LEDs MUST implement one of two physical hardware mounting structures:
     1. **Recessed Housing & Slot Geometry (Заглиблений паз)**: Carved slot/channel with interior drop shadows and chamfered bevel edges on parent surfaces.
     2. **Protruding Dome / Bulb Through Chassis Hole (Виступний кристал крізь отвір)**: Translucent dome/cylinder protruding ABOVE the surface, fitted inside a drilled metal bezel ring/collar, casting a contact drop shadow onto the faceplate and projecting 360-degree volumetric light outward onto surrounding materials.
   - Must implement multi-layer optical depth (Shadow/bevel housing -> Surface glow reflection -> Translucent body -> Neon halo -> High-intensity phosphor core).

---

## Key Architectural Decisions

### 1. Framework Standard
- **STRICT REQUIREMENT**: Strictly **JUCE 9** framework standard across all plugins and modules.

### 2. Three-Tier Component Architecture
- **Tier 1: Primitives & Atoms (`ui_atoms`, `dsp_atoms`)**
  - **Stateless Presentational UI**: Pure UI controls (e.g., `InvisKnob`, `InvisSwitch`, `InvisMeter`) that do not know about JUCE `AudioProcessor`, `APVTS`, or DSP logic. They receive values via setters (`setValue`) and emit user events via C++ lambdas (`std::function<void(float)>`).
  - **`InvisLED` Atom & `LEDBallistics` Engine**: Dedicated presentational UI atom (`InvisLED.h` / `.cpp`) with physical mounting styles (`LEDMountType::RecessedSlot` vs `LEDMountType::ProtrudingDome`), luminescence attack flash spikes (`attackFlash = 1.35f`), and phosphorescent exponential decay envelopes (~80ms smooth fade out). Integrated into `InvisKnob`.
  - **`InvisKnob` Core Features**:
    - Procedural 3D metallic geometry: 32 directional knurled grip teeth, CNC lathe concentric micro-grooves, recessed shadow moat, and integrated `InvisLED` / `LEDBallistics` pointer capsule slot.
    - Full range limits (e.g. 20 Hz to 20 kHz) are 100% active and reachable. `OFF` position operates as a dedicated detent beyond active limits or via explicit state.
    - Magnetic Sticky Snap Points / Glue detents (`setStickyPositions` / `setStickyPoints`) for easy tactile locking to key scale ticks / center values (with Shift key precision bypass).
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
- [x] **3D Hybrid Photorealistic UI Standard**: Photorealistic 3D brushed metal chassis background (`dark_metal_panel.jpg`) combined with high-contrast emissive vector typography and responsive layout bounds.
- [x] **3D Filmstrip Engine**: Integrated `InvisKnob::setFilmstrip(image, numFrames, isVertical)` supporting 64/100/128-frame pre-rendered 3D sprite sheets with fixed studio specular light physics.
- [x] **Native Vector 3D Turned Titanium Shader**: High-precision vector lathe shader with 64 dense dark gunmetal micro-knurled teeth ($45^\circ$ studio light source), anisotropic metallic gradients, polished chamfer rim, and `InvisLED` phosphor dot pointer.
- [x] **InvisKnobSize Preset Standard**: Implemented 5 size presets (`XS`, `S`, `M` default, `L`, `XL`). Within each size preset, font sizes of titles/values, LED dot radius, and track stroke width remain fixed/constant, while component bounds dictate physical dial diameter.
- [x] **Universal Dual CLIP & DAW Metering Standard**: `InvisLEDMeter` with stationary segment columns, fixed 0 dB gold reference line, independent latching `CLIP L` / `CLIP R` lamps with click-reset physics, and channel mirroring.
- [x] **Universal `InputSidebar` Functional Module Standard**: Integrated `InputSidebarDSP` and `InputSidebarUI` into `modules/invis_core/functional_modules/input_sidebar`. Provides a slim, full-height vertical input column containing 4 mandatory components: `INPUT` Trim (`InvisKnobSize::XS`), `InvisLEDMeter`, `HPF` (`InvisKnobSize::XS`), and `LPF` (`InvisKnobSize::XS`).
- [x] **Universal Chassis (`InvisChassisDSP` / `InvisChassisUI`)**: The frame every plugin wears, assembled and wired. Fixed parameter prefixes, enforced stage order around the plugin's own algorithm, all sidebar callbacks bound to the DSP, and a self-driven 60 Hz pump so a plugin cannot forget to keep the panel alive. A plugin supplies one workspace component.
- [x] **`InvisConstellation` Star-Chart Editor**: Flagship element of the CONSTELLATION series. Topology derived from drawn links (`StarLink`), components cut at bridges into serial stages and parallel clusters, enclosure taken as the union of cycle interiors, hue-wheel colour mixing, click-armed link building, sensitivity as a pie fill inside a transparent core.
- [x] **Universal `OutputSidebar` Functional Module Standard**: Integrated `OutputSidebarDSP` and `OutputSidebarUI` into `modules/invis_core/functional_modules/output_sidebar`. Provides a slim, full-height vertical output column on the far right containing: `OUTPUT` Gain (`InvisKnobSize::XS` $-48\text{ dB} \dots +12\text{ dB}$) and Output Peak Meter (`InvisLEDMeter`).
