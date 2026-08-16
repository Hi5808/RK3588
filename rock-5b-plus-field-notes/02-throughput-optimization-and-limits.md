# Squeezing the RK3588 NPU: Frequency Tuning, Multi-Batch Scaling, and the Real INT4 Wall

**A follow-up to the NPU-vs-CPU reproducibility study, covering every throughput lever tested after the vendor benchmark gap was closed**

## Abstract

After closing the gap to Rockchip's published TinyLlama-1.1B benchmark figure (see the companion paper, *"The RK3588 NPU Is Slower Than Its Own CPU..."*), this investigation asked what throughput headroom remains beyond that number, and where the genuinely hard limits are. Findings, in order of how surprising they were: locking every clock domain to its performance maximum simultaneously made things *worse*, not better, because CPU-side scheduling latency — not raw clock speed — turned out to be the dominant lever in an autoregressive decode loop; a CPU overclock overlay quietly cost ~5% NPU throughput via an inferred shared-power-budget effect, reversing an earlier "harmless, keep it enabled" call; NPU frequency scaling saturates completely above 700MHz while DMC (DDR) frequency scaling never saturates at all, confirming decode is fundamentally memory-bandwidth-bound; binding the NPU's hardware interrupts to a big CPU core (a lever borrowed from a different Rockchip toolkit's documentation, not RKLLM's own) produced the single cleanest result of the whole investigation; native multi-batch inference delivers a documented, officially-supported ~4.3× aggregate throughput multiplier that the actual serving wrapper this investigation used (`rkllama`) doesn't expose at all; and INT4 quantization — the one lever that looked most promising on paper — is dead at every level tested, from the standard conversion toolkit down to hand-written raw matmul calls against the hardware primitive itself, with independent third-party confirmation that this isn't specific to this board.

## Hardware and Software Setup

Same as the companion paper: Radxa ROCK 5B+ (RK3588, 8GB RAM), Armbian 26.8.1, `rkllm-runtime` v1.3.0 (`librkllmrt.so`, `rknpu` driver v0.9.8), served via `rkllama` for the baseline server behavior but bypassed directly (via raw C API calls) for every test below that `rkllama` doesn't expose. All tests in this paper occur *after* the DDR/DMC DVFS bootloader fix from the companion paper — `/sys/class/devfreq/dmc` is assumed available throughout.

## Methodology

Every "lever" below was isolated and tested individually, not assumed from documentation. Where an earlier result in this same investigation turned out to be confounded (the CPU overclock overlay, the "flat" RAM readings during batch testing), the correction is documented explicitly rather than silently revised.

## Results

### Available frequency ranges (research baseline)

| Domain | Range | sysfs path |
|---|---|---|
| Little cores (A55) | 408 MHz – 1.8 GHz | `.../cpu0/cpufreq/scaling_available_frequencies` |
| Big cores (A76) | 408 MHz – 2.256 GHz (2.4 GHz with an unused Armbian overlay) | `.../cpu4/cpufreq/scaling_available_frequencies` |
| DMC (DDR) | 534 MHz – 2.4 GHz | `/sys/class/devfreq/dmc/available_frequencies` |
| NPU | 300 MHz – 1.0 GHz | `/sys/class/devfreq/fdab0000.npu/available_frequencies` |
| GPU (Mali) | 300 MHz – 1.0 GHz | `/sys/class/devfreq/fb000000.gpu/available_frequencies` |

Two unused overclock device-tree overlays ship with this board's stock Armbian kernel package: a +6.4% CPU big-core overlay (2.256→2.4GHz, +50mV) and a DMC overlay claiming up to 3.5GHz. Both were tested directly rather than left as research.

### The CPU overclock overlay: real, stable, and a net negative for NPU throughput

The CPU-OC overlay applied cleanly and held stable under 3 minutes of `stress-ng --cpu 8` (peak 58.2°C, zero failures). Locked to 2.4GHz, NPU decode measured 13.82 ± 0.08 tok/s — flat versus the 14.25 tok/s non-OC baseline at the time. It was initially kept enabled on the assumption it was harmless.

**That assumption was wrong.** A later controlled A/B, run under the fully validated benchmark profile (see below), found:

| Condition | Result |
|---|---|
| CPU-OC enabled, big cores at 2.4 GHz | 21.73 ± 0.22 tok/s |
| CPU-OC **disabled**, stock 2.256 GHz | **22.93 ± 0.61 tok/s** |

The overlay was quietly costing ~5% NPU decode throughput the entire time it was enabled, and this fully explained a previously unexplained 7% gap between two earlier benchmark runs in this investigation. The likely mechanism (not directly instrumented, but consistent with everything else observed): the 2.4GHz OPP step runs at 1.05V versus the stock step's 1.00V, and even light CPU-side orchestration work at the higher voltage plausibly causes NPU voltage droop under a shared power budget — without ever showing up as a change in the NPU's own reported clock. **The CPU-OC overlay is now disabled by default on this board**, reversing the earlier call.

The companion DMC-OC overlay (claiming up to 3.5GHz) applied cleanly at the device-tree level but was a confirmed no-op: this specific chip's OTP-fused silicon bin gates the effective DDR ceiling at 2.4GHz regardless of what OPP steps the overlay adds, per `dmesg`'s own reported `performance_rate`/`boost_rate` values. Completely safe, zero effect — matches community reports that top-step DDR overclocking is per-chip and typically requires dedicated timing-retuning tools this investigation didn't pursue.

### Locking every clock to maximum simultaneously made things worse

The naive "compute method" — force DMC, NPU, GPU, and all 8 CPU cores to their `performance` governor, boot-persistent via a systemd unit — measured **21.02 ± 0.15 tok/s**, worse than an earlier ad-hoc 23.08 tok/s figure. Isolating each variable in turn:

| Change | Result | Conclusion |
|---|---|---|
| Baseline: everything locked `performance`, CPU-OC active | 21.02 ± 0.15 | worse than 23.08 |
| GPU released to `simple_ondemand` | 20.95 ± 0.20 | no change |
| DMC released to `dmc_ondemand` | 20.91 ± 0.19 | no change — `dmc_ondemand` ramps to the same 2.4GHz under real load anyway |
| CPU released to `ondemand` | 15.21 ± 0.14 | **large regression** — CPU governor is the dominant lever, not DMC/GPU/NPU clock choice |
| CPU back to `performance`, `cpuidle` state1 disabled, GUI/desktop services stopped | **21.55 ± 0.06** | best and tightest result of this sub-test |

**Interpretation:** autoregressive decode is latency-sensitive on the CPU-side orchestration path (each token genuinely waits on the previous one), so anything adding wake-up latency there — an `ondemand` governor's ramp delay, a `cpuidle` C-state exit, a background compositor stealing a scheduling slot — costs more than any DMC/GPU/NPU clock choice, once those domains are already fast enough on their own. Forcing DMC/GPU to a static maximum bought nothing because they were already reaching the same frequency on demand under real load.

The validated profile — NPU `performance`, CPU `performance`, `cpuidle` disabled, GUI services stopped, DMC/GPU left on their demand governors — was packaged as two manual toggle scripts (`benchmark_mode_on.sh` / `benchmark_mode_off.sh`) with matching desktop shortcuts, deliberately **not** a boot-persistent systemd service, since silently stopping GUI services on every boot is a real behavior change a benchmarking session shouldn't impose by default.

### NPU frequency saturates; DMC frequency never does

Two clean, isolated sweeps under the corrected (CPU-OC disabled) benchmark profile:

**NPU sweep** (DMC on `dmc_ondemand`):

| NPU freq | decode tok/s |
|---|---|
| 300 MHz | 12.67 ± 0.05 |
| 400 MHz | 15.66 ± 0.28 |
| 500 MHz | 19.24 ± 0.03 |
| 600 MHz | 22.09 ± 0.01 |
| 700 MHz | 24.08 ± 0.02 |
| **800 MHz** | **24.14 ± 0.03 (peak)** |
| 900 MHz | 24.08 ± 0.04 |
| 1000 MHz (stock max) | 23.94 ± 0.15 |

Throughput is nearly linear up to 700MHz, then fully flat through 1.0GHz — and the full ceiling is measurably (~0.8%) *worse* than 800MHz, the same shared-power-budget pattern seen with the CPU-OC overlay. **Practical implication: pinning NPU to 700-800MHz costs nothing over stock `performance` and should meaningfully reduce power draw for thermally-constrained deployments** — an actionable finding, not a curiosity.

**DMC sweep** (NPU pinned to 800MHz, its measured peak, to isolate DMC as the only variable):

| DMC freq | decode tok/s |
|---|---|
| 534 MHz | 7.29 ± 0.02 |
| 1320 MHz | 16.00 ± 0.02 |
| 1968 MHz | 21.11 ± 0.05 |
| 2400 MHz (max) | 24.11 ± 0.07 |

The opposite shape entirely — throughput more than triples with no sign of flattening even at the top step. **This confirms decode is fundamentally memory-bandwidth-bound, not NPU-compute-bound**, and — unlike NPU frequency — there is no free power-saving headroom on DMC: every step costs real throughput, so demand-based ramping to maximum under load is correct, not a lower fixed pin.

### NPU IRQ affinity: the single cleanest result in the whole investigation

Reading RKNN-Toolkit2's documentation (a separate, lower-level Rockchip toolkit sharing the same `rknpu` kernel driver — see below) surfaced a lever never mentioned in RKLLM's own docs: `cat /proc/interrupts | grep npu` showed all three NPU hardware interrupt lines landing 100% on CPU0, a little A55 core, while RKLLM's own inference threads run exclusively on the big cores (CPU4-7) — every NPU completion signal was crossing from a little core to a big core on every single token. Binding all three IRQs to CPU4 (`echo 4 > /proc/irq/<n>/smp_affinity_list`, fully reversible) and re-running the benchmark:

**23.54 ± 0.22 tok/s** — higher than the prior best (22.93 ± 0.61) *and* roughly 3× more consistent (lower stdev). This became the best clean throughput result in the entire investigation, found by reading documentation for a completely different toolkit rather than anything RKLLM-specific.

### Native multi-batch inference: a documented ~4.3× multiplier the serving layer doesn't expose

RKLLM's C API exposes `RKLLMExtendParam.n_batch` — genuine, documented, officially-supported concurrent-stream batching (SDK section 3.2.14), with Rockchip recommending up to 8 concurrent batches. **`rkllama`, the server used throughout this entire investigation, hardcodes `n_batch=1` and never exposes this parameter at all** — testing it required bypassing the Flask server and driving `librkllmrt.so` directly, via two independent implementations (a Python harness reusing `rkllama`'s own proven-correct ctypes struct definitions, and a standalone C++ program adapted from the SDK's own example) that were cross-checked against each other and agreed closely (Python: 1.50× speedup at `n_batch=2`; C++: 1.55×).

**Scaling curve, short generations (100 tokens/batch):**

| n_batch | aggregate tok/s | speedup vs. solo |
|---|---|---|
| 1 | 22.13 | 1.0× |
| 2 | 33.1 | 1.50× |
| 3 | 44.55 | 2.01× |
| 4 | 55.7 | 2.52× |

Pushed well past Rockchip's stated recommendation of 8, using an isolated-subprocess sweep design so a crash at high batch counts wouldn't take down the rest of the run:

| n_batch | aggregate tok/s | RAM used after run |
|---|---|---|
| 8 | 78.88 | — |
| 14 | 95.36 | 3502 MB |
| **16** | **95.45 (peak)** | 3503 MB |
| 17 | 87.56 | 3517 MB |
| 20 | 85.34 | 3528 MB |

**The cap is a sharp cliff at n_batch=16→17**, not a gradual plateau — 14 and 16 are statistically tied, then 17 drops straight back to roughly where n_batch=9 sat, and stays flat-to-declining through 20. This reads as a real internal scheduling/queue-depth threshold in the runtime, not a smooth compute-bound curve. **RAM was not the constraint** at any point in this sweep — usage stayed flat around 3.5GB (of 8GB total) across the entire 9-20 range, with 4.4GB+ still free at the worst point tested.

**Why batching works without contradicting the memory-bandwidth-bound finding:** single-stream decode reads the model's entire weight set from DRAM to generate one token (a batch-of-one GEMV, which is exactly why the DMC sweep never saturates). Batching reuses that same weight read across multiple concurrent sequences in a single pass (GEMV becomes GEMM), so DDR traffic doesn't scale proportionally with batch count the way compute does — which is precisely why aggregate throughput keeps climbing near-linearly instead of hitting the same DMC wall a single stream hits. This is a bigger, more broadly applicable lever than anything else in this investigation, and it costs nothing extra: the same `.rkllm` file works unchanged, `n_batch` is purely a runtime init parameter. The gap is entirely in `rkllama`'s request-handling model, which is one-request-in-one-response-out and would need real scheduling work to multiplex several HTTP requests into a shared `n_batch`-sized call.

### The shared-context ceiling: a second, independent cap discovered by accident

Pushing a long-context batch test (`max_new_tokens=1200`, `n_batch=32`, `ignore_eos_token=True`) surfaced unexpected deterministic truncation: every batch stopped at exactly 113 tokens. `4096 (max_context) / 32 (n_batch) ≈ 128`, minus the ~15-token prompt ≈ 113 — not a coincidence, confirmed by re-running at `n_batch=4`, where all four batches stopped at exactly 998 tokens (`4096/4=1024` minus a ~26-token prompt).

Setting `RKLLM_LOG_LEVEL=2` (a documented-but-previously-unused diagnostic flag) exposed the actual mechanism directly in runtime log output: **one shared KV-cache buffer with a single cumulative context counter across all `n_batch` streams combined**, not an even per-stream division. As the pool nears capacity, the runtime evicts the oldest cached entries (sliding-window eviction, the same strategy `llama.cpp` uses) before eventually erroring once eviction can't keep pace. The buffer size (88.00 MiB) was computed independently from the model's own architecture parameters and matched the runtime's reported figure exactly:

```
2 (K&V) × 22 layers × 4 kv_heads × 64 head_dim × 2 bytes (fp16) = 22,528 bytes/token
22,528 bytes/token × 4096 tokens (max_context) = 92,274,688 bytes = 88.00 MiB
```

This is genuinely new information not documented anywhere in the SDK — the manual describes `n_batch` and `max_context_len` as independent settings with no stated interaction. **Practical implication:** two independent ceilings exist and both need sizing for a real workload — a compute/scheduling wall around n_batch≈16, and a fixed, shared, `max_context`-derived context pool that more concurrent streams divide (not multiply) between them. Getting more aggregate context headroom requires reconverting the model with a larger `max_context` (up to 16,384, must be a multiple of 32 — a proportionally larger fixed buffer, e.g. 352 MiB at 16,384), not adjustable at runtime.

### RKNN-Toolkit2 vs. RKLLM: what a "real" custom engine would require

RKNN-Toolkit2 is Rockchip's general CNN/vision toolkit, and RKLLM is built on top of it — but RKNN-Toolkit2's standard `build()` path only accepts Caffe/TensorFlow/ONNX/PyTorch/DarkNet models with INT8-only quantization; there's no path to feed it a transformer and get a usable LLM out. The relevant part is its **low-level Matmul API** (`rknn_matmul_*`), which is architecturally different from RKLLM's `n_batch` in ways confirmed via the `RKLLM_LOG_LEVEL=2` diagnostics above:

- **True independent multi-core parallelism** via `rknn_dup_context()` (cheap context duplication, up to 2 duplicates for RK3588's 3 NPU cores) with explicit per-context core binding — RKLLM has no equivalent; NPU core count is fixed at conversion time via `num_npu_core` and isn't runtime-selectable.
- **956KB of on-chip SRAM**, directly relevant given decode is proven DDR-bandwidth-bound — but requires a kernel rebuild (`CONFIG_ROCKCHIP_RKNPU_SRAM`, confirmed **absent** from this board's kernel, not merely unconfigured — no `/sys/kernel/debug/rknpu/mm` node exists at all) plus a device-tree patch, the same class of risk as the DDR/DMC bootloader fix. **Reasoned through rather than attempted**: the SRAM region (956KB) is roughly 1,200× too small to hold any meaningful fraction of this model's ~1.1-1.2GB weight set, and per the SDK's own documentation it's meant for intermediate tensor buffers, not weights — it structurally cannot touch the dominant DDR traffic source this investigation already identified. Not worth the kernel-rebuild risk for this workload shape.
- **Genuine INT4 matmul primitives** exist at this level — confirming INT4 is real silicon capability, gated only at the standard conversion-pipeline level (see below).

Building a real custom engine around any of this means hand-writing an entire transformer forward pass — embedding lookup, per-layer attention/FFN matmuls, KV cache management, sampling — against raw primitives. RKLLM already does all of that; RKNN gives only the building blocks. Scoped as a deliberate, substantial future project, not attempted here.

### INT4 is dead at every level, confirmed down to the hardware primitive

Testing didn't stop at "the toolkit refuses it." A standalone C++ program (`matmul_int4_vs_int8.cpp`) called `rknn_matmul_run()` directly, bypassing both RKLLM's and RKNN-Toolkit2's `build()` gates entirely, using TinyLlama's real FFN projection shape:

- **INT8 baseline worked cleanly**: 958 calls/s, ~1.04ms/call.
- **`RKNN_INT4_MM_INT4_TO_INT16`** (pure INT4×INT4): context creation and memory allocation succeed (buffer sizes confirm correct 4-bit packing), but crashes with `Unsupport type bits 0` inside `rknn_matmul_set_io_mem()` for the weight matrix — a genuine internal fault, not a clean rejection, and unaffected by explicitly setting quantization params rather than relying on defaults.
- **`RKNN_FLOAT16_MM_INT4_TO_FLOAT16`** with grouped quantization (the exact hybrid primitive `w4a16_gX` would need — FP16 activations, grouped INT4 weights): a clean, explicit rejection once layout constraints are satisfied — `"unsupported matmul dtype ... in this platform"`.

This changes the likely explanation for RK3588's toolkit-level `w4a16` block from "unknown — accuracy vs. driver immaturity vs. deliberate segmentation" (left unresolved in the companion paper) to something more specific: **the runtime itself, not just the toolkit, refuses the exact primitive this quantization mode needs — evidence of incomplete driver support, not an arbitrary restriction on working hardware.**

Independently corroborated by three external sources: an unresolved upstream GitHub issue where a community commenter reports the identical FP16-activation/INT4-weight limitation; a 2023 third-party benchmark reporting the raw Matmul API as "completely broken" on an earlier RKNN version (multi-year history of instability at this level, not specific to this test); and a separate reverse-engineering writeup independently documenting other rough edges below the standard toolkit (a 32KB L1 SRAM scratchpad limit distinct from the 956KB external SRAM region, driver timeouts under high tile counts). The most likely clarifying explanation found: RK3588's 6 TOPS INT4 marketing figure applies to CNN/vision convolution operations, not transformer matmuls — a genuinely incomplete feature for this specific use case, not a deliberate product block.

**Bottom line: INT4 is not viable on this board today, at any level** — not through RKLLM, not through RKNN-Toolkit2's `build()`, not through hand-written raw matmul calls. This is very likely to reproduce on any RK3588/RK3588S board running a comparable RKNN runtime/driver version, since it traces to the shared vendor stack rather than anything board-specific.

## Discussion

The through-line across this whole set of experiments: **on this hardware, scheduling latency and memory bandwidth dominate over raw compute clock for LLM decode.** Every "obviously good" lever that just raises a clock (CPU overclock, full-max-everything, the top NPU frequency step) either did nothing or actively hurt, while every lever that reduced latency in the CPU-orchestration critical path (governor choice, cpuidle, IRQ affinity, stopping background services) delivered real, measurable gains — and DDR bandwidth was the one clock domain that never stopped mattering, at any frequency tested. For anyone tuning a similar board: chase scheduling/interrupt-affinity and DDR frequency before chasing NPU/CPU clock headroom, and if the workload can tolerate multiple concurrent requests, native multi-batch inference is a far larger lever than any of the tuning work combined — it just isn't wired up in the common serving wrapper for this runtime.

## Reproducibility Notes

- Frequency-sweep scripts: `sweep_npu.sh`, `sweep_dmc.sh`, `bench_npu_sweep.py`, `bench_dmc_sweep.py`; raw results in `npu_sweep_results.json`, `dmc_sweep_results.json`
- Multi-batch harnesses: `batch_infer_test.py` / `.cpp` (cross-checked implementations), `batch_infer_single.py` + `sweep_batch_cap.sh` (isolated-subprocess sweep past n_batch=8), results in `batch_cap_sweep_results.jsonl`, `batch_infer_test_result.json`
- RAM-stress harness: `sweep_ram_stress.sh`, results in `ram_stress_results.jsonl`, diagnostics in `ram_verify_loglevel2.log`
- INT4 proof-of-concept: `matmul_int4_vs_int8.cpp`
- Governor/profile scripts: `benchmark_mode_on.sh`, `benchmark_mode_off.sh`, `npu_max_perf.sh`, `lock_all_max.sh`
- NPU IRQ affinity: `echo 4 > /proc/irq/<n>/smp_affinity_list` for IRQs 44/45/46 (`fdab0000.npu`), reversible via `echo 0-7 > .../smp_affinity_list`
- SDK documentation referenced: `Rockchip_RKLLM_SDK_EN_1.3.0.pdf` (section 3.2.14, Multi-batch), `02_Rockchip_RKNPU_User_Guide_RKNN_SDK_V2.3.2_EN.pdf`
- `RKLLM_LOG_LEVEL=2` is the documented diagnostic flag used to observe the shared KV-cache eviction mechanism directly

## Limitations / What Wasn't Tested

- Perf-per-watt sweeps were scoped (a viable proxy sensor was identified — `/sys/class/hwmon/hwmon7`, the USB-C PD input current sensor) but not executed.
- NPU SRAM and true multi-core independent parallelism via RKNN's low-level API were reasoned through and explicitly not attempted, given the kernel-rebuild risk and (for SRAM specifically) a clear numerical case that it wouldn't help this workload shape.
- Multi-batch testing used short (100-token) generations for the compute-cliff sweep; the RAM-flat result specifically reflects that regime, not long-context concurrent batches, which were separately shown to hit the shared-context ceiling well before RAM pressure.
- IRQ affinity was tested only with NPU IRQs bound to CPU4; a full sweep across all four big cores wasn't performed.
- The shared-power-budget explanation for the CPU-OC and top-NPU-step throughput costs is inferred from consistent, reproducible measurement patterns, not confirmed via direct power-rail instrumentation.
