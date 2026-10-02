#include "app.h"
#include "gfx.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

static std::vector<std::wstring> CommandLineFiles() {
    std::vector<std::wstring> out;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; i++) {
        std::wstring a = argv[i];
        if (a.empty() || a[0] == L'/') continue;
        if (!IsUrl(a)) {
            wchar_t full[MAX_PATH * 4];
            if (GetFullPathNameW(a.c_str(), (DWORD)std::size(full), full, nullptr)) a = full;
        }
        out.push_back(a);
    }
    if (argv) LocalFree(argv);
    return out;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    std::vector<std::wstring> files = CommandLineFiles();

    // Single instance: forward files to the running player.
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"RetroAmp.SingleInstance.Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = nullptr;
        for (int i = 0; i < 50 && !other; i++) {
            other = FindWindowW(L"RetroAmpMain", nullptr);
            if (!other) Sleep(100);
        }
        if (other) {
            std::wstring data;
            for (auto& f : files) data += f + L"\n";
            COPYDATASTRUCT cds = {0x52414D50, (DWORD)(data.size() * sizeof(wchar_t)), (void*)data.c_str()};
            DWORD pid = 0;
            GetWindowThreadProcessId(other, &pid);
            AllowSetForegroundWindow(pid);
            SendMessageW(other, WM_COPYDATA, 0, (LPARAM)&cds);
        }
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    GdiplusStartupOnce();
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int rc = 0;
    {
        App app;
        if (app.init(inst, files)) rc = app.run();
    }
    GdiplusShutdownOnce();
    CoUninitialize();
    if (mutex) CloseHandle(mutex);
    return rc;
}
