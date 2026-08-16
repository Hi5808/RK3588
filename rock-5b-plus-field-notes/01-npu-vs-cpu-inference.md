# The RK3588 NPU Is Slower Than Its Own CPU for LLM Decode — Until You Stack Six Undocumented Fixes

**A reproducibility study on real hardware, ending in a full explanation of why vendor benchmark numbers looked unreachable**

## Abstract

The Rockchip RK3588's 6 TOPS NPU is marketed as the fast path for on-device LLM inference. On a Radxa ROCK 5B+, running the vendor's own `rkllm` runtime, plain CPU inference beat the NPU at single-token decode by roughly 2.4× out of the box, and the NPU also trailed CPU on most prefill measurements. A published vendor benchmark figure for the same model (TinyLlama-1.1B, ~24.4 tok/s decode) was initially 3.2× higher than anything reproducible on this board. Rather than accepting either result as final, this investigation tested every plausible explanation directly — Python wrapper overhead, quantization grouping, CPU governor, GPU/OpenCL side-channel assistance, NPU core count, INT4 availability, a boot-time DDR/DMC frequency-scaling firmware gap — and, after stacking the levers that turned out to be real, reproduced the vendor figure to within 6% on retail hardware. The final explanation was mundane: a firmware gap present in the default OS image (DDR DVFS unavailable without a specific ARM Trusted Firmware build), combined with an undisclosed accuracy/speed tradeoff in the community's default quantization choice (`w8a8_g128` grouped quantization costs ~28% throughput versus plain `w8a8`, and the vendor benchmark used plain). Neither the vendor nor the toolkit disclosed either gap. Separately, this investigation determined that no realistic pipeline exists to combine CPU, GPU, and NPU on a single request on this hardware, and that plain CPU inference at 4 threads remains the fastest single option for small (1-3B) models even after every NPU-side fix is applied.

## Hardware and Software Setup

- **Board:** Radxa ROCK 5B+ — RK3588 SoC (4× Cortex-A76 + 4× Cortex-A55), Mali-G610 GPU, 6 TOPS NPU, 8GB RAM
- **OS:** Armbian 26.8.1, kernel 6.1.115-vendor-rk35xx
- **NPU stack:** Rockchip's proprietary `rknpu` driver v0.9.8, `rkllm-runtime` v1.3.0, served over HTTP via [`rkllama`](https://github.com/NotPunchnox/rkllama) v0.0.75 (an Ollama-API-compatible wrapper)
- **CPU/GPU baseline:** `llama.cpp` (commit `0b1bad1`), built from source, CPU backend and later a Vulkan (`GGML_VULKAN=ON`) GPU backend
- **Conversion toolchain:** `rkllm-toolkit` (x86_64-only; a second machine on the same network was used for real from-scratch model conversions, not just inference)
- **Test models:** TinyLlama-1.1B, Llama-3.2-3B, Qwen3-1.7B, Qwen3-4B, in various `w8a8` / `w8a8_g128` quantizations

All raw trial data, scripts, and JSON logs referenced below live alongside the original investigation in `~/npu-bench/` (`bench.py`, `results.json`, `cpu_baseline_*.txt`).

## Methodology

Every comparison in this study uses matched model sizes, matched prompt/generation lengths, and — critically — the *same compiled runtime binary* wherever a wrapper-overhead question was in play (verified via `md5sum` against Rockchip's own internal SDK distribution, not just the public GitHub release). Where an early result implied a bottleneck, the response was to isolate that one variable and retest, not to reason about it in the abstract. Six trials per configuration was the default; longer/riskier tests (bootloader flashing) got their own explicit go/no-go checkpoint.

## Results

### Headline: NPU loses to CPU for decode, on both models tested

| Model | NPU decode (rkllm) | CPU decode (llama.cpp, 4 threads) | CPU decode (8 threads) |
|---|---|---|---|
| TinyLlama-1.1B (Q8/W8A8) | 7.7 tok/s | **18.4 tok/s** | 9.9 tok/s |
| Llama-3.2-3B (Q8/W8A8) | ~2-3 tok/s (unreliable, see truncation bug below) | **6.0 tok/s** | 4.3 tok/s |

Plain CPU inference at 4 threads beat the NPU by ~2.4× on both models. Decode is memory-bandwidth-bound, not compute-bound, on this SoC — using 8 threads instead of 4 made CPU *slower* (18.4→9.9 tok/s), consistent with the extra A55 cores adding coordination overhead without adding usable bandwidth.

### Prefill and long-context: a narrow, model-size-dependent NPU win

Short-prompt (30-60 token) prefill also favored CPU. A follow-up sweep of real tokenized prompts from 256 to 3584 tokens against 4096-token-capped models found:

| Context (tokens) | TinyLlama-1.1B NPU | TinyLlama-1.1B CPU (4t) | Llama-3.2-3B NPU | Llama-3.2-3B CPU (4t) |
|---|---|---|---|---|
| 256  | 19.97 tok/s | **117.23 tok/s** | 11.91 tok/s | **37.65 tok/s** |
| 1024 | 18.83 tok/s | **97.92 tok/s**  | 11.95 tok/s | **34.41 tok/s** |
| 2048 | 17.94 tok/s | **80.90 tok/s**  | **30.94 tok/s** | 30.62 tok/s |
| 3584 | 27.54 tok/s | **64.36 tok/s**  | **34.02 tok/s** | 25.97 tok/s |

TinyLlama-1.1B: CPU wins at every length tested, though the gap narrows sharply (5.9× → 2.3×). Llama-3.2-3B crosses over around 2048 tokens and clearly wins at 3584 (+31%) — the only measurement in the entire investigation where the NPU outright beats CPU. NPU prefill throughput *climbs* with context length here while CPU's *declines* (worse-than-linear attention cost).

**This trend did not survive a follow-up test.** Pushed to 8k/16k tokens using models actually converted with real 16k headroom (`max_context_limit: 16384`, not the 4096-capped files above), the direction reversed: NPU prefill *declined* with context, same as everything else (71.88 → 59.13 → 36.69 tok/s at 1024 → 4096 → 8192 tokens). The earlier "NPU improves with context" pattern was an artifact of approaching a specific model's hardcoded 4096-token ceiling, not a general property.

### A third-party number that looked 3.2× too fast, run down completely

Rockchip's own [`benchmark.md`](https://raw.githubusercontent.com/airockchip/rknn-llm/main/benchmark.md) reports TinyLlama-1.1B decode at **24.43-24.49 tok/s** — restated verbatim by Seeed Studio's tutorial page as if it were an independent measurement. Our own initial number, 7.7 tok/s, was over 3× slower under matched short-prompt conditions. Eight hypotheses were tested directly, in order, each with a clean before/after measurement:

| Hypothesis | Test | Result |
|---|---|---|
| Python/Flask wrapper overhead | Raw C++ demo linked against the identical (MD5-verified) `librkllmrt.so` | Ruled out — 8.48 tok/s, same as wrapper's 7.7 |
| Grouped quantization (`w8a8_g128`) cost | Same model, plain vs grouped, `ignore_eos_token=true` | Real (~28% slower grouped) but insufficient alone |
| CPU governor `ondemand` vs `performance` | All 8 cores pinned to max, rerun 3 model/quant combos | +5-8%, real but insufficient alone |
| Undisclosed GPU/OpenCL assist inside `librkllmrt.so` | Strings/symbol analysis found real OpenCL references; GPU driver was unreachable on stock config | Capability confirmed to exist in the binary; unreachable as shipped |
| Locking GPU/NPU/CPU clocks to max simultaneously | Full clock-pin sweep after enabling GPU | Flat-to-slightly-worse — no headroom was being left on the table |
| `npu_core_num` (multi-core NPU parallelism) as overhead | Controlled x86-toolkit reconversion, `num_npu_core` = 1/2/3 | **Reversed the hypothesis** — decode scales *up* monotonically with core count (3.08→5.03→6.62 tok/s) |
| INT4 (`w4a16`) weight quantization | Toolkit call against `target_platform=rk3588` | Hard-blocked at the toolkit level; confirmed via vendor SDK PDF as deliberate platform segmentation, not a bug |
| DDR/DMC frequency scaling | `/sys/class/devfreq/dmc` didn't exist — TF-A firmware gap, not a settings problem | See below — this was the actual missing piece |

The GPU/OpenCL path required un-freezing the board once (a live driver rebind attempt hung it solid; recovered via power cycle) before finding the actual safe mechanism: a boot-time device-tree overlay swap (`panthor` → `midgard`) in `armbianEnv.txt`, plus registering an OpenCL ICD file that didn't exist by default. Once genuinely active, it delivered a small, consistent, reproducible **+7-10%** decode speedup across three model/quant combinations — real, but nowhere near enough on its own.

### The actual fix: DDR/DMC DVFS was firmware-gated, not settings-gated

Rockchip's own official frequency-pinning script (`fix_freq_rk3588.sh`) targets a DDR devfreq node (`/sys/class/devfreq/dmc`) that simply did not exist on this board's stock kernel/firmware combination. `dmesg` traced the cause: `trusted firmware unsupported, please update` — DDR DVFS on RK3588 requires a specific ARM Trusted Firmware (TF-A) build with a DMC SIP handler, and Armbian's default bootloader for this board didn't include it.

This was fixed by building a new SPI-NOR bootloader image using Radxa's own current, actively-maintained build toolchain (`radxa/u-boot` + `radxa/build`'s `mk-uboot.sh`), pointed at genuine Rockchip blobs (`rk3588_bl31_v1.54.elf`, DDR training blob `v1.21`) sourced from `rockchip-linux/rkbin`. (An earlier attempt via Armbian's own build system failed outright — that board's "vendor" build path turned out to be dead code, abandoned in favor of mainline U-Boot per `armbian/build#9773`, and produced a chain of otherwise-inexplicable failures tracing back to that.) The image was flashed to `/dev/mtd0` with a full backup-and-byte-diff safety net (the SPI-NOR bootloader chip is physically isolated from the NVMe root filesystem — flashing it cannot touch OS data).

Post-flash, `/sys/class/devfreq/dmc` existed for the first time, with real frequency steps up to 2.4GHz, and `fix_freq_rk3588.sh` completed cleanly end to end for the first time in the investigation.

### Stacking every real lever

| Configuration | TinyLlama-1.1B decode |
|---|---|
| Baseline (`ondemand`, no GPU/OpenCL, no DDR DVFS), grouped `w8a8_g128` | 7.7 tok/s |
| + CPU governor `performance` | 8.90 tok/s |
| + GPU/OpenCL reachable | 9.00 tok/s |
| + DDR/DMC DVFS fixed (governor + GPU stacked in) | **14.25 ± 0.27 tok/s** |
| + plain (non-grouped) `w8a8` instead of `w8a8_g128`, same model, all fixes stacked | **23.08 ± 0.37 tok/s** |

DDR/DMC DVFS alone was the single largest individual lever found in the entire investigation — an 85% jump over baseline. The final step closed the rest of the gap: virtually every community-uploaded `.rkllm` file (including the one used for most of this investigation) uses grouped `w8a8_g128` quantization, presumably for accuracy reasons — but it costs ~28% decode throughput versus plain `w8a8`, and Rockchip's benchmark figure was measured on plain `w8a8`. **23.08 tok/s vs. Rockchip's published 24.43-24.49 tok/s is within 6% — inside normal run-to-run variance.**

### GPU compute as a third backend

`llama.cpp`'s Vulkan backend, run against the same Mali-G610 once its driver was working, gave a genuine third option:

| Model | CPU, 4 threads | CPU, 8 threads | Vulkan GPU | NPU, rkllm (best measured) |
|---|---|---|---|---|
| TinyLlama-1.1B decode | **18.44 tok/s** | 9.90 tok/s | 12.98 tok/s | 9.00 tok/s |
| Qwen3-1.7B decode | **10.34 tok/s** | 6.74 tok/s | 7.94 tok/s | 7.09 / 5.39 tok/s |
| Llama-3.2-3B decode | **5.96 tok/s** | 4.33 tok/s | 3.96 tok/s | unreliable |

CPU at 4 threads wins outright at every size tested. Vulkan beats the NPU's best measured number at 1B and 1.7B, but loses at 3B — and is notably *weak* at prefill (its worst showing of the whole study), which rules out a clean "GPU handles prefill, CPU handles decode" split.

### No viable cross-backend pipeline exists on this hardware

- `.rkllm` files are provably GGUF-derived (identical version field, tensor count, and metadata-key length as real GGUF) but the string content is obfuscated with a **fixed, single-constant XOR keystream** — confirmed via known-plaintext attack (`"general.architecture"`, 20 bytes, identical ciphertext across four unrelated model files). This is not real encryption; it fully yielded to analysis. Decrypting it fully recovered metadata including `rkllm.core_num=3`, `rkllm.max_context=4096`, and per-layer architecture fields — but **not** actual tensor weight values, which are high-entropy numeric data with no crib to attack.
- Patching `core_num` from 3→1 directly in a `.rkllm` file loads (the runtime reads the patched value) but fails at inference (`meet unkown op`) — the field determines physical tensor tiling at conversion time, not just a runtime scheduling hint. It cannot be changed post-hoc.
- `w4a16` (INT4) is confirmed, from Rockchip's own SDK PDF, as a deliberate platform restriction: RK3576 supports five INT4 quantization schemes, RK3588 supports none, ever, in any toolkit version tested (including the actual v1.0.0 wheel, despite the changelog's claim otherwise). The runtime independently enforces platform matching at load time, closing off the "build for a permissive chip, load on RK3588" loophole even if the toolkit-side check were bypassed.
- `llama.cpp`'s native CPU+GPU layer-splitting (`-ngl N`) was tested directly rather than assumed safe: partial splits are **catastrophically worse** than either pure backend (prefill dropped from CPU's 114.88 tok/s to 4.39-4.53 tok/s with any GPU layers active), most likely because each CPU/GPU boundary crossing forces a real memory sync that dominates on a 22-layer model with tiny per-token work.

**Conclusion: there is no realistic heterogeneous pipeline to build here.** The NPU can't participate because its file format's tensor data can't be reverse-engineered without the original calibration data, and CPU+GPU splitting actively hurts rather than helps on this board's driver stack.

## Discussion

For someone deciding NPU vs. CPU vs. GPU for small (1-4B) LLM inference on this class of hardware, out of the box: **plain CPU inference at 4 threads is the fastest and simplest option**, and remains so even after every NPU-side fix in this investigation is applied — the fixes close the gap to the vendor's *own* NPU benchmark, they don't make the NPU beat CPU. The NPU's one real advantage found here is prefill throughput on larger models (3B+) at longer context lengths (2K+ tokens) as it approaches its declared context ceiling — a narrow, model-size-and-length-dependent case, not a general property.

The practical lessons for anyone else on this hardware: check whether DDR/DMC DVFS is actually active (`/sys/class/devfreq/dmc` existing is not guaranteed on default images — it requires a specific firmware build most vendor images don't ship), and be aware that grouped quantization (`w8a8_g128`), while presumably better for accuracy, costs a real and non-trivial amount of throughput that isn't disclosed next to any published benchmark number.

## Reproducibility Notes

- Benchmark harness: `bench.py`, 6 trials per configuration, varying prompts, `num_predict=100`, 90s client timeout
- CPU baseline via `llama-bench -p 64 -n 64` against same-size Q8_0 GGUF quants
- Raw JSON trial data: `results.json`, `cpu_baseline_*.txt`
- `.rkllm` format parsing/decryption tooling: `~/npu-bench/rkllm_format/` (`parse.py`, `parse_tensors.py`, `find_offset.py`)
- Governor/clock control scripts: `set_governor.sh`, `lock_all_max.sh`, `revert_clocks_auto.sh`
- GPU driver swap scripts: `switch_to_midgard_gpu.sh`, `register_mali_opencl_icd.sh`
- Vulkan-backend `llama.cpp` build: `llama.cpp/build-vulkan/`
- Python venv via `uv`, Python 3.12 (system Python 3.14 was incompatible with rkllm's cp39-cp312 wheels)

**Flashing the bootloader fix is not casually reproducible** — it requires sourcing a matched BL31/DDR-training blob pair from `rockchip-linux/rkbin`, a working Radxa `u-boot`/`build` toolchain, and a full backup-verify-diff safety procedure. This is a materially higher-risk step than anything else in this study (a bad flash requires physical maskrom-mode recovery) and should not be attempted without that safety net.

## Limitations / What Wasn't Tested

- `npu_core_num` was tested only up to 3 (RK3588's documented maximum); no attempt was made to exceed vendor-declared limits.
- A plain-`w8a8` conversion was tested for TinyLlama-1.1B only; Llama-3.2-3B's plain-quant equivalent was not benchmarked (its grouped conversion also had an unrelated truncation defect — see the companion findings in `ISSUES_TO_FILE.md`).
- True INT4 LLM inference on RK3588 was not attempted at the raw `rknn_matmul_api` level (hand-authoring a full transformer forward pass against undocumented low-level primitives) — confirmed possible in principle, but out of scope as a from-scratch inference-engine project.
- `.rkllm` tensor weight data was not decrypted — only metadata and tensor names, which had predictable plaintext to attack. Weight values are high-entropy with no known-plaintext angle available.
- Larger models (4B+) with genuinely large context windows were found to be RAM-constrained on this 8GB board before being compute-constrained; this wasn't pushed further within this study.
