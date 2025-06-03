#pragma once
#include <iostream>
#include <functional>
#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <map>
#include <chrono>
#pragma comment(lib, "gdiplus.lib")
#include <windowsx.h>
#include <memory>
#ifndef UNICODE
#define UNICODE
#endif
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <wincodec.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#include <wrl.h>
#define D2D_USE_C_DEFINITIONS
#include <d2d1_1.h>  // 包含D2D 1.1特性
#include <d2d1effects.h> // 包含效果定义
#include <dwrite.h>
#include <wincodec.h>
#include <dshow.h>

// 添加必要的库链接
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

#include <VersionHelpers.h>  // 用于版本检测
#include <dwmapi.h>         // 用于DWM API
#pragma comment(lib, "dwmapi.lib")  // 链接DWM库

namespace OtterWindow {
    //页面函数
    class OtterWin {
    private:

        bool m_isAttached; // 标记是否是接管现有窗口
        // 添加成员变量
        //std::unique_ptr<OtterHtmlRenderer> htmlRenderer;
        // 基础窗口成员
        HINSTANCE hInstance = GetModuleHandle(NULL);
        int nCmdShow = SW_SHOWDEFAULT;
        std::wstring CLASS_NAME;
        WNDCLASS wc = {};
        HWND hwndr = NULL;
        MSG msg = {};
        bool isTrackingMouse = false;
        bool isLayeredWindow = false; // 标记是否为分层窗口

        // GDI+支持
        ULONG_PTR gdiplusToken;
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;

        // 回调函数存储
        std::function<void()> onPaintCallback;

        // 按键处理
        std::map<UINT, std::function<void()>> keyDownCallbacks;    // 按键按下
        std::map<UINT, std::function<void()>> keyUpCallbacks;      // 按键释放
        std::map<UINT, std::chrono::steady_clock::time_point> keyPressTimes; // 记录按键时间

        // 鼠标处理
        std::function<void(int x, int y)> onMouseMove;             // 鼠标移动
        std::function<void(int x, int y)> onLeftDown;              // 左键按下
        std::function<void(int x, int y)> onLeftUp;                // 左键释放
        std::function<void(int x, int y)> onRightDown;             // 右键按下
        std::function<void(int x, int y)> onRightUp;               // 右键释放
        std::function<void(int delta)> onMouseWheel;               // 滚轮滚动
        std::function<void(bool enter)> onMouseHover;              // 鼠标悬停/离开

        bool m_isDragging = false;
        POINT m_dragStartPos = { 0, 0 };
        POINT m_windowStartPos = { 0, 0 };

        // 处理拖动逻辑
        void StartDrag(int x, int y) {
            m_isDragging = true;
            m_dragStartPos = { x, y };
            RECT rect;
            GetWindowRect(hwndr, &rect);
            m_windowStartPos = { rect.left, rect.top };
            SetCapture(hwndr); // 捕获鼠标
        }

        void ContinueDrag(int x, int y) {
            if (!m_isDragging) return;

            int dx = x - m_dragStartPos.x;
            int dy = y - m_dragStartPos.y;

            SetWindowPos(hwndr, NULL,
                m_windowStartPos.x + dx,
                m_windowStartPos.y + dy,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        void EndDrag() {
            if (m_isDragging) {
                m_isDragging = false;
                ReleaseCapture(); // 释放鼠标捕获
            }
        }

        // 窗口过程
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
            OtterWin* pThis = nullptr;

            if (uMsg == WM_NCCREATE) {
                CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
                pThis = (OtterWin*)pCreate->lpCreateParams;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
                SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, IDC_ARROW));
            }
            else {
                pThis = (OtterWin*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            }

            if (uMsg == WM_SETCURSOR) {
                if (LOWORD(lParam) == HTCLIENT) {
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                    return TRUE;
                }
            }

            if (pThis) {
                switch (uMsg) {
                case WM_ERASEBKGND:
                    return 1;//经处理背景擦除

                case WM_SIZE:
                    if (pThis) {
                        // 标记尺寸需要更新
                        if (pThis->onPaintCallback) {
                            // 请求重绘
                            InvalidateRect(hwnd, NULL, FALSE);
                        }
                    }
                    break;
                    // 绘制消息
                case WM_PAINT:
                    if (pThis->onPaintCallback) {
                        pThis->onPaintCallback();
                        return 0;
                    }
                    break;

                    // 键盘消息
                case WM_KEYDOWN:
                    if (pThis->keyDownCallbacks.find(wParam) != pThis->keyDownCallbacks.end()) {
                        pThis->keyDownCallbacks[wParam]();
                        pThis->keyPressTimes[wParam] = std::chrono::steady_clock::now();
                    }
                    break;

                case WM_KEYUP:
                    if (pThis->keyUpCallbacks.find(wParam) != pThis->keyUpCallbacks.end()) {
                        pThis->keyUpCallbacks[wParam]();
                        pThis->keyPressTimes.erase(wParam);
                    }
                    break;

                    // 鼠标消息
                case WM_MOUSEMOVE:
                    if (pThis->m_isDragging) {
                        pThis->ContinueDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    }
                    // 首次移动时开始跟踪
                    if (!pThis->isTrackingMouse) {
                        TRACKMOUSEEVENT tme;
                        tme.cbSize = sizeof(tme);
                        tme.dwFlags = TME_HOVER | TME_LEAVE;
                        tme.hwndTrack = hwnd;
                        tme.dwHoverTime = HOVER_DEFAULT;
                        TrackMouseEvent(&tme);
                        pThis->isTrackingMouse = true;

                        // 触发进入事件
                        if (pThis->onMouseHover) {
                            pThis->onMouseHover(true);
                        }
                    }
                    if (pThis->onMouseMove) {
                        pThis->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    }
                    break;

                case WM_LBUTTONDOWN:
                    if (pThis->onLeftDown) {
                        pThis->onLeftDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    }
                    // 添加长按检测
                    SetTimer(hwnd, 1, 500, NULL); // 500ms后触发长按
                    return 0;

                case WM_TIMER:
                    if (wParam == 1) {
                        KillTimer(hwnd, 1);
                        // 检查鼠标是否仍在按下状态
                        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                            pThis->StartDrag(pThis->m_dragStartPos.x, pThis->m_dragStartPos.y);
                        }
                    }
                    return 0;

                case WM_LBUTTONUP:
                    KillTimer(hwnd, 1); // 取消长按计时器
                    pThis->EndDrag();
                    if (pThis->onLeftUp) {
                        pThis->onLeftUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    }
                    break;

                case WM_RBUTTONDOWN:
                    if (pThis->onRightDown) {
                        pThis->onRightDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    }
                    break;

                case WM_RBUTTONUP:
                    if (pThis->onRightUp) {
                        pThis->onRightUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    }
                    break;

                case WM_MOUSEWHEEL:
                    if (pThis->onMouseWheel) {
                        pThis->onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
                    }
                    break;

                case WM_MOUSEHOVER:
                    if (pThis->onMouseHover) {
                        pThis->onMouseHover(true);
                    }
                    pThis->isTrackingMouse = false; // 需要重新设置跟踪
                    break;

                case WM_MOUSELEAVE:
                    if (pThis->onMouseHover) {
                        pThis->onMouseHover(false);
                    }
                    pThis->isTrackingMouse = false;
                    break;

                case WM_DESTROY:
                    PostQuitMessage(0);
                    return 0;
                }
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

    public:



        //获取坐标
        RECT GetWindPos() {
            RECT rect;
            GetWindowRect(hwndr, &rect);
            return rect;
        }

    /*
        bool CreateHtmlRenderer() {
            if (!hwndr) return false;

            htmlRenderer = std::make_unique<OtterHtmlRenderer>(hwndr);
            return htmlRenderer->Initialize();
        }

        OtterHtmlRenderer* GetHtmlRenderer() {
            return htmlRenderer.get();
        }
        */

        // 添加获取分层模式状态的方法
        bool IsLayeredWindow() const { return isLayeredWindow; }

        // 设置窗口为分层模式（必须在CreateWindowNew前调用）
        void SetLayeredMode(bool enabled) {
            isLayeredWindow = enabled;
            if (enabled) {
                // 分层窗口需要特殊设置
                wc.hbrBackground = NULL; // 必须设为NULL
            }
            else {
                wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 传统窗口背景
            }
        }
        /******************** 基本功能 ********************/

        // 窗口类名，窗口扩展样式，标题文本，X,Y,W,H,父窗口,菜单句柄,应用程序数据
        void CreateWindowNew(std::wstring NAME, int A, std::wstring Lname, int X, int Y, int W, int H, HWND AW, HMENU men, LPVOID OID);

        // 判断窗口是否创建成功
        bool judgment() { return hwndr != NULL; }

        // 显示窗口
        void ShowWindowR();

        // 获取窗口句柄
        HWND GetHWND() { return hwndr; }

        // 运行消息循环
        void Run() { MsgBegin(); }

        /******************** 绘制功能 ********************/
        // 设置绘制回调 (RB = Render Backcall)
        void RB(std::function<void()> callback) {
            onPaintCallback = callback;
        }

        /******************** 键盘功能 ********************/
        // 设置按键按下回调
        void OnKeyDown(int vkCode, std::function<void()> callback) {
            keyDownCallbacks[vkCode] = callback;
        }

        // 设置按键释放回调
        void OnKeyUp(int vkCode, std::function<void()> callback) {
            keyUpCallbacks[vkCode] = callback;
        }

        // 检查按键是否被长按(毫秒)
        bool IsKeyLongPress(int vkCode, int thresholdMs = 500) {
            auto it = keyPressTimes.find(vkCode);
            if (it != keyPressTimes.end()) {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - it->second).count();
                return duration > thresholdMs;
            }
            return false;
        }

        /******************** 鼠标功能 ********************/
        // 设置鼠标移动回调
        void OnMouseMove(std::function<void(int x, int y)> callback) {
            onMouseMove = callback;
            // 启用鼠标追踪
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_HOVER | TME_LEAVE, hwndr, HOVER_DEFAULT };
            TrackMouseEvent(&tme);
        }

        // 设置左键按下回调
        void OnLeftDown(std::function<void(int x, int y)> callback) {
            onLeftDown = callback;
        }

        // 设置左键释放回调
        void OnLeftUp(std::function<void(int x, int y)> callback) {
            onLeftUp = callback;
        }

        // 设置右键按下回调
        void OnRightDown(std::function<void(int x, int y)> callback) {
            onRightDown = callback;
        }

        // 设置右键释放回调
        void OnRightUp(std::function<void(int x, int y)> callback) {
            onRightUp = callback;
        }

        // 设置滚轮回调
        void OnMouseWheel(std::function<void(int delta)> callback) {
            onMouseWheel = callback;
        }

        // 设置鼠标悬停/离开回调
        void OnMouseHover(std::function<void(bool enter)> callback) {
            onMouseHover = callback;
        }

        /******************** 构造/析构 ********************/
        OtterWin(HWND hwnd):hwndr(hwnd) {
            Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
        }

        ~OtterWin() {
            Gdiplus::GdiplusShutdown(gdiplusToken);
        }

        void SetWindowAlpha(BYTE alpha) {
            if (!isLayeredWindow || !hwndr) return;

            // 获取窗口尺寸
            RECT rect;
            GetClientRect(hwndr, &rect);

            // 创建兼容DC
            HDC hdc = GetDC(hwndr);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

            // 使用GDI+绘制内容
            {
                Gdiplus::Graphics graphics(memDC);
                graphics.Clear(Gdiplus::Color(alpha, 0, 0, 0)); // 设置背景色和透明度

                // 这里可以添加其他绘制内容
                Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 0, 0)); // 红色
                graphics.FillRectangle(&brush, 50, 50, 200, 100);
            }

            // 更新分层窗口
            BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
            POINT ptSrc = { 0, 0 };
            SIZE sizeWnd = { rect.right, rect.bottom };

            UpdateLayeredWindow(
                hwndr, hdc, NULL, &sizeWnd,
                memDC, &ptSrc, 0, &blend, ULW_ALPHA
            );

            // 清理资源
            SelectObject(memDC, hOldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(memDC);
            ReleaseDC(hwndr, hdc);
        }

        //查看鼠标焦点
        bool IsMouseFocused() const {
            return hwndr && (::GetCapture() == hwndr);
        }

        //查看键盘焦点
        bool IsKeyFocused() const {
            return hwndr && (::GetFocus() == hwndr);
        }

        //查看活动窗口
        bool IsActiveWindow() const {
            return hwndr && (::GetActiveWindow() == hwndr);
        }

    private:
        void MsgBegin() {
            while (GetMessage(&msg, NULL, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    };

    // 画笔工具类
    class OtterPaintbrush {
    private:
        HWND hwnd;
        HDC hdc;
        HDC memDC;
        HBITMAP hBitmap;
        Gdiplus::Graphics* graphics;
        int width, height;
        RECT rect;
        bool isLayered;
        bool antiAlias;
        std::vector<std::function<void(Gdiplus::Graphics&)>> layers; // 图层存储
        HBITMAP hBackBuffer; // 后备缓冲区
        HDC hBackDC;         // 后备设备上下文
        HBITMAP hOldBitmap; // 添加旧位图句柄保存

        std::map<std::string, std::unique_ptr<Gdiplus::Image>> images;
    public:
        Gdiplus::Graphics* GetGraphics() { return graphics; }
        // 构造函数
        OtterPaintbrush(HWND hwnd, bool layered, Gdiplus::Color bgColor = Gdiplus::Color(255, 255, 255, 255), bool useAntiAlias = true)
            : hwnd(hwnd), isLayered(layered), antiAlias(useAntiAlias) {

            GetClientRect(hwnd, &rect);
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;

            hdc = GetDC(hwnd);
            memDC = CreateCompatibleDC(hdc);
            hBitmap = CreateCompatibleBitmap(hdc, width, height);
            SelectObject(memDC, hBitmap);
            hOldBitmap = (HBITMAP)SelectObject(memDC, hBitmap); // 保存旧位图

            graphics = new Gdiplus::Graphics(memDC);

            // 设置抗锯齿
            if (antiAlias) {
                graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
            }

            // 背景设置
            if (isLayered) {
                graphics->Clear(Gdiplus::Color(0, 0, 0, 0)); // 透明背景
            }
            else {
                graphics->Clear(bgColor);
            }
        }

        ~OtterPaintbrush() {
            delete graphics;
            DeleteObject(hBitmap);
            DeleteDC(memDC);
            ReleaseDC(hwnd, hdc);
            SelectObject(memDC, hOldBitmap); // 恢复旧位图
        }

        // === 基本绘图方法 ===

        // 绘制文本
        void DrawText(const std::wstring& text, int x, int y,
            const std::wstring& fontName = L"Arial", float fontSize = 24.0f,
            Gdiplus::Color color = Gdiplus::Color(255, 255, 255, 255)) {
            Gdiplus::Font font(fontName.c_str(), fontSize);
            Gdiplus::SolidBrush brush(color);
            graphics->DrawString(text.c_str(), -1, &font, Gdiplus::PointF((float)x, (float)y), &brush);
        }

        // === 新增绘图方法 ===

        // 绘制矩形
        void DrawRectangle(int x, int y, int width, int height,
            Gdiplus::Color color, float penWidth = 1.0f) {
            Gdiplus::Pen pen(color, penWidth);
            graphics->DrawRectangle(&pen, x, y, width, height);
        }

        // 填充矩形
        void FillRectangle(int x, int y, int width, int height, Gdiplus::Color color) {
            Gdiplus::SolidBrush brush(color);
            graphics->FillRectangle(&brush, x, y, width, height);
        }

        // 绘制圆形
        void DrawCircle(int x, int y, int radius,
            Gdiplus::Color color, float penWidth = 1.0f) {
            Gdiplus::Pen pen(color, penWidth);
            graphics->DrawEllipse(&pen, x - radius, y - radius, radius * 2, radius * 2);
        }

        // 填充圆形
        void FillCircle(int x, int y, int radius, Gdiplus::Color color) {
            Gdiplus::SolidBrush brush(color);
            graphics->FillEllipse(&brush, x - radius, y - radius, radius * 2, radius * 2);
        }

        // 绘制多边形
        void DrawPolygon(const std::vector<Gdiplus::Point>& points,
            Gdiplus::Color color, float penWidth = 1.0f) {
            Gdiplus::Pen pen(color, penWidth);
            graphics->DrawPolygon(&pen, points.data(), (int)points.size());
        }

        // 填充多边形
        void FillPolygon(const std::vector<Gdiplus::Point>& points, Gdiplus::Color color) {
            Gdiplus::SolidBrush brush(color);
            graphics->FillPolygon(&brush, points.data(), (int)points.size());
        }

        // 绘制线条
        void DrawLine(int x1, int y1, int x2, int y2,
            Gdiplus::Color color, float penWidth = 1.0f) {
            Gdiplus::Pen pen(color, penWidth);
            graphics->DrawLine(&pen, x1, y1, x2, y2);
        }

        // === 状态设置 ===
        

        // 设置抗锯齿
        void SetAntiAlias(bool enabled) {
            antiAlias = enabled;
            if (enabled) {
                graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
            }
            else {
                graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
                graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintSystemDefault);
            }
        }

        //DriIMG位图绘制
        bool DreamIMG(const std::wstring& filePath, int x, int y) {
            // 检查文件是否存在
            if (!PathFileExists(filePath.c_str())) {
                std::wcerr << L"文件不存在: " << filePath << std::endl;
                return false;
            }

            Gdiplus::Image image(filePath.c_str());
            Gdiplus::Status status = image.GetLastStatus();

            if (status != Gdiplus::Ok) {
                std::wcerr << L"图片加载失败 (错误码: " << status << L"): " << filePath << std::endl;
                std::cout << status << std::endl;
                return false;
            }

            graphics->DrawImage(&image, x, y);
            return true;
        }

        // 检查图像尺寸是否合理
        bool IsImageSizeReasonable(const std::wstring& path, int maxWidth, int maxHeight) {
            Gdiplus::Bitmap image(path.c_str());
            return image.GetWidth() <= maxWidth && image.GetHeight() <= maxHeight;
        }

        // 更新到窗口（保持不变）
        void Update() {
            if (isLayered) {
                // 分层窗口使用UpdateLayeredWindow
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                POINT ptSrc = { 0, 0 };
                SIZE sizeWnd = { width, height };
                POINT ptPos = { 0, 0 };

                // 使用窗口DC而不是内存DC
                HDC windowDC = GetDC(hwnd);
                UpdateLayeredWindow(
                    hwnd, windowDC, NULL, &sizeWnd,
                    memDC, &ptPos, 0, &blend, ULW_ALPHA
                );
                ReleaseDC(hwnd, windowDC);
            }
            else {
                // 传统窗口使用双缓冲
                HDC windowDC = GetDC(hwnd);
                BitBlt(windowDC, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
                ReleaseDC(hwnd, windowDC);
            }
        }
    };


    using namespace Microsoft::WRL;
    class OtterHtmlRenderer {
    private:
        HWND m_hwndParent;
        ComPtr<ICoreWebView2> m_webView;
        ComPtr<ICoreWebView2Controller> m_webViewController;
        ComPtr<ICoreWebView2Environment> m_webViewEnvironment;
        bool m_isInitialized = false;

        // 环境创建回调
        HRESULT OnEnvironmentCreated(HRESULT result, ICoreWebView2Environment* env) {
            if (result != S_OK || !env) return result;

            env->CreateCoreWebView2Controller(
                m_hwndParent,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    this, &OtterHtmlRenderer::OnControllerCreated).Get());

            return S_OK;
        }

        // 控制器创建回调
        HRESULT OnControllerCreated(HRESULT result, ICoreWebView2Controller* controller) {
            if (result != S_OK || !controller) return result;

            m_webViewController = controller;
            m_webViewController->get_CoreWebView2(&m_webView);

            // 设置初始大小
            RECT bounds;
            GetClientRect(m_hwndParent, &bounds);
            m_webViewController->put_Bounds(bounds);

            // 启用开发者工具（可选）
            ComPtr<ICoreWebView2Settings> settings;
            m_webView->get_Settings(&settings);
            if (settings) {
                settings->put_AreDevToolsEnabled(TRUE);
            }

            m_isInitialized = true;
            return S_OK;
        }

    public:
        OtterHtmlRenderer(HWND parentHwnd) : m_hwndParent(parentHwnd) {}

        ~OtterHtmlRenderer() {
            if (m_webViewController) {
                m_webViewController->Close();
            }
        }

        // 初始化WebView2环境
        bool Initialize() {
            return SUCCEEDED(CreateCoreWebView2EnvironmentWithOptions(
                nullptr,    // 使用默认浏览器数据目录
                nullptr,    // 无用户数据文件夹
                nullptr,    // 无环境选项
                Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                    this, &OtterHtmlRenderer::OnEnvironmentCreated).Get()));
        }

        // 加载HTML内容
        void LoadHtml(const std::wstring& htmlContent) {
            if (m_webView) {
                m_webView->NavigateToString(htmlContent.c_str());
            }
        }

        // 加载URL
        void LoadUrl(const std::wstring& url) {
            if (m_webView) {
                m_webView->Navigate(url.c_str());
            }
        }

        // 调整大小
        void Resize(int x, int y, int width, int height) {
            if (m_webViewController) {
                RECT bounds = { x, y, x + width, y + height };
                m_webViewController->put_Bounds(bounds);
            }
        }

        // 执行JavaScript
        void ExecuteScript(const std::wstring& script,
            std::function<void(const std::wstring&)> callback = nullptr) {
            if (!m_webView) return;

            m_webView->ExecuteScript(
                script.c_str(),
                Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                    [callback](HRESULT errorCode, LPCWSTR result) -> HRESULT {
                        if (callback) {
                            callback(result ? result : L"");
                        }
                        return S_OK;
                    }).Get());
        }

        // 显示/隐藏
        void SetVisible(bool visible) {
            if (m_webViewController) {
                m_webViewController->put_IsVisible(visible ? TRUE : FALSE);
            }
        }
    };

   
    //窗口设置命名空间
    namespace OtterWindow {

        // 窗口样式设置类
        class OtterWindowSet {
        private:
            HWND m_hWnd;
            LONG_PTR m_originalStyle;
            LONG_PTR m_originalExStyle;

            // 检测是否为 Windows 11（Build >= 22000）
            bool IsWindows11OrGreater() {

                std::cout << IsWindows10OrGreater() &&
                    (DWORD)GetVersion() >= 22000;
                return true;
            }

        public:
            //鼠标拖动设置
            void EnableDragMove(bool enabled) {
                LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                if (enabled) {
                    style |= WS_CAPTION; // 确保有标题栏样式
                }
                else {
                    style &= ~WS_CAPTION;
                }
                SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
                UpdateWindowFrame();
            }

            // 构造函数
            explicit OtterWindowSet(HWND hWnd) : m_hWnd(hWnd) {
                // 保存原始样式
                m_originalStyle = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                m_originalExStyle = GetWindowLongPtr(m_hWnd, GWL_EXSTYLE);
            }

            // 恢复原始样式
            void RestoreOriginalStyle() {
                SetWindowLongPtr(m_hWnd, GWL_STYLE, m_originalStyle);
                SetWindowLongPtr(m_hWnd, GWL_EXSTYLE, m_originalExStyle);
                UpdateWindowFrame();
            }

            // 更新窗口框架（应用样式更改后调用）
            void UpdateWindowFrame() {
                SetWindowPos(m_hWnd, NULL, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }

            /******************** 窗口边框和标题栏控制 ********************/

            // 设置是否有标题栏
            void SetTitleBar(bool enabled) {
                LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                if (enabled) {
                    style |= WS_CAPTION;
                }
                else {
                    style &= ~WS_CAPTION;
                }
                SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
                UpdateWindowFrame();
            }

            // 设置是否有边框
            void SetBorder(bool enabled) {
                LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                if (enabled) {
                    style |= WS_BORDER;
                }
                else {
                    style &= ~WS_BORDER;
                }
                SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
                UpdateWindowFrame();
            }

            // 设置是否有系统菜单（窗口左上角的菜单）
            void SetSystemMenu(bool enabled) {
                LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                if (enabled) {
                    style |= WS_SYSMENU;
                }
                else {
                    style &= ~WS_SYSMENU;
                }
                SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
                UpdateWindowFrame();
            }

            /******************** 窗口大小调整控制 ********************/

            // 设置是否可调整大小
            void SetResizable(bool enabled) {
                LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                if (enabled) {
                    style |= WS_THICKFRAME;
                }
                else {
                    style &= ~WS_THICKFRAME;
                }
                SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
                UpdateWindowFrame();
            }

            // 设置是否有最大化按钮
            void SetMaximizeButton(bool enabled) {
                LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                if (enabled) {
                    style |= WS_MAXIMIZEBOX;
                }
                else {
                    style &= ~WS_MAXIMIZEBOX;
                }
                SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
                UpdateWindowFrame();
            }

            // 设置是否有最小化按钮
            void SetMinimizeButton(bool enabled) {
                LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                if (enabled) {
                    style |= WS_MINIMIZEBOX;
                }
                else {
                    style &= ~WS_MINIMIZEBOX;
                }
                SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
                UpdateWindowFrame();
            }

            /******************** 窗口行为控制 ********************/

            // 设置窗口是否可以移动
            void SetMovable(bool enabled) {
                // 通过移除/添加标题栏和边框来间接控制窗口移动
                // 实际移动需要通过处理鼠标消息实现
                LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
                if (enabled) {
                    style |= WS_CAPTION;
                }
                else {
                    style &= ~WS_CAPTION;
                }
                SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
                UpdateWindowFrame();
            }

            // 设置窗口是否置顶
            void SetAlwaysOnTop(bool enabled) {
                SetWindowPos(m_hWnd,
                    enabled ? HWND_TOPMOST : HWND_NOTOPMOST,
                    0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE);
            }

            // 设置窗口是否透明（点击穿透）
            void SetClickThrough(bool enabled) {
                LONG_PTR exStyle = GetWindowLongPtr(m_hWnd, GWL_EXSTYLE);
                if (enabled) {
                    exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
                }
                else {
                    exStyle &= ~WS_EX_TRANSPARENT;
                    // 保留 WS_EX_LAYERED 如果原本就有
                }
                SetWindowLongPtr(m_hWnd, GWL_EXSTYLE, exStyle);
                UpdateWindowFrame();
            }

            /******************** 实用方法 ********************/

            // 获取当前窗口样式
            DWORD GetCurrentStyle() const {
                return (DWORD)GetWindowLongPtr(m_hWnd, GWL_STYLE);
            }

            // 获取当前扩展样式
            DWORD GetCurrentExStyle() const {
                return (DWORD)GetWindowLongPtr(m_hWnd, GWL_EXSTYLE);
            }

            //窗口圆角(WIN11专用)
            void SetRoundedCorners(bool enabled) {
                // 定义圆角样式
                DWM_WINDOW_CORNER_PREFERENCE cornerPreference =
                    enabled ? DWMWCP_ROUND : DWMWCP_DEFAULT;

                // 调用 DWM API 设置窗口圆角
                DwmSetWindowAttribute(
                    m_hWnd,
                    DWMWA_WINDOW_CORNER_PREFERENCE,
                    &cornerPreference,
                    sizeof(DWM_WINDOW_CORNER_PREFERENCE)
                );
            }
            enum DWMSBT {
                DWMSBT_AUTO = 0,      // 系统默认
                DWMSBT_DISABLE = 1,   // 禁用
                DWMSBT_MAINWINDOW = 2,// Mica（主窗口）
                DWMSBT_TRANSIENT = 3, // Acrylic（浮动窗口）
                DWMSBT_TABBED = 4     // Tabbed（标签页窗口）
            };

            //设置窗口图标
            BOOL SetWindowIconFromPath(const std::wstring& iconPath, BOOL setBigIcon = TRUE)
            {
                if (!m_hWnd || iconPath.empty())
                {
                    return FALSE;
                }

                // 加载图标文件
                HICON hIcon = (HICON)LoadImage(
                    NULL,                   // 不使用模块句柄
                    iconPath.c_str(),       // 图标文件路径
                    IMAGE_ICON,             // 加载类型为图标
                    0,                      // 使用实际尺寸
                    0,                      // 使用实际尺寸
                    LR_LOADFROMFILE |       // 从文件加载
                    LR_DEFAULTSIZE |        // 使用默认尺寸
                    LR_SHARED               // 共享图标句柄
                );

                if (!hIcon)
                {
                    // 加载失败，尝试其他方法
                    hIcon = (HICON)LoadImage(
                        GetModuleHandle(NULL),
                        iconPath.c_str(),
                        IMAGE_ICON,
                        0,
                        0,
                        LR_LOADFROMFILE
                    );

                    if (!hIcon)
                    {
                        return FALSE;
                    }
                }

                // 设置窗口图标
                if (setBigIcon)
                {
                    // 设置大图标（窗口和Alt+Tab显示）
                    SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                }
                else
                {
                    // 设置小图标（标题栏显示）
                    SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                }

                return TRUE;
            }

            //窗口模糊(WIN11独有)
            void SetWindowBackdrop(DWMSBT backdropType) {
                DwmSetWindowAttribute(
                    m_hWnd,
                    DWMWA_SYSTEMBACKDROP_TYPE,
                    &backdropType,
                    sizeof(backdropType)
                );
            }

            //关闭窗口
            void QuitWindow() {
                SendMessageW(m_hWnd, WM_CLOSE, 0, 0);
            }

        };

        // 静态变量记录状态
        static bool s_bDragInitiated = false;
        static DWORD s_dwMouseDownTime = 0;

        //鼠标长按拖动
        inline void MouseDownMoveSet(HWND Releas) {
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                if (!s_dwMouseDownTime) {
                    // 首次检测到按下，记录时间
                    s_dwMouseDownTime = GetTickCount();
                }
                else if (!s_bDragInitiated && (GetTickCount() - s_dwMouseDownTime) > 200) {
                    // 长按超过500ms且未开始拖动
                    s_bDragInitiated = true;
                    ReleaseCapture();
                    SendMessage(Releas, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                }
            }
            else {
                // 鼠标释放时重置状态
                s_dwMouseDownTime = 0;
                s_bDragInitiated = false;
            }
        }
    }

    //子窗口嵌入
    inline bool EmbedWindowAsBottomLayerLERR(HWND hParent, HWND hChild) {
        if (!hParent || !hChild || !IsWindow(hParent) || !IsWindow(hChild)) {
            return false;
        }

        // 1. 设置父子关系
        SetParent(hChild, hParent);

        // 2. 设置窗口样式 - 禁用所有交互
        LONG_PTR style = GetWindowLongPtr(hChild, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        SetWindowLongPtr(hChild, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(hChild, GWL_EXSTYLE);
        exStyle |= WS_EX_TRANSPARENT;  // 鼠标穿透
        exStyle |= WS_EX_LAYERED;      // 支持透明度
        exStyle |= WS_EX_NOACTIVATE;   // 禁止激活
        SetWindowLongPtr(hChild, GWL_EXSTYLE, exStyle);

        // 3. 设置为最底层
        SetWindowPos(hChild, HWND_BOTTOM, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

        // 4. 禁用所有可能的交互
        EnableWindow(hChild, FALSE);  // 禁用窗口

        // 5. 更新窗口
        SetWindowPos(hChild, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        return true;
    }

}


#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#include <stdexcept>
#include <d2d1helper.h>  // 包含D2D辅助函数和常量

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wincodec.h>  // WIC (Windows Imaging Component)
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")  // WIC库

namespace OtterIMG {

    // 高性能图片渲染器
    class OtterImageRenderer {
    private:
        HWND m_hWnd;                    // 关联窗口句柄
        bool m_isLayered;               // 是否为分层窗口
        int m_width, m_height;          // 当前窗口尺寸

        // 双缓冲相关
        HDC m_hBackBufferDC;            // 后备缓冲区设备上下文
        HBITMAP m_hBackBuffer;          // 后备缓冲区位图
        HBITMAP m_hOldBitmap;           // 保存的旧位图

        // GDI+资源
        ULONG_PTR m_gdiplusToken;       // GDI+令牌
        std::unique_ptr<Gdiplus::Graphics> m_pGraphics; // 绘图表面

        // 图片缓存
        std::unordered_map<std::wstring, std::unique_ptr<Gdiplus::Bitmap>> m_imageCache;

        //视频参数


    public:

        // 绘制图片并适应窗口大小
        bool DrawImageFitWindow(const std::wstring& key,
            float opacity = 1.0f,
            bool keepAspectRatio = true) {
            int windowWidth, windowHeight;
            RECT rect;
            GetClientRect(m_hWnd, &rect);
            windowWidth = rect.right - rect.left;
            windowHeight = rect.bottom - rect.top;

            int imgWidth, imgHeight;
            if (!GetImageSize(key, imgWidth, imgHeight)) {
                return false;
            }

            if (keepAspectRatio) {
                // 保持宽高比
                float windowAspect = (float)windowWidth / windowHeight;
                float imgAspect = (float)imgWidth / imgHeight;

                int destWidth, destHeight;
                if (windowAspect > imgAspect) {
                    // 以高度为基准
                    destHeight = windowHeight;
                    destWidth = (int)(imgAspect * destHeight);
                }
                else {
                    // 以宽度为基准
                    destWidth = windowWidth;
                    destHeight = (int)(destWidth / imgAspect);
                }

                // 居中显示
                int x = (windowWidth - destWidth) / 2;
                int y = (windowHeight - destHeight) / 2;
                return DrawImage(key, x, y, opacity, destWidth, destHeight);
            }
            else {
                // 拉伸填充整个窗口
                return DrawImage(key, 0, 0, opacity, windowWidth, windowHeight);
            }
        }

        // 平铺图片适应窗口
        bool DrawImageTileWindow(const std::wstring& key, float opacity = 1.0f) {
            int imgWidth, imgHeight;
            if (!GetImageSize(key, imgWidth, imgHeight)) {
                return false;
            }

            RECT rect;
            GetClientRect(m_hWnd, &rect);
            int windowWidth = rect.right - rect.left;
            int windowHeight = rect.bottom - rect.top;

            for (int y = 0; y < windowHeight; y += imgHeight) {
                for (int x = 0; x < windowWidth; x += imgWidth) {
                    int drawWidth = min(imgWidth, windowWidth - x);
                    int drawHeight = min(imgHeight, windowHeight - y);
                    if (!DrawImage(key, x, y, opacity, drawWidth, drawHeight)) {
                        return false;
                    }
                }
            }
            return true;
        }

        // 构造函数
        OtterImageRenderer(HWND hWnd, bool isLayered)
            : m_hWnd(hWnd), m_isLayered(isLayered) {

            // 初始化GDI+
            Gdiplus::GdiplusStartupInput gdiplusStartupInput;
            Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

            // 初始化双缓冲
            InitializeBackBuffer();
        }

        // 析构函数
        ~OtterImageRenderer() {
            // 清理双缓冲资源
            if (m_hBackBufferDC) {
                SelectObject(m_hBackBufferDC, m_hOldBitmap);
                DeleteObject(m_hBackBuffer);
                DeleteDC(m_hBackBufferDC);
            }

            // 清理GDI+
            Gdiplus::GdiplusShutdown(m_gdiplusToken);
        }

        // 初始化双缓冲
        void InitializeBackBuffer() {
            RECT rect;
            GetClientRect(m_hWnd, &rect);
            m_width = rect.right - rect.left;
            m_height = rect.bottom - rect.top;

            // 清理旧资源
            if (m_hBackBufferDC) {
                SelectObject(m_hBackBufferDC, m_hOldBitmap);
                DeleteObject(m_hBackBuffer);
                DeleteDC(m_hBackBufferDC);
            }

            HDC hdc = GetDC(m_hWnd);

            // 创建兼容DC和位图
            m_hBackBufferDC = CreateCompatibleDC(hdc);
            m_hBackBuffer = CreateCompatibleBitmap(hdc, m_width, m_height);
            m_hOldBitmap = (HBITMAP)SelectObject(m_hBackBufferDC, m_hBackBuffer);

            // 创建GDI+绘图表面
            m_pGraphics = std::make_unique<Gdiplus::Graphics>(m_hBackBufferDC);
            m_pGraphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            m_pGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

            ReleaseDC(m_hWnd, hdc);
        }

        // 更新窗口尺寸
        void UpdateSize() {
            RECT rect;
            GetClientRect(m_hWnd, &rect);
            int newWidth = rect.right - rect.left;
            int newHeight = rect.bottom - rect.top;

            if (newWidth != m_width || newHeight != m_height) {
                m_width = newWidth;
                m_height = newHeight;

                // 重新创建双缓冲
                SelectObject(m_hBackBufferDC, m_hOldBitmap);
                DeleteObject(m_hBackBuffer);
                DeleteDC(m_hBackBufferDC);

                InitializeBackBuffer();
            }
        }

        // 加载图片到缓存
        bool LoadImage(const std::wstring& key, const std::wstring& filePath) {
            if (m_imageCache.find(key) != m_imageCache.end()) {
                return true; // 已加载
            }

            if (!PathFileExists(filePath.c_str())) {
                OutputDebugStringW((L"图片文件不存在: " + filePath + L"\n").c_str());
                return false;
            }

            try {
                auto bitmap = std::make_unique<Gdiplus::Bitmap>(filePath.c_str());
                if (bitmap->GetLastStatus() != Gdiplus::Ok) {
                    OutputDebugStringW((L"图片加载失败: " + filePath + L"\n").c_str());
                    return false;
                }

                m_imageCache[key] = std::move(bitmap);
                return true;
            }
            catch (...) {
                OutputDebugStringW(L"图片加载异常\n");
                return false;
            }
        }

        // 从内存加载图片
        bool LoadImageFromMemory(const std::wstring& key, const BYTE* data, size_t size) {
            IStream* pStream = SHCreateMemStream(data, static_cast<UINT>(size));
            if (!pStream) return false;

            auto bitmap = std::make_unique<Gdiplus::Bitmap>(pStream);
            pStream->Release();

            if (bitmap->GetLastStatus() != Gdiplus::Ok) {
                return false;
            }

            m_imageCache[key] = std::move(bitmap);
            return true;
        }

        // 开始绘制帧
        void BeginFrame(Gdiplus::Color clearColor = Gdiplus::Color(0, 0, 0, 0)) {
            // 清空背景
            if (m_isLayered) {
                m_pGraphics->Clear(Gdiplus::Color(0, 0, 0, 0)); // 透明背景
            }
            else {
                m_pGraphics->Clear(clearColor);
            }
        }

        // 绘制图片
        bool DrawImage(const std::wstring& key,
            int x, int y,
            float opacity = 1.0f,
            int destWidth = -1,
            int destHeight = -1,
            int srcX = 0, int srcY = 0,
            int srcWidth = -1, int srcHeight = -1) {

            auto it = m_imageCache.find(key);
            if (it == m_imageCache.end()) return false;

            Gdiplus::Bitmap* pBitmap = it->second.get();

            // 计算源矩形
            if (srcWidth == -1) srcWidth = pBitmap->GetWidth();
            if (srcHeight == -1) srcHeight = pBitmap->GetHeight();

            // 计算目标矩形
            if (destWidth == -1) destWidth = srcWidth;
            if (destHeight == -1) destHeight = srcHeight;

            // 使用透明度
            if (opacity < 1.0f) {
                Gdiplus::ColorMatrix matrix = {
                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, opacity, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f
                };

                Gdiplus::ImageAttributes attributes;
                attributes.SetColorMatrix(&matrix, Gdiplus::ColorMatrixFlagsDefault,
                    Gdiplus::ColorAdjustTypeBitmap);

                Gdiplus::Rect destRect(x, y, destWidth, destHeight);
                m_pGraphics->DrawImage(
                    pBitmap, destRect,
                    srcX, srcY, srcWidth, srcHeight,
                    Gdiplus::UnitPixel, &attributes);
            }
            // 不使用透明度
            else {
                if (srcX == 0 && srcY == 0 &&
                    srcWidth == pBitmap->GetWidth() &&
                    srcHeight == pBitmap->GetHeight()) {
                    // 完整图片
                    m_pGraphics->DrawImage(pBitmap, x, y, destWidth, destHeight);
                }
                else {
                    // 部分图片
                    Gdiplus::Rect destRect(x, y, destWidth, destHeight);
                    m_pGraphics->DrawImage(
                        pBitmap, destRect,
                        srcX, srcY, srcWidth, srcHeight,
                        Gdiplus::UnitPixel);
                }
            }

            return true;
        }

        // 结束帧并呈现
        void EndFrame() {
            HDC hdc = GetDC(m_hWnd);

            if (m_isLayered) {
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                SIZE size = { m_width, m_height };
                POINT ptSrc = { 0, 0 };
                UpdateLayeredWindow(
                    m_hWnd, hdc, NULL, &size,
                    m_hBackBufferDC, &ptSrc, 0, &blend, ULW_ALPHA);
            }
            else {
                BitBlt(hdc, 0, 0, m_width, m_height, m_hBackBufferDC, 0, 0, SRCCOPY);
            }

            ReleaseDC(m_hWnd, hdc);
        }

        // 其他实用方法
        bool GetImageSize(const std::wstring& key, int& width, int& height) {
            auto it = m_imageCache.find(key);
            if (it == m_imageCache.end()) return false;

            width = it->second->GetWidth();
            height = it->second->GetHeight();
            return true;
        }

        void RemoveImage(const std::wstring& key) {
            m_imageCache.erase(key);
        }

        void ClearCache() {
            m_imageCache.clear();
        }

        //图片绘制
        // 绘制线条
        void DrawLine(int x1, int y1, int x2, int y2,
            Gdiplus::Color color, float width = 1.0f) {
            Gdiplus::Pen pen(color, width);
            m_pGraphics->DrawLine(&pen, x1, y1, x2, y2);
        }

        // 绘制矩形（空心）
        void DrawRectangle(int x, int y, int width, int height,
            Gdiplus::Color color, float penWidth = 1.0f) {
            Gdiplus::Pen pen(color, penWidth);
            m_pGraphics->DrawRectangle(&pen, x, y, width, height);
        }

        // 填充矩形（实心）
        void FillRectangle(int x, int y, int width, int height,
            Gdiplus::Color color) {
            Gdiplus::SolidBrush brush(color);
            m_pGraphics->FillRectangle(&brush, x, y, width, height);
        }

        // 绘制圆形（空心）
        void DrawCircle(int x, int y, int radius,
            Gdiplus::Color color, float penWidth = 1.0f) {
            Gdiplus::Pen pen(color, penWidth);
            m_pGraphics->DrawEllipse(&pen, x - radius, y - radius,
                radius * 2, radius * 2);
        }

        // 填充圆形（实心）
        void FillCircle(int x, int y, int radius, Gdiplus::Color color) {
            Gdiplus::SolidBrush brush(color);
            m_pGraphics->FillEllipse(&brush, x - radius, y - radius,
                radius * 2, radius * 2);
        }

        // 绘制多边形（空心）
        void DrawPolygon(const std::vector<Gdiplus::Point>& points,
            Gdiplus::Color color, float penWidth = 1.0f) {
            Gdiplus::Pen pen(color, penWidth);
            m_pGraphics->DrawPolygon(&pen, points.data(), (int)points.size());
        }

        // 填充多边形（实心）
        void FillPolygon(const std::vector<Gdiplus::Point>& points,
            Gdiplus::Color color) {
            Gdiplus::SolidBrush brush(color);
            m_pGraphics->FillPolygon(&brush, points.data(), (int)points.size());
        }

        // 绘制文本
        void DrawText(const std::wstring& text, int x, int y,
            const std::wstring& fontName = L"Arial",
            float fontSize = 12.0f,
            Gdiplus::Color color = Gdiplus::Color(255, 0, 0, 0)) {
            Gdiplus::Font font(fontName.c_str(), fontSize);
            Gdiplus::SolidBrush brush(color);
            m_pGraphics->DrawString(text.c_str(), -1, &font,
                Gdiplus::PointF((float)x, (float)y), &brush);
        }

        // 设置抗锯齿
        void SetAntiAlias(bool enabled) {
            if (enabled) {
                m_pGraphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                m_pGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
            }
            else {
                m_pGraphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
                m_pGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintSystemDefault);
            }
        }

        // 创建颜色渐变画刷
        Gdiplus::LinearGradientBrush* CreateGradientBrush(
            int x1, int y1, int x2, int y2,
            Gdiplus::Color startColor, Gdiplus::Color endColor) {
            return new Gdiplus::LinearGradientBrush(
                Gdiplus::Point(x1, y1),
                Gdiplus::Point(x2, y2),
                startColor,
                endColor);
        }

        // 使用渐变画刷填充矩形
        void FillRectangleWithBrush(int x, int y, int width, int height,
            Gdiplus::Brush* brush) {
            m_pGraphics->FillRectangle(brush, x, y, width, height);
        }

        // 使用渐变画刷填充圆形
        void FillCircleWithBrush(int x, int y, int radius,
            Gdiplus::Brush* brush) {
            m_pGraphics->FillEllipse(brush, x - radius, y - radius,
                radius * 2, radius * 2);
        }

        // 使用渐变画刷填充多边形
        void FillPolygonWithBrush(const std::vector<Gdiplus::Point>& points,
            Gdiplus::Brush* brush) {
            m_pGraphics->FillPolygon(brush, points.data(), (int)points.size());
        }


    };

    class UltimateImageRenderer {
    private:
        HWND m_hWnd;                    // 关联窗口句柄
        bool m_isActive;                // 渲染器是否激活
        bool m_isLayered;               // 是否为分层窗口
        bool m_isSizeDirty;             // 尺寸是否需要更新

        // 双缓冲资源
        HDC m_hBackBufferDC;
        HBITMAP m_hBackBuffer;
        HBITMAP m_hOldBitmap;
        int m_bufferWidth, m_bufferHeight;

        // GDI+资源
        ULONG_PTR m_gdiplusToken;
        std::unique_ptr<Gdiplus::Graphics> m_pGraphics;

        // 图片缓存
        std::unordered_map<std::wstring, std::unique_ptr<Gdiplus::Bitmap>> m_imageCache;

        // 私有方法
        void CleanupBackBuffer() {
            if (m_hBackBufferDC) {
                if (m_hOldBitmap) {
                    SelectObject(m_hBackBufferDC, m_hOldBitmap);
                    m_hOldBitmap = NULL;
                }
                if (m_hBackBuffer) {
                    DeleteObject(m_hBackBuffer);
                    m_hBackBuffer = NULL;
                }
                DeleteDC(m_hBackBufferDC);
                m_hBackBufferDC = NULL;
            }
            m_pGraphics.reset();
        }

        void SetupBackBuffer(int width, int height) {
            CleanupBackBuffer();

            HDC hdc = GetDC(m_hWnd);
            m_hBackBufferDC = CreateCompatibleDC(hdc);
            m_hBackBuffer = CreateCompatibleBitmap(hdc, width, height);
            m_hOldBitmap = (HBITMAP)SelectObject(m_hBackBufferDC, m_hBackBuffer);

            m_pGraphics = std::make_unique<Gdiplus::Graphics>(m_hBackBufferDC);
            m_pGraphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            m_pGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

            m_bufferWidth = width;
            m_bufferHeight = height;

            ReleaseDC(m_hWnd, hdc);
        }

        void CheckAndUpdateSize() {
            if (!m_isActive || !m_isSizeDirty) return;

            RECT rect;
            GetClientRect(m_hWnd, &rect);
            int newWidth = rect.right - rect.left;
            int newHeight = rect.bottom - rect.top;

            if (newWidth != m_bufferWidth || newHeight != m_bufferHeight) {
                SetupBackBuffer(newWidth, newHeight);
            }

            m_isSizeDirty = false;
        }

    public:
        // 构造函数
        UltimateImageRenderer(HWND hWnd, bool isLayered, bool startActive = true)
            : m_hWnd(hWnd), m_isLayered(isLayered), m_isActive(startActive) {

            // 初始化GDI+
            Gdiplus::GdiplusStartupInput gdiplusStartupInput;
            Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

            // 初始尺寸标记为需要更新
            m_isSizeDirty = true;
            m_bufferWidth = m_bufferHeight = 0;
            m_hBackBufferDC = NULL;
            m_hBackBuffer = NULL;
            m_hOldBitmap = NULL;
        }

        // 析构函数
        ~UltimateImageRenderer() {
            SetActive(false); // 确保停止所有操作
            CleanupBackBuffer();
            Gdiplus::GdiplusShutdown(m_gdiplusToken);
        }

        // 设置激活状态
        void SetActive(bool active) {
            if (m_isActive == active) return;

            m_isActive = active;
            if (active) {
                m_isSizeDirty = true; // 激活时强制更新尺寸
            }
            else {
                CleanupBackBuffer(); // 停用时释放资源
            }
        }

        // 获取激活状态
        bool IsActive() const { return m_isActive; }

        // 标记尺寸需要更新（在窗口大小改变时调用）
        void MarkSizeDirty() {
            if (m_isActive) m_isSizeDirty = true;
        }

        // 加载图片到缓存
        bool LoadImage(const std::wstring& key, const std::wstring& filePath) {
            if (!m_isActive) return false;

            try {
                auto bitmap = std::make_unique<Gdiplus::Bitmap>(filePath.c_str());
                if (bitmap->GetLastStatus() != Gdiplus::Ok) {
                    return false;
                }

                m_imageCache[key] = std::move(bitmap);
                return true;
            }
            catch (...) {
                return false;
            }
        }

        // 开始绘制帧
        bool BeginFrame(Gdiplus::Color clearColor = Gdiplus::Color(0, 0, 0, 0)) {
            if (!m_isActive) return false;

            CheckAndUpdateSize(); // 确保尺寸正确

            if (!m_pGraphics) return false;

            // 清空背景
            if (m_isLayered) {
                m_pGraphics->Clear(Gdiplus::Color(0, 0, 0, 0)); // 透明背景
            }
            else {
                m_pGraphics->Clear(clearColor);
            }

            return true;
        }

        // 绘制图片
        bool DrawImage(const std::wstring& key,
            int x, int y,
            float opacity = 1.0f,
            int destWidth = -1,
            int destHeight = -1,
            int srcX = 0, int srcY = 0,
            int srcWidth = -1, int srcHeight = -1) {

            if (!m_isActive || !m_pGraphics) return false;

            auto it = m_imageCache.find(key);
            if (it == m_imageCache.end()) return false;

            Gdiplus::Bitmap* pBitmap = it->second.get();

            // 计算源矩形
            if (srcWidth == -1) srcWidth = pBitmap->GetWidth();
            if (srcHeight == -1) srcHeight = pBitmap->GetHeight();

            // 计算目标矩形
            if (destWidth == -1) destWidth = srcWidth;
            if (destHeight == -1) destHeight = srcHeight;

            // 使用透明度
            if (opacity < 1.0f) {
                Gdiplus::ColorMatrix matrix = {
                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, opacity, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f
                };

                Gdiplus::ImageAttributes attributes;
                attributes.SetColorMatrix(&matrix);

                Gdiplus::Rect destRect(x, y, destWidth, destHeight);
                m_pGraphics->DrawImage(
                    pBitmap, destRect,
                    srcX, srcY, srcWidth, srcHeight,
                    Gdiplus::UnitPixel, &attributes);
            }
            // 不使用透明度
            else {
                Gdiplus::Rect destRect(x, y, destWidth, destHeight);
                m_pGraphics->DrawImage(
                    pBitmap, destRect,
                    srcX, srcY, srcWidth, srcHeight,
                    Gdiplus::UnitPixel);
            }

            return true;
        }

        // 结束帧并呈现
        void EndFrame() {
            if (!m_isActive || !m_hBackBufferDC || !m_pGraphics) return;

            HDC hdc = GetDC(m_hWnd);

            if (m_isLayered) {
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                SIZE size = { m_bufferWidth, m_bufferHeight };
                POINT ptSrc = { 0, 0 };
                UpdateLayeredWindow(
                    m_hWnd, hdc, NULL, &size,
                    m_hBackBufferDC, &ptSrc, 0, &blend, ULW_ALPHA);
            }
            else {
                // 使用双缓冲技术呈现
                BitBlt(hdc, 0, 0, m_bufferWidth, m_bufferHeight,
                    m_hBackBufferDC, 0, 0, SRCCOPY);
            }

            ReleaseDC(m_hWnd, hdc);
        }

        // 其他方法
        bool GetImageSize(const std::wstring& key, int& width, int& height) {
            auto it = m_imageCache.find(key);
            if (it == m_imageCache.end()) return false;

            width = it->second->GetWidth();
            height = it->second->GetHeight();
            return true;
        }

        void RemoveImage(const std::wstring& key) {
            m_imageCache.erase(key);
        }

        void ClearCache() {
            m_imageCache.clear();
        }
    };
    
}

#pragma comment(lib, "winmm.lib")
// 将子窗口嵌入父窗口并禁用交互

#include <stdexcept>

//路径转换
inline std::wstring ConvertRelativePathToAbsolute(const std::wstring& relativePath) {
    // 获取当前工作目录
    DWORD bufferSize = GetCurrentDirectoryW(0, nullptr);
    if (bufferSize == 0) {
        throw std::runtime_error("Failed to get current directory size");
    }

    std::vector<wchar_t> currentDir(bufferSize);
    if (GetCurrentDirectoryW(bufferSize, currentDir.data()) == 0) {
        throw std::runtime_error("Failed to get current directory");
    }

    // 组合路径
    std::wstring combinedPath = currentDir.data();
    if (!combinedPath.empty() && combinedPath.back() != L'\\') {
        combinedPath += L'\\';
    }
    combinedPath += relativePath;

    // 转换为绝对路径
    wchar_t absolutePath[MAX_PATH];
    if (!GetFullPathNameW(combinedPath.c_str(), MAX_PATH, absolutePath, nullptr)) {
        throw std::runtime_error("Failed to convert path to absolute");
    }

    return absolutePath;
}