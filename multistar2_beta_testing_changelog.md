# Multistar2 Beta Testing Changelog

## Beta 3 - TBD

- Refactored dropped/recovering frame handling in `multistar2` to remove duplicated logic.
  - Added a shared helper (`SetDroppedFrameInfo`) to centralize `errorInfo` population, recovery status messaging, image logging, and optional auto-exposure reset.
  - Updated the `all_lost`, `no_contributors`, and `jump_reject` paths to use this helper with no intended behavior change.
- Updated Windows runtime packaging for newer Visual C++ builds.
  - Added `vcruntime140_1.dll` to the Windows copy list in `thirdparty/thirdparty.cmake`.
  - Added `vcruntime140_1.dll` to both installer manifests (`phd2-x64.iss.in` and `phd2-x86.iss.in`) for runtime parity.
- Beta distribution process update: beta builds are now delivered using the Windows installer package.

## Beta 2 - 2026-02-16

- Added/updated BSD license headers in `multistar2` source files.
- Improved instrumentation to more closely match `multistar` support/debug visibility.
  - Added structured `MultiStar2` debug lines for frame outcome, reject breakdown, and recovery state transitions.
  - Added distance-based success-frame image logging parity and explicit recovery-path logging/reset behavior.
- Added behavioral guardrails to more closely match `multistar` safety/recovery behavior.
- Updated headers for improved self-sufficiency (`multistar2` header now includes required STL headers directly).

## Beta 1 - 2026-02-06

- Initial beta release for external testing.
