"""End-to-end HTTP protocol test for the MusicVAE server.

Mirrors exactly what the C++ HttpModelInterface does:
  encode motif A/B -> latent;  interpolate midpoint;  decode -> notes.
"""
import json
import urllib.request

BASE = 'http://127.0.0.1:8765'


def get(path):
    return json.loads(urllib.request.urlopen(BASE + path, timeout=60).read())


def post(path, obj):
    req = urllib.request.Request(
        BASE + path,
        data=json.dumps(obj).encode('utf-8'),
        headers={'Content-Type': 'application/json'})
    return json.loads(urllib.request.urlopen(req, timeout=120).read())


A = [[60, 0, 1], [64, 1, 2], [67, 2, 3], [72, 3, 4], [67, 4, 5], [64, 5, 6], [60, 6, 8]]
B = [[67, 0, 0.5], [65, 0.5, 1], [64, 1, 2], [62, 2, 3], [60, 3, 4], [59, 4, 5], [57, 5, 6], [55, 6, 8]]

print('health:', get('/health'))

za = post('/encode', {'notes': A})['z']
zb = post('/encode', {'notes': B})['z']
print('encode OK: zA dim = %d, zB dim = %d' % (len(za), len(zb)))

zmid = [0.5 * za[i] + 0.5 * zb[i] for i in range(len(za))]
out = post('/decode', {'z': zmid, 'temperature': 0.5})
print('decode notes (A/B midpoint):', out['notes'])
