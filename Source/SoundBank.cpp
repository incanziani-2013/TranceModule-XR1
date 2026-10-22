#include "SoundBank.h"
#include <BinaryData.h>
#include <algorithm>

const std::vector<SoundEntry>& getSoundBank()
{
    static const std::vector<SoundEntry> bank = []
    {
        const juce::StringArray order { "Leads", "Brass", "Bells", "Pulsatings", "Seq" };
        struct Tmp { int cat; int num; SoundEntry e; };
        std::vector<Tmp> tmp;

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const char* id = BinaryData::namedResourceList[i];
            int size = 0;
            const char* data = BinaryData::getNamedResource(id, size);
            const auto base = juce::String(BinaryData::getNamedResourceOriginalFilename(id))
                                  .upToLastOccurrenceOf(".", false, false);
            const auto parts = juce::StringArray::fromTokens(base, "_", "");
            if (parts.size() < 2 || data == nullptr) continue;

            const int cat = juce::jmax(0, order.indexOf(parts[0]));
            const int num = parts[1].getIntValue();
            const auto tag = parts.size() > 2 ? parts[2] : juce::String();

            SoundEntry e;
            e.category   = parts[0];
            e.choiceName = parts[0] + " " + parts[1] + (tag.isNotEmpty() ? " " + tag : juce::String());
            e.lcdName    = parts[0].toUpperCase() + " " + parts[1];
            e.data = data;
            e.size = size;
            tmp.push_back({ cat, num, e });
        }

        std::stable_sort(tmp.begin(), tmp.end(), [](const Tmp& a, const Tmp& b)
                         { return a.cat != b.cat ? a.cat < b.cat : a.num < b.num; });

        std::vector<SoundEntry> out;
        for (auto& t : tmp) out.push_back(t.e);
        return out;
    }();
    return bank;
}
