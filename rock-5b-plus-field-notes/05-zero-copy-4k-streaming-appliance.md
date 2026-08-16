# A Single-Box 4K60 Capture-Encode-Stream Appliance on an RK3588 SBC — Built, Broken, and Fixed in Public

**From "does this board have a real HDMI capture port" to a working, tested, lightweight OBS alternative, with every dead end kept in the record**

## Abstract

Starting from a confirmed discovery that a Radxa ROCK 5B+ has genuine dedicated HDMI-input capture hardware (not just output), this investigation built a real single-box capture/preview/encode/stream pipeline — the pitch being that a normal streaming setup needs a capture card plus a separate PC, and this is testing whether one $200 SBC can do both. The naive version failed under real combined load (capture dropped from 60fps to ~24fps once local preview, hardware encode, and network upload all ran concurrently), root-caused to unaccelerated software preview rendering starving the capture process of CPU. A zero-copy GStreamer pipeline, built from source against Rockchip's own hardware video (MPP) and 2D-scaling (RGA) accelerators, fixed it completely: genuine sustained 51-60fps at full native 4K resolution with local preview, hardware H.264 encode, and network upload all running simultaneously, confirmed against Radxa's own 4Kp60 marketing claim with a real 4K source. Along the way, a fullscreen-specific RGA driver bug consumed a multi-day isolation effort — eleven falsified hypotheses using progressively more faithful standalone reproductions, a real (but ultimately unrelated) mislabeled-default bug fixed along the way, and a final resolution that turned out to be caused by the investigation's own per-frame debug logging perturbing pipeline timing enough to trigger a live-only race condition. The finished pipeline was then extended into three working streamer-facing features — RTMP push, an instant-replay clip buffer, and a browser-based control panel — with a fourth (NPU face-detection auto-framing) honestly documented as blocked on missing webcam hardware rather than faked or skipped silently.

## Hardware and Software Setup

- **Board:** Radxa ROCK 5B+, RK3588, 8GB RAM, boots into a full KDE Plasma Wayland desktop (SDDM), not a bare console
- **Capture:** `/dev/video0`, `rk_hdmirx` V4L2 driver, confirmed to support up to 4096×2160
- **Hardware video encode:** Rockchip MPP (`github.com/rockchip-linux/mpp`, built from source — note the `airockchip/mpp` mirror does not exist), exposed to GStreamer via `mpph264enc`
- **Hardware 2D scale/convert:** RGA (`github.com/airockchip/librga`, prebuilt aarch64 `.so`) exposed to GStreamer via a separately-sourced `rgavideoconvert` element (`github.com/corenel/gstreamer-rga`, patched during this investigation)
- **GStreamer plugins built from source:** `gstreamer-rockchip` (a fork explicitly "fixed for streaming," `github.com/BoxCloudIRL/gstreamer-rockchip`) and `gstreamer-rga`
- **Display:** `waylandsink` against the board's own `kwin_wayland` compositor (not `kmssink` — `kwin_wayland` holds DRM master, so `kmssink` fails with `Permission denied`)
- **Streaming target for testing:** `mediamtx` v1.20.0 (local RTMP server, no real platform stream key was available)

## Methodology

Every throughput claim below is a wall-clock, real-frame-count measurement, not an estimate — either `ffprobe -count_packets` against the actual output file, or live `fpsdisplaysink` instrumentation, cross-checked against each other where both were used. Where a pipeline configuration "looked" smooth to visual inspection, that observation is reported alongside the number, not instead of it. Failures were root-caused with real debugging (source-level debug prints, standalone C/C++ reproductions of the exact failing library call) rather than worked around and left unexplained.

## Results

### The naive pipeline: every stage is fast alone, and none of that matters combined

Tested each stage of "capture + local preview + hardware encode + network upload" in isolation first:

| Stage | Isolated result |
|---|---|
| Sustained capture (`ffmpeg -f v4l2 ... -f null -`) | 60fps, speed=1.00x, zero drops, 9.2% CPU |
| Local preview (`ffplay`, software SDL2 render) | Visually laggy — ~480% CPU, compositor at ~285% CPU, load average 13.45/8 cores |
| Hardware H.264 encode (raw MPP `mpi_enc_test`) | 132.80fps average on real captured 1080p frames — over 2× real-time |
| Network upload (Wi-Fi, real Cloudflare endpoint, 3 runs) | 70/240/204 Mbit/s vs. the encoder's actual 7.6Mbit/s output — ~9× headroom even in the worst case |

**Running all four concurrently broke the pipeline.** Capture, which sustained a clean 60fps/9.2%CPU alone, dropped to ~24-25fps under combined load, needing to duplicate 440 frames just to keep its output pipe fed. The hardware encoder still only took 4ms/frame on its own end — it simply wasn't being handed frames fast enough to average above 24.88fps. Network upload was **not** the problem; it held steady at 25-35MB/s throughout, matching the isolated measurements almost exactly. **Root cause: `ffplay`'s unaccelerated software rendering plus the desktop compositor's own overhead together starved the capture process of the CPU time it needed to sustain real-time reads and dual-pipe fan-out.** This was reported honestly as an open question rather than claimed solved — whether a properly hardware-accelerated local display path would avoid the contention was explicitly left untested at that point.

### The fix: zero-copy GStreamer, built from source, closing the open question

Replacing `ffplay` with `gst-launch-1.0`'s `v4l2src ! waylandsink` alone dropped the GStreamer process to ~1-2% CPU (down from `ffplay`'s ~480%) — `v4l2src` auto-negotiated true zero-copy DMABuf mode with no explicit flag needed. A first attempt to `tee` this into both display and encode made things *worse* (7.41fps) — GStreamer negotiates one shared upstream format across all `tee` branches, so the moment one branch needed software conversion, the *whole* pipeline (including the untouched display branch) fell out of zero-copy mode. The actual fix required building a real hardware encoder GStreamer element (none existed in the environment) and confirming `mpph264enc`'s sink caps directly accept `BGR` — the capture device's native format — so no conversion was needed on either tee branch, keeping the single upstream negotiation in DMABuf zero-copy the whole way through.

**Full working pipeline:** `v4l2src device=/dev/video0 io-mode=4 ! tee name=t` → branch A: `queue ! waylandsink sync=false fullscreen=true` (display) → branch B: `queue ! mpph264enc ! h264parse ! filesink` (encode).

**Result, all four original pieces running together:** ~60fps real encode throughput (708-709 real frames across two ~12-second runs, confirmed by two independent measurement methods — post-hoc `ffmpeg` frame counting and live `fpsdisplaysink` instrumentation reporting a clean average of 60.09). Upload held steady at 15-26MB/s throughout, unaffected. `gst-launch-1.0` itself stayed at 8-15% CPU under the full combined load. Confirmed at full native 1920×1080 resolution end to end (no `videoscale` element anywhere in the pipeline, verified from negotiated caps at every stage). **This closed the open question from the naive-pipeline section: the earlier failure was real for the tools tested at the time, not a hardware ceiling — a properly hardware-accelerated display path removes the contention entirely.**

### Confirming Radxa's 4Kp60 claim with a real 4K source

Every result above used whatever source was connected, which negotiated 1080p60. A genuine 4K-capable source was connected specifically to test Radxa's marketing claim rather than take the spec sheet on faith. Getting the signal connected required diagnosing two separate real issues (a remote-Windows-display-resolution gotcha unrelated to this board, and this board's own HDMI-in EDID reporting an internally inconsistent maximum TMDS clock that — contrary to a direct prediction — did not actually block true 4K60, most likely because the source negotiated via HDMI 2.1 FRL rather than legacy TMDS).

- **Capture and hardware encode: genuine 4K60**, matching the claim exactly, once one real format issue was fixed (the capture device doesn't support `BGR3` at 3840×2160 at all — bandwidth too high for that format path — only YUV-family formats; forcing `NV12` explicitly fixed it). Isolated encode-branch measurement: 806 frames/~13.4s ≈ 60.0fps, verified as valid decodable H.264 at full 3840×2160, ~62Mbps output.
- **Local zero-copy display at 4K failed outright**, root-caused precisely rather than left as "4K doesn't work": at 4K, `v4l2src` silently stops offering DMABuf memory and falls back to system-memory `NV12` — but `waylandsink`'s system-memory fallback path only accepts RGB-family formats, not `NV12`, and the capture device can't produce `BGR3` at this resolution either. A genuine three-way gap between what the capture device can produce, what memory type it hands over, and what the compositor's fallback path accepts.
- **Fixed the same session**: since the encode branch already proved it works fine with plain system-memory `NV12` at native 4K (no DMABuf required), only the display branch needed a fallback — `videoscale`+`videoconvert` down to 1080p specifically for local preview, running in parallel with an untouched native-4K encode branch. Worked immediately: encode held an unaffected ~60fps at full native 4K across two runs (862 frames/~14.4s and 1338 frames/~22.3s), while the display branch's real CPU cost (~118% CPU for the scale/convert work) stayed fully isolated to that branch. User-confirmed smooth across a 25-second run.

### Sustained real-content throughput: honestly lower than short-burst numbers

Every 4K figure above came from short (13-25 second) runs, several against largely static content. Running the fixed pipeline continuously against genuine full-motion video for several minutes gave a materially different, lower number:

| Run | Frames | Duration | Average FPS |
|---|---|---|---|
| Windowed preview, real video content | 12,242 | 229.5s | **~53.4fps** |
| Fullscreen preview, real video content | 6,583 | ~128s | **~51.4fps** |

The short-burst 60fps numbers ran against a mostly-static desktop/splash screen — cheap for both the display-branch scaling and the encoder. Genuine full-motion video gives both continuously-changing work every frame, and sustained throughput settles around 51-53fps instead — still comfortably smooth for a live stream, but reported explicitly as the more representative figure rather than letting the best-case number stand alone.

### Downscaling for 1080p-capped viewers: CPU scaling bottlenecks hard, RGA fixes it completely

A distinct real-world question: a streamer capturing at native 4K may want to *send* a 1080p stream for viewers/platforms capped there — not just a local preview, the actual outgoing encode. CPU-based `videoscale` inline before the encoder (a serial bottleneck, unlike the parallel-branch display case above) measured a real, repeatable ceiling of ~15-20fps (two independent runs: 15.4fps and 19.9fps) — well below the ~51-60fps this board hits for native 4K. GPU/EGL scaling was investigated and found not to be a dead end (a first failure was misdiagnosed as a broken GPU context — a synthetic test proved the context initializes fine; the real, narrower blocker was `v4l2src` failing to negotiate caps with `glupload` specifically) but wasn't pursued further once RGA gave a complete answer via a different path.

**RGA, Rockchip's dedicated 2D hardware accelerator, completely fixed the bottleneck** — built from scratch (`librga` plus a separate purpose-built GStreamer element, `github.com/corenel/gstreamer-rga`, distinct from the RGA support baked into the encoder plugin, which only covers internal format conversion, not general resize — confirmed by testing directly rather than assumed) despite community documentation elsewhere flagging RGA as "currently abnormal on RK3588" (tested directly rather than trusted either way). Encode-only: 4651 frames/68.6s ≈ 67.8fps. Full tee'd pipeline with both RGA-scaled preview and RGA-scaled 1080p encode running simultaneously: 2966 frames/49.7s ≈ 59.7fps, `gst-launch-1.0` itself at just ~23% CPU for both branches combined — a genuine 3-4× improvement over software scaling.

### The fullscreen RGA bug: eleven falsified hypotheses and a resolution nobody would have guessed up front

Pushing RGA to a fullscreen (1920×1080) local preview — as opposed to the smaller 1280×720 window that already worked — failed reliably (`RgaBlit fail: Invalid argument`), while the smaller window kept working in the exact same pipeline shape. This became the single longest isolation effort in the whole investigation.

**What was ruled out, systematically, each with a real test rather than reasoning alone:**
- Fullscreen mode itself, and the `tee` pipeline structure — both ruled out by bisection (windowed-at-exactly-1920×1080 failed identically; 1280×720 within a `tee` still worked)
- A hybrid CPU-scaled-display fallback was tried and *measured* to genuinely cost real throughput (29.9fps vs. ~60fps pure-RGA) — correcting an earlier assumption in this same investigation that a CPU-heavy display branch's cost stays cleanly isolated from an untouched encode branch; true for a *small* CPU cost, not true for a genuine full-resolution software scale
- Stride-math in the RGA element's format-setup code — a real bug **was** found here (the `core-mask` GObject property defaulted to a specific forced RGA3 core while being labeled "auto" in both a code comment and the property description, contradicting the actual value from the header's own enum) and fixed — but retesting showed the *exact same failure*, byte-for-byte identical error and buffer signature. This bug was real, worth fixing, and not the cause of the symptom under investigation.
- A "reboot fixes it" theory, formed by analogy to an earlier, unrelated, confirmed memory-fragmentation bug in the HDMI capture path — tested directly on a genuinely fresh reboot, and **failed 4/4 times**, unambiguously falsifying the theory rather than letting it stand unverified
- A sequence of increasingly faithful standalone C/C++ reproductions of the exact failing call (same resolutions, same pixel formats, same mmu/core flags) systematically ruled out: buffer size alone, a real DMA-BUF source with a `malloc`'d destination, the specific legacy blit API `gstreamer-rga` actually uses (as opposed to the modern API used in earlier reproduction attempts), a `memfd`+`mmap` shared-memory destination mimicking Wayland's allocator, the destination `fd` field's exact value convention, the specific DMA heap used for the source allocation (an initial apparent reproduction here was a false positive from misattributed `dmesg` timestamps, corrected via precise timestamp correlation), and — going as far as writing a minimal V4L2-multiplanar capture program against the real hardware device to export genuine in-flight capture buffers — even a fully live V4L2 capture buffer fed straight into the RGA call, 8/8 successes at both resolutions.
- The exact real memory allocator GStreamer uses for the destination buffer (`GstShmAllocator`, with its specific alignment padding) — reproduced exactly via GStreamer's own allocator API directly — still succeeded

**Every individual characteristic of the real failing call reproduced successfully in isolation.** Only the actual multi-threaded live `gst-launch-1.0` pipeline triggered the failure — pointing at a timing/concurrency interaction, not a static parameter mismatch.

**Resolution: the debug instrumentation added during this very isolation effort was itself the trigger.** The per-frame `g_printerr()` calls added to the plugin's source to debug the original failure — including a `gst_buffer_peek_memory()` call and a blocking stderr write on every single blit — were living in the pipeline's hot path. Once all of them were removed and the plugin rebuilt, **fullscreen 1920×1080 RGA display stopped failing entirely: 5/5 clean runs, zero errors, including one full 62-second sustained run (2350 frames, clean EOS)**. This is fully consistent with why none of the standalone C++ reproductions ever failed — none of them had per-frame debug I/O in a tight loop either. The most likely mechanism: the extra per-frame latency and stderr I/O was shifting frame timing enough to expose a real (and still not fully mechanistically explained at the driver level) race in the driver's buffer-pool/queue handling under the live pipeline's actual threading — removing the overhead removed the trigger, not just the symptom.

**A second, separate regression surfaced on a later retest** — both windowed and fullscreen configurations had converged on the same ~38-40fps, down from windowed's previous ~60fps. Investigated by elimination: CPU governor pinning, DMC/DDR frequency, GPU devfreq activity, and compositor CPU cost (measured via `/proc/<pid>/stat` tick deltas, not `ps`'s misleading lifetime-average `%cpu` for a multi-hour-old process) were each checked directly and ruled out in turn; raw 4K capture alone still hit its true ~58-60fps, ruling out a capture regression. **Real cause: `waylandsink`'s default `sync=true` clock-based frame pacing.** A/B tested twice, back-to-back: `sync=true` consistently ~39.8fps, `sync=false` consistently ~58fps. For a live capture source (as opposed to file/VOD playback where clock-accurate pacing matters), `sync=false` is the objectively correct setting, not a workaround — there's no reason to pace a live feed against wall-clock timestamps when the goal is always "show the most current frame." **Final confirmed numbers, both configurations, clean EOS, zero RGA errors:** windowed 1280×720 — 59.16fps; fullscreen 1920×1080 — 58.07fps.

## From Working Pipeline to Streamer-Facing Product

With 4K60 capture, hardware encode, RGA scaling, and a fixed local preview all proven, four concrete streamer-facing features were scoped: three built and tested end to end, one honestly documented as blocked.

### 1. RTMP push — closing the "can you actually go live" gap

No real platform stream key was available, so `mediamtx` (a local RTMP server, prebuilt binary, no compile needed) was run on the board itself to prove the mechanism honestly rather than skip it. The first attempt hit a **new, previously-untested manifestation of the RGA bug**: feeding RGA's `BGRx` output directly into `mpph264enc`'s own DMA-BUF-backed input pool (both RGA source *and* destination holding real file descriptors — a combination the eleven-hypothesis isolation pass above never actually covered, since it only ever exercised dmabuf-source + virtual-address-destination). Fixed by keeping the pixel format `NV12` end-to-end instead of converting to `BGRx`, matching the capture's native format all the way to the encoder. **Verified two independent ways**: the local RTMP server logged the stream as online and publishing, and `ffprobe` was pointed at the live `rtmp://` URL *while the stream was running* and successfully decoded it (`h264, 1280×720, ~62.5fps`) — proving the stream was genuinely valid and playable, not merely "connected." 30-second sustained run, clean EOS, zero RGA errors.

### 2. Instant replay / clip buffer

Built on `splitmuxsink`'s native `max-files` ring-buffer property — no custom buffer-management code needed. A save command concatenates whichever recent segments fall inside a requested time window into a standalone clip, keyed off file modification times rather than assuming fixed segment length (the most recent segment is usually still being written when a save triggers). Tested twice: standalone (produced a valid 8.03s clip from 3 segments mid-recording, confirmed the ring buffer correctly discarded old segments as new ones cycled in) and through the unified web panel below while a live RTMP push ran simultaneously (produced a valid 13.03s clip with zero interruption to the live stream).

### 3. A lightweight browser-based control panel

A single-file Flask app serving start/stop, connection settings, a save-clip button, and live status. This required a real design correction discovered during testing, not assumed up front: `/dev/video0` can only be held open by one process at a time (confirmed earlier via `Device busy` errors), so streaming and clip-recording can't run as two independent pipelines — the panel launches **one** `gst-launch-1.0` process with a `tee`, splitting a single capture into both the RTMP branch and the ring-buffer branch, the same way a real product (including OBS itself) would.

**A real bug was found and fixed, not just demoed working.** The first version's stop handler used `subprocess.Popen(shell=True)` and signaled only the returned PID; confirmed via `ps aux` that this left the actual `gst-launch-1.0` child process orphaned and still holding `/dev/video0` after the stop endpoint reported success. Root cause: `shell=True` with a quoted argument causes the shell to fork rather than exec-replace itself, so the Popen handle refers to the shell wrapper, not the real worker. Fixed with `preexec_fn=os.setsid` at launch and `os.killpg()` on the whole process group to stop — retested and confirmed clean: exactly the expected two processes while running, zero after stop, device release confirmed via `v4l2-ctl` immediately after. Tested through the full HTTP round-trip: start → confirm both branches live via the RTMP server's log and on-disk segments → trigger a clip → stop → confirm clean process and device release.

### 4. NPU face-detection auto-framing — genuinely blocked, not faked

This board's only video input is the HDMI capture device; `v4l2-ctl --list-devices` confirms no USB webcam is connected. The feature needs a second camera pointed at a person, which isn't physically present. Documented plainly as blocked on missing hardware rather than skipped silently or worked around with a substitute — with a scoping note for later: the stock detector benchmarked elsewhere in this investigation (see the companion vision-workloads paper) recognizes generic COCO classes like "person," not faces specifically, so this would need either a dedicated face-detection model or accepting coarse person-detection-based framing once a webcam is actually available to test against.

## Discussion

The overall arc here is a useful case study in what "zero-copy" actually requires in practice: every individual hardware capability (capture, MPP encode, RGA scale) worked correctly and fast in isolation from the very first test, and the entire multi-week effort was spent on *pipeline plumbing* — getting GStreamer's format negotiation, buffer-memory-type compatibility, and clock-sync defaults to actually route data through those capabilities without a silent software fallback anywhere in the chain. The fullscreen RGA saga specifically is worth internalizing as a debugging lesson: instrumentation added to observe a bug can itself become the bug, and the fix — removing debug prints — being the actual resolution is a real, if slightly embarrassing, possibility worth checking early rather than only after eleven other hypotheses are exhausted.

The finished system genuinely answers the "affordable single-box streaming appliance" question in the affirmative for this hardware class: 4K60 capture and hardware encode, a real (if resolution-fallback-required) local preview, RTMP push proven against a real ingest server, an instant-replay buffer, and a browser control surface, all running concurrently on one ~$200 board with headroom to spare (`gst-launch-1.0` itself staying under 25% CPU even under full combined load in the RGA-fixed configuration).

## Reproducibility Notes

- Core pipeline elements built from source: `github.com/rockchip-linux/mpp`, `github.com/BoxCloudIRL/gstreamer-rockchip`, `github.com/airockchip/librga`, `github.com/corenel/gstreamer-rga`
- Udev rule pattern needed twice for non-root device access: `KERNEL=="mpp_service"|"rga", MODE="0660", GROUP="video"` (plus the same for `/dev/dma_heap/*`)
- Working full pipeline (1080p, tee'd display + encode): `v4l2src device=/dev/video0 io-mode=4 ! tee name=t` → `queue max-size-buffers=1 leaky=downstream ! waylandsink sync=false fullscreen=true` + `queue max-size-buffers=2 leaky=downstream ! mpph264enc ! h264parse ! filesink`
- 4K local-preview fallback: `v4l2src(NV12, 4K) ! tee` → `queue ! videoscale ! video/x-raw,width=1920,height=1080 ! videoconvert ! waylandsink sync=false` (display) + unchanged native-4K encode branch
- RGA 1080p-downscale-for-viewers pipeline: replace `videoscale`/`videoconvert` with `rgavideoconvert` on the encode branch specifically
- `sync=false` should be present on every `waylandsink` instance for a live capture source — this was not the default and cost roughly 1.5× throughput when omitted
- Streaming appliance scripts: `~/npu-bench/clip_buffer.py`, `~/npu-bench/stream_panel.py`; local RTMP test server: `mediamtx` v1.20.0
- RGA fullscreen-bug isolation tooling and standalone reproduction programs (V4L2 capture, DMA-BUF, shm-allocator tests) are documented inline in `REPORT.md`; the final fix is the removal of debug instrumentation from `gstrgavideoconvert.c`, not a source change to the RGA call itself (beyond the separately-fixed, separately-real `core-mask` default bug)

## Limitations / What Wasn't Tested

- The compositor-side ~30fps display-only ceiling noted early in the investigation (distinct from the encode-branch throughput, which was cleanly measured at ~60fps) was root-caused only partially (likely `kwin`'s own frame-presentation loop, possibly related to its fractional display-scale factor) and left as a genuinely open question, since it didn't affect the branch that matters for what a viewer would see.
- NPU face-detection auto-framing remains untested pending webcam hardware.
- A real platform (Twitch/YouTube) stream key was never used — RTMP push was validated exclusively against a local `mediamtx` server, which proves the mechanism but not platform-specific ingest quirks (adaptive bitrate ladders, platform-side transcoding requirements, etc.).
- The RGA driver race that debug-print removal fixed was never mechanistically explained at the actual driver-source level (Rockchip's RGA kernel driver is closed-source) — the fix is empirically confirmed (5/5 clean runs) but the underlying race condition inside the driver's buffer-pool/queue handling remains a documented-but-unexplained finding, not a root-caused one at that layer.
- GPU/EGL-based scaling (as an alternative to RGA) was found to have a real, narrower blocker (`v4l2src`/`glupload` caps negotiation) that was diagnosed but not resolved, since RGA already gave a complete working answer via a different path.
