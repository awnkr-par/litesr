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
