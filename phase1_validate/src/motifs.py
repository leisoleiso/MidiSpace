"""Hand-authored 2-bar monophonic test motifs.

Each motif is a list of (pitch, start_quarter, end_quarter).  MusicVAE's
cat-mel_2bar_big model expects exactly 2 bars (8 quarter notes) of monophonic,
non-drum melody.

Both motifs use only C-major (white-key) notes so that the model's key
inference stays stable across encode/decode (no surprise transposition).
A and B are deliberately contrasting in contour, rhythm and register so the
interpolation experiment has something meaningful to traverse.

MIDI note numbers: C4=60, E4=64, G4=67, C5=72, F4=65, D4=62, B3=59, A3=57, G3=55.
"""

# Ascending C-major arpeggio, quarter notes, register C4..C5.
MOTIF_A = [
    (60, 0.0, 1.0),   # C4
    (64, 1.0, 2.0),   # E4
    (67, 2.0, 3.0),   # G4
    (72, 3.0, 4.0),   # C5
    (67, 4.0, 5.0),   # G4
    (64, 5.0, 6.0),   # E4
    (60, 6.0, 8.0),   # C4 (held 2 quarters)
]

# Descending stepwise, eighth-note pickup then quarters, register G4..G3.
MOTIF_B = [
    (67, 0.0, 0.5),   # G4 (eighth)
    (65, 0.5, 1.0),   # F4 (eighth)
    (64, 1.0, 2.0),   # E4
    (62, 2.0, 3.0),   # D4
    (60, 3.0, 4.0),   # C4
    (59, 4.0, 5.0),   # B3
    (57, 5.0, 6.0),   # A3
    (55, 6.0, 8.0),   # G3 (held 2 quarters)
]

MOTIFS = {'A': MOTIF_A, 'B': MOTIF_B}


def events_to_step_pitches(events, n_steps=32, steps_per_quarter=4):
    """Convert quarter-grid events to a per-step pitch list.

    Holds repeat the sounding pitch; silence is -1.  This is the ground-truth
    reference used by the metrics module (no magenta dependency).
    """
    pitches = []
    for i in range(n_steps):
        tq = i / float(steps_per_quarter)
        pitch = -1
        for p, sq, eq in events:
            if sq <= tq < eq:
                pitch = int(p)
                break
        pitches.append(pitch)
    return pitches


MOTIF_A_PITCHES = events_to_step_pitches(MOTIF_A)
MOTIF_B_PITCHES = events_to_step_pitches(MOTIF_B)
