# Guidelines for AI Agents (agents.md)

This file specifies rules and best practices for AI coding agents developing in the `invis-audio-plugins` repository.

---

## Code & Architecture Principles

### 0. Framework Standard
- **STRICT REQUIREMENT**: Strictly use **JUCE 9** for all CMake build configurations, plugin targets, and core modules.

### 1. Mandatory Universal Plugin Requirements
Every plugin developed in this repository MUST implement the following 4 features:
1. **Resizable UI & Aspect Ratio Preservation**:
   - Always call `setResizable(true, true)` in `PluginEditor`.
   - Ensure UI controls, modules, and ALL text labels (titles, values, tick marks) scale proportionally, preserving aspect ratio during resizing.
   - NEVER hardcode static font sizes (`Font(11.0f)`) or static pixel offsets in `paint()` / `resized()`. Compute all font sizes dynamically relative to bounds.
   - **NEVER hardcode static pixel bounds for text labels** (e.g., `Rectangle(..., 28, 14)`) or static radial offsets. Compute `labelRect` width/height dynamically relative to font size (`fontSize * 3.5f`) and radial distance relative to `radius` to prevent text truncation ("OFF" -> "O") or overlaps.
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
- All UI controls in `modules/invis_core/ui_atoms` (e.g., `InvisKnob`, `InvisSwitch`) **MUST BE STATELESS** relative to plugin parameters or audio engine state.
- NEVER include `juce::AudioProcessor`, `juce::AudioProcessorValueTreeState`, or `APVTS` header dependencies inside `ui_atoms`.
- Communicate UI changes via `std::function` callbacks (`onValueChanged`, `onToggle`, etc.).
- **`InvisKnob` Standard**: Must support optional `OffPosition` (`Start`/`End`) with visual detent gap (preserving full active range limits like 20 kHz), optional angle sweep range in degrees (`setAngleRange`), scale ticks & labels passed via array (`setScaleTicks`), scale curve mapping (`Linear`, `Logarithmic`, `InverseLogarithmic`), double-click reset to `defaultValue`, and inline keyboard text editing when clicking the value label.

### 3. Functional Modules (DSP + UI)
- Modules in `modules/invis_core/functional_modules` combine:
  - `*DSP` class handling audio processing (`processBlock`), parameter creation (`addParameters`), and scaling math.
  - `*UI` container class handling parameter attachments (`APVTS`), atom positioning (`resized`), and response visualizations.

### 4. Design System & Theme Overriding
- Always inherit or query effective theme tokens via `InvisTheme` / `InvisThemeSupplier`.
- Never hardcode color values (`juce::Colours::red`, `#ff0000`) inside `paint()` methods of UI atoms or modules. Use theme tokens (`theme.accentPrimary`, `theme.background`, etc.).
- Allow parents (modules, plugins) to override child themes gracefully using `setModuleTheme` / `setThemeOverride`.

### 5. Language & Communication
- All design documentation, commit messages, and project plans MUST be written in **Ukrainian** as requested by the lead developer.
- Keep architectural logs up to date in `memory.md`.

---

## Project Structure Checklist
- Shared C++/JUCE module code lives under `modules/invis_core`.
- Plugin targets live under `plugins/<PluginName>`.
- Build configurations use CMake with JUCE 9 modular targets.
