#include <Windows.h>
#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <mutex>
#include <locale>
#include <codecvt>

using namespace std;

// 常量定义
constexpr wchar_t DEFAULT_TEXT[] = L"作用\n\n格式\n\n参数\n\n返回值\n\n使用";
constexpr wchar_t LOG_DIR[] = L"%APPDATA%\\mouse\\log";
constexpr wchar_t LOG_PATH[] = L"%APPDATA%\\mouse\\log\\error.txt";

// 线程同步
atomic<bool> g_threadCompleted{true};
mutex taskMutex;
mutex clipboardMutex;

class ScopedHook {
private:
    HHOOK m_hook;

public:
    ScopedHook(int type, HOOKPROC proc) : m_hook(SetWindowsHookEx(type, proc, GetModuleHandle(nullptr), 0)) {
        if (!m_hook) throw runtime_error("Failed to set hook");
    }

    ~ScopedHook() {
        if (m_hook) UnhookWindowsHookEx(m_hook);
    }

    HHOOK Get() const { return m_hook; }
};

class ClipboardManager {
public:
    static bool CopyToClipboard(const wstring& text) {
        lock_guard<mutex> lock(clipboardMutex);
        if (!OpenClipboard(nullptr)) return false;

        EmptyClipboard();
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (!hGlobal) {
            CloseClipboard();
            return false;
        }

        wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hGlobal));
        if (!pData) {
            GlobalFree(hGlobal);
            CloseClipboard();
            return false;
        }
        wcscpy_s(pData, text.size() + 1, text.c_str());
        GlobalUnlock(hGlobal);

        bool result = SetClipboardData(CF_UNICODETEXT, hGlobal) != nullptr;
        CloseClipboard();
        return result;
    }
};

class InputSimulator {
public:
    static void SendKeyCombo(const vector<WORD>& keys, DWORD delay = 50) {
        vector<INPUT> inputs;
        inputs.reserve(keys.size() * 2);

        // 按下按键
        for (WORD key : keys) {
            INPUT down = {0};
            down.type = INPUT_KEYBOARD;
            down.ki.wVk = key;
            down.ki.dwFlags = 0;
            inputs.push_back(down);
        }

        // 释放按键（反向顺序）
        for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
            INPUT up = {0};
            up.type = INPUT_KEYBOARD;
            up.ki.wVk = *it;
            up.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(up);
        }

        // 发送输入事件
        UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        if (sent != inputs.size()) {
            throw runtime_error("Failed to send all input events");
        }
        Sleep(delay);
    }
};

void LogException(const exception& e) {
    // 展开环境变量
    WCHAR expandedPath[MAX_PATH];
    ExpandEnvironmentStringsW(LOG_PATH, expandedPath, MAX_PATH);
    WCHAR expandedDir[MAX_PATH];
    ExpandEnvironmentStringsW(LOG_DIR, expandedDir, MAX_PATH);

    // 创建日志目录
    CreateDirectoryW(expandedDir, nullptr);

    // 将宽字符路径转换为UTF-8字符串
    wstring_convert<codecvt_utf8<wchar_t>> converter;
    string narrowPath = converter.to_bytes(expandedPath);

    // 打开日志文件并写入异常信息
    ofstream logFile(narrowPath, ios::app);
    if (logFile) {
        logFile << "[" << __DATE__ << " " << __TIME__ << "] ERROR: " << e.what() << endl;
    }
}

void ExecuteSafely(function<void()> task) noexcept {
    try {
        lock_guard<mutex> lock(taskMutex);
        if (g_threadCompleted.exchange(false)) {
            thread([task] {
                try {
                    task();
                } catch (const exception& e) {
                    LogException(e);
                }
                g_threadCompleted = true;
            }).detach();
        }
    } catch (const exception& e) {
        LogException(e);
    }
}

LRESULT CALLBACK MouseHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= HC_ACTION && wParam == WM_XBUTTONDOWN) {
        const auto& info = *reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        switch (GET_XBUTTON_WPARAM(info.mouseData)) {
            case XBUTTON1:
                ExecuteSafely([] {
                    if (ClipboardManager::CopyToClipboard(DEFAULT_TEXT)) {
                        InputSimulator::SendKeyCombo({VK_CONTROL, 0x56}); // V键的虚拟键码
                    }
                });
                return 1;
            case XBUTTON2:
                ExecuteSafely([] {
                    InputSimulator::SendKeyCombo({VK_RETURN, VK_TAB});
                });
                return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= HC_ACTION && wParam == WM_KEYDOWN) {
        const auto& info = *reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (info.vkCode == VK_CAPITAL) {
            ExecuteSafely([] {
                InputSimulator::SendKeyCombo({VK_MENU, VK_TAB}, 600);
            });
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    try {
        ScopedHook keyboardHook(WH_KEYBOARD_LL, KeyboardHook);
        ScopedHook mouseHook(WH_MOUSE_LL, MouseHook);

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        while (!g_threadCompleted) {
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    } catch (const exception& e) {
        LogException(e);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}