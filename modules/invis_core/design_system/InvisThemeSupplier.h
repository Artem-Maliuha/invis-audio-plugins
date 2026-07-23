#pragma once

#include "InvisTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace invis::ui {

/**
 * Interface implemented by parent containers/modules to supply custom themes
 * down the JUCE component tree hierarchy.
 */
class InvisThemeSupplier {
public:
    virtual ~InvisThemeSupplier() = default;
    virtual InvisTheme getEffectiveTheme() const = 0;

    static InvisTheme getParentTheme(const juce::Component* component) {
        if (component == nullptr)
            return InvisTheme::getGlobalDefault();

        if (auto* supplier = component->findParentComponentOfClass<InvisThemeSupplier>())
            return supplier->getEffectiveTheme();

        return InvisTheme::getGlobalDefault();
    }
};

} // namespace invis::ui
