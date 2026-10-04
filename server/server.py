"""Local MusicVAE HTTP server for the MidiSpace plugin.

JSON over HTTP (no MIDI-file transfer):

  POST /encode  {"notes": [[pitch, start_quarter, end_quarter], ...]}  -> {"z": [512 floats]}
  POST /decode  {"z": [512 floats], "temperature": 0.5}               -> {"notes": [[...], ...]}
  GET  /health  -> {"status": "ok"}
"""
import json
import os
import sys
import threading

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'phase1_validate', 'src'))

from model import MusicVAEModel
from midi_io import make_note_sequence, note_sequence_to_step_pitches
from paths import find_checkpoint_prefix

MODEL = None
LOCK = threading.Lock()


def get_model():
    global MODEL
    if MODEL is None:
        ckpt = find_checkpoint_prefix()
        print('loading model from', ckpt, flush=True)
        MODEL = MusicVAEModel(ckpt)
        print('model loaded', flush=True)
    return MODEL


def notes_to_note_sequence(notes):
    return make_note_sequence([(int(p), float(sq), float(eq)) for (p, sq, eq) in notes])


def step_pitches_to_notes(pitches, steps_per_quarter=4):
    """Merge a per-step pitch list into (pitch, start_quarter, end_quarter) events."""
    events = []
    n = len(pitches)
    i = 0
    while i < n:
        p = pitches[i]
        if p < 0:
            i += 1
            continue
        j = i
        while j < n and pitches[j] == p:
            j += 1
        events.append([int(p), i / float(steps_per_quarter), j / float(steps_per_quarter)])
        i = j
    return events


class Handler(BaseHTTPRequestHandler):
    def _send(self, obj, code=200):
        body = json.dumps(obj).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == '/health':
            self._send({'status': 'ok'})
        else:
            self._send({'error': 'not found'}, 404)

    def do_POST(self):
        try:
            length = int(self.headers.get('Content-Length', 0))
            req = json.loads(self.rfile.read(length).decode('utf-8'))
        except Exception as e:
            self._send({'error': 'bad request: %s' % e}, 400)
            return

        model = get_model()
        try:
            if self.path == '/encode':
                with LOCK:
                    z = model.encode_mean([notes_to_note_sequence(req['notes'])])[0]
                self._send({'z': z.tolist()})
            elif self.path == '/decode':
                import numpy as np
                z = np.asarray(req['z'], dtype=np.float32)[None, :]
                temp = float(req.get('temperature', 0.5))
                with LOCK:
                    ns = model.decode(z, temperature=temp)[0]
                pitches = note_sequence_to_step_pitches(ns)
                self._send({'notes': step_pitches_to_notes(pitches)})
            else:
                self._send({'error': 'not found'}, 404)
        except Exception as e:
            self._send({'error': repr(e)}, 500)

    def log_message(self, *args):
        pass


def main():
    port = int(os.environ.get('MIDISPACE_PORT', '8765'))
    get_model()
    print('MusicVAE server listening on 127.0.0.1:%d' % port, flush=True)
    ThreadingHTTPServer(('127.0.0.1', port), Handler).serve_forever()


if __name__ == '__main__':
    main()
