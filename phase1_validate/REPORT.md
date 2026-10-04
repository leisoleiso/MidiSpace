# Phase 1 验证报告 — MusicVAE 重建 / anchor / 插值

日期：2026-09-23　模型：`cat-mel_2bar_big`（官方 2-bar 单声部 melody，z_size=512，checkpoint 1.6 GB）

---

## 0. 环境（已固化，可复现）

| 项 | 值 |
|---|---|
| conda 环境 | `musicvae`（隔离，不动 base） |
| Python | 3.7.12 |
| TensorFlow | 1.15.0（CPU） |
| numpy | 1.21.6 |
| magenta | 1.1.7（`--no-deps` 安装，仅补 music_vae 运行时所需依赖） |
| 其它关键 | protobuf 3.20.3, pretty_midi, mido, intervaltree, music21, matplotlib, IPython, bokeh, librosa, pandas, tensorflow-probability 0.7.0, tensorflow-datasets 2.1.0 |

完整依赖见 `requirements.txt`。重建环境：

```powershell
conda create -n musicvae python=3.7 pip -y
& "C:\Users\26212\.conda\envs\musicvae\python.exe" -m pip install tensorflow==1.15.0 numpy==1.19.5 protobuf==3.20.3
& "...\python.exe" -m pip install magenta==1.1.7 --no-deps
& "...\python.exe" -m pip install -r requirements.txt   # 其余运行时依赖
```

> 注意：`magenta` 是单体包，`pip install magenta` 会拖入 librosa/numba/tensor2tensor/gym/sox/sonnet 等与
> music_vae 无关的重依赖并触发源码编译失败。正确做法是 `--no-deps` + 按需补依赖（本报告已趟通）。

---

## 1. Experiment 1 — Reconstruction（重建保真度）

`A → encoder → zA → decoder → A'`，量化 A' 与 A 的相似度。

**延迟（CPU，RTX 4060 未启用）**：encode ≈ 0.350 s，decode ≈ 0.766 s（单次往返 ~1.1 s）。

| motif | temperature | exact_step | pitch_match | onset_f1 | pitchclass_cos | edit_dist |
|---|---|---|---|---|---|---|
| A（上行琶音 C4→C5） | greedy(0.01) | **1.000** | 1.000 | 1.000 | 1.000 | **0** |
| A | 0.5 / 1.0 | 0.875 | 0.875 | 0.833 | 0.957 | 4 |
| B（下行音阶 G4→G3） | greedy(0.01) | **1.000** | 1.000 | 1.000 | 1.000 | **0** |
| B | 0.5 | **1.000** | 1.000 | 1.000 | 1.000 | **0** |
| B | 1.0 | 0.938 | 0.938 | 0.933 | 0.985 | 2 |

**结论**：greedy（argmax）重建 **完美逐音复现**（edit_distance=0）。temperature 升高引入的偏差都很小
（A 的 C5 被拉平到 G4、B 的八分音符 pickup 被并入后续音），pitch-class 余弦仍 ≥0.95。
→ 编码器/解码器往返忠实保留音高与节奏，满足「A' 与 A 足够接近」。

---

## 2. Experiment 2 — Anchor 身份保持

SpatialEngine（纯几何，与模型解耦）在 Ball 落到 anchor 时的行为：

| Ball 位置 | 权重 |
|---|---|
| 恰好 A `[0,0]` | `[1.0, 0.0]`（one-hot） |
| 距 A 1e-5（snap 内） | `[1.0, 0.0]` |
| 中点 `[0.5,0]` | `[0.5, 0.5]` |

greedy 解码 A 得到 A'，与原 A **字节级相同**（sha256 一致）——这本身说明重建极强。
但 anchor bypass 仍然必须：它保证「到达 A 就是 A」**不受 temperature/采样随机性影响**，且**零推理延迟**
（根本不调用 decoder）。这是空间身份保持的硬约束，已作为规则写死在 `SpatialEngine.nearest_anchor` + 管线判断中。

---

## 3. Experiment 3 — Latent 插值连续性（A → B）

对 posterior mean `mu` 做插值（temperature=0.5），观察 sim_to_A / sim_to_B 与 step-to-step 变化。

**slerp（magenta 官方默认）**：

| t | sim_to_A | sim_to_B | step_delta(edit) |
|---|---|---|---|
| 0.00 | 1.000 | 0.125 | — |
| 0.25 | 0.750 | 0.188 | 8 |
| 0.50 | 0.250 | 0.188 | 16 |
| 0.75 | 0.125 | 0.438 | 8 |
| 1.00 | 0.125 | **1.000** | 18 |

**lerp**：

| t | sim_to_A | sim_to_B | step_delta(edit) |
|---|---|---|---|
| 0.00 | 0.875 | 0.125 | — |
| 0.25 | 0.875 | 0.125 | **0** |
| 0.50 | 0.500 | 0.188 | 12 |
| 0.75 | 0.125 | 0.562 | 20 |
| 1.00 | 0.125 | **1.000** | 14 |

**结论**：
- 两端点严格还原 A/B（slerp t=0 = A 本身，t=1 = B 本身）。
- sim_to_A 单调下降、sim_to_B 单调上升，中间点是**介于 A/B 之间的新旋律**（非随机），
  实际音符显示音区连续下移（C5 峰值 → A3/G3），结尾音 C4→A3→G3 平滑过渡 —— 音乐「连续地从 A 走向 B」成立。
- **slerp 优于 lerp**：lerp 出现「t=0→0.25 输出完全不变（step_delta=0）再跳变」的黏滞现象，
  slerp 把变化更均匀地铺在路径上（8/16/8/18）。与 magenta 官方用 slerp 的选择一致。

---

## 4. 关键发现与风险

1. **核心概念成立**：用户定义 2D 空间 → 权重 → latent 插值 → 解码，三实验全部通过。
2. **延迟可接受**：CPU 单次生成 ~1.1s，符合「release → 300–500ms debounce → 后台生成」的交互模型
   （拖动期间只算权重，绝不触发推理）。
3. **GPU 限制（重要）**：本机 RTX 4060 Laptop GPU 存在，但 **TF 1.15 需要 CUDA 10.0**（4060 仅支持
   CUDA 11/12），因此第一版只能 CPU。若 Phase 4 追求更低延迟，备选路径：
   - 换更小的 `cat-mel_2bar_small`（若有 checkpoint）或蒸馏；
   - 把 checkpoint 转成 ONNX 跑 GPU / 或迁移到 TF2 重新加载权重。
4. **anchor bypass 是硬规则**：即使 greedy 恰好字节级还原，也永远走「近 anchor 直接返回原 MIDI」。

---

## 5. Phase 2 建议（下一步）

1. 最小 JUCE 原型：Canvas + 3 个 MIDI Node + 可拖 Ball（本阶段**只做位置与权重**，不接 AI）。
2. 把 `spatial_engine.py` 原样移植为 C++ `SpatialEngine`（已解耦、可插拔）。
3. `ModelInterface`（C++ 抽象）沿用 `model.py` 的 `encode/decode/interpolate` 形状；
   Phase 2 先落地「本地 Python 服务」作为 `MusicVAEModel` 的实现，通过 `ModelInterface` 隔离。
4. 交互闭环（Phase 4）：Ball release → debounce → 后台线程 → 服务端推理 → 回传 MIDI → 播放/保存/加为新 node。
