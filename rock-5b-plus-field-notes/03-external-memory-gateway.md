# External Memory for a Hard-Capped On-Device LLM: Persistence, Eviction, Summarization, and Retrieval on an 8GB RK3588 Board

**Building indefinite-length conversation on top of a runtime whose own KV cache silently discards history**

## Abstract

The RKLLM runtime's KV cache is a fixed-size, shared buffer sized exactly from `max_context` at model-conversion time (confirmed precisely: 352MB at the SDK's own 16,384-token maximum, via runtime diagnostics and an independently-derived per-token formula that matched to the byte). Once a conversation exceeds that window, the runtime silently evicts the oldest tokens — RAM is never the real constraint (confirmed up to the SDK's documented ceiling, with 5.4GB still free), the model's own attention window is. This paper documents building a thin external memory layer — a stateless-proxy Flask gateway sitting in front of the existing `rkllama` server, persisting full conversation history in SQLite and constructing a bounded window each turn — and then following that design through eviction, model-generated summarization, retrieval-augmented recall, and a full regression suite. Several summarization/compaction attempts failed in the same underlying way (asking a language model to re-synthesize already-summary-shaped text causes it to echo or truncate rather than genuinely re-condense) before a deterministic, non-model-based redesign fixed it permanently. Separately, two genuine recall failures were traced not to the memory system at all, but to the *main conversation model's* own capability to extract facts from context it was correctly given — a distinction with real design implications for anyone building similar systems.

## Hardware and Software Setup

- **Board:** Radxa ROCK 5B+, RK3588, 8GB RAM
- **Base LLM server:** `rkllama` (port 8080), unmodified
- **New component:** `memory_gateway.py`, a standalone Flask service (port 8081), SQLite storage at `memory_gateway.db`
- **Models used:** `TinyLlama-1.1B` (fast, weaker recall/instruction-following), `Qwen3-4B-rk3588-16k` (slower, stronger recall — used both as an alternate main-conversation model and as a dedicated summarizer)
- **Embedding model (added later):** `sentence-transformers/all-MiniLM-L6-v2` (22M params, 384-dim, ~90MB fp32), reusing the `torch`/`transformers` already present in the environment

## Design

### Why an external proxy, not a change to `rkllama`

`rkllama` is fully stateless — the client resends the entire `messages` array every call, with no conversation-ID concept anywhere. Its existing prompt-*cache* mechanism (SHA256-hash-derived filenames) is a reprocessing speedup, not a memory system: the actual on-NPU KV cache is cleared after every single inference call, and the cache hash silently misses whenever the resent history doesn't match exactly. Since the server works purely off whatever `messages` array it's handed, the cleanest approach — and one that touches zero existing `rkllama` code — is a thin proxy: persist full history externally, construct a bounded window each turn, forward that window unchanged to `rkllama`'s real `/api/chat`. `rkllama`'s own prompt-cache mechanism keeps working underneath for free whenever the window doesn't change between calls.

### Core mechanism

- **Storage:** SQLite, one row per turn (`conversation_id`, `turn_index`, `role`, `content`, `created_at`), primary key `(conversation_id, turn_index)`.
- **Conversation ID:** explicit, client-supplied (or gateway-generated on first use) — replaces `rkllama`'s fragile "no assistant messages yet = new conversation" heuristic with a reliable identifier.
- **Per-request flow:** load full history for the conversation; append the new user message immediately (durable even if generation later fails); build a token-budgeted window (pin the system message if present, walk backward from the most recent turns accumulating an approximate `len(text)/4` token estimate until hitting a configurable budget, then reverse to chronological order); forward only that window to `rkllama`, unchanged shape; on response, persist the assistant's reply as the next turn.

## Results

### Basic persistence and windowing: works as designed

A real multi-turn conversation through the gateway produced correct model replies with `conversation_id` tracking and a growing window (`window_turns_sent`: 2 → 4 across two turns). Forcing eviction with a deliberately tiny test budget (15 estimated tokens) correctly collapsed the window to `[system, most-recent-turn]`, while a direct SQLite query confirmed **all 5 original turns remained fully intact** — nothing is ever lost externally; only what's fed to the model per turn is bounded.

### The KV-cache formula that motivated this whole design, confirmed exactly

Independently derived earlier in this investigation from the model's own reported architecture (`hidden_size=2048`, `num_hidden_layers=22`, `num_key_value_heads=4`, `head_dim=64`):

```
2 (K&V) × 22 layers × 4 kv_heads × 64 head_dim × 2 bytes (fp16) = 22,528 bytes/token
```

Reconverting the model with `max_context=16384` (the SDK's documented maximum) and checking the runtime's own diagnostic output confirmed this to the byte: `KV cache buffer size: 352.00 MiB`, exactly matching `22,528 × 16,384 = 369,098,752 bytes`. Pushing a single stream to ~15,900 tokens held process RSS flat at ~1.93GB for the entire run (weights + the fixed 352MB buffer + overhead) — the buffer is allocated once at init, not grown incrementally — with system-wide free RAM never dropping below 5.4GB even at the SDK's own documented ceiling. **RAM was never the constraint anywhere in this investigation; the model's fixed attention window was.** That's the premise this whole external-memory design responds to.

### Summarization: three failures with the same root cause, then a fix that doesn't ask the model to do the failing task

Extended the gateway so evicted turns fold into a rolling summary, generated by the model itself and re-injected on every subsequent turn after the system prompt.

**Basic mechanism worked on the first real test.** A 4-turn trip-planning conversation, forced through eviction with a tiny budget, correctly triggered summarization at turn 3, and turn 4's question ("What was the first city I mentioned?") was answered correctly *using only the summary* — genuine recall-via-summary, not recall-via-still-present-raw-text.

**But the *incremental update* step — folding new evictions into an existing summary — failed reliably, and not just with the small model.** Both `TinyLlama-1.1B` and (after swapping in) `Qwen3-4B` produced near-verbatim echoes of the old summary when asked to merge new content into it (`",Kyoto is a great destination..."`, stray leading comma, new content entirely ignored). Root-caused directly: a fresh call asking the model to summarize *only new content* works reliably every time; the failure is specific to prompts containing a block of text that already *looks like* a summary — the model treats generation as "complete/echo this" rather than "synthesize something new," regardless of whether the prior summary is presented as quoted text or as a genuine prior assistant turn (both structures tested, both failed identically).

**Fix #1: stop asking the model to merge.** Rewrote the summarizer to always process only the newly-evicted content in isolation, then append the result to the existing summary via plain string concatenation — an append-log, not a re-compressed paragraph. This worked immediately and held up across a full 4-turn re-test.

**Two follow-on quality issues, each found and fixed in turn:**
1. Summary chunks sometimes cut off mid-word, hitting the token budget before finishing a sentence. Raising the budget (120→250 tokens) fixed the mid-word cutoff but exposed a distinct issue underneath: the model often ends generation on its own, well short of a complete sentence, regardless of budget.
2. Added `ensure_sentence_complete()` — if a chunk doesn't end in terminal punctuation, issue a bounded number of follow-up continuation calls (2 was insufficient in testing, 4 was used) asking the model to finish it. This deliberately leans on the *same* "complete the given text" tendency that broke the incremental-merge case — there it was the bug, here it's exactly the desired behavior, since completion (not synthesis of new content) is what's wanted. Verified across 5 fresh trials: **5/5 converged to genuine terminal punctuation with real, on-topic added content**, none needing the fallback. A pragmatic force-close fallback (strip trailing punctuation, append a period) handles the cases that don't converge in time, guaranteeing every stored chunk is at least syntactically complete.

**Fix #2 was still incomplete: the append-log grows unboundedly.** The known tradeoff of the append-log design — it grows roughly linearly with eviction count instead of staying compact — needed an actual fix for long conversations. Two model-based compaction attempts were tried and both failed the same way as the incremental-merge bug:
- **Attempt 1** (regenerate a compact summary from the raw original messages, not the summary text): dropped 3 of 4 topics and **fabricated a detail never mentioned anywhere** ("traditional tea ceremonies").
- **Attempt 2** (tightened prompt: explicit per-topic sentence count, bigger budget, explicit anti-hallucination instruction): hallucination gone, but still silently covered only the first topic and dropped the rest — the `ensure_sentence_complete()` check verifies grammatical completeness, not content coverage, so a single complete sentence about only the first topic passes it fine.

This was the third time in the same investigation that "ask the model to re-synthesize a block of already-processed, summary-shaped text in one shot" failed the same way — a strong, consistent signal that no amount of prompt engineering was going to fix it reliably.

**Final redesign: deterministic, non-model compaction.** Restructured storage from one growing string into a `summary_chunks` table — one immutable row per eviction event, never concatenated or re-summarized. The displayed summary is reconstructed by windowing chunks by recency and a character budget, the exact same pattern already used for the conversation window itself and for retrieval's budget, just applied one level up. Re-running the identical 5-topic test correctly windowed to the 4 most recent topics, cleanly dropping only the oldest, with zero hallucination and zero unpredictable coverage gaps. **The right fix was architectural, not a smarter prompt.**

### Retrieval-augmented recall: the mechanism works perfectly; whether it helps depends entirely on the main model

The rolling summary is deliberately lossy — specific facts (exact numbers, names) are exactly what compression is expected to drop. Retrieval closes that gap by matching the current message against the *full* persisted history and injecting the exact original wording of anything relevant.

No embedding model was available on the board initially, so a pragmatic v1 used keyword-overlap scoring (stopword-filtered word-set intersection against every historical turn outside the recency window, top-2 matches injected verbatim).

**Test:** mentioned a fabricated hotel confirmation number (`XJ4471`) early in a conversation, forced it out of both the window and the summary via eviction, then asked for it back.

- The summary genuinely lost the detail, as expected (`"The user provided their hotel confirmation number"` — no actual code).
- Retrieval triggered correctly, and direct inspection of the actual model input confirmed `XJ4471` was **literally present, verbatim**, in the retrieved block.
- **The end-to-end answer was still wrong** — `TinyLlama-1.1B` gave a generic non-answer despite the correct fact being right there in its context.

Two isolation tests (removing the summary block; restructuring the retrieved content as natural `user`/`assistant` turns instead of a synthetic system block) each ruled out a specific structural hypothesis without fixing the failure. The decisive test: identical structure, `Qwen3-4B` instead of `TinyLlama-1.1B` — immediate correct answer.

**Conclusion, and the paper's central finding:** retrieval-augmented recall is fully validated as a *mechanism* — the correct data reliably reaches the model — but whether that translates into a correct answer depends on the main conversation model's own capability to extract precise facts from provided context, a separate axis entirely from whether the fact is present at all. This exact pattern — small model fails despite correct information being present — recurred across summarization quality and retrieval, and was confirmed to resolve completely by switching only the main conversation model, with zero changes to the retrieval/summarization infrastructure itself:

| Test | TinyLlama-1.1B | Qwen3-4B |
|---|---|---|
| "First city mentioned" recall (raw window, no eviction) | Wrong ("Tokyo," never mentioned) | Correct ("Kyoto") |
| Hotel confirmation number (retrieval-mediated) | Wrong (generic non-answer despite correct context) | Correct (`XJ4471`) |

The cost: `Qwen3-4B` is meaningfully slower per turn (~8-18s vs. TinyLlama's sub-2s) — a real, expected recall-accuracy-vs-throughput tradeoff, consistent with the size/speed relationship established across this investigation's other benchmarking work.

### Semantic retrieval: proven to actually be semantic, plus a real `rkllama` bug found while wiring it in

Keyword-overlap retrieval was replaced with real embeddings (`all-MiniLM-L6-v2`, a lazy-loaded singleton, one embedding computed per turn and stored as a BLOB column, cosine similarity at query time, falling back to keyword-overlap if the embedding model fails to load or a row predates the column). Proven to be genuinely semantic, not keyword overlap in disguise: the query *"What's the booking reference for my accommodation?"* against the stored turn *"My hotel confirmation number for the Kyoto trip is XJ4471"* — zero literal words in common — was still correctly ranked top-1 by the embedding path, which the old keyword path could never have matched at all.

**While wiring this in, RAM checking surfaced a real, independently-confirmed `rkllama` bug.** Board RAM was found pinned at ~78MB free with 3.4GB swapped — both `TinyLlama-1.1B` and `Qwen3-4B` were resident simultaneously despite calling `rkllama`'s bulk-unload endpoint. `POST /unload_models` (plural) returns `200 OK` / "Models successfully unloaded!" but genuinely unloads nothing — `/api/ps` confirmed both models still resident afterward. Root cause in `rkllama`'s own `server.py`: `variables.worker_manager_rkllm.stop_all` — a bare attribute reference, missing the `()` needed to actually call it. A silent no-op that always claims success. The singular per-model endpoint (`POST /unload_model` with a `model_name` key) works correctly and was used instead, freeing RAM from 78MB to 6.2GB per call.

A related capacity finding surfaced in the same session: `TinyLlama-1.1B` and `Qwen3-4B` **cannot both be resident on this board simultaneously** — confirmed via the kernel OOM-killer terminating `rkllama_server` twice while cold-loading `Qwen3-4B` on top of an already-loaded `TinyLlama-1.1B`. Loading `Qwen3-4B` alone, after explicitly unloading the small model first, succeeded cleanly in 17 seconds. Any multi-model orchestration on this board must unload before switching, not layer.

## Discussion

The practical shape of this system, after all the iteration above: full history is never lost (SQLite is the source of truth, unconditionally), what the model sees each turn is a bounded window plus a deterministically-compacted rolling summary plus (when relevant) verbatim-retrieved fragments of anything older. None of the compression steps are model-based anymore — every place a language model was asked to re-synthesize its own prior output, it failed in some form of "echo or truncate," and every fix that actually held up replaced that step with deterministic string/data operations instead. The one place a language model genuinely needs to do real synthesis work (summarizing *new*, never-before-summarized content) works reliably; the pattern that fails is specifically re-processing already-summary-shaped text.

The retrieval/recall findings generalize beyond this specific project: building correct memory infrastructure (persistence, windowing, retrieval) is necessary but not sufficient — a weak main-conversation model can be handed exactly the right fact, verbatim, in a clearly-labeled context block, and still fail to use it. Anyone building a similar system on constrained hardware should budget separately for "does my model actually use context correctly" as a distinct concern from "does my system deliver the right context."

## Reproducibility Notes

- Gateway: `~/npu-bench/memory_gateway.py`, SQLite at `~/npu-bench/memory_gateway.db`
- Regression suite: `~/npu-bench/test_memory_gateway.py` — real pass/fail assertions covering persistence/windowing, sentence-completeness convergence (5 trials), deterministic compaction, and end-to-end retrieval; all 14 checks pass with the embedding path wired in
- `rkllama` runs unmodified on port 8080; the gateway listens on port 8081
- Token-budget estimation is intentionally approximate (`len(text)/4`); exact tokenization via the model's own tokenizer was scoped as a natural refinement, not implemented
- `RKLLM_LOG_LEVEL=2` was the diagnostic flag used to confirm the KV-cache buffer size exactly

## Limitations / What Wasn't Tested

- Retrieval is single-hop, top-2-match — no multi-hop reasoning or re-ranking was attempted.
- The token-budget estimator's conservativeness was observed empirically (real 10-turn conversations triggered eviction much later than tiny synthetic-budget tests implied) but not formally characterized against exact tokenizer counts.
- No load/concurrency testing was done — the gateway was exercised with single-conversation, sequential-turn traffic throughout.
- The append-log-to-chunk-table redesign bounds summary growth per conversation but does not itself cap total SQLite size for an extremely long-running conversation; periodically re-summarizing the growing chunk log itself was identified as a natural follow-up, not implemented.
- Both `TinyLlama-1.1B` and `Qwen3-4B` were tested as main-conversation models; no attempt was made to characterize where between those two sizes the recall failures stop occurring.
