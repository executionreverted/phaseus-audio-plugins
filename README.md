# VST_Library

Single-repo JUCE setup with shared framework and per-plugin folders.

## Structure

```text
VST_Library/
  JUCE/                      # shared JUCE source (clone once)
  Plugins/
    ReverbPlugin/
      Source/
      CMakeLists.txt
  Templates/
    DefaultPluginTemplate/
  createplugin
  CMakeLists.txt
```

## Setup

1. Clone JUCE into the root:

```bash
git clone https://github.com/juce-framework/JUCE.git JUCE
```

2. Configure and build:

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Add a new plugin

```bash
./createplugin Reverb2
```

This creates:

```text
Plugins/Reverb2/
  Source/
  CMakeLists.txt
```

`Plugins/CMakeLists.txt` auto-discovers plugin folders, so no manual CMake edit is needed.
