# RK3588 Field Notes: NPU Inference, Vision Workloads, and a Zero-Copy Streaming Appliance

A series of five reproducibility-focused write-ups from a multi-week, hands-on investigation of the Rockchip RK3588 SoC on a Radxa ROCK 5B+ (8GB RAM), covering LLM inference on the NPU vs. CPU vs. GPU, throughput tuning, an external memory system for indefinite-length LLM conversations, vision workloads (VLM/CLIP/object detection), and a real single-box 4K60 capture/encode/stream pipeline.

The throughline across all five: every claim is backed by a real measurement on real hardware, false leads are kept in the record rather than smoothed over, and vendor documentation/marketing numbers are treated as hypotheses to verify, not facts to cite.

## Papers

1. **[The RK3588 NPU Is Slower Than Its Own CPU for LLM Decode — Until You Stack Six Undocumented Fixes](01-npu-vs-cpu-inference.md)**
   Reproduces a vendor benchmark figure that initially looked 3.2× unreachable, by finding and stacking six real, independently-tested levers — ending in a firmware gap (DDR/DMC DVFS) most default OS images don't ship a fix for, plus an undisclosed quantization-scheme cost. Also covers the `.rkllm` file format (reverse-engineered), why no CPU+GPU+NPU pipeline is viable on this hardware, and why plain CPU inference remains the fastest single option regardless.

2. **[Squeezing the RK3588 NPU: Frequency Tuning, Multi-Batch Scaling, and the Real INT4 Wall](02-throughput-optimization-and-limits.md)**
   What's left after the vendor-benchmark gap is closed. Locking every clock to maximum made things worse, not better; NPU frequency saturates above 700MHz while DDR frequency never saturates; a documented ~4.3× multi-batch throughput multiplier exists in the runtime but isn't exposed by the common serving layer; and INT4 quantization is dead at every level tested, down to hand-written raw hardware matmul calls.

3. **[External Memory for a Hard-Capped On-Device LLM: Persistence, Eviction, Summarization, and Retrieval on an 8GB RK3588 Board](03-external-memory-gateway.md)**
   A thin external-memory layer built on top of a runtime whose own KV cache silently discards history past a fixed window. Covers three separate model-based summarization designs that each failed the same way, the deterministic redesign that fixed it, and a recurring finding that "the right data reaching the model" and "the model actually using it correctly" are separate problems — same infrastructure, different model, different (correct) outcome.

4. **[Three Ways to Put a Camera on an RK3588 NPU: VLM, CLIP, and a Real-Time Detector, Benchmarked Head-to-Head](04-vision-workloads-vlm-clip-detector.md)**
   A general-purpose vision-language model, an embedding-only classifier, and a purpose-built object detector, benchmarked against each other on the same images. A detector is ~800× faster than a VLM on the same photo and more accurate for counting; the VLM's genuine edge is free-form understanding and reading text a fixed-vocabulary detector structurally can't produce.

5. **[A Single-Box 4K60 Capture-Encode-Stream Appliance on an RK3588 SBC — Built, Broken, and Fixed in Public](05-zero-copy-4k-streaming-appliance.md)**
   From "does this board have a real HDMI capture port" to a working streaming appliance with RTMP push, an instant-replay clip buffer, and a browser control panel. Includes a multi-day, eleven-falsified-hypothesis debugging saga for a driver race condition whose actual trigger turned out to be the investigation's own debug logging.

## Hardware and Software Baseline

- **Board:** Radxa ROCK 5B+ — RK3588 (4× Cortex-A76 + 4× Cortex-A55), Mali-G610 GPU, 6 TOPS NPU, 8GB RAM
- **OS:** Armbian, kernel 6.1.115-vendor-rk35xx
- **NPU stack:** Rockchip `rknpu` driver, `rkllm-runtime` v1.3.0, served via [`rkllama`](https://github.com/NotPunchnox/rkllama)
- **CPU/GPU baseline:** `llama.cpp`, CPU and Vulkan backends
- **Conversion toolchain:** `rkllm-toolkit` and `rknn-toolkit2` (x86_64-only — a second machine was used for all from-scratch model conversions)

## A Note on Method

Nothing here is presented as a final word from the vendor or a definitive benchmark suite — it's a record of what was actually run, what broke, what the fix turned out to be, and what remains genuinely unresolved. Where an earlier finding in one paper was later superseded by a more complete test (e.g. a "harmless" overclock overlay later found to cost throughput; a "different board" hypothesis later replaced by a missing quantization-scheme variable), both the original finding and the correction are kept in the text rather than silently editing history.
