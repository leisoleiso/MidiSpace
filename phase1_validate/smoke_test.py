"""Smoke test: import model, load checkpoint, encode/decode one motif.

Run after dependencies + checkpoint are in place:
    python smoke_test.py
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

from paths import find_checkpoint_prefix, CONFIG_NAME, RESULTS       # noqa: E402
from motifs import MOTIF_A                                           # noqa: E402
from midi_io import make_note_sequence, write_midi                   # noqa: E402
from model import MusicVAEModel                                      # noqa: E402


def main():
    ckpt = find_checkpoint_prefix()
    print('config        :', CONFIG_NAME)
    print('checkpoint    :', ckpt)
    assert ckpt, 'no checkpoint found under checkpoints/'

    m = MusicVAEModel(ckpt)
    print('model loaded  : z_size =', m.z_size)

    ns_a = make_note_sequence(MOTIF_A)
    mu = m.encode_mean([ns_a])
    print('mu shape      :', mu.shape)

    out = m.decode(mu, temperature=0.01)
    print('decoded       :', len(out), 'sequence(s)')

    os.makedirs(RESULTS, exist_ok=True)
    write_midi(out[0], os.path.join(RESULTS, 'smoke_a_recon.mid'))
    print('SMOKE OK')


if __name__ == '__main__':
    main()
