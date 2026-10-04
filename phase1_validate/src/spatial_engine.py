"""SpatialEngine v1 — pure geometry + weights, fully model-agnostic.

This is the module that will be ported to C++ in Phase 3.  It knows NOTHING
about MusicVAE, MIDI, or audio.  It maps a ball position in the user-defined
2D space to a set of influence weights over anchors.

Design constraint (anchor preservation): when the ball is at (or within
snap_eps of) an anchor, weights become one-hot for that anchor.  The caller
uses that to return the original anchor MIDI instead of decoding.
"""
import numpy as np


class Anchor:
    __slots__ = ('id', 'position')

    def __init__(self, anchor_id, position):
        self.id = anchor_id
        self.position = np.asarray(position, dtype=float)


class SpatialEngine:
    def __init__(self, anchors):
        self.anchors = list(anchors)

    @property
    def positions(self):
        return np.array([a.position for a in self.anchors], dtype=float)

    def nearest_anchor(self, ball_pos, snap_eps=1e-3):
        """Return (index, distance) of the nearest anchor.

        Returns (None, inf) when the ball is not within snap_eps of any anchor.
        """
        if not self.anchors:
            return None, float('inf')
        d = np.linalg.norm(self.positions - np.asarray(ball_pos, dtype=float), axis=1)
        k = int(np.argmin(d))
        if d[k] <= snap_eps:
            return k, float(d[k])
        return None, float(d[k])

    def weights(self, ball_pos, method='idw', power=2.0, snap_eps=1e-3):
        """Influence weights over anchors, summing to 1.0.

        method: 'idw' (inverse-distance weighting, default) or 'nearest'.
        Within snap_eps of an anchor the result is one-hot (identity).
        More methods (gaussian, barycentric, local) can be added later without
        touching any other module.
        """
        if not self.anchors:
            return np.array([], dtype=float)
        k, _ = self.nearest_anchor(ball_pos, snap_eps=snap_eps)
        if k is not None:
            w = np.zeros(len(self.anchors), dtype=float)
            w[k] = 1.0
            return w

        d = np.linalg.norm(self.positions - np.asarray(ball_pos, dtype=float), axis=1)
        if method == 'idw':
            with np.errstate(divide='ignore', invalid='ignore'):
                inv = 1.0 / np.maximum(d, 1e-12) ** power
            return inv / inv.sum()
        if method == 'nearest':
            w = np.zeros(len(self.anchors), dtype=float)
            w[int(np.argmin(d))] = 1.0
            return w
        raise ValueError('unknown weighting method: %r' % method)
