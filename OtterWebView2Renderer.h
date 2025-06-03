// OtterWebView2Renderer.h
#pragma once
#include "Otter.h"
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <wrl/client.h>
#include <functional>
#include <algorithm>
namespace OtterWindow {

    class OtterWebView2Renderer {
    private:
        HWND m_hWnd;                        // 父窗口句柄
        RECT m_rect;                        // 渲染区域
        bool m_isVisible = true;            // 是否可见

        // WebView2相关
        Microsoft::WRL::ComPtr<ICoreWebView2> m_webView;
        Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_webViewController;
        Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_webViewEnvironment;
        bool m_isInitialized = false;

        // 鼠标穿透支持
        bool m_mouseTransparent = false;

        // 环境创建回调
        HRESULT OnEnvironmentCreated(HRESULT result, ICoreWebView2Environment* env) {
            if (result != S_OK || !env) return result;

            env->CreateCoreWebView2Controller(
                m_hWnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    this, &OtterWebView2Renderer::OnControllerCreated).Get());

            return S_OK;
        }

        // 更新WebView2控件的位置和大小
        void UpdateWebViewBounds() {
            if (m_webViewController) {
                m_webViewController->put_Bounds(m_rect);
            }
        }

        // 更新鼠标穿透状态
        void UpdateMouseTransparency() {
            if (!m_webViewController) return;

            // 获取当前窗口句柄
            HWND hWndWebView = nullptr;
            m_webViewController->get_ParentWindow(&hWndWebView);
            if (!hWndWebView) return;

            // 设置鼠标穿透
            LONG_PTR exStyle = GetWindowLongPtr(hWndWebView, GWL_EXSTYLE);
            if (m_mouseTransparent) {
                exStyle |= WS_EX_TRANSPARENT;
            }
            else {
                exStyle &= ~WS_EX_TRANSPARENT;
            }
            SetWindowLongPtr(hWndWebView, GWL_EXSTYLE, exStyle);
            SetWindowPos(hWndWebView, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }

        // 初始化回调
        std::function<void()> m_initializedCallback;

    public:
        void OverrideWindowOpen() {
            if (m_webView) {
                std::wstring script = L"window.open = function(url) {"
                    L"  window.location.href = url;"
                    L"  return window;"
                    L"};";

                m_webView->ExecuteScript(script.c_str(), nullptr);
            }
        }

        OtterWebView2Renderer(HWND hWnd, const RECT& rect)
            : m_hWnd(hWnd), m_rect(rect) {
            Initialize();
        }

        ~OtterWebView2Renderer() {
            if (m_webViewController) {
                m_webViewController->Close();
            }
        }

        void SetZOrder(HWND hWndInsertAfter) {
            if (m_webViewController) {
                HWND hWndWebView = nullptr;
                m_webViewController->get_ParentWindow(&hWndWebView);
                if (hWndWebView) {
                    SetWindowPos(hWndWebView, hWndInsertAfter, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }
        }

        // 设置鼠标穿透
        void SetMouseTransparent(bool transparent) {
            m_mouseTransparent = transparent;
            if (m_webViewController) {
                HWND hWndWebView = nullptr;
                m_webViewController->get_ParentWindow(&hWndWebView);
                if (hWndWebView) {
                    LONG_PTR exStyle = GetWindowLongPtr(hWndWebView, GWL_EXSTYLE);
                    if (transparent) {
                        exStyle |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
                    }
                    else {
                        exStyle &= ~WS_EX_TRANSPARENT;
                    }
                    SetWindowLongPtr(hWndWebView, GWL_EXSTYLE, exStyle);
                    SetWindowPos(hWndWebView, nullptr, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                }
            }
        }

        // 是否鼠标穿透
        bool IsMouseTransparent() const {
            return m_mouseTransparent;
        }

        // 控制器创建回调
        HRESULT OnControllerCreated(HRESULT result, ICoreWebView2Controller* controller) {
            if (result != S_OK || !controller) return result;

            m_webViewController = controller;
            m_webViewController->get_CoreWebView2(&m_webView);

            UpdateWebViewBounds();
            UpdateMouseTransparency();

            Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
            m_webView->get_Settings(&settings);
            if (settings) {
                settings->put_AreDevToolsEnabled(TRUE);
            }

            // 处理新窗口请求
            m_webView->add_NewWindowRequested(
                Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                    [this](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                        LPWSTR uri;
                        args->get_Uri(&uri);
                        m_webView->Navigate(uri);
                        args->put_Handled(TRUE);
                        CoTaskMemFree(uri);
                        return S_OK;
                    }).Get(), nullptr);

            // 强制链接在当前窗口打开
            ForceLinksToOpenInCurrentWindow();

            // 重写window.open
            OverrideWindowOpen();

            m_isInitialized = true;

            if (m_initializedCallback) {
                m_initializedCallback();
            }

            return S_OK;
        }

        // 初始化WebView2环境
        bool Initialize() {
            return SUCCEEDED(CreateCoreWebView2EnvironmentWithOptions(
                nullptr,    // 使用默认浏览器数据目录
                nullptr,    // 无用户数据文件夹
                nullptr,    // 无环境选项
                Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                    this, &OtterWebView2Renderer::OnEnvironmentCreated).Get()));
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

        // 设置渲染区域
        void SetRect(const RECT& rect) {
            m_rect = rect;
            UpdateWebViewBounds();
        }

        // 获取渲染区域
        RECT GetRect() const {
            return m_rect;
        }

        // 显示/隐藏
        void SetVisible(bool visible) {
            m_isVisible = visible;
            if (m_webViewController) {
                m_webViewController->put_IsVisible(visible ? TRUE : FALSE);
            }
        }

        // 是否可见
        bool IsVisible() const {
            return m_isVisible;
        }

        // 是否初始化完成
        bool IsInitialized() const {
            return m_isInitialized;
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

        // 设置初始化回调
        void SetInitializedCallback(std::function<void()> callback) {
            m_initializedCallback = callback;
            if (m_isInitialized && m_initializedCallback) {
                m_initializedCallback();
            }
        }

        // 渲染方法 - 与OtterImageRenderer兼容
        void Render(OtterIMG::OtterImageRenderer& imageRenderer) {
            if (!m_isVisible || !m_isInitialized) return;

            // 这里可以添加WebView2的覆盖层绘制
            imageRenderer.DrawRectangle(
                m_rect.left, m_rect.top,
                m_rect.right - m_rect.left,
                m_rect.bottom - m_rect.top,
                Gdiplus::Color(255, 0, 0, 0),
                1.0f);
        }

        //注入js
        void ForceLinksToOpenInCurrentWindow() {
            if (m_webView) {
                std::wstring script = L"(function() {"
                    L"  function forceLinks() {"
                    L"    var links = document.getElementsByTagName('a');"
                    L"    for (var i = 0; i < links.length; i++) {"
                    L"      links[i].setAttribute('target', '_self');"
                    L"    }"
                    L"  }"
                    L"  forceLinks();"  // 立即执行一次
                    L"  new MutationObserver(forceLinks).observe("
                    L"    document.body, {childList: true, subtree: true}"
                    L"  );"  // 监控DOM变化并持续执行
                    L"})();";

                m_webView->ExecuteScript(
                    script.c_str(),
                    Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                        [](HRESULT errorCode, LPCWSTR result) -> HRESULT {
                            if (errorCode != S_OK) {
                                // 处理错误
                            }
                            return S_OK;
                        }).Get());
            }
        }

    };

} // namespace OtterWindow