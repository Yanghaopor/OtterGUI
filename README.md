# OtterGUI 使用文档

## 概述
OtterGUI 是一个基于 Win32 API 和 GDI+ 的轻量级 GUI 框架，提供了简化的窗口创建、绘图和事件处理接口，适合需要高性能自定义 UI 的场景。

## 基础使用

### 1. 创建窗口
```cpp
OtterWindow::OtterWin window;
window.CreateWindowNew(L"MyWindowClass", 0, L"My Window", 100, 100, 800, 600, NULL, NULL, NULL);
window.ShowWindowR();
window.Run();
```
#### OtterWindow为水獭GUI的win类，用于创建与初始窗口创建已经基础画笔

#### OtterWindow::OtterWin窗口类
此类方法包括
#### window.CreateWindowNew
负责创建窗口，具体为
```cpp
void OtterWindow::OtterWin::CreateWindowNew(std::wstring NAME, int A, std::wstring Lname,int X, int Y, int W, int H,HWND AW, HMENU men, LPVOID OID)
（窗口类名，窗口扩展样式，标题文本，X,Y,W,H,父窗口,菜单句柄,应用程序数据）
```
### judgment
```cpp
负责查看窗口是否正常创建

bool judgment()

布尔类型无需传入参数
```
### ShowWindowR
```cpp
void ShowWindowR()

无需传入参数，是于创建完成窗口后进行窗口显示
```
### GetHWND
```cpp
HWND GetHWND()
用于接收句柄
```
### Run()
```cpp
void Run()
创建完成后实现消息循环达到消息目的
```


