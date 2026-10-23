#pragma once

#include <juce_core/juce_core.h>

/** Writes the GUI as layered PNGs + JSON manifests into dir (see tools/make_psd.py). */
bool exportSkin (const juce::File& dir, float scale = 4.0f);
