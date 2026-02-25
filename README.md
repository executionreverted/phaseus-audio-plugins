# PHASEUS Audio Plugins

Custom VST development repository for my own artist tools and sound design workflow.

Artist: **PHΛSEUS Ω**  
SoundCloud: [https://soundcloud.com/iamphaseus](https://soundcloud.com/iamphaseus)

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

## Create New Plugins

```bash
./createplugin MyNewPlugin
```

Plugin folders are auto-discovered by `Plugins/CMakeLists.txt`.
