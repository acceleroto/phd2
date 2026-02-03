## Classic multistar vs multistar2: core behavior differences

### Short answer
**No** — multistar2 inherits a lot of the *UI/features* from classic multistar, but it does **not** implement all of the same *behavioral safeguards* (especially around secondary-star “sanity checking” like the classic zero-count / miss-count / sigma-based gating).

### What’s the same (user-facing “features”)
Because `GuiderMultiStar2` **inherits from** `GuiderMultiStar` and does **not** replace the click handler, you still get the classic multistar interaction model:

- **Click to select a guide star**: **Yes**. The click handling is in `GuiderMultiStar::OnLClick` (wired via the base class event table), and multistar2 doesn’t override that handler.
- **Shift-click deselect**, **Ctrl-click bookmarks**, etc.: **Yes**, those behaviors are also in the inherited `OnLClick` logic.
- **Auto-select star behavior**: **Yes**. The auto-select code lives in `GuiderMultiStar` and multistar2 doesn’t override it.
- **Multi-star mode enable/disable** (the checkbox in Advanced Settings): **Yes**, that’s inherited state (`m_multiStarMode`) and config.

### What’s different (important)
multistar2 overrides the core tracking math via `GuiderMultiStar2::UpdateCurrentPosition`, so the “feature parity” breaks down mostly in **how secondary stars are vetted and how their measurements are accepted/rejected**.

#### 1) Hot-pixel / “fake star” rejection
- **Classic multistar** has explicit secondary-star rejection logic for suspicious cases, e.g.:
  - **“Exactly zero movement”** handling and a **`zeroCount`** that eventually **drops the star** (`DZ` path).
  - **Large excursions** vs the primary’s sigma (`missCount`, eventually reset/reference-point logic).
- **multistar2** does **not** implement that same zero-count / miss-count / sigma excursion framework. It relies more on:
  - `Star::Find` constraints (min/max HFD, saturation ADU, etc.)
  - a **per-star “reacquire good frames” gate**
  - a **per-star mass-change rejection** window

So: **hot pixel rejection is not identical**. Some hot pixels may still get filtered by HFD/SNR logic, but multistar2 does not have the classic “DZ/zeroCount” mechanism as written.

#### 2) “Only apply multistar if it helps” rule
- **Classic multistar** computes a refined (multi-star) offset, but then applies it only if it’s **smaller than the primary-only delta** (the `hypot(sumX,sumY) < primaryDistance` check).
- **multistar2** does not do this “only if smaller” gating; it computes the aggregate displacement from eligible stars and uses it.

#### 3) Stabilization / recovery behavior around lock-position changes and noisy frames
- **Classic multistar** has a stabilization concept (enter/exit stabilization, update reference points after lock position moves, etc.) and uses statistics like `primarySigma`.
- **multistar2** has a different continuity model (pinning `referencePoint` when a star becomes eligible/reacquires), and it does **not** use the classic stabilization/sigma/miss-count framework in the same way.

### Practical takeaway
- If by “features” you mean **UI affordances** (click-to-select, auto-select, etc.): **multistar2 has them**, because it inherits them.
- If you mean **all the classic multistar robustness heuristics** (hot-pixel rejection mechanics, miss-count, sigma-based stabilization, “only apply if better”): **multistar2 differs**, and in some cases is missing those exact mechanisms (intentionally or not).

If you want, list the specific classic behaviors you care most about (“DZ hot pixel removal”, “miss-count reset”, “stabilization period”, etc.) and I’ll map each one to: **present as-is / replaced by different mechanism / currently absent** in multistar2.

