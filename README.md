# Logo MIDI

Windows x64 VST3 MIDI effect and Logo graphics editor. The plugin manufacturer
is **Dexter U.S.**

See [native/README.md](native/README.md) for the project details and build
instructions. The preserved FMSLogo source and its original notices are in
[`vendor/fmslogo-upstream`](vendor/fmslogo-upstream).

## Download the Windows package

The **Windows x64 package** GitHub Actions workflow builds the plugin with Visual
Studio and the Steinberg VST3 SDK, runs Steinberg's validator and the native
tests, and checks the bundle vendor metadata. A successful run uploads one
`logo-midi-windows-x64` artifact containing the CLI, VST3 bundle, and `examples`
folder. Download and extract it from the workflow run's **Artifacts** section.

The workflow can be started manually with **Actions → Windows x64 VST3 → Run
workflow**, or it runs when relevant project files change. The editor still
needs to be opened and exercised in a Windows DAW to verify host-specific GUI
behavior.

The command-line app can render Logo turtle scripts to SVG without a DAW:

```text
logo-midi.exe --graphics examples\turtle-square.lgo --output square.svg
```

It can also render the included music example to a MIDI file. The `examples`
folder is included for portable script editing and file dialogs.

## Licensing

The FMSLogo source remains under its original terms; retain
[`vendor/fmslogo-upstream/LICENSE.TXT`](vendor/fmslogo-upstream/LICENSE.TXT)
and all accompanying notices when distributing it.
