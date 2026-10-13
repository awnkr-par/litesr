# LiteSR

**LiteSR** is an open-source Windows screen-recording and RTMP/RTMPS-streaming project targeting older and low-spec hardware, including Intel HD Graphics 4400.

> **Status: v0.1 experimental proof of concept.** This release is a *C++ command-line launcher around FFmpeg*, **not** a finished OBS replacement, not Direct3D Desktop Duplication capture, and not proven to improve Minecraft FPS. It has **no watermark**, **no artificial recording-time limit**, and **no account requirement**.

## Current features

- Capture the Windows desktop through FFmpeg's `gdigrab` input
- Record MP4 with H.264
- Stream to an RTMP or RTMPS server (e.g. compatible streaming platforms)
- Choose Intel Quick Sync (`h264_qsv`) or software (`libx264`) encoding
- Set output resolution, FPS, and bitrate
- No proprietary paywall or artificial time limit

**Not implemented yet:** audio, GUI, game/window-only capture, overlays, multiple scenes, replay buffer, GPU-native capture, or simultaneous recording and streaming. Recording performance is unverified.

## Build (Windows)

Prerequisites: Windows 10/11, Visual Studio 2022 C++ build tools, CMake 3.20+, and an FFmpeg build available as `ffmpeg.exe` on PATH.

In a Developer PowerShell for Visual Studio:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Then run `build\Release\litesr.exe` from a terminal.

## Record

```powershell
.\build\Release\litesr.exe record --output test.mp4 --fps 30 --width 1280 --height 720 --bitrate 3000 --encoder qsv
```

## Stream

```powershell
.\build\Release\litesr.exe stream --url "rtmps://YOUR_SERVER/app/YOUR_STREAM_KEY" --fps 30 --width 1280 --height 720 --bitrate 2500 --encoder qsv
```

**Keep stream keys secret**. Avoid posting command lines or console logs containing your key, and prefer testing against a private RTMP server initially. A future GUI should keep keys out of visible command lines.

To end either session gracefully, focus the FFmpeg terminal, type **q**, and press **Enter**. Abruptly closing the process can make MP4 files unplayable.

## Intel HD 4400 notes

Some HD 4400 hardware/driver configurations support H.264 Quick Sync, but FFmpeg `h264_qsv` may not initialize with older Windows drivers. When testing, try QSV first. `--encoder cpu` is a diagnostic fallback and can cause severe FPS loss.

**Important:** `gdigrab` captures through a CPU-accessible path; QSV encoding does **not** make capture GPU-native. Our next milestone is Windows Desktop Duplication / Direct3D 11 capture with minimal copy overhead and careful frame pacing. 720p/30 FPS is an initial test configuration, not a performance guarantee.

## Project roadmap

1. Baseline benchmarks: Minecraft FPS without capture vs. capture (record and stream)
2. Native Direct3D 11 capture path, GPU-side conversion where feasible
3. Audio capture and synchronization
4. Small Windows GUI + start/stop hotkeys
5. Safer streaming-key storage; stream reconnection and error handling
6. Game/window capture and optional simultaneous output

Contributions welcome. Be sure not to commit credentials or private streaming keys.
