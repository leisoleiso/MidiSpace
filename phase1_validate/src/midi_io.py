"""MIDI <-> NoteSequence helpers (thin wrappers over magenta's midi_io)."""
from magenta.music import midi_io as _magenta_midi
from magenta.protobuf import music_pb2


def make_note_sequence(note_events, qpm=120.0, velocity=80):
    """Build a monophonic 2-bar NoteSequence from (pitch, start_quarter, end_quarter).

    Times are in absolute seconds (magenta convention): at qpm=120 one quarter
    note = 0.5 s, so 2 bars = 8 quarters = 4.0 s total.
    """
    ns = music_pb2.NoteSequence()
    ns.ticks_per_quarter = 220
    ts = ns.time_signatures.add()
    ts.time = 0.0
    ts.numerator = 4
    ts.denominator = 4
    tempo = ns.tempos.add()
    tempo.time = 0.0
    tempo.qpm = qpm
    spq = 60.0 / qpm
    total = 0.0
    for pitch, sq, eq in note_events:
        n = ns.notes.add()
        n.pitch = int(pitch)
        n.velocity = int(velocity)
        n.start_time = sq * spq
        n.end_time = eq * spq
        total = max(total, eq * spq)
    ns.total_time = total
    return ns


def write_midi(ns, path):
    _magenta_midi.note_sequence_to_midi_file(ns, path)


def load_midi(path):
    return _magenta_midi.midi_file_to_note_sequence(path)


def note_sequence_to_step_pitches(ns, n_steps=32, steps_per_quarter=4):
    """Quantize a monophonic NoteSequence to a per-step pitch list (rest = -1).

    Robust to float fuzz by rounding note boundaries to the nearest step.
    """
    qpm = ns.tempos[0].qpm if ns.tempos else 120.0
    spq = 60.0 / qpm
    step_time = spq / float(steps_per_quarter)
    pitches = [-1] * n_steps
    for note in ns.notes:
        s = int(round(note.start_time / step_time))
        e = int(round(note.end_time / step_time))
        s = max(0, min(s, n_steps - 1))
        e = max(0, min(e, n_steps))
        for i in range(s, e):
            pitches[i] = int(note.pitch)
    return pitches
