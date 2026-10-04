# -*- mode: python ; coding: utf-8 -*-
# PyInstaller spec for the MidiSpace melody server.
#
# Build with:
#   pyinstaller --clean --noconfirm midispace_server.spec
#
# The melody model (cat-mel_2bar_big, 1.5 GB) is NOT bundled: it lives next to
# the exe in checkpoints/ (see phase1_validate/src/paths.py).

import os

block_cipher = None

SRC = r'C:\Users\26212\midi-space\phase1_validate\src'

hiddenimports = [
    'magenta.models.music_vae',
    'magenta.models.music_vae.configs',
    'magenta.models.music_vae.lstm_models',
    'magenta.models.music_vae.data',
    'magenta.models.music_vae.base_model',
    'magenta.music.melodies_lib',
    'magenta.music.midi_io',
    'magenta.music.protobuf.music_pb2',
    'magenta.music.sequences_lib',
    'magenta.music.constants',
    'magenta.pipelines.melody_pipelines',
    'magenta.pipelines.statistics',
    'model',
    'midi_io',
    'paths',
    'astor',
    'gast',
]

a = Analysis(
    ['server.py'],
    pathex=[os.path.abspath('.'), SRC],
    binaries=[],
    datas=[],
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='midispace_server',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
