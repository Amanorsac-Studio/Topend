# Top End

> **Beta.** Top End 1.0.0 is in testing. Testers: see [`beta/README.md`](beta/README.md)
> to install it, report problems and send back presets. Changes are in
> [`CHANGELOG.md`](CHANGELOG.md).

A guitar amp, pedalboard, looper and metronome for the stage, from **Amanorsac
Studio** — plus a Learn tab that listens to a song and shows its chords (coloured by
their job in the key), key, beat and notes on a guitar fretboard, with speed, pitch,
A–B loop, count-in and click, and can split it into stems to mute the guitar and
play along.

Forked from Low End (the bass app) and shares its engine: the looper fixes and the
metronome came from there. Guitar-specific: six amp models with Clean/Crunch/Lead
channels, 1×12 / 2×12 / 4×12 cabs, guitar pedals (Clear Drive, Wah, …), a 6-string
tuner and pitch range (drop D to the 24th fret). "Top End" is a working title.

The UI (`resources/ui/index.html`) is built to fit one screen on every tab, iPad
included (1024×768 and up, either way round); what does not fit opens with a tap.

Known limits: the 4-stem separation puts guitar and keys together ("Guitar & keys");
amp captures (NAM) are not supported, cabinet IRs are.

Ships as a standalone application and a VST3 / Audio Unit plug-in, for Windows and
macOS.

## Building

Requires CMake 3.24+ and a C++20 compiler. JUCE 8 is fetched automatically if not
found beside this checkout.

```bash
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release
```

On Windows, the WebView2 SDK is needed at configure time:

```bash
nuget install Microsoft.Web.WebView2 -Version 1.0.2903.40 -OutputDirectory <dir>
```

### Windows installer

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-windows-installer.ps1
```

Add `-CertThumbprint <sha1>` to sign it, or `-Tester` to build a key-gated test
build instead of a release (see `src/plugin/TesterGate.h`).

### macOS package

```bash
scripts/build-mac-pkg.sh
```

Signs, notarises and staples when `APPLE_TEAM_ID`, `ASC_KEY_PATH`, `ASC_KEY_ID` and
`ASC_ISSUER_ID` are set (see `docs/DELIVERY-AUDIT-1.0.0.md` for what each requires);
otherwise it produces an unsigned `.pkg` for local testing. Set `TOPEND_TESTER=1`
for a test build. Needs the separation model in `models/` first — see
`.github/workflows/release.yml` for how CI fetches it, or place it there by hand
(`htdemucs_fwd.onnx` + `htdemucs_fwd.onnx.data` from
[Intel/demucs-openvino](https://huggingface.co/Intel/demucs-openvino), MIT licence).

The macOS release workflow (`.github/workflows/release.yml`) runs on a `v*` tag or
by hand from the Actions tab, and needs the organisation's signing secrets to
produce a signed package — see `docs/DELIVERY-AUDIT-1.0.0.md`.

## Status

Not yet released. See `docs/DELIVERY-AUDIT-1.0.0.md` for the current line-by-line
state against the Amanorsac Studio build and packaging standards.

## Licence

© Amanorsac Studio. Third-party components are listed in `THIRD_PARTY_NOTICES.txt`.
