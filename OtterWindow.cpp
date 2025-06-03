#include "Otter.h"

void OtterWindow::OtterWin::CreateWindowNew(
    std::wstring NAME, int A, std::wstring Lname,
    int X, int Y, int W, int H,
    HWND AW, HMENU men, LPVOID OID)
{
    this->CLASS_NAME = NAME;
    this->wc.lpfnWndProc = WindowProc;
    this->wc.lpszClassName = this->CLASS_NAME.c_str();
    this->wc.hInstance = this->hInstance;
    this->wc.hCursor = LoadCursor(NULL, IDC_ARROW); // 设置默认光标

    // 注意：hbrBackground在SetLayeredMode中设置
    RegisterClass(&this->wc);

    // 根据模式设置扩展样式
    DWORD exStyle = A;
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

    if (isLayeredWindow) {
        exStyle |= WS_EX_LAYERED;
        style = WS_POPUP | WS_VISIBLE; // 分层窗口可见
    }

    hwndr = CreateWindowExW(
        exStyle,
        CLASS_NAME.c_str(),
        Lname.c_str(),
        style, // 使用修改后的窗口样式
        X, Y, W, H,
        AW, men, hInstance, this
    );
}

void OtterWindow::OtterWin::ShowWindowR() {
    if (isLayeredWindow) {
        // 对于分层窗口，设置初始透明度
        SetWindowAlpha(255); // 完全不透明

        // 确保窗口可见
        ::ShowWindow(hwndr, nCmdShow);
        UpdateWindow(hwndr);

        // 将窗口置于前台
        SetForegroundWindow(hwndr);
    }
    else {
        // 传统窗口的显示方式保持不变
        ::ShowWindow(hwndr, nCmdShow);
        UpdateWindow(hwndr);
    }
}

