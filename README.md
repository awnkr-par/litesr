# LiteSR

LiteSR is an experimental Windows x64 screen recorder and RTMP/RTMPS streamer targeting older computers. The installer and portable ZIP bundle FFmpeg.

## Streaming setup

Open LiteSR, select **Live stream**, then select YouTube, Twitch, or Custom. The server URL is the ingest endpoint, while the stream key is the secret provided by your broadcasting platform. YouTube defaults to `rtmps://a.rtmp.youtube.com/live2`; Twitch defaults to `rtmp://live.twitch.tv/app`. Always confirm the service endpoint in your account dashboard. Never share stream keys. The key is kept in memory only (not saved in settings), but the current FFmpeg subprocess command line can expose it to local process inspection.

Use 720p/30FPS/2500-3500Kbps for the first Intel HD 4400 trial, then test 480p if performance suffers. You can choose 480p/720p/1080p, 30/60 FPS, bitrate 300-50000 Kbps, Intel QSV or CPU x264. These options do not guarantee your PC can sustain the chosen quality.

## Recording

Select **Record to MP4** and choose an output filename. End with **Stop** to finalize video safely. **Audio is not available yet**; the audio selector explicitly shows this rather than claiming unsupported microphone/system capture. Recording and streaming simultaneously, game-specific capture, and scene overlays are also not yet implemented.

## Settings and updates

Non-secret settings are stored at `%APPDATA%\\LiteSR\\settings.ini`. Stream keys are not persisted. Stable and Beta/Alpha update checking is built in; updater verifies release installer SHA256 and asks before installing. GitHub builds are unsigned. The update flow still needs testing on real Windows hardware.

## Development

C++17 and Win32, CMake 3.20+, VS2022. `cmake -S . -B build -A x64`, then `cmake --build build --config Release`.

**Status:** early alpha. Desktop capture uses FFmpeg gdigrab and can cause major FPS drops. Future work includes WASAPI audio capture, stream connection diagnostics, log viewer, a Direct3D capture pipeline, scene/compositing support, and independently signed updates.

## Capture selection
Choose Monitor or App window, then pick the displayed source. Refresh to rediscover windows. The selected source applies to recording and streaming. Minimized or protected applications may not capture properly; window capture uses FFmpeg gdigrab HWND selection.

## Streaming troubleshooting (alpha.8)
Streaming now includes a silent stereo AAC track (real microphone and game audio are not yet implemented). FFmpeg writes diagnostics to `%APPDATA%\\LiteSR\\ffmpeg.log`. Click **Open stream log** after a failed launch to see why it stopped. Do not share stream keys or command lines. If QSV fails, try CPU x264 at 480p/30FPS for a diagnostic test.

## Studio and updates (alpha.9)
The studio layout now groups destination, capture source, video quality, and application controls. After you approve an update, LiteSR finishes an active recording or stream, closes itself, and the updater waits for exit before running the new installer. No unattended software installation; consent is required. The current updater should be tested on a real Windows machine and remains unsigned.

Alpha.10 adds a darker studio appearance, while retaining the consent-based updater and shutdown handshake from alpha.9. Theme support varies by Windows release.

## Appearance (alpha.11)
MazClient-inspired blue-accented studio theme with a bottom-right Light/Dark button. Your preference is stored in `%APPDATA%\\LiteSR\\settings.ini`, applies immediately, and persists after restart. The design uses native Win32 controls; custom themes may vary slightly by Windows release.

## alpha.12 display fix
Replaces incorrectly themed standard Win32 text labels, dropdown items and buttons with theme-aware custom drawn controls. Removes clashing native groupbox outlines and uses plain text theme labels to avoid encoding problems. This update addresses reported mixed white and dark rendering in Windows 11.

## Alpha.13 OBS-inspired studio
Wide workspace with live low-rate GDI preview, scene/source docks, audio status, output configuration, stream controls and theme switch. Scene is currently single and audio mixing is not yet implemented; this is NOT OBS-compatible multi-layer compositing. Window preview uses visible desktop pixels and may not show obscured windows; capture itself still uses FFmpeg.

## Capstone milestone alpha.14
- Global F9 start and F10 stop hotkeys while LiteSR runs (hotkeys may conflict with other apps).
- Live recording/streaming elapsed timer in the workspace.
- Screen preview pause/resume button. Pausing reduces extra GDI preview work; it does not stop active FFmpeg capture.
- Remember last chosen source label and preview preference in settings; if the source is unavailable, falls back to an available source.
- Recording no longer silently overwrites an existing MP4: warns for existing file and uses FFmpeg `-n` as a second safeguard.
- Continues to be an alpha: real audio, game-specific capture, true multi-scene compositing and measured recording FPS/bitrate statistics are not implemented. Windows GUI appearance/performance needs real-device validation.

## Capstone 2: alpha.15 recording workflow
- Browse for a recording location using the Windows Save As dialog; existing output files are still protected against overwrite.
- Open the recording folder from the studio.
- Three capture presets (Low impact 480p30/1200kbps, Balanced 720p30/2500kbps, High quality 1080p30/4500kbps). Settings remain editable after applying a preset.
- Free disk space indicator, updated periodically.
- Completed recording path appears in the status line when FFmpeg reports a successful exit.
- Preview refresh reduced from 2 to 1 times per second to reduce UI overhead; this is not a GPU capture pipeline.
- Known issues: No microphone/system audio, no real scene compositor or true stream connection feedback; visual layout still requires testing on Windows hardware.

## Capstone 3: microphone capture (alpha.16)
LiteSR now enumerates Windows microphone input devices and provides a Scan button. Choose a microphone to add a separate FFmpeg DirectShow audio input, encoded as 128-kbps AAC for MP4 recording and RTMP streaming. This replaces the previous silent-only streaming path when a microphone is selected; streams without a microphone still carry silent AAC for compatibility. Microphone selection is saved, not a recording of microphone data. Some DirectShow names differ from legacy Windows waveIn names and may need a different selection method; if recording stops immediately, inspect ffmpeg.log. No system/game audio capture or audio level meter yet. Test actual capture on Windows before relying on it.

## Alpha.17 — fullscreen and responsive studio
- The window can now be maximized or resized using the standard Windows title bar.
- F11 or the **Fullscreen [F11]** button switches to true borderless fullscreen on the current monitor; F11 or Esc exits.
- The workspace scales its preview and control positions to the available window size instead of remaining fixed at 1260×925.
- Works independently of recording and streaming sessions. Layout and theme should be validated on different display resolutions; custom Win32 controls remain experimental.

## Alpha.18 — C drive recording folder
On launch LiteSR attempts to create `C:\\LiteSR` and uses `C:\\LiteSR\\recording.mp4` as the default path for new installations. If Windows denies creation at the drive root, it falls back to the user's Videos directory. Existing saved recording paths remain unchanged. The installer stays per-user and does not require administrator privileges.

## Capstone alpha.19 — capture diagnostics and backend experiment
LiteSR now offers a choice between the legacy FFmpeg `gdigrab` capture method and an experimental GPU-based Windows Desktop Duplication (`ddagrab`) source on monitor capture. The Desktop Duplication pipeline downloads frames back to CPU memory for encoder compatibility and is **not guaranteed to be faster**. Window capture retains GDI mode. If the experimental pipeline fails, select GDI and view the FFmpeg log. **Do not infer actual delivered recording FPS from the requested output FPS.**

An FFmpeg `-progress` telemetry file is stored at `%APPDATA%\\LiteSR\\capture-progress.txt`, with the UI displaying reported encode FPS, duplicate/drop counters when present, and encode speed. These are FFmpeg encoder statistics, not independent graphics API capture metrics. It is intentionally a troubleshooting alpha and requires testing on a Windows device. This version does not claim to fix the earlier ~25 FPS recording until measured in a like-for-like trial.

## Alpha.20 — Settings and accessibility
Move server URL, resolution, frame rate, bitrate, encoder and capture backend into the Settings dialog. The studio no longer shows the Stable/Beta selector (alpha builds check beta releases) or a fullscreen button (F11 / Escape still work). Accessibility settings include high contrast, larger text and reduced preview-update frequency, saved persistently. Stream keys remain on the studio and are never saved. Settings affect the next capture session; test keyboard navigation and sizing on Windows before relying on accessibility controls for critical operation.

## Alpha.21 — integrated Settings workspace
Replaces the separate popup window with a full-width Settings page inside LiteSR. Navigate with Settings, Save settings, and Back to Studio. Sections group Streaming/Output and Accessibility/Performance; the main Studio keeps the source preview, recording and streaming controls. Accessibility checkboxes now retain normal native checkbox behavior. This is an initial integrated-page capstone and still needs real Windows visual/usability testing, especially at narrow widths.

## Alpha.22 — animated navigation and capture feedback
Adds smooth slide-in transitions between the in-app Studio and Settings pages, a subtle animated recording/live indicator, and a persisted Reduce motion accessibility setting that disables both. Animations use an independent short UI timer and never alter the FFmpeg capture frame rate. If animations cause display problems, enable Reduce motion in Settings. Animation behavior and accessibility require real-machine visual testing.

## Capstone alpha.23: updater reliability
Updates now compare prerelease versions numerically (so alpha.23 is correctly newer than alpha.9), check at startup and roughly hourly while LiteSR remains open, use the executing application's version instead of only a stale script constant, and record diagnostics to `%APPDATA%\\LiteSR\\updater.log`. Check Updates opens the same verified-download and user-approved installer flow. The updater sends an application-specific process-ID shutdown request after verifying SHA256, and the app waits for FFmpeg to finalize before exiting. A View updater log control appears in Settings. Windows release CI exercises version ordering before building installers.

**Important migration note:** Existing alpha.22 users may need to install alpha.23 once manually because their older update logic is the component being repaired. Future releases need a real-machine update test to confirm the entire download / shutdown / install loop.

## Alpha.24 — photosensitivity safety fix
Removes the pulsing recording indicator and animated Studio / Settings page transitions entirely. Recording status is indicated by a static red dot and persistent text. These animations are disabled unconditionally: no special accessibility setting is required. The capture preview continues showing the selected display or window (external video/game content can still flash). A static safety note replaces the motion toggle in Settings.

## Alpha.25 — recording reliability capstone

- Automatic, collision-safe MP4 naming: existing `recording.mp4` remains untouched and new sessions use `recording (1).mp4`, `recording (2).mp4`, and so on (up to 9,999 attempts).
- Preflight free-disk-space check: at least 512 MiB plus about ten minutes of configured video bitrate before starting new MP4 recordings; this is an estimate rather than a guaranteed available duration.
- Session history: successful and failed recording/stream sessions are appended to `%APPDATA%\\LiteSR\\sessions.tsv` with time, source, resolution, target FPS, encoder and exit code. Stream destination and secret key are never logged in history.
- Stop/finalize is idempotent: repeated button presses do not send repeated FFmpeg stop commands. The GUI remains responsive while FFmpeg finishes.
- CI validates that the bundled FFmpeg can encode and probe a synthetic MP4 containing H.264 video and AAC audio, as well as building the Windows application and installer.
- Photosensitivity safety: no pulsing, flashing or page-slide animations were restored.

These tests do not establish real-world screen-capture FPS or prove microphone device compatibility. Session history is plain text under the user's profile; delete it when no longer wanted.
