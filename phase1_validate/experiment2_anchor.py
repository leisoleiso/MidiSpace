"""Experiment 2 — Anchor identity preservation.

The core requirement: when the ball sits at (or near) an anchor, the output
must BE that anchor's original MIDI — not a re-decoded approximation.

This experiment:
  1. shows SpatialEngine returns a one-hot weight vector at/near an anchor;
  2. shows that decoding zA actually produces A' != A (byte-level), proving
     that the bypass is necessary, not optional;
  3. demonstrates the pipeline decision the C++ side will implement.
"""
import hashlib
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

from paths import find_checkpoint_prefix, RESULTS                           # noqa: E402
from motifs import MOTIF_A, MOTIF_B, MOTIF_A_PITCHES                       # noqa: E402
from midi_io import make_note_sequence, note_sequence_to_step_pitches, write_midi  # noqa: E402
from metrics import compare, format_metrics                                  # noqa: E402
from model import MusicVAEModel                                              # noqa: E402
from spatial_engine import Anchor, SpatialEngine                             # noqa: E402


def sha256(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def main():
    m = MusicVAEModel(find_checkpoint_prefix())
    os.makedirs(RESULTS, exist_ok=True)

    ns_a = make_note_sequence(MOTIF_A)
    ns_b = make_note_sequence(MOTIF_B)
    write_midi(ns_a, os.path.join(RESULTS, 'exp2_A_original.mid'))
    write_midi(ns_b, os.path.join(RESULTS, 'exp2_B_original.mid'))

    eng = SpatialEngine([Anchor('A', [0.0, 0.0]), Anchor('B', [1.0, 0.0])])

    print('\n=== Experiment 2: anchor identity preservation ===')
    print('ball at A   -> weights =', eng.weights([0.0, 0.0]))
    print('ball near A -> weights =', eng.weights([1e-5, 0.0]))
    print('ball at mid -> weights =', eng.weights([0.5, 0.0]))

    # Decode zA and show A' differs from A (this is why bypass is required).
    mu_a = m.encode_mean([ns_a])
    recon = m.decode(mu_a, temperature=0.01)[0]
    write_midi(recon, os.path.join(RESULTS, 'exp2_A_decoded.mid'))
    print('\ndecoded A\' (greedy) vs A :', format_metrics(
        compare(MOTIF_A_PITCHES, note_sequence_to_step_pitches(recon))))

    h_orig = sha256(os.path.join(RESULTS, 'exp2_A_original.mid'))
    h_deco = sha256(os.path.join(RESULTS, 'exp2_A_decoded.mid'))
    print('original A  sha256:', h_orig)
    print('decoded A\'  sha256:', h_deco)
    print('byte-identical?     ', h_orig == h_deco)

    k, _ = eng.nearest_anchor([0.0, 0.0])
    print('\nnearest_anchor(ball at A) =', k,
          '-> return original MIDI of anchor "%s"' % eng.anchors[k].id)
    print('CONCLUSION: at an anchor we bypass the decoder and return the '
          'original clip byte-for-byte.')


if __name__ == '__main__':
    main()
