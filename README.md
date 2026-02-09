## PHD2 Guiding

PHD2 is the enhanced, second generation version of the popular PHD guiding software from Stark Labs. PHD2 is free and open source, and development is community-driven.

## Getting started

PHD2’s built-in help file is an excellent way to get started using the program, regardless of whether you’ve had previous experience with the original PHD.

## Links

- **Project website / news**: [openphdguiding.org](https://openphdguiding.org/)
- **User support + development forum**: [Open PHD Guiding forum (Google Groups)](https://groups.google.com/forum/#!forum/open-phd-guiding)
- **Source code repository (upstream)**: [github.com/OpenPHDGuiding](https://github.com/OpenPHDGuiding)

## Features of this fork

- **Multi-guider plumbing**: This fork adds infrastructure to support selecting between multiple guider implementations at runtime (via Advanced Settings) without destabilizing the rest of the app. The intent is to make it easier to develop, test, and compare alternative guider strategies in parallel with the existing guider(s).
- **Experimental `multistar2` guider**: This fork adds `GuiderMultiStar2`, an opt-in experimental multi-star guiding implementation designed to avoid multistar’s “primary star” single-point-of-failure and to preserve guiding solution continuity as stars are lost/reacquired. It also adds UI overlays and status text so users can see contributing vs lost stars and confirm they’re running the experimental implementation.
  - **To enable the new multistar2**, open the Guide/Advanced Settings menu, then select the Guiding tab, then select the "Use multiple stars" radio button and finally "Multistar2" from the dropdown menu. 
  - **Multistar overview**: [`multistar_description.md`](multistar_description.md)
  - **Multistar2 overview**: [`multistar2_description.md`](multistar2_description.md)
  - **Development phases (A/B/C)**: [`multistar2_development_phases.md`](multistar2_development_phases.md)

## Fork status

- Windows 32-bit version is built.
- One night of initial testing is done. One crash and several small bugs were fixed.
- Lost/gained-star solution appears to be working well with no jumps or drifts in the target aiming.
- Added some debug logging for easier testing analysis.
- First night was very poor seeing. During decent seeing periods, autoguiding RMS looked good.
- Ready for additional testing. Next: an average or better seeing night.

## Installing and running this fork

- **Required dependency**: Install the latest Microsoft Visual C++ Redistributable for Visual Studio (x64). ([Microsoft Redistributable Downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#latest-supported-redistributable-version))
- **Run**: Unzip the build folder and double-click `phd2.exe`.

## License

See [`LICENSE.txt`](LICENSE.txt).

