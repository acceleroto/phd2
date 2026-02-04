# Multistar2 pre-PR notes (strict review)

### Verdict (strict)
**As-is, `multistar2` would probably *not* be accepted as a PR by a mature open-source team**—not because the idea is bad, but because it currently reads like a fork/experimental branch: inconsistent file headers/licensing, inconsistent “project style” conventions, and some integration/behavior mismatches with existing guider expectations.

That said: **as an opt-in experimental feature, it’s plausibly PR-able** *after* a cleanup pass that makes it look like it belongs in PHD2 (style, logging, compatibility knobs, and documentation accuracy).

---

### Coding style: `guider_multistar` vs `guider_multistar2`

- **File header / licensing (major red flag)**
  - `guider_multistar.{h,cpp}` has the full project BSD header block.
  - `guider_multistar2.{h,cpp}` currently does not; it’s a short comment header.
  - Open-source maintainers are often strict here.

- **Header self-sufficiency (likely review comment)**
  - `guider_multistar2.h` uses `std::deque`, `std::vector`, `std::pair` but doesn’t include the needed standard headers (it relies on transitive includes).
  - In a large team, this is usually treated as a correctness/maintainability issue.

- **Debug macro defaults (very likely rejection in review)**
  - The comment says “Enable by adding `-D...=1`”, but the code defaults it to **1**.
  - That means **debug logging is on by default** unless someone explicitly disables it—surprising for “extra debug logging”, and the kind of thing maintainers push back on hard.

- **Comment accuracy (trust issue)**
  - Both `guider_multistar2.h/.cpp` claim “Phase A: scaffold only. Behavior matches classic”, but the implementation is clearly beyond a scaffold (full alternate solution-point logic, reacquire gating, per-star mass checks, new overlays, changed semantics). This mismatch will trigger “what else is misdescribed?” scrutiny.

- **Project idioms**
  - `guider_multistar.cpp` uses the project’s common pattern of try/catch with `ERROR_INFO/THROW_INFO`, consistent `Debug.Write(...)` usage, and existing helper/stat structs.
  - `guider_multistar2.cpp` is more “modern C++” (lambdas, local structs, vectors), which isn’t inherently bad—but it stands out vs surrounding code and will prompt maintainers to ask for consistency or justification.

---

### Implementation style (behavioral + architectural differences)

- **Scope and coupling**
  - `GuiderMultiStar` is primary-star-centric and uses a *secondary refinement* step (`RefineOffset`) under specific conditions.
  - `GuiderMultiStar2` replaces the core model with an **aggregate “solution star”** and a separate **display star**. That’s a big conceptual change, not a refactor.

- **Compatibility with existing guider behaviors (big risk area)**
  - Multistar integrates:
    - mass-change handling via `MassChecker`
    - distance/jump tolerance via `DistanceChecker`
    - consistent image logging, error reporting, and auto-exposure reset behavior
  - `multistar2` implements new per-star mass rejection and reacquire gating (good idea), but **does not mirror several existing guardrails** (e.g., the distance/jump recovery logic). Maintainers will likely ask: “Are we dropping existing knobs/behavior for users without realizing it?”

- **Instrumentation/logging parity**
  - `GuiderMultiStar` logs consistently (including successful frames, failure reasons).
  - `GuiderMultiStar2` logs deselect/lost cases, but doesn’t appear to provide the same depth/format consistency on success frames. In PHD2, logging consistency is a big deal for support.

- **UI/paint handling**
  - The implementation correctly accounts for wxWidgets event-table behavior and works around it by binding paint in the constructor. This is pragmatic, but it *will* raise eyebrows because it’s a wx-specific “gotcha”. Expect a maintainer to ask for a comment (there is one) and possibly a more centralized solution.

- **Settings plumbing quality (this part is actually pretty good)**
  - Opt-in selection is persisted in profile (`/guider/multistar/implementation`) and switching triggers a safe recreate after the dialog closes. This is the kind of “team-friendly” glue code open-source maintainers like.

---

### Would an open-source team be ok with it as a PR?
**Not in its current form.** The two biggest “team” blockers are:

- **Process/maintenance blockers**
  - missing/incorrect license headers on new source files
  - non-self-contained headers
  - debug logging enabled by default
  - inaccurate “Phase A / matches classic” claims

- **Technical review blockers**
  - behavioral divergence vs multistar without clearly preserving existing guider safeguards (distance/jump tolerance, logging parity, etc.)
  - substantial code duplication (e.g., `StarStatus2`, mass-check logic) without a clear reuse strategy
  - user-visible behavior changes (star count semantics, overlays, untranslated strings in overlays)

If those are addressed—and the PR is presented explicitly as **experimental, opt-in, low-risk, with clear compatibility constraints and a test/log validation plan**—then **yes, a team could accept it**, but they’d almost certainly request iterations before merge.

