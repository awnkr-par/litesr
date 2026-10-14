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
