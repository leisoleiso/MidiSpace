#pragma once

#include "ModelInterface.h"
#include <vector>

namespace midispace {

// Chord type -> semitone offsets from root.
inline std::vector<int> chordOffsets(int type) {
    switch (type) {
        case 0:  return { 0, 4, 7 };            // major
        case 1:  return { 0, 3, 7 };            // minor
        case 2:  return { 0, 3, 6 };            // dim
        case 3:  return { 0, 4, 8 };            // aug
        case 4:  return { 0, 2, 7 };            // sus2
        case 5:  return { 0, 5, 7 };            // sus4
        case 6:  return { 0, 4, 7, 11 };        // maj7
        case 7:  return { 0, 3, 7, 10 };        // min7
        case 8:  return { 0, 4, 7, 10 };        // dom7
        case 9:  return { 0, 3, 6, 10 };        // half-dim7 (m7b5)
        case 10: return { 0, 3, 6, 9 };         // dim7
        case 11: return { 0, 3, 7, 11 };        // minMaj7
        case 12: return { 0, 4, 7, 11, 14 };    // maj9
        case 13: return { 0, 3, 7, 10, 14 };    // min9
        case 14: return { 0, 4, 7, 10, 14 };    // dom9
        case 15: return { 0, 4, 7, 14 };        // add9
        case 16: return { 0, 3, 7, 14 };        // mAdd9
        case 17: return { 0, 4, 7, 9 };         // 6
        case 18: return { 0, 3, 7, 9 };         // m6
        default: return { 0, 4, 7 };
    }
}

inline const char* chordTypeName(int type) {
    static const char* names[] = {
        "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4",
        "Maj7", "Min7", "Dom7", "Half-Dim7", "Dim7", "MinMaj7",
        "Maj9", "Min9", "Dom9", "Add9", "mAdd9", "6", "m6"
    };
    static const int n = static_cast<int>(sizeof(names) / sizeof(names[0]));
    return (type >= 0 && type < n) ? names[type] : "Major";
}
inline int chordTypeCount() { return 19; }

// Scale/mode -> semitone offsets from root.
inline std::vector<int> scaleOffsets(int type) {
    switch (type) {
        case 0:  return { 0, 2, 4, 5, 7, 9, 11 };   // major (ionian)
        case 1:  return { 0, 2, 3, 5, 7, 8, 10 };   // natural minor (aeolian)
        case 2:  return { 0, 2, 3, 5, 7, 8, 11 };   // harmonic minor
        case 3:  return { 0, 2, 3, 5, 7, 9, 11 };   // melodic minor
        case 4:  return { 0, 2, 3, 5, 7, 9, 10 };   // dorian
        case 5:  return { 0, 1, 3, 5, 7, 8, 10 };   // phrygian
        case 6:  return { 0, 2, 4, 6, 7, 9, 11 };   // lydian
        case 7:  return { 0, 2, 4, 5, 7, 9, 10 };   // mixolydian
        case 8:  return { 0, 1, 3, 5, 6, 8, 10 };   // locrian
        case 9:  return { 0, 2, 4, 7, 9 };          // pentatonic major
        case 10: return { 0, 3, 5, 7, 10 };         // pentatonic minor
        case 11: return { 0, 3, 5, 6, 7, 10 };      // blues
        case 12: return { 0, 2, 4, 6, 8, 10 };      // whole tone
        default: return { 0, 2, 4, 5, 7, 9, 11 };
    }
}

inline const char* scaleTypeName(int type) {
    static const char* names[] = {
        "Major (Ionian)", "Natural Minor", "Harmonic Minor", "Melodic Minor",
        "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian",
        "Pentatonic Major", "Pentatonic Minor", "Blues", "Whole Tone"
    };
    static const int n = static_cast<int>(sizeof(names) / sizeof(names[0]));
    return (type >= 0 && type < n) ? names[type] : "Major (Ionian)";
}
inline int scaleTypeCount() { return 13; }

// Chord arpeggio: tones up, an octave, then back down (quarter-note grid).
inline std::vector<NoteEvent> makeArpeggio(int rootMidi, const std::vector<int>& chordTones) {
    std::vector<int> seq;
    for (int t : chordTones)
        seq.push_back(rootMidi + t);
    seq.push_back(rootMidi + 12);
    for (int i = static_cast<int>(chordTones.size()) - 1; i >= 0; --i)
        seq.push_back(rootMidi + chordTones[i]);

    if (static_cast<int>(seq.size()) > 8)
        seq.resize(8);

    std::vector<NoteEvent> notes;
    const int n = static_cast<int>(seq.size());
    for (int i = 0; i < n; ++i) {
        const float sq = static_cast<float>(i);
        const float eq = (i == n - 1) ? 8.0f : static_cast<float>(i + 1);
        notes.push_back({ seq[i], sq, eq });
    }
    return notes;
}

// Scale run: ascend the scale then reach the octave (quarter-note grid).
inline std::vector<NoteEvent> makeScaleRun(int rootMidi, const std::vector<int>& scaleTones) {
    std::vector<int> pitches;
    for (int t : scaleTones)
        pitches.push_back(rootMidi + t);
    pitches.push_back(rootMidi + 12);
    if (static_cast<int>(pitches.size()) > 8)
        pitches.resize(8);

    std::vector<NoteEvent> notes;
    const int n = static_cast<int>(pitches.size());
    for (int i = 0; i < n; ++i) {
        const float sq = static_cast<float>(i);
        const float eq = (i == n - 1) ? 8.0f : static_cast<float>(i + 1);
        notes.push_back({ pitches[i], sq, eq });
    }
    return notes;
}

} // namespace midispace
