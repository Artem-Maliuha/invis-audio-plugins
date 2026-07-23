# Guidelines for AI Agents (agents.md)

This file specifies rules and best practices for AI coding agents developing in the `invis-audio-plugins` repository.

---

## Code & Architecture Principles

### 0. Framework Standard
- **STRICT REQUIREMENT**: Strictly use **JUCE 9** for all CMake build configurations, plugin targets, and core modules.

### 1. Mandatory Universal Plugin Requirements
Every plugin developed in this repository MUST implement the following 4 features:
1. **Resizable UI & Proportional Layout Scaling Standard**:
   - Always call `setResizable(true, true)` in `PluginEditor` and maintain a fixed aspect ratio (`getConstrainer()->setFixedAspectRatio(...)`).
   - All visual elements, UI controls, module panels, borders, and ALL text labels (titles, values, tick marks) MUST scale proportionally with component bounds during resizing.
   - **NEVER hardcode static font sizes** (e.g. `Font(11.0f)`) or static pixel offsets in `paint()` or `resized()`. Compute all font sizes dynamically relative to bounds (e.g. `bounds.getHeight() * ratio` or `radius * ratio`).
   - **NEVER hardcode static pixel bounds for text labels** (e.g. `Rectangle(x, y, 28, 14)`). Bounding box dimensions MUST scale dynamically with the active font size (e.g. `rectWidth = fontSize * 3.8f`, `rectHeight = fontSize * 1.4f`) to prevent text truncation ("OFF" -> "O") or overlaps.
   - **Proportional Spacing & Proximity Standard**: Control titles, dial bodies, scale ticks, and value labels MUST follow strict proportional vertical allocations:
     - Header Title Area: $12\%$ of component height (`titleHeight = bounds.getHeight() * 0.12f`, font size $65\%$).
     - Value Text Field Area: Compact $10\%$ of component height (`valueHeight = std::clamp(bounds.getHeight() * 0.10f, 12.0f, 20.0f)`).
     - Control Body Diameter: $58\%$ of remaining knob bounds (`diameter = minArea * 0.58f`).
     - Value Label Proximity: Value text MUST sit directly below the control body with minimal gap ($\le 3\text{px}$) to prevent loose layout gaps and keep controls tightly grouped.
   - **Centered Responsive Layout**: Sub-modules and container panels MUST scale dynamically with window bounds and stay centered (`withSizeKeepingCentre()`) or fill proportional grid cells without leaving unwanted asymmetric gaps.
2. **Dual Oversampling (Online / Offline)**:
   - Must include oversampling controls with separate **Online** (realtime) and **Offline** (bounce/export) settings.
   - Default setting MUST be **Off (1x)**.
   - Handle `isNonRealtime()` automatic quality switching in `processBlock`.
3. **DAW Automation Integration**:
   - All parameters must be registered in `APVTS`.
   - UI controls MUST properly send `beginChangeGesture()` and `endChangeGesture()` during mouse interaction (`mouseDown`, `mouseUp`) for correct Touch/Latch/Write DAW automation.
4. **Assignable MIDI Learn**:
   - Integrate the shared `MidiLearnModule` allowing users to bind MIDI CC messages to any parameter.

### 2. UI Atom Statelessness & Controls Standard
- All UI controls in `modules/invis_core/ui_atoms` (e.g., `InvisKnob`, `InvisSwitch`, `InvisLED`, `InvisLEDMeter`, `InvisCellSelector`) **MUST BE STATELESS** relative to plugin parameters or audio engine state.
- NEVER include `juce::AudioProcessor`, `juce::AudioProcessorValueTreeState`, or `APVTS` header dependencies inside `ui_atoms`.
- Communicate UI changes via `std::function` callbacks (`onValueChanged`, `onToggle`, `onIndexChanged`, etc.).
- **`InvisLED` Standard**: Presentational LED atom supporting `LEDMountType::RecessedSlot` and `LEDMountType::ProtrudingDome`, driven by `LEDBallistics` for physical $2.4\times$ ignition attack flash spikes, smooth logarithmic settling, and $\approx 60\text{ms}$ phosphorescent decay envelopes (2x slower release).
- **`InvisLEDMeter` Standard**: Presentational vertical channel strip LED meter supporting Mono (1 column) and Stereo (2 columns), configurable scale position (`MeterScalePosition::Left` / `Right`), fast 5ms PPM attack, smooth 300ms VU decay, 800ms peak-hold retention, and per-segment 200ms phosphorescent decay tails.
- **`InvisCellSelector` Standard** (aka `InvisLEDCell`): Compact presentational parameter cell selector atom integrating the parameter label inside the cell slot, featuring square technical monospaced typography, self-emissive phosphor text radiance glow, and click-triggered ignition flash slide animation.
- **`InvisKnob` Standard**: Must support `InvisKnobSize` presets (`XS`, `S`, `M` default, `L`, `XL`) where font sizes, LED dot radius, and track stroke width remain fixed/constant per size preset, optional `OffPosition` (`Start`/`End`) with visual detent gap (preserving full active range limits like 20 kHz), optional angle sweep range in degrees (`setAngleRange`), optional magnetic sticky snap points (`setStickyPositions` / `setStickyPoints`), custom value arc origins (`ValueArcOrigin::Start`, `ValueArcOrigin::Center` for bipolar pan/EQ, `ValueArcOrigin::End`, or custom normalized position), scale ticks & labels passed via array (`setScaleTicks`), scale curve mapping (`Linear`, `Logarithmic`, `InverseLogarithmic`), double-click reset to `defaultValue`, inline keyboard text editing when clicking the value label, and unified theme accent color synchronization for value arc, value label, and LED pointer dot.

### 3. Functional Modules (DSP + UI)
- Modules in `modules/invis_core/functional_modules` combine:
  - `*DSP` class handling audio processing (`processBlock`), parameter creation (`addParameters`), and scaling math.
  - `*UI` container class handling parameter attachments (`APVTS`), atom positioning (`resized`), and response visualizations.

### 4. Design System & Theme Overriding
- Always inherit or query effective theme tokens via `InvisTheme` / `InvisThemeSupplier`.
- Never hardcode color values (`juce::Colours::red`, `#ff0000`) inside `paint()` methods of UI atoms or modules. Use theme tokens (`theme.accentPrimary`, `theme.background`, etc.).
- Allow parents (modules, plugins) to override child themes gracefully using `setModuleTheme` / `setThemeOverride`.

### 5. Volumetric Light & Emissive Surface Interaction Standard
- Any light-emitting element (e.g. LED pointer slots, glowing arcs, active meters, status indicators, power LEDs) MUST NOT be drawn flatly on top of surfaces.
- All LEDs MUST implement one of two authentic physical hardware mounting structures:
  1. **Recessed Housing & Slot Geometry (Заглиблений паз)**: Carved slot or channel with interior drop shadows and chamfered bevel edges on parent surfaces.
  2. **Protruding Dome / Bulb Through Chassis Hole (Виступний кристал крізь отвір)**: Translucent dome or cylinder protruding ABOVE the surface, fitted inside a drilled metal bezel ring/grommet collar, casting a contact drop shadow onto the faceplate and projecting 360-degree volumetric light outward onto surrounding materials.
- **Ambient Light Propagation**: Emissive elements MUST project soft light fields onto adjacent materials (e.g. metal cap lathe grooves, surrounding faceplate).
- **Realistic Optical Layering**: Shadow/bevel housing $\rightarrow$ Surface glow reflection $\rightarrow$ Translucent body $\rightarrow$ Neon halo $\rightarrow$ High-intensity phosphor core.

### 6. Language & Communication
- All design documentation, commit messages, and project plans MUST be written in **Ukrainian** as requested by the lead developer.
- Keep architectural logs up to date in `memory.md`.

---

## Project Structure Checklist
- Shared C++/JUCE module code lives under `modules/invis_core`.
- Plugin targets live under `plugins/<PluginName>`.
- Build configurations use CMake with JUCE 9 modular targets.
