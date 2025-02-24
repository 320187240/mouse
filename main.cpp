#include <Windows.h>
#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <mutex>
#include <locale>
#include <iostream>

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
        if (!OpenClipboard(nullptr)) {
            return false;
        }

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

// 递归创建目录的辅助函数
bool CreateDirectoryRecursively(const wstring& path) {
    size_t pos = 0;
    wstring currentPath;

    // 逐级检查并创建目录
    while ((pos = path.find(L'\\', pos)) != wstring::npos) {
        currentPath = path.substr(0, pos);
        if (!currentPath.empty() && !CreateDirectoryW(currentPath.c_str(), nullptr)) {
            DWORD error = GetLastError();
            return false; // 创建失败且不是因为目录已存在
        }
        pos++;
    }

    // 创建最后一级目录
    if (!CreateDirectoryW(path.c_str(), nullptr)) {
        DWORD error = GetLastError();
        return error == ERROR_ALREADY_EXISTS; // 如果已存在，返回 true
    }

    return true;
}

void LogMessage(const wstring& message) {
    WCHAR expandedDir[MAX_PATH];
    WCHAR expandedPath[MAX_PATH];

    // 展开环境变量
    if (ExpandEnvironmentStringsW(LOG_DIR, expandedDir, MAX_PATH) == 0) {
        MessageBoxW(nullptr, L"Failed to expand LOG_DIR", L"Error", MB_OK);
        return;
    }

    if (ExpandEnvironmentStringsW(LOG_PATH, expandedPath, MAX_PATH) == 0) {
        MessageBoxW(nullptr, L"Failed to expand LOG_PATH", L"Error", MB_OK);
        return;
    }

    // 创建日志目录
    if (!CreateDirectoryRecursively(expandedDir)) {
        WCHAR errorMsg[256];
        swprintf_s(errorMsg, L"Failed to create directory: %lu", GetLastError());
        MessageBoxW(nullptr, errorMsg, expandedDir, MB_OK);
        return;
    }

    // 打开日志文件
    wofstream logFile(expandedPath, ios::app);
    if (!logFile.is_open()) {
        DWORD error = GetLastError();
        WCHAR errorMsg[256];
        swprintf_s(errorMsg, L"Failed to open log file: %lu", error);
        MessageBoxW(nullptr, errorMsg, L"Error", MB_OK);
        return;
    }

    // 写入日志并刷新
    logFile << L"[DEBUG] Log file opened successfully" << endl;
    logFile << message << endl;
    logFile.flush();  // 强制刷新

    logFile.close();
}

void LogException(const exception& e) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR timeStr[64];
    swprintf_s(timeStr, L"%04d-%02d-%02d %02d:%02d:%02d",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    string narrowWhat = e.what();
    wstring wideWhat(narrowWhat.begin(), narrowWhat.end());

    wstring message = L"[" + wstring(timeStr) + L"] ERROR: " + wideWhat;
    LogMessage(message);
}

void ExecuteSafely(function<void()> task) noexcept {
    try {
        lock_guard<mutex> lock(taskMutex);
        if (g_threadCompleted.exchange(false)) {
            thread([task] {
                try {
                    task();
                } catch (const exception& e) {
                    LogException(e);  // 捕获并记录异常
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
    // 隐藏控制台窗口
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // 弹出启动成功消息框
    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR timeStr[64];
    swprintf_s(timeStr, L"%04d-%02d-%02d %02d:%02d:%02d",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    wstring startMessage = L"=====应用启动成功=时间: " + wstring(timeStr) + L"=====";
    LogMessage(startMessage);

    MessageBoxW(nullptr, L"应用启动成功", L"启动", MB_OK);

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
