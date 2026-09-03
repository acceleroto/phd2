---
name: Multistar2 jump diagnostics
overview: Add temporary, compile-time-gated multistar2 diagnostics that preserve 20 frames of pre-event context, distinguish candidate/displayed/accepted/applied solutions, and capture enough per-star state to identify membership, reacquisition, mass, or peak-switching failures. Target the newer `origin/master` implementation without changing guiding behavior.
todos:
  - id: baseline-guard
    content: Confirm the working tree is on the selected origin/master baseline, requesting explicit permission before any source-control synchronization.
    status: pending
  - id: diagnostic-state
    content: Add compile-time-gated temporary snapshot, last-accepted, and rolling-buffer state to multistar2.
    status: pending
  - id: capture-trigger
    content: Instrument per-star and aggregate calculations, implement episode triggers, and emit 20 pre/10 post-frame diagnostic blocks.
    status: pending
  - id: verify-diagnostics
    content: Build with diagnostics on/off and validate trigger, correlation, cooldown, and no-behavior-change guarantees.
    status: pending
isProject: false
---

# Multistar2 Jump Diagnostics

## Context and scope
- Repository identity: this work belongs in `https://github.com/acceleroto/phd2`, the fork whose `origin/master` contains the newer multistar2 implementation. The development/build machine is Windows host `check6`.
- Observed anomaly: rarely, the green aggregate solution box appears to alternate by several pixels between two repeatable points on consecutive image frames. It is not yet known whether an equivalent guiding correction or mount movement occurs.
- Leading hypotheses are contributor eligibility toggling, reacquisition reference-pinning math, one tracked star alternating between image peaks, or a jump-rejected candidate remaining visible.
- Existing newer code already has `DistanceChecker2`, `MULTISTAR2_DEBUG_LOG`, frame summaries, contributor-change logging, and jump-rejection logging. Extend these rather than replacing or duplicating them.
- This phase is diagnostic only: do not change guiding math, rejection policy, reference pinning, display behavior, or correction scheduling. Do not commit, push, pull, merge, or otherwise mutate source control without explicit permission in the executing chat.

## Baseline and temporary boundary
- Before implementation, verify the checkout matches the selected `origin/master` baseline; if synchronization is required, pause for explicit source-control permission.
- In [`src/guider_multistar2.h`](src/guider_multistar2.h), add a dedicated `MULTISTAR2_JUMP_DIAGNOSTICS` compile-time flag, enabled by default for this investigation and nested under the existing `MULTISTAR2_DEBUG_LOG` facility.
- Mark every added block with one searchable cleanup tag such as `TODO(multistar2-jump-diagnostics): TEMPORARY`, including a short removal checklist. Compile all diagnostic types, buffers, constants, and state out when disabled.

## Capture accepted state and per-frame evidence
- In [`src/guider_multistar2.h`](src/guider_multistar2.h), add diagnostic-only state for `lastAcceptedDisp`, validity, episode/cooldown tracking, a 20-frame rolling deque, and per-frame/per-star snapshots.
- In [`src/guider_multistar2.cpp`](src/guider_multistar2.cpp), capture for every image frame:
  - frame/time, guiding/settling context, lock position, image scale;
  - previous accepted, candidate, displayed, and applied camera/mount displacements in pixels and arcseconds;
  - jump-check outcome/state and whether the normal correction was handed off;
  - contributor mask, weights, `baseDisp`, final aggregate, and aggregate delta;
  - per-star expected search point, measured point, reference, displacement, SNR, mass, mass bounds/rejection, reacquisition count, eligibility reason, and weight;
  - reference repinning with old/new references and the pre-pin displacement.
- Update `lastAcceptedDisp` only after the candidate passes all checks and `UpdateCurrentPosition` succeeds; never advance it for lost, mass-only, no-contributor, or jump-rejected frames. Reset it with the existing multistar2 invalidation path.
- Preserve existing algorithm ordering and outputs; diagnostics observe values but do not change star selection, weighting, rejection, display state, or correction scheduling. Use existing `SchedulePrimaryMove` and guide-log records to confirm eventual pulses rather than modifying generic mount behavior.

## Trigger and flush diagnostic episodes
- Keep the latest 20 snapshots continuously. Trigger one parseable diagnostic episode during steady guiding when any of these occurs:
  - candidate differs from `lastAcceptedDisp` by more than 2 arcsec at least twice within 20 seconds;
  - an A→B→A pattern has two consecutive moves over 2 arcsec and returns within 0.5 arcsec of the solution two frames earlier;
  - the jump checker rejects a candidate;
  - a reference is repinned; or
  - contributor membership changes while the aggregate changes by more than 2 arcsec.
- Use full 2-D camera displacement, log axis components too, and fall back to a 1-pixel threshold when image scale is unknown.
- On trigger, flush the 20 pre-event frames, retain/log 10 post-event frames, then apply a 60-second cooldown so one oscillation produces one bounded block. Include an episode ID and trigger reason on every line for correlation with the standard debug and guide logs.

## Verification
- Build with diagnostics enabled and disabled to prove the temporary code is self-contained and compiles away cleanly.
- Run the repository’s available tests/build checks, then exercise deterministic diagnostic sequences for: one isolated jump, two jumps in 20 seconds, A→B→A, membership change, repinning, jump rejection, unknown image scale, post-trigger capture, and cooldown coalescing.
- Confirm logs distinguish the expected signatures: display-only rejected candidate, contributor toggling, mass-boundary toggling, one star alternating positions, reacquisition repinning, and weight-only movement.
- Review the diff to ensure no guiding decisions or normal correction values changed and that every temporary addition is discoverable through the cleanup tag.