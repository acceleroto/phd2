# PHD2 Loop Frame Server Output PR Notes

## Goal

Propose an additive `PHD2` server-interface enhancement so clients can receive
live RA/Dec-like drift data while exposures are looping, a star is selected,
and guiding is off.

## Problem Statement

Today, `PHD2` server clients can get mount-axis drift values from `GuideStep`,
but that event is guiding-only.

For tools like `PECWizard`, the required operating mode is:

- looping exposures
- star selected
- guiding off

In that mode, the current server event stream exposes state changes such as
`LoopingExposures`, but not the selected-star drift values needed for live PE
measurement.

## Requested Change

Add a new event, rather than changing `GuideStep`.

Suggested event name:

- `LoopFrameOffset`

Suggested emission conditions:

- loop frame acquired
- valid selected star
- valid lock position
- guiding off

Suggested payload:

- `Frame`
- `Timestamp`
- `dx`
- `dy`
- `RADistanceRaw`
- `DECDistanceRaw`
- `StarPosition`
- `LockPosition`
- `Calibrated`

## Why A New Event

Changing `GuideStep` would risk breaking existing clients that rely on its
current meaning and timing.

A new event is:

- additive
- easy for current clients to ignore
- clear in semantics
- lower-risk for backward compatibility

## Important Technical Note

This should not be implemented from `get_star_image.star_pos`.

Reason:

- `get_star_image.star_pos` is crop-local, not full-frame
- it is not sufficient to reconstruct full-frame star drift relative to lock
  position

The correct data should come from `PHD2`'s internal full-frame current star
position and lock position, transformed using the same internal
camera-to-mount-coordinate logic already used for guiding.

## Internal Source Areas

Likely relevant files:

- `src/event_server.cpp`
- `src/mount.cpp`
- `src/guider.cpp`

Key existing pieces:

- `GuideStep` event already emits camera and mount offsets
- `TransformCameraCoordinatesToMountCoordinates` already exists
- calibration geometry is already exposed through `get_calibration_data`

## Compatibility Intent

This PR should:

- preserve existing `GuideStep` behavior exactly
- preserve existing RPC behavior
- only add new event-server functionality

## Optional Follow-Up

Also consider a small RPC:

- `get_current_star_position`

returning full-frame current selected-star centroid coordinates.

That would be useful independently of the new event and would fill a gap in the
current server API.

## Suggested Example Payload

```json
{
  "Event": "LoopFrameOffset",
  "Timestamp": 1774205123.412,
  "Host": "OBS-PC",
  "Inst": 1,
  "Frame": 284,
  "dx": 1.237,
  "dy": -0.418,
  "RADistanceRaw": 0.932,
  "DECDistanceRaw": -0.901,
  "StarPosition": [642.381, 511.927],
  "LockPosition": [641.144, 512.345],
  "Calibrated": true
}
```

## Validation Suggestions

1. Verify new event appears during looping with selected star and guiding off.
2. Verify `GuideStep` behavior is unchanged during guiding.
3. Verify event values move consistently with known star drift.
4. Verify existing server clients are unaffected if they ignore unknown events.

## One-Sentence PR Summary

Add a backward-compatible event-server output for looping selected-star drift so
non-guiding clients can consume live mount-axis offsets without changing
`GuideStep` semantics.
