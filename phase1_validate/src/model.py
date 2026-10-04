"""ModelInterface-style wrapper around magenta MusicVAE (cat-mel_2bar_big).

This is the Phase-1 Python prototype of the C++ ModelInterface that the JUCE
plugin will call.  It owns exactly three concerns:

    encode(clips)   -> latent vectors
    decode(z)       -> clips
    interpolate(...)-> latent walk

No spatial, UI, timing, or MIDI-timing logic belongs here.
"""
import numpy as np


class MusicVAEModel:
    def __init__(self, checkpoint, config_name='cat-mel_2bar_big',
                 batch_size=4, length=32):
        """checkpoint: path to the checkpoint PREFIX (e.g. .../cat-mel_2bar_big.ckpt)."""
        from magenta.models.music_vae import TrainedModel, configs
        config = configs.CONFIG_MAP[config_name]
        self._config_name = config_name
        self._length = int(length)
        self.z_size = int(config.hparams.z_size)
        self._model = TrainedModel(config, batch_size=batch_size,
                                   checkpoint_dir_or_path=checkpoint)

    def encode(self, note_sequences):
        """Encode a list of NoteSequence -> (z, mu, sigma), each [n, z_size]."""
        z, mu, sigma = self._model.encode(list(note_sequences))
        return np.asarray(z), np.asarray(mu), np.asarray(sigma)

    def encode_mean(self, note_sequences):
        """Deterministic latent (posterior mean) used for interpolation."""
        return self.encode(note_sequences)[1]

    def decode(self, z, temperature=1.0):
        """Decode z ndarray [n, z_size] -> list of NoteSequence (length n)."""
        z = np.asarray(z, dtype=np.float32)
        if z.ndim == 1:
            z = z[None, :]
        return self._model.decode(z, length=self._length, temperature=temperature)

    @staticmethod
    def slerp(a, b, t):
        """Spherical linear interpolation (same method as magenta's interpolate)."""
        a = np.asarray(a, dtype=np.float64)
        b = np.asarray(b, dtype=np.float64)
        na = np.linalg.norm(a)
        nb = np.linalg.norm(b)
        if na == 0.0 or nb == 0.0:
            return (1.0 - t) * a + t * b
        dot = float(np.clip(np.dot(a / na, b / nb), -1.0, 1.0))
        omega = np.arccos(dot)
        if np.isclose(omega, 0.0):
            return (1.0 - t) * a + t * b
        so = np.sin(omega)
        return (np.sin((1.0 - t) * omega) / so) * a + (np.sin(t * omega) / so) * b

    def interpolate(self, mu_a, mu_b, t, method='slerp'):
        """Interpolate two latent means. method: 'slerp' (default) or 'lerp'."""
        if method == 'slerp':
            return self.slerp(mu_a, mu_b, t)
        if method == 'lerp':
            return (1.0 - t) * np.asarray(mu_a, float) + t * np.asarray(mu_b, float)
        raise ValueError('unknown interpolation method: %r' % method)
