# Logo MIDI

Windows x64 VST3 MIDI effect and Logo graphics editor. The plugin manufacturer
is **Dexter U.S.**

See [native/README.md](native/README.md) for the project details and build
instructions. The preserved FMSLogo source and its original notices are in
[`vendor/fmslogo-upstream`](vendor/fmslogo-upstream).

## Download the Windows VST3

The **Windows x64 VST3** GitHub Actions workflow builds the plugin with Visual
Studio and the Steinberg VST3 SDK, runs Steinberg's validator, and checks the
bundle vendor metadata. A successful run uploads a
`logo-midi-vst3-windows-x64` artifact. Download it from the workflow run's
**Artifacts** section and extract the included `.vst3` bundle.

The workflow can be started manually with **Actions → Windows x64 VST3 → Run
workflow**, or it runs when relevant project files change. The editor still
needs to be opened and exercised in a Windows DAW to verify host-specific GUI
behavior.

## Download the Windows command-line app

The same successful Windows x64 workflow also uploads a
`logo-midi-cli-windows-x64` artifact. Extract it and run the included example:

```text
logo-midi.exe --graphics examples\turtle-square.lgo --output square.svg
```

The app can also render the included music example to a MIDI file. Both
downloads are available in the successful workflow run's **Artifacts** section.

## Licensing

The FMSLogo source remains under its original terms; retain
[`vendor/fmslogo-upstream/LICENSE.TXT`](vendor/fmslogo-upstream/LICENSE.TXT)
and all accompanying notices when distributing it.
