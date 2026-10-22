#pragma once
#include "Common.h"
#include <vector>

// Lista de sonidos incrustados (ordenados por categoria y numero).
struct SoundEntry
{
    juce::String category;   // "Leads"
    juce::String choiceName; // "Leads 01 Oryus"  (lista de automatizacion del host)
    juce::String lcdName;    // "LEADS 01"
    const char*  data = nullptr;
    int          size = 0;
};

const std::vector<SoundEntry>& getSoundBank();
