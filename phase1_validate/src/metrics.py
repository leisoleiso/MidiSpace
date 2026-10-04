"""Quantitative similarity metrics between two monophonic step-pitch lists.

All functions take/return plain Python/numpy values.  Used by Experiment 1
(reconstruction fidelity) and Experiment 3 (interpolation continuity).
"""
import numpy as np


def onset_pattern(pitches):
    """Boolean array: True where a new note starts (pitch differs from previous)."""
    p = np.asarray(pitches, dtype=int)
    onsets = np.zeros(len(p), dtype=bool)
    if len(p) == 0:
        return onsets
    onsets[0] = p[0] >= 0
    for i in range(1, len(p)):
        onsets[i] = p[i] >= 0 and p[i] != p[i - 1]
    return onsets


def pitch_histogram(pitches, n=12):
    """Normalised pitch-class histogram (MIDI pitch mod 12)."""
    p = np.asarray(pitches, dtype=int)
    p = p[p >= 0]
    hist = np.zeros(n, dtype=float)
    for v in p:
        hist[int(v) % n] += 1.0
    total = hist.sum()
    if total > 0:
        hist /= total
    return hist


def cosine(a, b):
    a = np.asarray(a, dtype=float)
    b = np.asarray(b, dtype=float)
    na, nb = np.linalg.norm(a), np.linalg.norm(b)
    if na == 0.0 or nb == 0.0:
        return 0.0
    return float(a @ b / (na * nb))


def edit_distance(a, b):
    """Levenshtein distance over the full pitch list (rests = -1)."""
    a = [int(x) for x in a]
    b = [int(x) for x in b]
    m, n = len(a), len(b)
    dp = np.zeros((m + 1, n + 1), dtype=int)
    for i in range(m + 1):
        dp[i, 0] = i
    for j in range(n + 1):
        dp[0, j] = j
    for i in range(1, m + 1):
        for j in range(1, n + 1):
            cost = 0 if a[i - 1] == b[i - 1] else 1
            dp[i, j] = min(dp[i - 1, j] + 1, dp[i, j - 1] + 1, dp[i - 1, j - 1] + cost)
    return int(dp[m, n])


def _f1(a, b):
    a = np.asarray(a, dtype=bool)
    b = np.asarray(b, dtype=bool)
    tp = float((a & b).sum())
    fp = float((a & ~b).sum())
    fn = float((~a & b).sum())
    if tp + fp == 0.0 or tp + fn == 0.0:
        return 0.0
    p = tp / (tp + fp)
    r = tp / (tp + fn)
    if p + r == 0.0:
        return 0.0
    return 2.0 * p * r / (p + r)


def compare(pitches_a, pitches_b):
    """Compare two step-pitch lists; returns a dict of similarity measures."""
    a = np.asarray(pitches_a, dtype=int)
    b = np.asarray(pitches_b, dtype=int)
    n = max(len(a), len(b))
    aa = np.full(n, -1, dtype=int)
    bb = np.full(n, -1, dtype=int)
    aa[:len(a)] = a
    bb[:len(b)] = b

    sounding = (aa >= 0) & (bb >= 0)
    exact_step_match = float((aa == bb).mean())
    pitch_match_ratio = float((aa == bb)[sounding].mean()) if sounding.any() else 0.0

    onset_a = onset_pattern(aa)
    onset_b = onset_pattern(bb)
    onset_f1 = _f1(onset_a, onset_b)
    hist_cos = cosine(pitch_histogram(aa), pitch_histogram(bb))
    lev = edit_distance(aa.tolist(), bb.tolist())

    return {
        'exact_step_match': exact_step_match,
        'pitch_match_ratio': pitch_match_ratio,
        'onset_f1': onset_f1,
        'pitch_class_cosine': hist_cos,
        'note_count_a': int(onset_a.sum()),
        'note_count_b': int(onset_b.sum()),
        'edit_distance': lev,
    }


def format_metrics(d):
    """One-line human-readable summary of a compare() dict."""
    return (
        f"exact_step={d['exact_step_match']:.3f} "
        f"pitch_match={d['pitch_match_ratio']:.3f} "
        f"onset_f1={d['onset_f1']:.3f} "
        f"pitchclass_cos={d['pitch_class_cosine']:.3f} "
        f"notes={d['note_count_a']}/{d['note_count_b']} "
        f"edit_dist={d['edit_distance']}"
    )
