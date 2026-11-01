# Changelog

## 1.0.0 — 2026-11-01

First public release.

- Analysis: STFT, voice activity, pitch, loudness and true peak on the captured take; 22 problem checks with probability, severity and confidence.
- Planner and style recipes for 16 styles (4 general, 12 artist styles); per-style insert order.
- Offline verification: the plan is rendered on the captured take, measured again and adjusted (up to three rounds).
- 20 modules: gate, tune, formant, corrective EQ with plosive guard, tone EQ, leveler, two compressors, de-esser with a final s-guard, Smooth (resonance suppression), Glow, character saturation, dirt, air, octave layer, doubler, delay, reverb, width and a true-peak limiter.
- Editor: role select, listening progress, spectrum and meters, seven macros, chain cards with reasons and locks, level-matched A/B and DELTA, presets, undo/redo.
- Artist-named styles are labelled as style descriptions ("… tarzı") with a note that the artists have not endorsed the plug-in.
- Build: JUCE 7.0.12 fetched by CMake; GPLv3 licence; Windows installer and zip from GitHub Actions.
- Tools: `SweaxRender` (WAV → WAV + JSON report), `SweaxShots` (renders the editor off-screen for documentation), demo generator.
