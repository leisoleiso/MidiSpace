#pragma once

#include "ModelInterface.h"
#include <vector>

namespace midispace {

// The three Phase-2 anchor motifs (same as Phase 1's A/B, plus a distinct C).
// 2 bars, monophonic, C-major scale.  A node's latent vector is produced by
// encoding its motif once, then cached.
inline std::vector<NoteEvent> motifA() {
    return { {60, 0.0f, 1.0f}, {64, 1.0f, 2.0f}, {67, 2.0f, 3.0f}, {72, 3.0f, 4.0f},
             {67, 4.0f, 5.0f}, {64, 5.0f, 6.0f}, {60, 6.0f, 8.0f} };
}

inline std::vector<NoteEvent> motifB() {
    return { {67, 0.0f, 0.5f}, {65, 0.5f, 1.0f}, {64, 1.0f, 2.0f}, {62, 2.0f, 3.0f},
             {60, 3.0f, 4.0f}, {59, 4.0f, 5.0f}, {57, 5.0f, 6.0f}, {55, 6.0f, 8.0f} };
}

inline std::vector<NoteEvent> motifC() {
    return { {64, 0.0f, 1.0f}, {64, 1.0f, 2.0f}, {62, 2.0f, 3.0f}, {64, 3.0f, 4.0f},
             {67, 4.0f, 5.0f}, {67, 5.0f, 6.0f}, {64, 6.0f, 8.0f} };
}

inline std::vector<std::vector<NoteEvent>> allMotifs() {
    return { motifA(), motifB(), motifC() };
}

} // namespace midispace
