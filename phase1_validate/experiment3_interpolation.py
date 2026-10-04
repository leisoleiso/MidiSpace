"""Experiment 3 — Latent interpolation continuity A -> B.

Encode A and B to posterior means mu_a, mu_b; walk the latent path at
t = 0, .25, .5, .75, 1.0; decode each point; report similarity to A and to B
plus step-to-step change.  Runs BOTH spherical (slerp, the magenta-default)
and linear (lerp) interpolation so we can compare continuity.  Writes MIDI.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

from paths import find_checkpoint_prefix, RESULTS                            # noqa: E402
from motifs import MOTIF_A, MOTIF_B, MOTIF_A_PITCHES, MOTIF_B_PITCHES       # noqa: E402
from midi_io import make_note_sequence, note_sequence_to_step_pitches, write_midi  # noqa: E402
from metrics import compare, edit_distance                                   # noqa: E402
from model import MusicVAEModel                                              # noqa: E402


def main():
    m = MusicVAEModel(find_checkpoint_prefix())
    os.makedirs(RESULTS, exist_ok=True)

    ns_a = make_note_sequence(MOTIF_A)
    ns_b = make_note_sequence(MOTIF_B)
    mu = m.encode_mean([ns_a, ns_b])      # shape [2, z_size]
    mu_a, mu_b = mu[0], mu[1]

    ts = [0.0, 0.25, 0.5, 0.75, 1.0]
    print('\n=== Experiment 3: latent interpolation A -> B ===')

    for method in ['slerp', 'lerp']:
        print('\n--- %s ---' % method)
        prev = None
        for t in ts:
            z = m.interpolate(mu_a, mu_b, t, method=method)
            ns = m.decode(z[None, :], temperature=0.5)[0]
            pitches = note_sequence_to_step_pitches(ns)
            da = compare(MOTIF_A_PITCHES, pitches)
            db = compare(MOTIF_B_PITCHES, pitches)
            delta = '' if prev is None else '  step_delta(edit)=%d' % edit_distance(prev, pitches)
            print('t=%4.2f  sim_to_A=%.3f  sim_to_B=%.3f  %s%s'
                  % (t, da['pitch_match_ratio'], db['pitch_match_ratio'], pitches, delta))
            prev = pitches
            write_midi(ns, os.path.join(RESULTS, 'exp3_%s_t%03d.mid'
                                        % (method, int(round(t * 100)))))

    print('\n(intermediate MIDI written to results/ — listen for continuous A->B morph)')


if __name__ == '__main__':
    main()
