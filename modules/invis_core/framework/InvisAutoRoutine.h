#pragma once

#include "../design_system/InvisMotion.h"
#include <functional>
#include <vector>
#include <algorithm>

namespace invis::framework {

/**
 * INVIS FRAMEWORK - MACHINE-DRIVEN PARAMETER MOTION
 * =================================================
 *
 * This layer exists because the same behaviour kept being re-implemented per module and drifting:
 * the input AUTO grew a proper phased state machine while the output AUTO ended up as two bare
 * value jumps. That divergence is a framework gap, not a module bug - so the shared behaviour
 * lives here and every routine inherits identical character for free.
 *
 * Everything here is JUCE-free and APVTS-free: routines describe WHEN things move, callers own
 * WHAT moves.
 */

/**
 * A single scalar being driven by the machine rather than by the user's hand.
 *
 * Per the Invis motion standard it GLIDES - it has no way to jump. `settle()` exists only for
 * initialisation, never for recall.
 */
class ParameterGlide {
public:
    void begin(float from, float to, float durationSeconds)
    {
        startValue = from;
        targetValue = to;
        duration = std::max(ui::motion::kMinRecallGlide, durationSeconds);
        elapsed = 0.0f;
        moving = true;
        current = from;
    }

    /** Direct assignment. Initialisation ONLY - using this for a recall violates the standard. */
    void settle(float v)
    {
        current = startValue = targetValue = v;
        moving = false;
        elapsed = duration;
    }

    void tick(float dt)
    {
        if (!moving) return;

        elapsed += dt;
        if (elapsed >= duration)
        {
            current = targetValue;
            moving = false;
            return;
        }

        current = ui::motion::glide(startValue, targetValue, elapsed / duration);
    }

    float value() const { return current; }
    float target() const { return targetValue; }
    bool isMoving() const { return moving; }

private:
    float startValue { 0.0f };
    float targetValue { 0.0f };
    float current { 0.0f };
    float duration { ui::motion::kRecallGlideDefault };
    float elapsed { 0.0f };
    bool moving { false };
};

/**
 * Phase-sequenced routine engine.
 *
 * A routine is an ordered list of phases, each with a fixed duration and a per-frame callback
 * that receives its own normalized progress. The engine owns the timeline, the phase transitions
 * and the running flag; the caller owns the meaning.
 *
 * Every Invis AUTO routine is built from this, so they all share the same shape:
 *   RESET (glided, never snapped)  ->  HOLD (settle)  ->  work  ->  glide to result.
 */
class InvisAutoRoutine {
public:
    struct Phase {
        const char* name { "" };
        float duration { 1.0f };

        /** Called every frame with progress 0..1 through this phase, plus the frame delta.
            Convergence loops (slow residual correction) need real time, not just progress. */
        std::function<void(float progress, float dt)> onTick;

        /** Called once as the phase begins - capture starting values here. */
        std::function<void()> onEnter;
    };

    void setPhases(std::vector<Phase> newPhases) { phases = std::move(newPhases); }

    void start()
    {
        if (phases.empty()) return;

        elapsed = 0.0f;
        currentPhase = -1;
        setRunning(true);
        advanceToPhase(0);
    }

    void cancel() { setRunning(false); }

    void tick(float dt)
    {
        if (!running) return;

        elapsed += dt;

        // Walk forward rather than assuming one phase per frame: a long frame hitch must not be
        // able to strand the routine inside a phase whose window has already closed.
        while (currentPhase < static_cast<int>(phases.size())
               && elapsed >= phaseStart(currentPhase) + phases[static_cast<size_t>(currentPhase)].duration)
        {
            const int next = currentPhase + 1;
            if (next >= static_cast<int>(phases.size()))
            {
                // Let the final phase land exactly on its endpoint before shutting down
                if (auto& tickFn = phases.back().onTick) tickFn(1.0f, dt);
                setRunning(false);
                return;
            }
            advanceToPhase(next);
        }

        if (currentPhase < 0 || currentPhase >= static_cast<int>(phases.size())) return;

        const auto& phase = phases[static_cast<size_t>(currentPhase)];
        const float within = elapsed - phaseStart(currentPhase);
        const float progress = (phase.duration > 0.0f)
                                 ? std::clamp(within / phase.duration, 0.0f, 1.0f) : 1.0f;

        if (phase.onTick) phase.onTick(progress, dt);
    }

    bool isRunning() const { return running; }
    int getCurrentPhaseIndex() const { return currentPhase; }
    const char* getCurrentPhaseName() const
    {
        return (currentPhase >= 0 && currentPhase < static_cast<int>(phases.size()))
                 ? phases[static_cast<size_t>(currentPhase)].name : "";
    }

    float getElapsed() const { return elapsed; }

    float getTotalDuration() const
    {
        float total = 0.0f;
        for (const auto& p : phases) total += p.duration;
        return total;
    }

    /** Fired when the routine starts and again when it finishes. Drive the AUTO lamp from this. */
    std::function<void(bool running)> onRunningChanged;

private:
    float phaseStart(int index) const
    {
        float t = 0.0f;
        for (int i = 0; i < index && i < static_cast<int>(phases.size()); ++i)
            t += phases[static_cast<size_t>(i)].duration;
        return t;
    }

    void advanceToPhase(int index)
    {
        currentPhase = index;
        if (auto& enterFn = phases[static_cast<size_t>(index)].onEnter) enterFn();
    }

    void setRunning(bool shouldRun)
    {
        if (running == shouldRun) return;
        running = shouldRun;
        if (onRunningChanged) onRunningChanged(running);
    }

    std::vector<Phase> phases;
    int currentPhase { -1 };
    float elapsed { 0.0f };
    bool running { false };
};

} // namespace invis::framework
