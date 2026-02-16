# Multistar2 pre-PR notes (strict review, refreshed)

This is an updated strict review based on the **current state** of the branch, including recent comment/doc cleanup and GuideLog+DebugLog testing analysis.

## Current verdict
`multistar2` is now much closer to reviewable than before.  
As an **opt-in experimental PR**, it looks plausible for upstream consideration, but it still has a few likely review blockers and several expected “please tighten this” comments.

## Status of earlier blockers

- **Resolved**
  - Full BSD-style license headers are now present in `src/guider_multistar2.h` and `src/guider_multistar2.cpp`.
  - Phase-specific comments in code were removed/reworded to behavior-based comments.
  - Most terminology cleanup is done (`multistar` vs `classic` wording improved in docs/comments).
  - Runtime implementation selection and safe guider recreation plumbing remain solid.
  - Key user-visible strings discussed earlier are now wrapped for translation in code (e.g., `_("Multistars: %u/%u")`, `_("MultiStar2: ")`).
  - Testing evidence has improved significantly (including DebugLog↔GuideLog correlation for 2026-02-07).

- **Still open (likely PR feedback)**
  - **Header self-sufficiency**: `src/guider_multistar2.h` uses `std::deque`, `std::vector`, and `std::pair` but does not include standard headers directly (relies on transitive includes).
  - **Debug macro default**: `MULTISTAR2_DEBUG_LOG` defaults to `1` in the header; this is likely too noisy/surprising for upstream defaults.
  - **Behavioral guardrail parity**: `multistar2` still does not mirror some existing multistar protections (notably distance/jump recovery behavior like `DistanceChecker` in `GuiderMultiStar`).
  - **Instrumentation parity**: logging format/depth is still different from multistar’s established support/debug patterns.
  - **Top-right overlay tag**: `"multistar2"` draw tag remains in `OnPaint()` and is plain text (not translated). If this tag is intended to stay, it should be justified and internationalized; if not, remove it.
  - **gettext artifacts**: code is translation-ready, but `locale/messages.pot` / `locale/*/messages.po` updates still depend on running extraction/merge targets when wording is finalized.

## Implementation-style assessment (current)

- **Architecture**
  - `GuiderMultiStar2` is intentionally not a small refactor; it introduces an aggregate solution model (with separate display star) and membership-aware behavior. This is acceptable for an experimental path, but reviewers will treat it as a substantial algorithm fork.

- **Risk areas maintainers will focus on**
  - Correctness under mount/pathological conditions (e.g., meridian safety-stop segments) and how gracefully the guider handles them.
  - Behavior compatibility with existing configuration knobs (`TolerateJumps`, recovery behavior expectations, image logging semantics).
  - Long-term maintainability of duplicated logic (`StarStatus2`, per-star mass-window logic vs existing `MassChecker` model).

## Testing confidence (current)

- **Positive**
  - Feb 7 session correlation supports primary-loss failover goal:
    - `primaryContrib==0` with `used>0` across many frames
    - similar GuideLog stability metrics during primary-contributing and primary-not-contributing intervals
  - GuideLog-based continuity-at-membership-change metric has now been computed (filtered) and is reasonably close to non-change baseline.

- **Caveats**
  - Strong results are from a limited number of sessions and seeing conditions.
  - One session includes mount safety-stop behavior; correctly treated as external, but still a stress case worth explicit guardrails.
  - Need replication across additional nights / forced-loss scenarios.

## Would upstream accept this as a PR now?

Probably **yes as an experimental/opt-in draft PR**, with expected review iterations.  
Probably **not as merge-ready on first pass** until the open items above are addressed, especially:

- debug logging default behavior
- header include hygiene
- clarity on mount/pathological recovery guardrails
- final i18n artifact update (`messages.pot` / `messages.po`) once wording is frozen

