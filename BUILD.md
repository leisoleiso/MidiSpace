# Building MidiSpace from source

MidiSpace has two buildable parts: the **JUCE plugin** (C++) and the **inference server** (Python, optionally bundled to an exe with PyInstaller).

## Prerequisites

### Plugin (C++)

- Windows 10/11 x64
- Visual Studio 2022 (C++ desktop workload)
- CMake ≥ 3.22
- [JUCE](https://github.com/juce-framework/JUCE) (tested with JUCE 9)

### Inference server (Python)

- Python 3.7 (TensorFlow 1.15 is not available for newer Python on Windows)
- A conda/virtualenv with:
  - `tensorflow==1.15.0`
  - `magenta==1.1.7` (installed with `--no-deps`)
  - `numpy==1.21.6`

## 1. Get the model

```powershell
mkdir phase1_validate\checkpoints
curl -L -o phase1_validate\checkpoints\cat-mel_2bar_big.tar `
  "https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-mel_2bar_big.tar"
tar -xf phase1_validate\checkpoints\cat-mel_2bar_big.tar -C phase1_validate\checkpoints
```

Extracts `cat-mel_2bar_big.ckpt.index` + `cat-mel_2bar_big.ckpt.data-00000-of-00001`.

## 2. Build the plugin

Edit `plugin/CMakeLists.txt` and point it at your JUCE clone, then:

```powershell
cd plugin
cmake -S . -B ../build -G "Visual Studio 17 2022" -A x64
cmake --build ../build --config Release --target MidiSpacePlugin_VST3 MidiSpacePlugin_Standalone
```

Outputs:

- `../build/MidiSpacePlugin_artefacts/Release/VST3/MidiSpace.vst3`
- `../build/MidiSpacePlugin_artefacts/Release/Standalone/MidiSpace.exe`

## 3. Run the inference server (development)

From the `midi-space` root:

```powershell
& "C:\path\to\python.exe" server\server.py
```

It listens on `127.0.0.1:8765` (`/encode`, `/decode`, `/health`).

## 4. Bundle the server into an exe (for distribution)

```powershell
cd server
& "C:\path\to\env\Scripts\pyinstaller.exe" --clean --noconfirm midispace_server.spec
```

Produces `server/dist/midispace_server.exe` (~400 MB). The model is **not** bundled inside the exe — it sits next to it in `checkpoints/`.

## 5. Package and install

Place these side by side:

```
MidiSpace.vst3
midispace_server.exe
checkpoints/   (the two .ckpt files)
install.ps1
```

Run `install.ps1` as administrator. It copies the VST3 to `C:\Program Files\Common Files\VST3\` and the server + model to `%ProgramData%\MidiSpace\server\`.

## How the plugin finds the server

On load, the plugin checks `http://127.0.0.1:8765/health`; if down, it launches `midispace_server.exe` from `%ProgramData%\MidiSpace\server\` and kills it on unload.

## Troubleshooting

- **"Latent load failed"** — the server isn't running or the model isn't found. The plugin retries the connection for up to 60 s while the server loads its 1.5 GB model.
- **No sound** — check the MIDI port routing.
- **Wrong tempo capture** — ensure the DAW is playing during capture so the host BPM is reported.
