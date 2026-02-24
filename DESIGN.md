# PHASEUS UI Design System

## Vision

- Minimal, modern, future-garage vibe.
- Sketchy but clean and readable.
- Mostly black/white visual language.
- Fast readability for sound-design sessions.

## Global Rules (All Plugins)

- Keep one static header across all PHASEUS plugins.
- Header layout:
  - Left: plugin title
  - Right: dark mode toggle
  - Thin divider line at bottom
- Never place knobs or mode selectors inside the header area.

## Layout Structure

- Header (fixed height): branding + theme control.
- Control bar (row under header): mode selector and high-priority controls.
- Main content: mode-specific controls.
- Side panel (right): Wet/Dry as large hero knob.

## Visual Direction

- Base palette:
  - Dark mode: near-black backgrounds, off-white foreground.
  - Light mode: near-white backgrounds, charcoal foreground.
- Accent style:
  - Neutral grayscale accents only.
  - Subtle gradients, no colorful neon.
- Typography:
  - High contrast, medium-large labels.
  - Clear hierarchy: title > section labels > parameter labels.

## Interaction Rules

- Dark mode toggle always visible at top-right.
- Mode changes show only relevant controls.
- Wet/Dry remains visible in all modes.
- Knob text boxes remain readable at a glance.

## Current Plugin Mapping

- Simple: Time, Feedback, Wet/Dry.
- PingPong: Time L/R, Feedback L/R, sync toggles, Wet/Dry.
- Grain: Base Time, Grain Size, Rate, Amount/Pan, Feedback, Grain PingPong, Wet/Dry.

## Audio UX Safety Notes

- Grain mode should avoid click/pop artifacts:
  - Use per-grain windowing (fade-in/fade-out envelope).
  - Smooth wet output transitions.
  - Avoid abrupt grain retrigger when previous grain is active.
- Keep feedback bounded to prevent sudden output explosions.

## Common Mistakes To Avoid

- Tiny global controls (Wet/Dry must be visually dominant).
- Title/control overlap in top area.
- Too many controls shown at once across modes.
- Grain output hard on/off transitions without envelope.
- Theme toggle hidden deep in UI.

## Future Decisions

- If we add more plugins, implement a shared `PhaseusHeader` component.
- Move repeated colour tokens to one shared theme config.
- Keep this document as the source of truth for UI decisions.
