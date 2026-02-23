Yes — for this repo, Windows installer creation is an **Inno Setup** flow, not CMake/VS “Install”.

## Practical Flow (x64)

1. **Build Release binaries**
   - Configure/open: `run_cmake_x64.bat` (creates `tmp64\phd2.sln`)
   - In Visual Studio, build **Release** (usually `phd2` or `ALL_BUILD`).

2. **Create a staging folder named `Release`**
   - Put `phd2.exe` and required DLLs there.
   - The x64 installer script expects paths like `Release\phd2.exe`, `Release\*.dll`, `Release\locale\*`, `Release\PHD2GuideHelp.zip`.
   - If your build output is elsewhere, copy/sync files into this `Release` folder.

3. **Prepare installer script from template**
   - Use `phd2-x64.iss.in` as the base.
   - Replace `@VERSION@` with your version (e.g. `2.6.13-ms2b3`).
   - Save as `phd2-x64.iss`.

4. **Prepare README used by installer**
   - Script expects `README-PHD2.txt` in the same folder as the `.iss`.
   - You only have `README-PHD2.txt.in`, so generate `README-PHD2.txt` by replacing `@VERSION@` there too.

5. **Compile installer with Inno Setup**
   - GUI: open `phd2-x64.iss` in Inno Setup Compiler and click **Compile**.
   - CLI:
     - `iscc "H:\phd2\phd2-x64.iss"`

6. **Get output installer**
   - Output is controlled by script:
   - `OutputBaseFilename=phd2-x64-v{#APP_VERSION}-installer`
   - So you’ll get `phd2-x64-v<version>-installer.exe` in `OutputDir` (currently `.`).

---

## Important gotchas

- The `.iss` currently hardcodes many files in `[Files]`; compile fails if any are missing.
- `README-PHD2.txt` must exist (the script references it directly).
- `..\build\dark_mover.vbs` is also referenced; keep that relative path valid.
- If you only want a private beta package, a zip of the built `Release` folder can be easier than maintaining the full installer file list.

If you want, I can draft a tiny PowerShell script that:
- copies build outputs into `Release`,
- replaces `@VERSION@` in both templates,
- and runs `iscc` in one command.
