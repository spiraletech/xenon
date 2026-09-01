#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <commdlg.h>
#include <mfapi.h>

#include "xenon/cortex.hpp"
#include "xenon/engine.hpp"
#include "xenon/organic.hpp"
#include "xenon/player_engine.hpp"
#include "xenon/producer.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"XENON_WORKSTATION_WINDOW";
constexpr UINT_PTR kUiTimer = 1;
constexpr UINT kUiTimerMs = 33;

enum ControlId : int {
    IdOpen = 100,
    IdPlay,
    IdRender,
    IdPrompt
};

struct AppState {
    xenon::PlayerEngine player;
    xenon::Cortex cortex;
    xenon::Organic organic;
    xenon::Producer producer;
    xenon::Engine engine;
    std::filesystem::path current_track;
    std::wstring status{L"XENON ONLINE"};
    HWND prompt{};
};

AppState* g_app{};

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(std::max(0, size)), '\0');
    if (size > 0) WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::filesystem::path chooseAudio(HWND owner) {
    wchar_t buffer[32768]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = static_cast<DWORD>(std::size(buffer));
    ofn.lpstrFilter = L"Audio Files (*.mp3;*.wav)\0*.mp3;*.wav\0MP3 Files (*.mp3)\0*.mp3\0WAV Files (*.wav)\0*.wav\0\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return {};
    return std::filesystem::path(buffer);
}

void drawText(HDC dc, const std::wstring& value, RECT rect, int size, bool bold = false) {
    HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    const auto old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 231, 220));
    DrawTextW(dc, value.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, old);
    DeleteObject(font);
}

void drawSpectrum(HDC dc, const RECT& rect, const xenon::SpectrumFrame& frame) {
    HBRUSH panel = CreateSolidBrush(RGB(9, 9, 11));
    FillRect(dc, &rect, panel);
    DeleteObject(panel);

    HPEN grid = CreatePen(PS_SOLID, 1, RGB(44, 39, 25));
    const auto oldPen = SelectObject(dc, grid);
    for (int i = 0; i <= 8; ++i) {
        const int x = rect.left + (rect.right - rect.left) * i / 8;
        MoveToEx(dc, x, rect.top, nullptr);
        LineTo(dc, x, rect.bottom);
    }
    for (int i = 0; i <= 5; ++i) {
        const int y = rect.top + (rect.bottom - rect.top) * i / 5;
        MoveToEx(dc, rect.left, y, nullptr);
        LineTo(dc, rect.right, y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(grid);

    if (frame.bars.empty()) return;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int bars = static_cast<int>(frame.bars.size());
    const int gap = 3;
    const int barWidth = std::max(1, (width - gap * (bars - 1)) / bars);
    HBRUSH amber = CreateSolidBrush(RGB(242, 195, 61));
    HBRUSH peak = CreateSolidBrush(RGB(255, 232, 145));

    for (int i = 0; i < bars; ++i) {
        const float value = std::clamp(frame.bars[static_cast<std::size_t>(i)], 0.0f, 1.0f);
        const int h = std::max(2, static_cast<int>(value * height));
        const int x = rect.left + i * (barWidth + gap);
        RECT b{x, rect.bottom - h, x + barWidth, rect.bottom};
        FillRect(dc, &b, amber);
        const float p = i < static_cast<int>(frame.peaks.size()) ? frame.peaks[static_cast<std::size_t>(i)] : value;
        const int py = rect.bottom - static_cast<int>(std::clamp(p, 0.0f, 1.0f) * height);
        RECT pk{x, py, x + barWidth, py + 2};
        FillRect(dc, &pk, peak);
    }
    DeleteObject(amber);
    DeleteObject(peak);
}

void renderFromPrompt(HWND hwnd) {
    if (!g_app) return;
    wchar_t prompt[2048]{};
    GetWindowTextW(g_app->prompt, prompt, static_cast<int>(std::size(prompt)));
    std::wstring request(prompt);
    if (request.empty()) request = L"make a spacious instrumental inspired by what is playing";

    xenon::music::MusicFrameV1 musicFrame;
    musicFrame.track_id = utf8(g_app->current_track.filename().wstring());
    musicFrame.title = utf8(g_app->current_track.stem().wstring());
    musicFrame.position_seconds = g_app->player.positionSeconds();
    const auto& spectrum = g_app->player.currentSpectrum();
    musicFrame.brightness = spectrum.brightness;
    musicFrame.spectral_density = spectrum.spectral_density;
    musicFrame.transient_density = spectrum.transient_density;

    xenon::CortexContext context;
    context.spectrum = spectrum;
    context.preferences = g_app->organic.preferences();
    context.revision_notes = g_app->organic.revision_notes();

    auto intent = g_app->cortex.interpret(utf8(request), musicFrame, context);
    if (intent.project_id.empty()) intent.project_id = "xenon_session";
    if (!g_app->current_track.empty()) intent.reference_audio = g_app->current_track;
    const auto plan = g_app->producer.compile(intent);
    intent.bpm = plan.bpm;
    intent.key = plan.key;
    intent.drum_density = plan.drum_density;
    intent.bass_weight = plan.bass_weight;
    intent.vocal_space = plan.vocal_space;
    intent.texture_grit = plan.texture_grit;
    intent.transient_density = plan.transient_density;

    try {
        const auto artifact = g_app->engine.render(intent, std::filesystem::path{"renders"});
        g_app->organic.set_project(intent.project_id);
        g_app->organic.remember_revision(utf8(request) + " -> " + artifact.audio_path.string());
        g_app->status = L"RENDERED: " + artifact.audio_path.wstring();
    } catch (...) {
        g_app->status = L"RENDER FAILED";
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM) {
    switch (message) {
    case WM_CREATE: {
        g_app = new AppState();
        CreateWindowW(L"BUTTON", L"OPEN", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 24, 24, 92, 32, hwnd,
                      reinterpret_cast<HMENU>(IdOpen), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"PLAY / PAUSE", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 124, 24, 120, 32, hwnd,
                      reinterpret_cast<HMENU>(IdPlay), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"GENERATE", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 252, 24, 104, 32, hwnd,
                      reinterpret_cast<HMENU>(IdRender), nullptr, nullptr);
        g_app->prompt = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"make a spacious instrumental inspired by this track",
                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 380, 24, 560, 32, hwnd,
                                        reinterpret_cast<HMENU>(IdPrompt), nullptr, nullptr);
        SetTimer(hwnd, kUiTimer, kUiTimerMs, nullptr);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IdOpen: {
            const auto path = chooseAudio(hwnd);
            if (!path.empty()) {
                try {
                    g_app->player.open(path);
                    g_app->player.play();
                    g_app->current_track = path;
                    g_app->status = L"PLAYING: " + path.filename().wstring();
                } catch (...) {
                    g_app->status = L"FAILED TO OPEN TRACK";
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case IdPlay:
            if (g_app->player.isPlaying()) g_app->player.pause(); else g_app->player.play();
            return 0;
        case IdRender:
            renderFromPrompt(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_TIMER:
        if (wParam == kUiTimer) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH bg = CreateSolidBrush(RGB(5, 5, 7));
        FillRect(dc, &client, bg);
        DeleteObject(bg);

        RECT title{24, 76, client.right - 24, 112};
        drawText(dc, L"XENON // AI MUSIC WORKSTATION", title, 25, true);
        RECT status{24, 110, client.right - 24, 140};
        drawText(dc, g_app ? g_app->status : L"BOOTING", status, 15, false);

        RECT spectrumRect{24, 158, client.right - 24, 470};
        if (g_app) drawSpectrum(dc, spectrumRect, g_app->player.currentSpectrum());

        RECT labels{24, 486, client.right - 24, 520};
        drawText(dc, L"EARS: ETHERPLAYER FFT 72 // MIND: SPIRAL CORTEX + ORGANIC // HANDS: PRODUCER + RENDERER", labels, 15, true);

        if (g_app) {
            wchar_t timing[128]{};
            swprintf_s(timing, L"PLAYBACK %.1fs / %.1fs", g_app->player.positionSeconds(), g_app->player.durationSeconds());
            RECT timeRect{24, 526, client.right - 24, 560};
            drawText(dc, timing, timeRect, 15, false);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, kUiTimer);
        delete g_app;
        g_app = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, 0);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    if (FAILED(MFStartup(MF_VERSION))) return 1;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) {
        MFShutdown();
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, kWindowClass, L"XENON", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1000, 650,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        MFShutdown();
        return 1;
    }
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    MFShutdown();
    return static_cast<int>(msg.wParam);
}
