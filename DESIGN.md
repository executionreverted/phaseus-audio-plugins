# PHASEUS UI Design System

## Vision

- Minimal, modern, future-garage vibe.
- Sketchy but clean and readable.
- Strict dark, black/white dominant visual language.
- Fast readability for sound-design sessions.

## Global Rules (All Plugins)

- Keep one static header across all PHASEUS plugins.
- Header layout:
  - Left: plugin title
  - Center: preset bar (shared component)
  - Right: utility controls (`Output Gain`, quick toggles like `LoFi`)
- Thin divider line at bottom
- Never place knobs or mode selectors inside the header area.
- Full dark mode only. No dark/light switch.

## Layout Structure

- Header (fixed height): branding + preset + utility controls.
- Control bar (row under header): mode selector and high-priority controls.
- Main content: mode-specific controls.
- Left panel:
  - Upper: delay controls
  - Lower: fixed filter section (always anchored, not floating on right)
- Right panel: global controls (`Wet/Dry`, reverse, ducking, diffusion, etc.)

## Visual Direction

- Base palette:
  - Dark background layers (`~#0A0A0C` family)
  - Off-white text (`~#F3F3F3`)
  - Cyan accent for active points/handles.
- Accent style:
  - Subtle gradients + dark overlays.
  - Limited accent usage; avoid rainbow UI.
- Typography:
  - High contrast, medium-large labels.
  - Clear hierarchy: title > section labels > parameter labels.
- Buttons:
  - Square geometry (no rounded corners).
  - Consistent hover/down brightness shift.

## Interaction Rules

- Preset save naming modal opens centered and uses dark theme.
- LoFi toggle header right side for quick access.
- Mode changes show only relevant controls.
- Wet/Dry remains visible in all modes.
- Knob text boxes remain readable at a glance.
- Sync modda time knob kaybolmaz; muziksel division text'iyle calisir.
- PingPong chain icons:
  - Time chain icon only visible when time link/sync relation is active.
  - Feedback chain icon only visible when feedback link is active.
- Random knobs:
  - Random-enabled parameters use `main knob + random knob on the right` layout (vertical stacking yok).

## Current Plugin Mapping

- Simple: Time, Feedback, Wet/Dry.
- PingPong: Time L/R, Feedback L/R, sync toggles, Wet/Dry.
- Grain: Base Time, Grain Size, Rate, Amount/Pan, Feedback, Grain PingPong, Wet/Dry.
- Filter:
  - Left-bottom fixed section
  - Slot tabs: `Filter 1`, `Filter 2`
  - Shared edit panel (type tabs + response graph + compact sliders)
  - Wet-chain behavior:
    - `Filter 1` input always wet
    - `Filter 2` enabled ise `F1 -> F2 -> Out`
    - `Filter 2` disabled ise `F1 -> Out`

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
- Sag panel overflowunu sadece "daha buyuk pencere" ile cozmeye calismak.
- Filter alanini fazla mini knob ile doldurmak.

## Future Decisions

- If we add more plugins, implement a shared `PhaseusHeader` component.
- Move repeated colour tokens to one shared theme config.
- Keep this document as the source of truth for UI decisions.

## Recent UI Decisions (Lock)

- Header yuksekligi kompakt (ilk versiyona gore yaklasik yari).
- Header center'da reusable preset bar kalir.
- Header sagda `Output Gain` kalici kontrol olarak korunur.
- Sag panelde scrollbar her zaman gorunur; overflow durumunda etkilesimli.
- Filter details compact:
  - kisa tab row
  - daha kisa response graph
  - daha dar value text box
- Ping/Pong link gorseli image asset ile cizilir (zincir icon), text glyph degil.
- Background image karartmali overlay ile kullanilir; readability her zaman oncelikli.
- Delay knob sizing:
  - Autoscale kaldirildi.
  - Fixed readable standard sizes kullaniliyor (tutarlilik ve kontrol icin).
