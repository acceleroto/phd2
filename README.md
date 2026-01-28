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
- **Experimental `multistar2` guider**: This fork adds `GuiderMultiStar2`, an opt-in experimental multi-star guiding implementation designed to avoid classic multi-star’s “primary star” single-point-of-failure and to preserve guiding solution continuity as stars are lost/reacquired. It also adds UI overlays and status text so users can see contributing vs lost stars and confirm they’re running the experimental implementation.
  - **Classic multi-star overview**: [`multistar_description.md`](multistar_description.md)
  - **Multistar2 overview**: [`multistar2_description.md`](multistar2_description.md)
  - **Development phases (A/B/C)**: [`multistar2_development_phases.md`](multistar2_development_phases.md)

## License

See [`LICENSE.txt`](LICENSE.txt).

