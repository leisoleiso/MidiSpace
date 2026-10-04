"""Experiment 1 — Reconstruction fidelity.

For each motif X in {A, B}:  X -> encoder -> zX -> decoder -> X'
Measure how close X' is to X (note identity, pitch class, rhythm, note count)
at three decode temperatures, and time encode/decode (relevant to Phase 4's
interaction-latency requirement).  Writes reconstructions to results/.
"""
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

from paths import find_checkpoint_prefix, RESULTS                            # noqa: E402
from motifs import MOTIF_A, MOTIF_B, MOTIF_A_PITCHES, MOTIF_B_PITCHES       # noqa: E402
from midi_io import make_note_sequence, note_sequence_to_step_pitches, write_midi  # noqa: E402
from metrics import compare, format_metrics                                  # noqa: E402
from model import MusicVAEModel                                              # noqa: E402


def main():
    m = MusicVAEModel(find_checkpoint_prefix())
    os.makedirs(RESULTS, exist_ok=True)

    # Time the pipeline once (single encode + single decode).
    ns_a = make_note_sequence(MOTIF_A)
    t0 = time.time()
    mu_a = m.encode_mean([ns_a])
    t_enc = time.time() - t0
    t0 = time.time()
    _ = m.decode(mu_a, temperature=0.01)
    t_dec = time.time() - t0
    print('\n[latency] encode = %.3f s   decode = %.3f s' % (t_enc, t_dec))

    for name, events, truth in [('A', MOTIF_A, MOTIF_A_PITCHES),
                                ('B', MOTIF_B, MOTIF_B_PITCHES)]:
        ns = make_note_sequence(events)
        mu = m.encode_mean([ns])
        print('\n=== Motif %s : A -> zA -> A\' ===' % name)
        print('  ground truth :', truth)
        for temp, label in [(0.01, 'greedy'), (0.5, 'temp0.5'), (1.0, 'temp1.0')]:
            recon = m.decode(mu, temperature=temp)[0]
            pitches = note_sequence_to_step_pitches(recon)
            mets = compare(truth, pitches)
            print('  [%-8s] %s' % (label, format_metrics(mets)))
            print('             ', pitches)
            write_midi(recon, os.path.join(RESULTS, 'exp1_%s_recon_%s.mid' % (name, label)))


if __name__ == '__main__':
    main()
