# MidiSpace JUCE 插件（Phase 2–4 原型）

用户定义的 2D 空间 → Ball 位置 → SpatialEngine 权重 → MusicVAE latent 插值 → 解码 → MIDI。

## 目录

- `src/SpatialEngine.h` — 空间权重引擎（IDW / nearest，anchor 就近 one-hot）
- `src/ModelInterface.h` — 抽象模型边界（encode/decode）
- `src/HttpModelInterface.h/.cpp` — 通过 HTTP 调用本地 Python 服务
- `src/GenerationManager.h/.cpp` — Ball 释放 → 400ms debounce → 后台线程解码
- `src/CanvasComponent.h/.cpp` — Canvas 渲染 + 拖拽 + 实时权重显示
- `src/Motifs.h` — 三个 anchor motif（A/B/C）
- `src/PluginProcessor.*` / `src/PluginEditor.*` — 插件骨架 + 接线

## 依赖

- 本地 MusicVAE 服务：`../server/server.py`（先启动，监听 127.0.0.1:8765）
- JUCE 框架：`../JUCE`（git clone）

## 构建与运行

```powershell
# 1) 启动服务（首次加载 1.6GB 模型约 40s）
& "C:\Users\26212\.conda\envs\musicvae\python.exe" "..\server\server.py"

# 2) 配置 + 编译
cmake -S . -B ../build -G "Visual Studio 17 2022" -A x64
cmake --build ../build --config Release --target MidiSpacePlugin_Standalone --parallel

# 3) 运行
..\build\MidiSpacePlugin_artefacts\Release\Standalone\MidiSpace.exe
```

窗口：拖 Ball 释放后，顶部状态会显示 `Generating... → Ready`，绿色文字显示生成的音符名
（如 `F4 E4 G4 E4 C4 A3`）。把 Ball 拖到某个节点上，权重变 one-hot，解码结果即该节点 motif。

## VST3 构建 + FL Studio 加载

VST3 格式已编译并安装到 `C:\Program Files\Common Files\VST3\MidiSpace.vst3`。

```powershell
# 编译 VST3（JUCE 9 已内置完整 VST3 SDK，无需外部 SDK）
cmake --build ../build --config Release --target MidiSpacePlugin_VST3 --parallel

# 安装到系统 VST3 目录（需要管理员，会弹 UAC）
# 方式一：提权复制
$s = "C:\Users\26212\midi-space\build\MidiSpacePlugin_artefacts\Release\VST3\MidiSpace.vst3"
Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile','-Command',"Copy-Item -Recurse -Force '$s' 'C:\Program Files\Common Files\VST3\'"
```

FL Studio 加载步骤：

1. **先启动本地服务**：`& "C:\Users\26212\.conda\envs\musicvae\python.exe" "..\server\server.py"`（否则生成会报失败）
2. FL Studio 21 → `Options → Manage plugins`
3. 确认 `VST 3` 搜索路径里有 `C:\Program Files\Common Files\VST3`
4. 点 `Find more plugins`（或 `Start scan`）扫描
5. 扫描完成后在插件列表里找到 **MidiSpace**（当前归类为 Effect/Fx），加载到 mixer 即可打开 UI 拖动

> 注意：当前插件是 `Fx`（效果）分类，因为还没输出 MIDI/音频。Phase 5 加入 MIDI 输出后，
> 会切到 `IS_SYNTH TRUE`（instrument/发生器），FL 才能把它当发生器、把生成的 MIDI 路由给其它音源。
