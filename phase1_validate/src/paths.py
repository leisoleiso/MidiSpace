"""Central path/config constants for Phase 1 validation."""
import os
import sys

# When frozen by PyInstaller, resolve checkpoints relative to the executable;
# otherwise use the source-tree layout.
if getattr(sys, 'frozen', False):
    ROOT = os.path.dirname(os.path.abspath(sys.executable))
else:
    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRC = os.path.join(ROOT, 'src')
CHECKPOINTS = os.path.join(ROOT, 'checkpoints')
ASSETS = os.path.join(ROOT, 'assets')
RESULTS = os.path.join(ROOT, 'results')

# Official Magenta 2-bar monophonic melody model (TF1 checkpoint).
CONFIG_NAME = 'cat-mel_2bar_big'
CHECKPOINT_URL = (
    'https://storage.googleapis.com/magentadata/models/'
    'music_vae/checkpoints/cat-mel_2bar_big.tar'
)

# 2 bars * 4 quarters/bar * 4 steps/quarter = 32 steps.
N_STEPS = 32
STEPS_PER_QUARTER = 4


def find_checkpoint_prefix():
    """Return the checkpoint prefix path (e.g. .../cat-mel_2bar_big.ckpt).

    The tar extracts only a *.ckpt.index + *.ckpt.data-* pair (no TF
    'checkpoint' state file, no .meta).  TF's Saver.restore accepts the prefix
    directly, which is what magenta's TrainedModel expects when passed a
    non-directory path.
    """
    if not os.path.isdir(CHECKPOINTS):
        return None
    for dirpath, _dirnames, filenames in os.walk(CHECKPOINTS):
        for f in filenames:
            if f.endswith('.index'):
                return os.path.join(dirpath, f[:-len('.index')])
    return None
