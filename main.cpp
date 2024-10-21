#include <iostream>
#include <Windows.h>
#include <fstream>
#include <conio.h>
#include <future>
#include <thread>

using namespace std;
// 全局变量用于标志线程是否执行完成
bool threadCompleted = false;
// 要复制到剪贴板的文本
const wchar_t *text = L"作用\n\n格式\n\n参数\n\n返回值\n\n使用";

// 函数用于将文本复制到剪贴板
bool CopyTextToClipboard() {

    if (!OpenClipboard(NULL)) {
        return false;
    }

    // 清空剪贴板内容
    EmptyClipboard();


    // 分配并设置剪贴板数据
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (wcslen(text) + 1) * sizeof(wchar_t));
    if (hGlobal == NULL) {
        CloseClipboard();
        return false;
    }

    wchar_t *pClipboardData = static_cast<wchar_t *>(GlobalLock(hGlobal));
    wcscpy_s(pClipboardData, wcslen(text) + 1, text);
    GlobalUnlock(hGlobal);

    if (SetClipboardData(CF_UNICODETEXT, hGlobal) == NULL) {
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}


void PressCtrlV() {

    threadCompleted = false;
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));
    CopyTextToClipboard();
//
//    // 释放 alt 键
//    inputs[2].type = INPUT_KEYBOARD;
//    inputs[2].ki.wVk = 'V';
//    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
//
//    // 按下 Ctrl 键
//    inputs[0].type = INPUT_KEYBOARD;
//    inputs[0].ki.wVk = VK_CONTROL;
//
//    // 按下 V 键
//    inputs[1].type = INPUT_KEYBOARD;
//    inputs[1].ki.wVk = 'V';
//
//    // 释放 V 键
//    inputs[2].type = INPUT_KEYBOARD;
//    inputs[2].ki.wVk = 'V';
//    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
//
//    // 释放 Ctrl 键
//    inputs[3].type = INPUT_KEYBOARD;
//    inputs[3].ki.wVk = VK_CONTROL;
//    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
//
//    // 发送输入事件
//    SendInput(4, inputs, sizeof(INPUT));

    // 线程执行完成后设置标志
    threadCompleted = true;

}


void SendEnterAndTabToActiveWindow() {

    threadCompleted = false;
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));

    // 发送tab键事件（按下tab）
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[0].ki.dwFlags = 0; // 0 表示按下按键
    inputs[0].ki.time = 0;
    inputs[0].ki.dwExtraInfo = 0;

    // 发送tab键事件（释放tab）
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_RETURN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP; // 释放按键
    inputs[1].ki.time = 0;
    inputs[1].ki.dwExtraInfo = 0;

    // 发送回车键事件（按下回车）
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_TAB;
    inputs[2].ki.dwFlags = 0; // 0 表示按下按键
    inputs[2].ki.time = 0;
    inputs[2].ki.dwExtraInfo = 0;

    // 发送回车键事件（释放回车）
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_TAB;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP; // 释放按键
    inputs[3].ki.time = 0;
    inputs[3].ki.dwExtraInfo = 0;

    // 发送输入事件
    SendInput(4, inputs, sizeof(INPUT));
    // 线程执行完成后设置标志
    threadCompleted = true;
}


//模拟同时按下和释放 alt+tab
void SendAltTab() {

    threadCompleted = false;

    // 模拟按下Alt键
    keybd_event(VK_MENU, 0, 0, 0);

    // 等待一小段时间，模拟按键持续时间
    Sleep(600);

    // 模拟按下Tab键
    keybd_event(VK_TAB, 0, 0, 0);
    Sleep(100);

    // 释放Tab键
    keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0);

    // 释放Alt键
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    // 线程执行完成后设置标志
    threadCompleted = true;

}


HHOOK mouseHook;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT *pMouseStruct = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);
        if (wParam == WM_XBUTTONDOWN) {
            //监听鼠标侧键是否按下
            if (pMouseStruct->mouseData == 131072) {
                //模拟将回车（Enter）和制表符（Tab）输入到当前活动窗口
                std::thread(SendEnterAndTabToActiveWindow).detach();
                // 屏蔽原按键事件
                return 1;
            }
            //监听鼠标侧键是否按下
            if (pMouseStruct->mouseData == 65536) {
                // 模拟按下 CTRL+V
                std::thread(PressCtrlV).detach();
                // 屏蔽原按键事件
                return 1;

            }
        }
    }

    // 如果不需要拦截事件，可以调用下面这行代码来传递事件给下一个钩子或默认处理
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);


}


HHOOK keyboardHook = 0;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kbStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
//        if (kbStruct->vkCode) {
//            std::cout << kbStruct->vkCode << " key down" << std::endl;
//        }
        // 检查是否按下了大写锁定键
        if (kbStruct->vkCode == VK_CAPITAL && wParam == WM_KEYDOWN) {
            // 创建新线程立即执行 SendAltTab
            std::thread(SendAltTab).detach();
            // 屏蔽原按键事件
            return 1;
        }

    }

    // 调用下一个钩子
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}



int main() {

    // 创建一个不可见的窗口
    HWND hWnd = GetForegroundWindow();
    // 隐藏窗口
    ShowWindow(hWnd, SW_HIDE);


    try {
        // 设置键盘钩子
        keyboardHook = SetWindowsHookEx(
                WH_KEYBOARD_LL,            // 钩子类型，WH_KEYBOARD_LL 为键盘钩子
                KeyboardProc,    // 指向钩子函数的指针
                GetModuleHandleA(NULL),    // Dll 句柄
                0
        );

        // 设置鼠标钩子
        mouseHook = SetWindowsHookEx(
                WH_MOUSE_LL,
                MouseProc,
                GetModuleHandleA(NULL),
                0
        );



        //鼠标钩子异常记录
        if (mouseHook == NULL || mouseHook == 0) {
            throw std::runtime_error("Failed to set mouse hook");
        }

        //键盘钩子异常记录
        if (keyboardHook == NULL || keyboardHook == 0) {
            throw std::runtime_error("Failed to set keyboard hook");
        }


        // 进入消息循环
        MSG msg;
        // MSG 接收这个消息
        while (GetMessage(&msg, NULL, 0, 0)) {
            // 把按键消息传递给字符消息
            TranslateMessage(&msg);
            // 将消息分派给窗口程序
            DispatchMessage(&msg);
        }

        // 卸载钩子
        UnhookWindowsHookEx(mouseHook);
        UnhookWindowsHookEx(keyboardHook);

        // 在程序结束前等待 SendAltTab 线程的完成
        while (!threadCompleted) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 等待一段时间，避免忙等
        }


    } catch (const std::exception &e) {
        // 打开文件并写入异常信息
        // 使用 std::ofstream 定义文件输出流，文件流将在离开作用域时自动关闭，文件不存在时，自动创建，存在时追加数据到文件中
        std::ofstream outputFile("C:/log/error.txt", std::ios::app);
        if (outputFile.is_open()) {
            outputFile << "Exception: " << e.what() << std::endl;
            outputFile.close();
        } else {
            std::cerr << "Failed to open the error log file" << std::endl;
        }
        return 1;
    }

    return 0;

}
