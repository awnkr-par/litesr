// LiteSR v0.1: Windows CLI recording/streaming prototype.
// FFmpeg performs capture and encoding; this launcher does NOT yet implement
// GPU-native capture. Test performance before claiming it is lightweight.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using Args = std::vector<std::wstring>;

static std::wstring quote(const std::wstring& value) {
    // Quote Windows command-line arguments using CommandLineToArgvW rules.
    if (value.find_first_of(L" \t\n\v\"") == std::wstring::npos) return value;
    std::wstring result = L"\"";
    unsigned backslashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++backslashes; continue; }
        if (c == L'"') { result.append(backslashes * 2 + 1, L'\\'); result += c; backslashes = 0; continue; }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result += c;
    }
    result.append(backslashes * 2, L'\\');
    return result + L"\"";
}

static bool flagValue(const Args& argv, const std::wstring& key, std::wstring& output) {
    for (size_t i = 2; i < argv.size(); ++i) {
        if (argv[i] == key && i + 1 < argv.size()) {
            output = argv[i + 1]; return true;
        }
    }
    return false;
}
static bool positiveNumber(const std::wstring& s, int minVal, int maxVal, int& out) {
    try {
        size_t length = 0;
        int n = std::stoi(s, &length);
        if (length != s.size() || n < minVal || n > maxVal) return false;
        out = n; return true;
    } catch (...) { return false; }
}
static void usage() {
    std::wcout <<
        L"LiteSR v0.1 (experimental Windows CLI)\n"
        L"Usage:\n"
        L"  litesr record --output video.mp4 [--fps 30] [--width 1280] [--height 720] [--bitrate 3000] [--encoder qsv|cpu]\n"
        L"  litesr stream --url rtmps://server/app/key [same options]\n"
        L"  litesr --help\n\n"
        L"Requires ffmpeg.exe on PATH. Press q then Enter in FFmpeg to stop gracefully.\n"
        L"No audio or region/window selection yet. Default encoder is qsv (Intel Quick Sync).\n";
}
int wmain() {
    int argc = 0;
    LPWSTR* raw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!raw) return 1;
    Args argv(raw, raw + argc);
    LocalFree(raw);

    if (argc < 2 || argv[1] == L"--help" || argv[1] == L"-h") {
        usage(); return argc < 2 ? 1 : 0;
    }
    const bool recording = argv[1] == L"record";
    const bool streaming = argv[1] == L"stream";
    if (!recording && !streaming) { usage(); return 2; }

    std::wstring output, encoder = L"qsv", value;
    if (!flagValue(argv, recording ? L"--output" : L"--url", output) || output.empty()) {
        std::wcerr << L"Missing --output or --url.\n"; return 2;
    }
    if (recording && output[0] == L'-') { std::wcerr << L"Invalid output filename.\n"; return 2; }
    if (streaming && output.rfind(L"rtmp://", 0) != 0 && output.rfind(L"rtmps://", 0) != 0) {
        std::wcerr << L"Streaming URL must start with rtmp:// or rtmps://\n"; return 2;
    }
    if (flagValue(argv, L"--encoder", value)) encoder = value;
    if (encoder != L"qsv" && encoder != L"cpu") {
        std::wcerr << L"Encoder must be qsv or cpu.\n"; return 2;
    }
    int fps = 30, width = 1280, height = 720, bitrate = 3000;
    struct NumOpt { const wchar_t* name; int* destination; int min; int max; };
    NumOpt nums[] = {{L"--fps", &fps, 1, 60}, {L"--width", &width, 320, 3840},
                     {L"--height", &height, 240, 2160}, {L"--bitrate", &bitrate, 250, 50000}};
    for (auto& item : nums) {
        if (flagValue(argv, item.name, value) && !positiveNumber(value, item.min, item.max, *item.destination)) {
            std::wcerr << L"Invalid numeric option: " << item.name << L"\n"; return 2;
        }
    }
    if (width % 2 || height % 2) {
        std::wcerr << L"Width and height must be even.\n"; return 2;
    }

    wchar_t exePath[MAX_PATH]{}; GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring localExe(exePath); localExe = localExe.substr(0, localExe.find_last_of(L"\\/") + 1) + L"ffmpeg\\bin\\ffmpeg.exe";
    if (GetFileAttributesW(localExe.c_str()) == INVALID_FILE_ATTRIBUTES) { std::wcerr << L"Bundled FFmpeg not found. Reinstall LiteSR.\\n"; return 1; }
    Args ff = {localExe, L"-hide_banner", L"-loglevel", L"warning",
               L"-stats", L"-f", L"gdigrab", L"-framerate", std::to_wstring(fps),
               L"-i", L"desktop", L"-an",
               L"-vf", L"scale=" + std::to_wstring(width) + L":" + std::to_wstring(height) + L",format=nv12"};
    if (encoder == L"qsv") {
        ff.insert(ff.end(), {L"-c:v", L"h264_qsv", L"-preset", L"veryfast"});
    } else {
        ff.insert(ff.end(), {L"-c:v", L"libx264", L"-preset", L"ultrafast", L"-tune", L"zerolatency"});
    }
    ff.insert(ff.end(), {L"-b:v", std::to_wstring(bitrate) + L"k",
                         L"-maxrate", std::to_wstring(bitrate) + L"k",
                         L"-bufsize", std::to_wstring(bitrate * 2) + L"k",
                         L"-g", std::to_wstring(fps * 2), L"-pix_fmt", L"nv12"});
    if (recording) {
        ff.insert(ff.end(), {L"-y", L"-movflags", L"+faststart", output});
    } else {
        ff.insert(ff.end(), {L"-f", L"flv", output});
    }

    std::wstring cmdline;
    for (const auto& arg : ff) {
        if (!cmdline.empty()) cmdline += L' ';
        cmdline += quote(arg);
    }
    STARTUPINFOW start{};
    start.cb = sizeof(start);
    PROCESS_INFORMATION proc{};
    // Inherit terminal stdin so FFmpeg can gracefully finalize files on 'q'.
    std::vector<wchar_t> writable(cmdline.begin(), cmdline.end());
    writable.push_back(L'\0');
    std::wcout << (recording ? L"Recording" : L"Streaming")
               << L" at " << width << L"x" << height << L" / " << fps
               << L" FPS (" << encoder << L").\n"
               << L"FFmpeg: press q then Enter to stop.\n";
    std::wcout.flush();

    if (!CreateProcessW(nullptr, writable.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &start, &proc)) {
        std::wcerr << L"Could not start FFmpeg (Windows error " << GetLastError()
                   << L"). Verify LiteSR installation.\n";
        return 1;
    }
    WaitForSingleObject(proc.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(proc.hProcess, &code);
    CloseHandle(proc.hThread);
    CloseHandle(proc.hProcess);
    if (code) std::wcerr << L"FFmpeg exited with code " << code
                         << L". If QSV failed, update Intel drivers or try --encoder cpu.\n";
    return static_cast<int>(code);
}
