# 水獭GUI (Otter_GUI) 开发框架文档
## 项目介绍
水獭GUI(Otter_GUI)是一个轻量级的Windows GUI开发框架，基于原生Win32 API和GDI+构建，提供了以下核心功能：\
窗口创建与管理\
2D图形绘制(GDI+)\
图像处理与渲染\
HTML内容渲染(通过WebView2)\
输入事件处理\
窗口样式控制\
框架采用面向对象设计，封装了Win32 API的复杂性，同时保留了原生Windows GUI的性能优势。

## 环境配置
### 环境配置
Windows 10或更高版本\
Visual Studio 2019或更高版本\
Windows 10 SDK (10.0.19041.0或更高)

### 项目设置步骤
#### 创建新项目
在Visual Studio中创建新的C++空项目\
配置为"Windows桌面向导应用程序"
#### 添加头文件
将Otter.h和OtterWindow.cpp添加到项目中
#### 配置项目属性
右键项目 → 属性 → 配置属性\
确保以下设置正确：

```cpp
目标平台版本: Windows 10 (10.0.19041.0或更高)
字符集: 使用Unicode字符集
```
#### 链接器 → 输入:
```cpp
附加依赖项:
gdiplus.lib
Shlwapi.lib
ole32.lib
user32.lib
d2d1.lib
dwrite.lib
windowscodecs.lib
```
#### WebView2运行时
需VS2022包管理安装WebView2和对应的“运行时”包

## 核心类说明
### OtterWin 窗口管理类
#### 功能概述
OtterWin是框架的核心类，负责窗口的创建、管理和消息循环处理。
##### 主要成员函数
<div class="markdown-table-wrapper"><table><thead><tr><th>函数</th><th>描述</th></tr></thead><tbody><tr><td><code>CreateWindowNew()</code></td><td>创建新窗口</td></tr><tr><td><code>ShowWindowR()</code></td><td>显示窗口</td></tr><tr><td><code>RB()</code></td><td>设置绘制回调</td></tr><tr><td><code>OnKeyDown()</code></td><td>设置按键按下回调</td></tr><tr><td><code>OnMouseMove()</code></td><td>设置鼠标移动回调</td></tr><tr><td><code>Run()</code></td><td>启动消息循环</td></tr></tbody></table></div>

#### 分层窗口注意事项
-分层窗口(透明窗口)功能尚未完全完善
-当前实现可能存在显示问题，特别是当窗口大小改变时
-使用SetLayeredMode(true)启用分层模式，必须在CreateWindowNew前调用

#### OtterPaintbrush 绘图工具类
##### 功能概述
提供基于GDI+的2D绘图功能，支持双缓冲技术。
###### 主要成员函数
<div class="markdown-table-wrapper"><table><thead><tr><th>函数</th><th>描述</th></tr></thead><tbody><tr><td><code>DrawText()</code></td><td>绘制文本</td></tr><tr><td><code>DrawRectangle()</code></td><td>绘制矩形</td></tr><tr><td><code>FillRectangle()</code></td><td>填充矩形</td></tr><tr><td><code>DrawCircle()</code></td><td>绘制圆形</td></tr><tr><td><code>DreamIMG()</code></td><td>绘制图像</td></tr><tr><td><code>Update()</code></td><td>更新到窗口</td></tr></tbody></table></div>

### OtterWindowSet 窗口样式设置类
#### 功能概述
提供窗口样式和行为的控制接口。
#### 主要成员函数
<div class="markdown-table-wrapper"><table><thead><tr><th>函数</th><th>描述</th></tr></thead><tbody><tr><td><code>SetTitleBar()</code></td><td>设置标题栏可见性</td></tr><tr><td><code>SetBorder()</code></td><td>设置边框可见性</td></tr><tr><td><code>SetResizable()</code></td><td>设置是否可调整大小</td></tr><tr><td><code>SetAlwaysOnTop()</code></td><td>设置窗口置顶</td></tr></tbody></table></div>
<h3>OtterImageRenderer 图像渲染类</h3>
<h4>功能概述</h4>
<p class="ds-markdown-paragraph">高性能图像渲染器，支持多种图像操作和效果。</p>
<h4>主要成员函数</h4>
<div class="markdown-table-wrapper"><table><thead><tr><th>函数</th><th>描述</th></tr></thead><tbody><tr><td><code>LoadImage()</code></td><td>加载图像到缓存</td></tr><tr><td><code>DrawImage()</code></td><td>绘制图像</td></tr><tr><td><code>DrawImageFitWindow()</code></td><td>自适应窗口绘制图像</td></tr><tr><td><code>BeginFrame()</code>/<code>EndFrame()</code></td><td>帧绘制控制</td></tr></tbody></table></div>
<h2>基础教程</h2>
<h3>1. 创建基本窗口</h3>
```cpp
#include "Otter.h"

int main() {
    OtterWindow::OtterWin window;
    
    // 创建窗口
    window.CreateWindowNew(
        L"MyWindowClass",    // 窗口类名
        0,                  // 扩展样式
        L"My First Window", // 窗口标题
        100, 100,           // 位置(X,Y)
        800, 600,           // 尺寸(W,H)
        NULL, NULL, NULL    // 父窗口,菜单,附加数据
    );
    
    // 设置绘制回调
    window.RB([&]() {
        OtterWindow::OtterPaintbrush brush(window.GetHWND(), false);
        brush.DrawText(L"Hello, Otter_GUI!", 50, 50, L"Arial", 36.0f);
        brush.Update();
    });
    
    // 显示窗口
    window.ShowWindowR();
    
    // 运行消息循环
    window.Run();
    
    return 0;
}
```
```
### 2. 处理输入事件
```cpp
// 在窗口创建后添加事件处理
window.OnMouseMove([](int x, int y) {
    std::wcout << L"鼠标位置: " << x << L", " << y << std::endl;
});

window.OnKeyDown(VK_SPACE, []() {
    std::wcout << L"空格键按下" << std::endl;
});

window.OnLeftDown([](int x, int y) {
    std::wcout << L"左键点击: " << x << L", " << y << std::endl;
});
```

<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></pre></div>
<h3>3. 使用图像渲染器</h3>

```cpp
OtterIMG::OtterImageRenderer renderer(window.GetHWND(), false);

// 加载图像
renderer.LoadImage(L"background", L"C:\\path\\to\\image.png");

// 在绘制回调中使用
window.RB([&]() {
    renderer.BeginFrame();
    renderer.DrawImageFitWindow(L"background");
    renderer.EndFrame();
});
```

# 注意
OtterPaintbrush与OtterImageRenderer渲染不兼容\
如需要图片上进行图形渲染\
可以使用OtterImageRenderer内置的图形渲染函数，与OtterPaintbrush一致






