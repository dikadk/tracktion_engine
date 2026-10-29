# Signalsmith backend sources

This dependency subset is vendored with one allocation-bound fix below. No download or
external dependency is needed at configure, build or runtime.

- Signalsmith Stretch 1.4.0: `Signalsmith-Audio/signalsmith-stretch`, commit
  `a670068d9aeb64913331d5cc29337b19a457a7df`.
  `signalsmith-stretch.h` and `LICENSE.txt` are copied from the repository root.
  Source: https://github.com/Signalsmith-Audio/signalsmith-stretch/tree/a670068d9aeb64913331d5cc29337b19a457a7df
  The release tag is 1.4.0; its header's internal version tuple is still 1.3.2.
- Signalsmith Linear 0.6.4: `Signalsmith-Audio/linear`, commit
  `de55e6a50ffcf6f8f43f649692d94691c7025151` (the version specified by
  Stretch 1.4.0's CMakeLists.txt). Root `fft.h`, `stft.h` and `LICENSE.txt`
  are copied into `signalsmith-linear/`. Only the built-in C++ FFT is enabled;
  no optional Accelerate/IPP/PFFFT/CMSIS/xsimd dependency is used.
  Source: https://github.com/Signalsmith-Audio/linear/tree/de55e6a50ffcf6f8f43f649692d94691c7025151

Both libraries are MIT licensed. Their full copyright and permission notices
are retained in `LICENSE.txt` and `signalsmith-linear/LICENSE.txt` and must
accompany redistributed copies or substantial portions.

Local differences from the pinned source: Stretch `configure` reserves
`(bands + 1)/2` peaks instead of `bands/2`. An odd number of spectral bands can
contain that many separated peak regions, so this prevents capacity growth in
`findPeaks` during processing. The Stretch licence has a final newline added;
all other dependency files match the pinned source byte-for-byte.

The adapter/API wiring was informed by Tracktion Engine develop at commit
`a5e44996f5f08dfa6af5f5d02d900fb3cfa37422`:
https://github.com/Tracktion/tracktion_engine/blob/a5e44996f5f08dfa6af5f5d02d900fb3cfa37422/modules/tracktion_engine/timestretch/tracktion_TimeStretch.cpp
Tracktion remains at the existing engine revision
`76933e6b2f5e5386c9abd42047c0fafaacc98876`; its GPL/commercial licensing is
unchanged. This is a narrow adapter backport, not a Tracktion upgrade.

## Compatibility contract

`TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH` defaults to 0. IDs 0–14 and all
flag-off default precedence remain unchanged. Explicit opt-in makes ID 15
(`signalsmithDefault`) the default; ID 16 is `signalsmithCheaper`. The existing
`checkModeIsAvailable` fallback resolves disabled legacy algorithms to the
compiled default; stored legacy IDs are not rewritten or reinterpreted.

SoundTouch's BPM detector was historically compiled even when its stretcher
was disabled. `TRACKTION_ENABLE_SOUNDTOUCH_BPM` now defaults to 1 to preserve
existing apps. RadioMama explicitly sets both this flag and the SoundTouch
stretch flag to 0. In that configuration `TempoDetect` retains its public API
but performs no audio analysis and returns 0 (unknown) from detection calls;
metadata/duration helpers are unaffected. The SoundTouch detector, peak finder
and FIFO source are then excluded from the translation unit. This optional
detector gate also changes `timestretch/tracktion_TempoDetect.h`; it is local
adapter integration, not a change to the pinned Signalsmith dependencies.

Initialise off the audio thread. Supported configurations are 1–64 channels,
1000–768000 Hz and block sizes 1–1048576. Duration ratios 0.125–8 and pitch
shifts -48–48 semitones are accepted; invalid/nonfinite parameters return false
without changing the active settings. Ratios greater than 1 lengthen the sound.
Calls on one instance must be serialised, as with existing backends.

The old API has no latency query. The adapter collects `inputLatency()` samples
for `seek`, drops `outputLatency()` output samples, and includes both in the
bounded initial input request. Partial/short inputs work; flush pads analysis
with silence, returns the complete source duration in block-sized chunks, and
terminates at `round(sum(inputFrames * activeRatio))`. The synthesis flush is
called once. Reset preserves speed/pitch and clears priming, fractional frame
accounting and drain state. Runtime ratio changes take effect on the next input
block; Signalsmith interpolates its analysis timeline internally. A final zero
input process advances the analysis to EOF before the one terminal flush.

Both values of the old API's `realtime` hint use `splitComputation=false`.
Split mode defers spectrum/synthesis steps across output samples and keeps a
separate stashed output cursor; the pinned upstream terminal `flush` finishes
and reads the live STFT output directly. This adapter's seek/latency-drop/drain
sequence does not preserve that split schedule. Native 16 kHz tests observed
nearly silent 17/127-frame impulses and EOF peaks early by 409/249 frames with
split mode; the same timing/presence tests passed without splitting. Disable
splitting at the adapter boundary rather than altering upstream scheduling or
weakening the timing contract. Preset windows/intervals and public mode IDs are
unchanged. Processing remains preallocated, but complete-spectrum work is more
concentrated; realtime CPU/deadline acceptance requires app/device measurement.

All adapter buffers and pointer arrays are allocated/pre-touched in initialise.
The upstream work buffers are sized in preset/configure; process/seek/flush
resize within that capacity. The adapter does not allocate, lock, log or wait
in parameter, reset, processing or drain calls. This source review does not
establish measured real-time CPU cost or app/device playback acceptance.
