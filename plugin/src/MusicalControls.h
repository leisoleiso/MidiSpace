#pragma once

#include "ModelInterface.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace midispace {

// Phase 6: simple, predictable post-processing controls applied to a generated
// motif before playback/display.  Kept fully separate from SpatialEngine and
// ModelInterface so more sophisticated conditioning can replace them later.
struct MusicalControls {
    int transposeSemitones = 0;    // Key shift (whole melody)
    int minPitch = 36;             // C2
    int maxPitch = 96;             // C7
    float density = 1.0f;          // 0.1..1.0 note density

    std::vector<NoteEvent> apply(std::vector<NoteEvent> notes) const {
        // 1) Transpose.
        for (auto& n : notes)
            n.pitch += transposeSemitones;

        // 2) Pitch range: octave-fold any note back into [minPitch, maxPitch].
        const int range = maxPitch - minPitch;
        if (range > 0) {
            for (auto& n : notes) {
                while (n.pitch < minPitch) n.pitch += 12;
                while (n.pitch > maxPitch) n.pitch -= 12;
            }
        }

        // 3) Density: keep the first/last note and every keepEvery-th note.
        if (density < 0.999f && notes.size() > 2) {
            const int keepEvery = std::max(1, (int) std::lround(1.0f / std::max(density, 0.1f)));
            std::vector<NoteEvent> kept;
            kept.reserve(notes.size());
            for (size_t i = 0; i < notes.size(); ++i)
                if (i == 0 || i + 1 == notes.size() || static_cast<int>(i) % keepEvery == 0)
                    kept.push_back(notes[i]);
            notes = std::move(kept);
        }

        return notes;
    }
};

} // namespace midispace
