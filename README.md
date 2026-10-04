# MidiSpace

**[English](#english) · [中文](#中文) · [Italiano](#italiano)**

A VST3 / Standalone MIDI generator that turns a 2D control space into melodies via Magenta's **MusicVAE** latent-space interpolation.

---

## English

### What it does

Drag a ball across a circular space between user-defined "melody nodes". Each node holds a 2-bar monophonic melody; the ball's position blends their latent vectors, and MusicVAE decodes the result into a new melody sent out as MIDI to any instrument in your DAW.

```
2D space → SpatialEngine weights → MusicVAE latent interpolation → decode → MIDI melody
```

### Model

Magenta's pretrained [`cat-mel_2bar_big`](https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-mel_2bar_big.tar) — 2 bars, monophonic melody, z=512.

### How to use

1. Load **MidiSpace** in your DAW and route its **MIDI Out Port** to an instrument's **MIDI In Port** (e.g. Analog V, or any synth).
2. **Drag the ball** — it blends between nodes and generates a new melody when you release it.
3. **Click a node** to select it (white outline), then:
   - **Arp** — assign a preset arpeggio: pick a **Root** note, choose **Chord** or **Scale** mode, and pick the **Type** (the type list changes with the mode — chord variants like Maj7/sus4, or scales/modes like Dorian/Phrygian).
   - **Capture** — record your own melody: click Capture, write notes in the DAW piano roll, press play for ~2 bars. It records into the selected node.
4. **Key / Density / Min / Max** sliders — transpose the result, thin the note density, and fold pitches into a range.
5. **Add result** — turn the currently generated melody into a new node.

### Notes

- **Play / Stop / Regen** control the loop. The plugin also follows the host transport: pressing your DAW's stop button stops the loop (detected via MIDI all-notes-off).
- The new melody switches seamlessly — it waits for the current note to finish before the new one starts.
- Note names are shown in FL Studio's octave convention (middle C = C5).

### Install (end users)

Download the release zip, extract, right-click `install.ps1` → **Run with PowerShell** (admin). The inference server (`midispace_server.exe` + model) is installed to `%ProgramData%\MidiSpace\server\` and launches automatically when the plugin loads.

### Build from source

See [BUILD.md](BUILD.md).

---

## 中文

### 功能

在一个圆形 2D 空间里，把球拖到几个「旋律节点」之间。每个节点保存一段 2 小节的单音旋律；球的位置对节点的潜向量（latent）做插值，再由 MusicVAE 解码成一段新的旋律，以 MIDI 输出到你 DAW 里的任意乐器。

```
2D 空间 → SpatialEngine 权重 → MusicVAE 潜空间插值 → 解码 → MIDI 旋律
```

### 模型

Magenta 预训练模型 [`cat-mel_2bar_big`](https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-mel_2bar_big.tar) — 2 小节、单音旋律、z=512。

### 使用方法

1. 在 DAW 里加载 **MidiSpace**，把它的 **MIDI Out Port** 路由到一个乐器的 **MIDI In Port**（如 Analog V，或任意合成器）。
2. **拖球** —— 松开时在节点之间插值，生成新的旋律。
3. **点选一个节点**（白色描边），然后：
   - **Arp** —— 套用预设琶音：选 **Root** 根音，选 **Chord** 或 **Scale** 模式，再选 **Type**（类型列表随模式变化 —— 和弦模式是 Maj7/sus4 等变体，音阶模式是 Dorian/Phrygian 等调式）。
   - **Capture** —— 录制你自己的旋律：点 Capture，在 DAW 钢琴卷帘写音符，按播放约 2 小节，即录入选中的节点。
4. **Key / Density / Min / Max** 滑块 —— 移调、稀疏化音符密度、把音高折叠到指定范围。
5. **Add result** —— 把当前生成的旋律变成一个新节点。

### 说明

- **Play / Stop / Regen** 控制循环。插件也跟随宿主 transport：按 DAW 的停止键会停止循环（通过 MIDI all-notes-off 检测）。
- 新旋律会丝滑切换 —— 等当前音符自然结束后才接入新旋律。
- 音名按 FL Studio 的八度标注显示（中央 C = C5）。

### 安装（最终用户）

下载 release 压缩包，解压，右键 `install.ps1` → **使用 PowerShell 运行**（管理员）。推理服务器（`midispace_server.exe` + 模型）安装到 `%ProgramData%\MidiSpace\server\`，插件加载时自动启动。

### 从源码构建

见 [BUILD.md](BUILD.md)。

---

## Italiano

### Cosa fa

Trascina una pallina in uno spazio 2D circolare tra "nodi di melodia" definiti dall'utente. Ogni nodo contiene una melodia monofonica di 2 battute; la posizione della pallina fonde i loro vettori latenti, e MusicVAE decodifica il risultato in una nuova melodia inviata come MIDI a qualsiasi strumento nel DAW.

```
Spazio 2D → pesi SpatialEngine → interpolazione latente MusicVAE → decodifica → melodia MIDI
```

### Modello

Modello pre-addestrato Magenta [`cat-mel_2bar_big`](https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-mel_2bar_big.tar) — 2 battute, melodia monofonica, z=512.

### Come si usa

1. Carica **MidiSpace** nel DAW e instrada la sua **MIDI Out Port** verso la **MIDI In Port** di uno strumento (es. Analog V, o qualsiasi synth).
2. **Trascina la pallina** — fonde i nodi e genera una nuova melodia al rilascio.
3. **Clicca un nodo** per selezionarlo (contorno bianco), poi:
   - **Arp** — assegna un arpeggio preimpostato: scegli la **Root**, la modalità **Chord** o **Scale**, e il **Type** (la lista cambia con la modalità — varianti di accordi come Maj7/sus4, oppure scale/modi come Dorio/Frigio).
   - **Capture** — registra la tua melodia: clicca Capture, scrivi le note nel piano roll del DAW, premi play per ~2 battute. Viene registrata nel nodo selezionato.
4. Slider **Key / Density / Min / Max** — traspone, dirada la densità e ripiega le altezze in un intervallo.
5. **Add result** — trasforma la melodia generata in un nuovo nodo.

### Note

- **Play / Stop / Regen** controllano il loop. Il plugin segue anche il transport dell'host: premendo stop nel DAW il loop si ferma (rilevato via MIDI all-notes-off).
- La nuova melodia cambia senza strappi — attende che finisca la nota corrente prima di iniziare.
- I nomi delle note seguono la convenzione di ottava di FL Studio (do centrale = C5).

### Installazione (utenti finali)

Scarica lo zip della release, estrailo, clicca col destro su `install.ps1` → **Esegui con PowerShell** (amministratore). Il server di inferenza (`midispace_server.exe` + modello) viene installato in `%ProgramData%\MidiSpace\server\` e si avvia automaticamente al caricamento del plugin.

### Compilare dai sorgenti

Vedi [BUILD.md](BUILD.md).
