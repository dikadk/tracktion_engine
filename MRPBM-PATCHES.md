# Mrpbm patches to Tracktion Engine

This fork (`dikadk/tracktion_engine`) carries changes the Mrpbm apps depend on. Each patched spot in the source
is marked with a `[Mrpbm patch: <name>]` comment. When merging upstream, check every patch below is still in
place: `git grep -n "Mrpbm patch"` must list all of them.

## plugin signal tap (2026-09-30)

Samurai's effect faceplates draw each plugin instance's real audio and real internal state; nothing is simulated
for display.

- `plugins/tracktion_Plugin.h/.cpp`: `Plugin::SignalTap`, an abstract interface with `pluginInput()` and
  `pluginOutput()`, and `Plugin::setSignalTap()` / `getSignalTap()`. `applyToBufferWithAutomation()` hands the
  tap `pc.destBuffer` just before and just after `applyToBuffer()`, on every path. The plugin owns the tap for its
  whole life (set once on the message thread, never removed), so the audio thread never sees it dangle. With no tap
  the cost is one atomic load per block. Implementations must be real-time safe.
- `plugins/effects/tracktion_Chorus.h/.cpp`: `ChorusPlugin::getLfoPhase()`, the left channel's LFO phase at the end
  of the last block, stored once per block (relaxed atomic).
- `plugins/effects/tracktion_Phaser.h/.cpp`: `PhaserPlugin::getSweepHz()` (the all-pass break frequency) and
  `getLfoPhase()` (0 at the bottom of the sweep, 0.5 at the top), stored once per block (relaxed atomics).

Used by `Source/features_plugins/features_plugins/SignalTap.h` and `Source/features_fx_ui/features_fx_ui/rack/` in
the Mrpbm repo.
