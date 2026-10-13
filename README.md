# LiteSR

LiteSR is an experimental Windows desktop recorder and RTMP/RTMPS streamer for older PCs, including Intel HD Graphics 4400.

## Install
Download the newest [Windows installer](https://github.com/awnkr-par/litesr/releases). FFmpeg is **bundled**. Start Menu shortcuts and uninstall support are included. Portable ZIP includes FFmpeg and updater too.

## Updates (alpha.5)
LiteSR checks for new releases after launch and can install updates from within the app. Select **Stable** or **Beta/Alpha** in the Updates menu. On startup it only prompts when a newer version is available. **Check releases** manually checks now. The updater asks permission to download and install; it does **not** silently install upgrades.

The updater uses the official GitHub Releases API, downloads the Inno Setup installer and its SHA256SUMS.txt from HTTPS GitHub URLs, and verifies the installer SHA-256 before running it. It waits for LiteSR to exit, then runs the installer and displays the result. Stable ignores prereleases; Beta includes them. For a safer production channel, future releases should be code-signed and supply independently signed update manifests (checksums from the same compromised release are not a signature).

**Important:** The updater uses the built-in Windows PowerShell and executes the update script shipped with LiteSR. It does not download and execute arbitrary scripts from the web. The installer is currently unsigned. Only use releases from this project's official repository.

## Known limitations
- Video-only FFmpeg gdigrab desktop capture at fixed 720p/30 FPS in the GUI. CLI offers customizable settings.
- No audio capture, game capture, scene system, GPU-native capture, or performance guarantee yet.
- Quick Sync may fail on some Intel HD 4400 drivers; CPU fallback can lower Minecraft FPS.
- No silent auto-install; user consent is required. Auto-update has not been tested on the target PC.
- To update, stop any recording/stream; close LiteSR after approving the update.
- Portable users can update with the installer, which installs LiteSR into the user profile rather than overwriting the portable folder.

## Build
On Windows, with CMake 3.20+ and Visual Studio 2022 C++ tools:
```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

GitHub Actions builds versioned installers and ZIPs and fetches the FFmpeg essentials distribution from gyan.dev. Check released `SHA256SUMS.txt` when downloading files manually.
