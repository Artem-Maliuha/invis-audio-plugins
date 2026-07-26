# Guidelines for AI Agents (agents.md)

This file specifies rules and best practices for AI coding agents developing in the `invis-audio-plugins` repository.

---

## Repository Layout

```
modules/invis_core/
  design_system/       tokens only - theme, layout grid, motion. No behaviour, no widgets.
  framework/           shared BEHAVIOUR engines. JUCE-free, APVTS-free.
  ui_atoms/            presentational widgets. MUST NOT include AudioProcessor / APVTS.
  functional_modules/  DSP + UI features. May use every layer above.
plugins/
  InvisDemoPlugin/     the BENCH - see below
```

Dependency direction is strictly downward: `design_system → framework → ui_atoms → functional_modules`. A lower layer never includes a higher one, and functional modules never reach into each other's internals.

### `plugins/InvisDemoPlugin` is a BENCH, not a plugin
It is a development board for exercising `invis_core`. It is **not a product**, and **not the template real plugins are copied from**. Therefore: Standalone + VST3 only (VST3 exists so the bench can be fed real audio in a DAW), never installed into the system plug-in folders unless `-DINVIS_INSTALL_BENCH=ON`.

**The bench synthesises its own 100 Hz sine.** Metering, LUFS, K-weighting, filter-energy taps and the AUTO routines have therefore never been validated against real programme material — treat their tuning as provisional until they have.

---

## Code & Architecture Principles

### 0. Framework Standard
- **STRICT REQUIREMENT**: Strictly use **JUCE 9** for all CMake build configurations, plugin targets, and core modules.

### 1. Mandatory Universal Plugin Requirements
Every plugin developed in this repository MUST implement the following 4 features:
1. **Fixed Design Grid & Global Zoom Scaling Standard** (`design_system/InvisLayout.h`):
   - Every editor is authored ONCE at a fixed design resolution measured in **design pixels (dp)**, declared as `kDesignWidth` / `kDesignHeight`. Resizing the plugin window is a **pure zoom**: a single `juce::AffineTransform::scale(k)` applied to ONE root canvas component (`invis::ui::applyDesignZoom`). Use `invis::ui::applyDesignResizeLimits` to install `setResizable(true, true)`, the fixed aspect ratio, and the zoom limits.
   - **ALL layout and paint code inside the canvas is written in absolute design pixels.** Font sizes, stroke widths, LED radii, paddings and gaps are absolute constants — they are correct by construction because the transform scales them together with everything else.
   - **PERCENTAGE-OF-PARENT LAYOUT IS FORBIDDEN.** Never write `bounds.getHeight() * 0.155f`, `getWidth() * 0.42f`, `roundToInt(h * 0.09f)` or an ad-hoc `aspectScale`. Mixing absolute paint constants with relative layout is precisely what makes proportions drift as the window grows, and it is the single most expensive class of bug in this codebase.
   - **The editor's `resized()` contains exactly one statement**: `applyDesignZoom(canvas, kDesignWidth, kDesignHeight, getLocalBounds())`. All real layout lives in `layoutCanvas()`, in design pixels.
   - **Initial `setSize()` goes LAST in the editor constructor**, after every child exists and has been given its final size preset — the first layout pass reads intrinsic sizes, so it must not run against half-configured atoms.
   - **SEMANTIC SPACING between controls.** Pick the gap by RELATIONSHIP, never by how it happens to look in the one panel in front of you — spacing is the only thing telling the user which controls belong together, so choosing it by eye per panel makes the same relationship read differently in every strip.
     - `kGapBonded` (4) — a control and its own satellite (a knob and the cell that configures it): one control wearing two parts.
     - `kGapRelated` (8) — two controls serving one concern (a trim and the meter showing its result): distinct controls, same thought.
     - `kGapGroup` (14) — separate concerns in one panel. Usually prefer an `InvisSeparator`, which carries its own margins and states the boundary out loud.
     - `kGapSection` (22) — major sections of a frame.
     - **HARD RULE — NOTHING TOUCHES.** Two atoms may never sit adjacent with zero space. `kMinControlGap` is the floor. The mistake looks like two consecutive `removeFromTop()` / `removeFromBottom()` calls with nothing between them: that is a spacing decision skipped, not made.
   - The raw `kGapXS`..`kGapXL` scale is for CHROME — panel padding, insets, gutters — where the question is "how much air", not "how are these two related".
   - Spacing uses the tokens in `invis::ui::layout` (`kGapXS`..`kGapXL`, `kPanelPadding`, `kPanelCorner`, `kIndicatorLabelGap`), not ad-hoc numbers. **A spacing decision that recurs across components belongs in the design system, not duplicated into each component's metrics table.** `kIndicatorLabelGap` is the worked example: the clearance between a text label and an indicator lamp beside it is one constant shared by every label+indicator pair, so the rhythm reads identically next to an XS control and an XL one. Convention: the indicator sits to the RIGHT of the label, and the pair is centred as a single unit.
   - **Strict Proportionality Integrity Policy for Base UI Atoms (Заборона спотворення пропорцій базових елементів)**:
     - Every base UI atom (`InvisKnob`, `InvisLEDMeter`, `InvisSwitch`, `InvisLED`) **OWNS ITS SIZE**. A size preset IS a size: `InvisKnob::getIntrinsicSize(InvisKnobSize::M)` and `InvisLEDMeter::getIntrinsicSize(InvisMeterSize::M)` return fixed dp dimensions derived from a `KnobMetrics` / `MeterMetrics` table of absolute constants.
     - Atoms MUST be placed with **`setBoundsCentredIn(area)`**, never with a hand-rolled `setBounds()`. An atom that receives foreign bounds renders **centred at its intrinsic size (letterbox) and fires a `jassert`** — it will never stretch, squash, or shrink to fit.
     - NEVER derive an atom's internal geometry from `getLocalBounds()` (no `diameter = min(w,h) * ratio`, no `segHeight = barHeight / numSegments`). Physical objects — LED segments, grip skirts, dial bodies — have constant physical dimensions per preset.
     - Relief detail must shrink WITH the preset: the knurled grip ring ("skirt"), rim chamfers, 3D elevation offsets, and knurl tooth count are all absolute per-preset constants (`skirtWidth`, `rimGap`, `elevation`, `numTeeth`). A ratio-with-a-floor such as `max(3.8f, r * 0.16f)` is banned — it makes small knobs wear a disproportionately fat skirt.
     - If layout space is tight: **expand the container or the design resolution, NEVER deform the atom.** Leftover space becomes air, not distortion.
   - **One geometry, one source**: an atom computes its layout ONCE (`computeLayout()`) and shares that result between `paint()`, hit-testing, and any child editors. Recomputing geometry with different constants in `mouseDown()` silently decouples hit zones from drawn pixels.
   - **Functional modules and shared frames expose an intrinsic size too** (`InputSidebarUI::getIntrinsicWidth()`, `InputFilterUI::getIntrinsicSize()`), derived from the atoms they host. The universal Input/Output sidebar frame draws every dimension from the single shared contract in `functional_modules/InvisSidebarLayout.h`, so the left and right sides are symmetric by construction.
   - Text bounding boxes still scale with the active font size (e.g. `rectWidth = fontSize * 3.8f`) so labels never truncate ("OFF" -> "O"); the font size itself is an absolute preset constant.
   - Value Label Proximity: value text MUST sit directly below the dial body with a minimal gap (`KnobMetrics::valueGap`, $\le 3\text{ dp}$) to keep controls tightly grouped.
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
- **`InvisLevelLamp` Standard**: Single-lamp level indicator atom answering "did this control actually do any work". Fed a normalized 0..1 "voltage" via `setLevel()`, it lights along an extinguished → green → amber → crimson ramp; use cases are filter energy removed, compressor gain reduction, and clipper depth. Rules:
  - Hue and luminance are driven SEPARATELY — hue encodes how deep into the danger zone the value is, brightness encodes magnitude.
  - The ramp interpolates through **amber as an explicit waypoint**. A direct green→red sRGB lerp passes through muddy olive and reads as "dirty" rather than "warning".
  - It uses `LampBallistics` (~40ms attack / ~380ms release), **never `LEDBallistics`** — the latter's 2.4× ignition flash is designed for binary on/off state and would fire on every transient, making a continuous-level lamp unreadable.
  - `setActive(false)` (stage BYPASSED) renders a dead socket and MUST read differently from "powered but currently removing nothing", which shows a faint cold glass. These are two distinct physical states and conflating them lies to the user.
  - dB→normalized mapping lives with the CALLER (`lamp::levelFromDb`), never inside the atom: the atom is a dumb 0..1 device, and the meaning of the dB figure belongs to whoever measured it.
  - `InvisKnob` embeds one optionally via `setIndicatorLampVisible()`; it is drawn inline (not as a child component) so the volumetric spill lands on the knob's own surface instead of being clipped, and enabling it never changes the knob's intrinsic size.
- **`InvisLEDMeter` Standard**: Presentational vertical channel strip LED meter with `InvisMeterSize` presets (`S`, `M` default, `L`) defining a fixed intrinsic size — constant segment height, column width, and clip lamp height, so the LED ladder NEVER rubber-bands with the host panel. A **40-segment** ladder (`kSegmentDbTable`) weighted for fine 0.5 dB resolution around unity and coarse steps in the noise floor, with segment index `zeroSegmentIndex` sitting EXACTLY on 0.0 dB — the golden reference line and every scale label key off segment indices, so that alignment must never drift. Scale labels are derived from the table plus a per-`MeterScaleType` offset (dBFS +0, dBU +18, VU +20, PPM +9), not from hand-maintained per-scale label tables. Carries a **numeric readout block (RMS / PEAK / LUFS)** below the ladder, fed by `setReadouts()`; all integration lives in `InvisLoudnessAnalyser` on the audio thread (300ms RMS, 800ms-hold peak, and K-weighted **LUFS-S** — short-term 3s, because Momentary is too jumpy to read as a digit and Integrated needs a user-facing reset the atom has no business owning). Supports Mono (1 column) and Stereo (2 columns), configurable scale position (`MeterScalePosition::Left` / `Right`), fast 5ms PPM attack, smooth 300ms VU decay, 800ms peak-hold retention, and per-segment 200ms phosphorescent decay tails.
- **`InvisCellSelector` Standard** (aka `InvisLEDCell`): Compact presentational parameter cell selector atom integrating the parameter label inside the cell slot, featuring square technical monospaced typography, self-emissive phosphor text radiance glow, and click-triggered ignition flash slide animation.
- **`InvisKnob` Standard**: Must support `InvisKnobSize` presets (`XS`, `S`, `M` default, `L`, `XL`), each a full physical specification in `KnobMetrics` — dial diameter, track stroke, LED dot radius, all font sizes, tick ring geometry, grip skirt width, rim chamfers, 3D elevation, and knurl tooth count are absolute constants per preset, and together they define the preset's fixed intrinsic size. Plus optional `OffPosition` (`Start`/`End`) with visual detent gap (preserving full active range limits like 20 kHz), optional angle sweep range in degrees (`setAngleRange`), optional magnetic sticky snap points (`setStickyPositions` / `setStickyPoints`), custom value arc origins (`ValueArcOrigin::Start`, `ValueArcOrigin::Center` for bipolar pan/EQ, `ValueArcOrigin::End`, or custom normalized position), scale ticks & labels passed via array (`setScaleTicks`), scale curve mapping (`Linear`, `Logarithmic`, `InverseLogarithmic`), double-click reset to `defaultValue`, inline keyboard text editing when clicking the value label, and unified theme accent color synchronization for value arc, value label, and LED pointer dot.

### 2b. Motion Standard & Framework Layer
- **NO MACHINE-DRIVEN PARAMETER RECALL IS EVER INSTANTANEOUS** (`design_system/InvisMotion.h`). Preset load, A/B/C compare switch, AUTO calibration, reset-to-default, automation jump — whenever a control's value is set by the machine rather than by the user's hand, it MUST glide. It must never snap.
  - A discontinuous parameter jump produces an audible click or zipper in the signal, and on screen it destroys the user's ability to see *what* changed: a knob that teleports conveys nothing, a knob that turns tells you which direction and how far.
  - Durations: `motion::kRecallGlideFast` (0.25s, single control) / `kRecallGlideDefault` (0.45s, preset or compare recall moving many controls) / `kRecallGlideSlow` (1.0s, a deliberate watch-me gesture). **`kMinRecallGlide` (0.25s) is a floor, not a suggestion** — below roughly that a gain change stops being a fade and becomes a step edge again.
  - `motion::logEase` is the house curve for machine-driven motion: quick off the mark, decelerating into the target. Use `motion::glide()` rather than hand-rolling interpolation.
- **The `framework/` layer holds shared BEHAVIOUR engines** — JUCE-free, APVTS-free logic that more than one module needs. It exists because duplicated behaviour drifts: the input AUTO grew a proper phased state machine while the output AUTO degenerated into two bare value jumps. **Divergence like that is a framework gap, not a module bug.**
  - `framework::InvisAutoRoutine` — phase-sequenced routine engine. A routine is an ordered list of phases, each with a fixed duration, an `onEnter` (capture starting values) and an `onTick(progress)`. The engine owns the timeline, phase transitions and running flag; the caller owns the meaning.
  - **Every AUTO routine in the system MUST be built from it**, so they all share one shape: `RESET (glided) → HOLD (settle) → work → glide to result`. Never hand-roll a timeline inside a `*UI` class.
  - `framework::ParameterGlide` — a single machine-driven scalar. It has no way to jump; `settle()` is for initialisation only.
  - Layer order: `design_system` (tokens) → `framework` (shared behaviour) → `ui_atoms` (presentational widgets) → `functional_modules` (DSP + UI features).

### 2c. A/B/C Compare Convention
- Compare slots are **seeded EAGERLY** with the state the plugin loaded with — all of them, at construction. Never with plugin defaults, and never lazily on first visit.
  - Not defaults: comparing your work against an init patch is what loading Init is for. The point of A/B is comparing an EDIT against its STARTING POINT.
  - Not lazy: a slot filled on first visit inherits whatever you have already dialled in, so switching to it changes nothing and there is nothing to compare. That breaks the primary use case outright.
- Switching slots saves the on-screen state into the slot being left, then loads the slot being entered. Nothing is ever lost by switching.
- **An explicit copy gesture is mandatory** — every plugin with A/B has one. Right-click a slot: copy current state to that slot, copy current state to ALL slots, or reset that slot to plugin defaults.
- "Reset to defaults" builds its snapshot by overwriting values in a copy of the tree, NOT by driving the parameters to their defaults — the latter would be audible and would fight whatever the host has already restored.

### 3. Functional Modules (DSP + UI)
- Modules in `modules/invis_core/functional_modules` combine:
  - `*DSP` class handling audio processing (`processBlock`), parameter creation (`addParameters`), and scaling math.
  - `*UI` container class handling parameter attachments (`APVTS`), atom positioning (`resized`), and response visualizations.
- **Metering & indicator measurement rules** (the DSP side of `InvisLevelLamp` / `InvisLEDMeter`):
  - Cross the audio→UI boundary with relaxed `std::atomic` values. The DSP **normalizes to 0..1 itself**, so no dB maths ever appears in the UI layer.
  - **Smooth linear power (mean-square), never dB.** A dB-domain follower hits `log(0)` on digital silence and the resulting `-inf` poisons the whole envelope.
  - Both taps of any ratio measurement MUST share identical time constants, or the ratio jitters from mismatched smoothing alone, independent of what the stage is actually doing.
  - **Gate before dividing.** Below `kEnergyGateDb` (−60 dBFS) there is no signal to have removed energy from, so the ratio is meaningless rather than merely noisy — release the indicator instead of computing it.
  - **Measure each stage across ITSELF only.** A serial LPF's reference tap is the signal *after* the HPF; using the original input would double-count the HPF's cut into the LPF's reading whenever both are engaged.
  - A stage that is engaged but legitimately removing nothing MUST read as active-and-idle, not as bypassed. Bypass is signalled explicitly via the engaged flag, never inferred from the measurement landing near zero.

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

### 5b. Constellation Routing Law (`InvisConstellation`)
- The chart is the flagship element of the series. **The shape you draw IS the signal path** — topology is DERIVED from the drawn links, never stored as a mode or a toggle.
- **THE LAW, and it composes**: *anything serial is serial; anything closed behaves as ONE node with everything parallel inside*. These are not alternatives for a whole component. A component is cut at its **bridges**; each surviving cycle collapses to one parallel cluster; the bridges are the serial hops between clusters. Contracting cycles always leaves a tree, so any drawing is an ordered run of stages.
- A star takes ANY number of lines. Never reintroduce a socket cap to force a shape — read the shape instead.
- Entry into a chain is the star nearest the observer, gated by `halo × sensitivity`; hops between stages are 100%. Sensitivity is **bipolar** and acts as dry/wet against what arrived.
- **A closed cluster encloses what its CYCLES enclose, never the convex hull.** The hull invents edges across sky nobody joined. This governs both the wash and the acoustic "inside" test.
- **Colour mixes on the hue wheel, never per channel.** A per-channel average of red, green and blue is literally grey, which is what every closed figure used to glow. Saturation is taken at its strongest, not averaged.
- **Glow is NOT the mix share.** Amounts inside a closed cluster are normalised, so driving brightness from them makes an instrument dim as members join. `StarContribution::glow` is carried separately and follows how strongly the stage is fed.
- **Negative sensitivity does not glow at all.** Light meaning "removing light" reads as nonsense; polarity is stated by the bar through the core.
- Routing is re-derived on every question asked of it, so a frame derives it ONCE (`ChartFrame`) and passes it down. A painter that cannot re-derive cannot quietly disagree with the routing.

### 6. Language & Communication
- All design documentation, commit messages, and project plans MUST be written in **Ukrainian** as requested by the lead developer.
- Keep architectural logs up to date in `memory.md`.

---

## Project Structure Checklist
- Shared C++/JUCE module code lives under `modules/invis_core`.
- Plugin targets live under `plugins/<PluginName>`.
- Build configurations use CMake with JUCE 9 modular targets.
