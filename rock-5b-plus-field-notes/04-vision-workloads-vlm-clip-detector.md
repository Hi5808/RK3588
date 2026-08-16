# Three Ways to Put a Camera on an RK3588 NPU: VLM, CLIP, and a Real-Time Detector, Benchmarked Head-to-Head

**Why "live video AI" means very different things depending on which vision model you pick — with real numbers for all three, on the same board, against the same images**

## Abstract

Following an LLM-focused investigation into RK3588 NPU inference (see companion papers), this study turned to vision workloads: a general-purpose vision-language model (Qwen2-VL-2B), an embedding-only classifier (CLIP), and a purpose-built object detector (YOLOv5s), all benchmarked on the same Radxa ROCK 5B+ board, and in one case against the exact same test image, to get an honest three-way comparison rather than three isolated claims. The results diverge sharply: the VLM takes ~15-19 seconds per single-image query and cannot approach real-time video at all (the vision-encode step alone is 5.76 seconds); CLIP classifies confidently in 2-5 seconds total but has no notion of object location or count; YOLOv5s runs at 41-42 FPS regardless of scene complexity — roughly 800× faster than the VLM on the same photo — but only from a fixed 80-class vocabulary with no free-form understanding. Separately, this investigation confirmed the board has genuine dedicated HDMI-input capture hardware (not just a webcam-style option), tracked down a real memory-fragmentation bug that broke capture after 27 hours of uptime, and found two reproducible bugs in Rockchip's own vision SDK tooling along the way.

## Hardware and Software Setup

- **Board:** Radxa ROCK 5B+, RK3588, 8GB RAM, 6 TOPS NPU
- **VLM:** `3ib0n/Qwen2-VL-2B-rkllm` — pre-converted, ~2.0GB language model (`.rkllm`) + ~1.4GB vision encoder (`.rknn`), prebuilt aarch64 binaries
- **CLIP:** `openai/clip-vit-base-patch32`, converted from ONNX via Rockchip's `rknn_model_zoo/examples/clip` conversion script, `fp16`, no quantization — image encoder (352MB ONNX → 187MB `.rknn`) and text encoder (254MB ONNX → 130MB `.rknn`) as separate models
- **Detector:** YOLOv5s (`yolov5s_relu_rk3588.rknn`, 8.4MB), a pre-existing model from an untouched `~/traffic-detection` project on this board — benchmarked read-only via a new standalone script, without modifying that project
- **Runtime:** `rknnlite` (on-device inference runtime; conversion itself required the x86-only `rknn-toolkit2` for CLIP)
- **HDMI capture:** `/dev/video0`, `rk_hdmirx` driver (`fdee0000.hdmirx-controller`)

## Methodology

Each model was run against real test images (not synthetic/cherry-picked inputs), with wall-clock timing measured end to end (model load + inference, not just inference alone, since load time matters for any interactive use case). Where a bug was hit in vendor-provided tooling, it was root-caused rather than worked around silently, and a minimal reproducing wrapper was built instead of patching the vendor binary.

## Results

### The VLM: works, is accurate, and is nowhere near real-time

Ran `Qwen2-VL-2B` via its bundled interactive demo binary — no conversion work needed, no webcam needed (VLMs process static images, not live streams). Standalone vision encoding alone: 2.4s model load + 5.76s per-image encode. The full pipeline (vision encoder + language model) correctly described a bundled test image in genuine, coherent detail (identifying it as "a surreal or humorous take on space exploration" featuring an astronaut and the Earth/moon) — not a generic non-answer.

**A real, reproducible bug was found in the vendor's own binary.** The bundled `demo` REPL doesn't exit cleanly on stdin EOF — once the one real prompt is consumed, it spins forever printing `E rkllm: prompt must contains <image> tag` with no backoff or exit condition. Hit twice (once producing a 2.4GB log, once a 1.3GB log) before a clean wrapper (`vlm_infer.py`) was built: it streams the subprocess's stdout, detects both models finishing load, sends exactly one prompt, captures exactly one answer block, and kills the process the instant the loop starts repeating. Two clean runs with this wrapper:

| Query | Result | Total time |
|---|---|---|
| "What is in this image?" | Correct description | 18.25s (3.6s LLM load + 2.8s vision load + generation) |
| "How many people are in this image?" | "There is one person in the image." | 15.66s |

**This directly answers "is live video AI detection possible with a VLM" — no, not on this hardware.** RK3588 has no NPU-level video pipeline at all; video decode/encode is a separate hardware block (the VPU, via Rockchip's MPP library) entirely disconnected from the NPU, which only ever does single-tensor inference. Any "live video AI" is always a software loop — decode a frame, run NPU inference on it as a plain image, repeat — and for a VLM specifically, the vision-encode step alone (5.76s) is a hard ceiling well under 1 FPS before the language model has even started generating.

**Types of VLM considered for context**, in order of increasing weight relative to what was tested: general-purpose chat VLMs (Qwen2-VL, MiniCPM-V, LLaVA-class — what was tested here, the heaviest practical option); OCR/document VLMs (same shape, narrower training focus, not lighter); Video-LLMs (multiple sampled frames feeding one context — strictly heavier, reinforces the finding above rather than offering a way around it); grounding/detection VLMs (bounding-box output from open-vocabulary text queries — a full vision-transformer forward pass, not obviously lighter); and embedding-only vision-language models (CLIP, SigLIP — no LLM decode step at all, the one category structurally lighter than what was tested) — tested directly next.

### CLIP: confirms the "structurally lighter" prediction, but it's a classifier, not a detector

CLIP was officially supported for RK3588 (Rockchip's own `rknn_model_zoo`) but only as ONNX source plus a conversion script — no pre-converted download existed, unlike the VLM. Conversion ran on the x86 machine with `rknn-toolkit2`; both encoders converted cleanly with no errors.

**A real bug was found bringing it up on-device**, distinct from the VLM's bug: `rknnlite` globally monkeypatches Python's stdlib `logging._nameToLevel` at import time, replacing standard level names (`'WARNING'`, `'INFO'`) with single-letter keys (`'W'`, `'I'`) — which silently breaks any library imported afterward that calls `logger.setLevel()` with a standard name. `torch` (pulled in transitively by `transformers`, needed for CLIP's tokenizer) does exactly this and failed with `ValueError: Unknown level: 'WARNING'` until import order was flipped (`transformers` before `rknnlite`). A genuine, reproducible bug in Rockchip's SDK — global mutation of stdlib state that any other library sharing the process can trip over.

**Results — confident, correct classification:**

| Image | Candidate labels | Winner | Score |
|---|---|---|---|
| Bundled test photo (a dog) | dog / cat / car / person | dog | **0.974** |
| `bus.jpg` (real photo, multiple people boarding a bus) | bus / car / dog / bicycle / traffic light | bus | **0.999** |

Wrong labels scored near-zero (0.0001–0.015) in both cases — confident correct discrimination, not a coin flip. Total wall time: 2.16–4.86s including both model loads, both encode passes, and similarity scoring — noticeably faster than the VLM's 15-18s per turn, confirming the "structurally lighter, no decode step" prediction with a real measurement.

**Honest scope: CLIP is a classifier, not a detector.** It confidently answers "is this image a bus/car/bicycle?" for an image someone already cropped or framed, but has no notion of *where* an object is or *how many* there are, and produces no bounding boxes at all. Its genuine fit is narrower than a detector's: whole-image classification/tagging, or image retrieval/search.

### YOLOv5s: the fastest, most real-time-capable workload found in this entire investigation

Benchmarked read-only (no modification to the source project), against a standalone script porting the same preprocessing/decode/NMS logic to the on-device `rknnlite` runtime:

| Image | Mean FPS | Mean latency | Detections |
|---|---|---|---|
| Street scene (bus + pedestrians) | 41.37 | 24.2ms | bus (0.69), 4× person (0.30-0.88) |
| Crowded scene (16 people, sports gear) | 41.70 | 24.0ms | 16× person, sports ball, bat/glove, chairs, backpack |
| Street scene (bicycles/motorbikes) | ~consistent (5-iter spot check) | ~24ms | 5× person, bicycle (0.71), 3× motorbike (0.51-0.61) |

Model load + `init_runtime`: under 0.1s. Throughput held at ~24ms/frame regardless of scene complexity (1 object vs. 25+ objects) — NMS/postprocessing cost doesn't meaningfully move the needle at these object counts. Correct class labels across the full tested range, including non-traffic classes in the crowded scene, ruling out an overfit/narrow result.

### A real three-way comparison on identical input

To avoid three isolated, non-comparable claims, all three models were run against the same photo (the bus/pedestrian street scene) with the same implicit task ("how many people, what vehicle"):

| Model | Answer | Accuracy | Time |
|---|---|---|---|
| YOLOv5s | 4× person detected (0.30-0.88 conf), bus (0.69 conf) | Correct (this is the well-known COCO `bus.jpg`, which does show 4 people) | **24ms** |
| Qwen2-VL-2B | "Two people... a blue bus with the text 'cero emisiones' (zero emissions)..." | Undercounted (said 2, actual 4); correctly read livery text off the bus (genuine OCR-adjacent capability, unprompted) | 19.07s |

**YOLO is roughly 800× faster and more accurate for counting/localization; the VLM's genuine edge is reading text and producing free-form scene description that a detector's fixed 80-class vocabulary structurally cannot do.** These are different tools for different questions, not a strict speed/accuracy hierarchy — a system that needs both counting and "what does the sign say" would plausibly want both models, used for what each is actually good at.

### The board has real HDMI-input capture hardware — and a real fragmentation bug was found and root-caused

Separately from any of the model benchmarking: `/dev/video0` is a functional V4L2 capture device (`rk_hdmirx`), confirmed via `v4l2-ctl` to support up to 4096×2160 (4K), 20-600MHz pixel clock, CTA-861 timings, progressive and interlaced — genuine dedicated HDMI capture, not just an HDMI output port.

**First attempt with a real source connected failed**, despite the driver correctly locking a real 1920×1080p60 signal: `VIDIOC_REQBUFS returned -1 (Cannot allocate memory)`, with a kernel warning trace showing `rk_hdmirx: dma alloc of size 6221824 failed`. Root-caused via the trace: this driver doesn't use the CMA reserved memory pool for capture buffers — it calls the direct DMA allocator, which needs a genuinely physically-contiguous ~6MB block from the general page allocator. After ~27 hours of uptime and this investigation's own heavy memory churn (repeatedly loading/unloading multiple multi-GB LLM models), physical memory was fragmented enough that no contiguous 6MB block remained, even though total free memory (including a healthy 182MB of free CMA specifically) was plentiful. **Not a hardware or driver defect — a consequence of extended uptime and this session's own workload pattern.**

Rebooting (fresh boot = maximally contiguous memory) fixed it immediately: a real 1920×1080 BGR frame (6,220,800 bytes, matching exactly) was captured and verified as genuine non-degenerate image data (mean 46.4, std 14.0 — a clean splash-screen image, not noise or corruption). Three repeated single-shot captures measured 217-323ms each — this includes CLI process startup/device-open/buffer-negotiation overhead per invocation, not a sustained streaming rate; the signal itself runs at its native 60fps once a persistent capture loop keeps buffers allocated instead of reopening the device per frame (see the companion streaming-appliance paper for the actual sustained-throughput pipeline built on top of this).

## Discussion

For anyone choosing a vision workload for this class of hardware, the decision tree is fairly clean based on these measurements: if the task is genuinely real-time (anything approaching video framerates) and the set of things you need to recognize is fixed and known in advance, a YOLO-class detector is the only viable option by roughly three orders of magnitude — nothing else tested comes close. If the task is single-image or low-frequency and needs free-form understanding (reading text, describing a scene, answering an open-ended question), a VLM is viable but should be budgeted at 15-20 seconds per query, not treated as anything approaching interactive. CLIP occupies a real but narrow middle ground — fast, confident whole-image classification with zero localization — useful for tagging/retrieval, not detection.

The HDMI-capture fragmentation bug is a practical operational lesson independent of any model choice: a board doing heavy, repeated large-allocation/deallocation cycles (like this investigation's own multi-GB model loading/unloading) can silently degrade an unrelated subsystem's ability to get a large contiguous DMA buffer, purely from uptime and memory-pressure history, with a fix as simple as a reboot once diagnosed.

## Reproducibility Notes

- VLM wrapper: `~/npu-bench/vlm_test/vlm_infer.py`
- CLIP inference script: `~/npu-bench/clip_test/clip_infer.py` (uses `rknnlite.api.RKNNLite`, not the x86-only `rknn.api.RKNN` the model zoo's demo assumes; import `transformers` before `rknnlite` to avoid the `logging._nameToLevel` monkeypatch conflict)
- YOLOv5s benchmark: `~/npu-bench/traffic_yolo_bench.py` (reads the pre-existing `~/traffic-detection` project's model file read-only; does not modify that project)
- HDMI capture verification: standard `v4l2-ctl -d /dev/video0 --all` / `--stream-mmap`
- CLIP conversion (x86 only): Rockchip's `rknn_model_zoo/examples/clip/convert.py`, `target_platform=rk3588`, default fp16, no quantization

## Limitations / What Wasn't Tested

- Grounding/detection VLMs (Grounding DINO, OWL-ViT class) were considered but not benchmarked — flagged as a plausible middle ground between CLIP and a full chat VLM, not tested.
- Video-LLMs (multi-frame VLM variants) were reasoned through as strictly heavier than single-image VLM inference but not empirically tested.
- CLIP was tested only for whole-image single-label classification against a small candidate label set (4-5 labels); retrieval/search use cases and larger label vocabularies weren't exercised.
- YOLOv5s throughput was measured on static test images run repeatedly, not against a genuinely continuous live capture stream from the HDMI-input hardware — the companion streaming-appliance paper covers the actual sustained live-capture pipeline separately.
- No attempt was made to combine the detector with the HDMI capture path into a live overlay in this specific investigation — that combination is covered in a separate section of this project (see the companion paper on the streaming appliance) which did test NPU-plus-encode concurrent operation directly.
