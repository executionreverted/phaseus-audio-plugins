# PHASEUS Audio Plugins

Custom VST development repository for my own artist tools and sound design workflow.

Artist: **PHΛSEUS Ω**  
SoundCloud: [https://soundcloud.com/iamphaseus](https://soundcloud.com/iamphaseus)

![PHASEUS Dayi Delay](assets/dayidelay-screenshot.jpg)

## About

This is a JUCE-based plugin monorepo:

- one shared JUCE source
- plugin-per-folder architecture
- reusable UI/components across plugins
- template + script flow for fast new plugin creation

## Current Plugins

## 1) PHASEUS Dayi Delay

Multi-mode delay (`Simple`, `PingPong`, `Grain`) with:

- tempo sync + random controls
- reverse / ducking / lofi / diffusion
- dual filter workflow
- preset system

Manual: [manuals/DAYI_DELAY_MANUAL.md](manuals/DAYI_DELAY_MANUAL.md)  
Screenshot: [assets/dayidelay-screenshot.jpg](assets/dayidelay-screenshot.jpg)

## Project Layout

```text
phaseus-audio-plugins/
  JUCE/
  Plugins/
    PHASEUS_Delay/
  Shared/
  Templates/
  manuals/
  assets/
  createplugin
  CMakeLists.txt
```

## Build

```bash
cmake -S . -B build
cmake --build build --config Release --target PHASEUS_Delay_VST3
```

macOS VST3 install path:

```text
~/Library/Audio/Plug-Ins/VST3/PHASEUS_Delay.vst3
```

## macOS Install (From GitHub Releases)

1. Download the latest `PHASEUS_Delay-*-macOS.zip` from the GitHub `Releases` page.
2. Extract the zip and copy `PHASEUS_Delay.vst3` into:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R /path/to/PHASEUS_Delay.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

3. Remove macOS quarantine attributes:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/PHASEUS_Delay.vst3
```

4. Fully quit and reopen your DAW.
5. Run plugin scan/rescan and load `PHASEUS_Delay`.

Notes:
- If the build is not notarized/signed, macOS Gatekeeper may show a warning.
- If needed, go to `System Settings > Privacy & Security` and click `Open Anyway`.

## Create New Plugins

```bash
./createplugin MyNewPlugin
```

Plugin folders are auto-discovered by `Plugins/CMakeLists.txt`.
