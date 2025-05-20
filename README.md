### OtterGUI海獭软件框架开发文档(支持AI解答)
#### #开发配置
开发框架现于3.0Releas版本为最新版本,其框架文件包含以下:
______________
|头文件名称|详细功能|
|:----|:----|
|Otter.h|Otter框架主体文件，负责窗口搭建，图形绘制|
|otterTCP.h|负责网络通信功能，其主要支持-TCP-协议于-http-协议|
|OtterWebView2Renderer.h|基于WebView2控件，主要负责兼容HTML画面渲染|
|Otter_control.h|Otter图形的基础按钮控件类，提供基础基类|
|OtterWindow.cpp|负责Otter的声明函数构造|
|Json.h|Json文件读取|
|JsonEditor.h|Json文件修改|

---
这就是OtterGUI框架的基本头文件，其派生类为基于HWinfo的OtterInfo，但是此属于派生类不被包含\
开发环境配置
---
#### 初始化项目
1. ---开发环境IDE推荐：Visual Studio2022
2. ---跨平台支持:Windows7-11,**暂不支持其它平台**
3. ---语言与初始设置:C++ 17/无SDL检查
4. ---于C/C++->命令输入"/utf-8" 实现UTF兼容
5. ---导入框架文件,将对应的 **.h**头文件放入对应的文件夹项目，**.cpp同理**
6. ---**注意:**于项目->管理NuGet程序包->搜寻并安装:
		1. Microsoft.Web.WebView2
		2. Microsoft.Windows.ImplementationLibrary
		3. WebView2.Runtime.X64
三个包于支持Web控件渲染

#### 项目头编写
```cpp
#define _HAS_STD_BYTE 0  //处理框架冲突的宏错误
#include "otterTCP.h"  //如需要TCP请写到Otter.h前，否则会出现定义冲突
#include "Otter.h"	//如下即可不用考虑排序
#include "OtterWebView2Renderer.h"
```

#### 代码范例
<details>
  <summary>您的第一个OtterGUI项目窗口</summary>
  	
  ```cpp
    #define _HAS_STD_BYTE 0
	#include "Otter.h"
	
	int main(){
		OtterWindow::OtterWin Win;
		Win.SetLayeredMode(false); //必须false，现在分层有问题
		Win.CreateWindowNew(L"CMCK", 0, L"Otter", 200, 400, 900, 700, NULL, NULL, NULL);
		Win.ShowWindowR();
		Win.Run();
	}
  ```
  如此便可以运行此项目，此时你会看到位于屏幕坐标(200,400)px出现宽高(900,700)px的界面\
  它的标题为**Otter**
  
</details>
<br></br>

---


## 代码Otter.h->OtterWin类详解
**Otter.h**类的应用与详解
1. OtterWindow::OtterWin窗口创建与管理类
	- 窗口创建函数->**CreateWindowNew**
		- **功能**：创建窗口并初始化基础属性
		- **参数列表**: 窗口类名,窗口样式(默认0),窗口标题,X,Y,W,H,父句柄,菜单句柄,数据包

**CreateWindowNew的调用方法**

```cpp
OtterWindow::OtterWin Win; //创建Otter窗口类
Win.CreateWindowNew(L"CMCK", 0, L"Otter", 200, 400, 900, 700, NULL, NULL, NULL); //填入参数构造窗口
```
<br></br>

----
**ShowWindowR**\
**功能**:显示窗口，无详解，因创建窗口后默认不显示，需要使用此显示窗口\
调用

```cpp
Win.ShowWindowR();
```
<br></br>

----
**Run**\
**功能**:启动消息循环直到窗口关闭或者程序结束\
调用

```cpp
Win.Run();
```
<br></br>

----
### 回调说明
**解释**\
***回调函数是一个窗口的核心，也是窗口消息循环机制的核心***\
_回调是指窗口于每帧所接收消息后所做出的指定回应_

#### RB():绘制回调
绘制回调是于窗口每一帧绘制窗口，用来绘制窗内部样式，如文字图形\
**注意**:此函数为唯一的，不可重复调用\
具体绘制方法为**Lambda表达式**绘制\
调用方法:
```cpp
Win.RB([&](){
/*此内为绘制内容*/
});
```
<br></br>
---
#### OnKeyDown/OnKeyUp鼠标下按/抬起回调
**解释**\
于指定按键位于窗口内的抬起或者按下做出指定变化的回调函数\
**Lambda表达式**回应回调\
调用方法

```cpp
Win.OnKeyDown('w',[&](){
	/*
		当按下w时进行对应的逻辑回调
		注意:w与W等价，不区分大小写
	*/
});
```
<br></br>
---

#### OnMouseMove鼠标移动时回调
**解释**\
此回调是指鼠标在窗口中移动所造成的回调\
**内部需参**:X鼠标以窗口为基点的X坐标,Y鼠标以窗口为基点的Y坐标\
**Lambda表达式**回应回调\
调用方法
```cpp
	Win.OnMouseMove([&](int x,int y){
		std::cout << x << "\t" << y << std::endl;
		/*
			此为范例，指移动时候显示坐标
			x与y会自动赋值
		*/
	});
```
<br></br>

---
#### IsKeyLongPress判断按键按下回调
**解释**\
bool类型，判断按键长按到特定ms时间\
调用方法
```cpp
	Win.IsKeyLongPress('w',500);
	/*如果长按w>=500s将会返回true值*/
```
<br></br>

---
#### OnLeftDown/OnLeftUp：左键按下/释放回调
#### OnRightDown/OnRightUp：右键按下/释放回调
**解释**\
此四个回调函数基本用法一致\
**内部需参**:X鼠标以窗口为基点的X坐标,Y鼠标以窗口为基点的Y坐标\
**Lambda表达式**回应回调\
调用方法
```cpp
	Win.OnLeftDown([&](int x,int y){
		std::cout << "左键鼠标按下" << std::endl;
	});
```
<br></br>

---

#### OnMouseWheel鼠标滚轮回调 
**解释**\
此为获取鼠标滚轮值，向上则是1，向下则是-1，默认0，int值
**Lambda表达式**回应回调\
调用方法
```cpp
	Win.OnMouseWheel([&](int len){
		std::cout << len << std::endl;
		/*显示滚轮值*/
	});
```

<br></br>
---
#### OnMouseHover窗口鼠标捕获
**解释**\
此为判断鼠标是否为于窗口，如果位于窗口则进行对应回调\
**内部需参**:bool Lends 用来判断鼠标是否位于窗口\
**Lambda表达式**回应回调\
调用方法
```cpp
	Win.OnMouseHover([&](bool Lends){
		if(Lends)std::cout << "鼠标进入窗口";
		else std::cout << "鼠标离开窗口";
	});
```
<br></br>
---

#### 窗口属性与状态
1. **GetHWND()**
 - 功能：返回窗口句柄 HWND
 - 返回值：HWND 或 NULL（未创建时）

2. **SetLayeredMode(bool)**

 - 功能：启用/禁用分层窗口模式（需在 CreateWindowNew 前调用)/不推荐使用，现在分层还有BUG

 - 参数：enabled（true 启用透明效果）。

3. **SetWindowAlpha(int)**

 - 功能：设置分层窗口的透明度（0-255）。

 - 参数：alpha（透明度值）。

4. **IsLayeredWindow()**

 - 功能：检查是否为分层窗口。

 - 返回值：bool。

5. **GetWindPos()**

 - 功能：获取窗口位置和尺寸（RECT 结构）。

6. **IsMouseFocused/IsKeyFocused/IsActiveWindow**

 - 功能：检查窗口是否具有鼠标/键盘焦点或为活动窗口。

7. **MouseDownMoveSet(HWND)**
 - 功能:设置窗口是否可以拖动
 - 需要于鼠标移动回调中调用

<br></br>
---

### OtterPaintbrush简单绘制类调用
**解释**OtterPaintbrush类是基于GDI+的绘制类，用来绘制无图片的简单绘画类\
初始化创建
```cpp
	Win.RB([&](){
		OtterWindow::OtterPaintbrush brush(Win.GetHWND(),Win.IsLayeredWindow());//获取对应的窗口句柄与分层效果
	});
```
#### 方法构造与调用
1. void **DrawText** 于指定文字绘制 字符为L“宽字符类型“
 - 具体参数
  	- const std::wstring& text 绘制的文本
	- int x X坐标
	- int y y坐标
	- const std::wstring& fontName = L"Arial" 字体，默认为Arial字体
	- float fontSize = 24.0f 字符大小，默认24.0f
 	- Gdiplus::Color color = Gdiplus::Color(255, 255, 255, 255) 颜色，默认白色不透明
2. void **DrawRectangle** 绘制矩形
 - 参数列表
 	- int x, int y, int width, int height,Gdiplus::Color color, float penWidth = 1.0f
3. void **FillRectangle** 填充矩形
 - 参数列表
 	- int x, int y, int width, int height, Gdiplus::Color color

4. void **DrawCircle** 绘制圆形
 - 参数列表
 	- int x, int y, int radius,Gdiplus::Color color, float penWidth = 1.0f

5. void**FillCircle**  填充圆形
 - 参数列表
 	- int x, int y, int radius, Gdiplus::Color color
6. void **DrawPolygon** 绘制多边形
 - 参数列表
 	- const std::vector<Gdiplus::Point>& points,Gdiplus::Color color, float penWidth = 1.0f
7. void **FillPolygon** 填充多边形
 - 参数列表
 	- const std::vector<Gdiplus::Point>& points, Gdiplus::Color color
	
8. void **DrawLine** 绘制线
 - 参数列表
 	- int x1, int y1, int x2, int y2,Gdiplus::Color color, float penWidth = 1.0f

9. bool **SetAntiAlias(Bool)** 设置抗锯齿
	- true为开启，false关闭
	- 默认开启

10. void **Update()** 更新窗口
	- **注意**:每次绘制完成必须调用此函数

#### 范例
绘制正方形
```cpp
	
	Win.RB([&](){
		OtterWindow::OtterPaintbrush brush(Win.GetHWND(),Win.IsLayeredWindow());
		brush.FillRectangle(0,0,500,500,Gdiplus::Color(255,255,0,0,0)); //填充一个红色的矩形位于0,0
		brush.Update(); //将绘制内容应用到窗口
	});

```
<br></br>
---


### OtterWindowSet 窗口样式设置类
**详解**:\
此为设置窗口样式的基本类\
创建类
```cpp
	OtterWindow::OtterWindowSet(Win.GetHWND());
	/*
		获取之后即可设置
	*/
```
#### 方法构造与调用
1. void **SetTitleBar(bool enabled)** 启用/禁用窗口标题栏
2. void **SetSystemMenu(bool enabled)** 启用/禁用系统菜单（窗口左上角菜单）
3. void **SetResizable(bool enabled)** 启用/禁用窗口大小调整功能
4. void **SetMaximizeButton(bool enabled)** 启用/禁用最大化按钮
5. void **SetMinimizeButton(bool enabled)** 启用/禁用最小化按钮
6. void **SetMovable(bool enabled)** 启用/禁用窗口移动功能
7. void **SetAlwaysOnTop(bool enabled)** 设置窗口是否置顶
8. void **SetClickThrough(bool enabled)** 设置窗口是否可点击穿透
9. void **RestoreOriginalStyle()** 恢复窗口到原始样式
10. void **GetCurrentStyle() const** 获取当前窗口样式
11. void **GetCurrentExStyle() const** 获取当前窗口扩展样式
12. void **SetRoundedCorners(bool enabled)** 设置窗口圆角效果（仅限 Windows 11）

<br></br>
---
### OtterImageRenderer高性能渲染器
**详解**\
一个高性能的 Windows 图像渲染类，基于 GDI+ 实现，支持双缓冲、透明窗口和多种绘图操作,图片绘制\
***与OtterPaintbrush不兼容***\
**初始化图片与创建**
```cpp
	OtterIMG::OtterImageRenderer IMG(Win.GetHWND(),Win.IsLayeredWindow());//初始化构建类
	IMG.LoadImage(L"A",L".src/a.jpg"); //从文件读取图片存储键名为A
	Win.RB([&](){
		IMG.BeginFrame(Gdiplus::Color(255, 255, 255, 255)); //开始渲染，默认背景为白色
		IMG.DrawImageTileWindow(L"A",1.0f);//平铺图片A，透明为1.0f(不透明)
		IMG.EndFrame();//结束渲染:双缓存机制
	});
```
#### 公共函数调用
1. **bool LoadImage(const std::wstring& key, const std::wstring& filePath)** 从文件加载图像到缓存
2. **bool LoadImageFromMemory(const std::wstring& key, const BYTE* data, size_t size)** 从内存加载图像到缓存
3. **void RemoveImage(const std::wstring& key)** 从缓存中移除指定图像
4. **void ClearCache()** void ClearCache()
5. **bool GetImageSize(const std::wstring& key, int& width, int& height)** 获取缓存中图像的尺寸
6. **void BeginFrame(Gdiplus::Color clearColor = Gdiplus::Color(0, 0, 0, 0))** 开始新的一帧绘制
7. **void EndFrame()** 结束绘制
8. **void UpdateSize()** 更新窗口尺寸并重新创建双缓冲
9. **bool DrawImage** 绘制指定图像
 - 内部参数
			  const std::wstring& key, int x, int y, 
              float opacity = 1.0f, int destWidth = -1, 
              int destHeight = -1, int srcX = 0, 
              int srcY = 0, int srcWidth = -1, 
              int srcHeight = -1

10. **bool DrawImageFitWindow** 绘制图像并适应窗口大小
 - 内部参数
 			const std::wstring& key, 
             float opacity = 1.0f, 
             bool keepAspectRatio = true
			 
11. **bool DrawImageTileWindow** 平铺图片
 - 内部参数
 			const std::wstring& key, 
            float opacity = 1.0f

12. **void DrawLine** 绘制线条
 - 内部参数
 			int x1, int y1, int x2, int y2,
             Gdiplus::Color color, float width = 1.0f

13. **void DrawRectangle** 绘制空心矩形
 - 内部参数
 			int x, int y, int width, int height,
                  Gdiplus::Color color, float penWidth = 1.0f
				  
14. **void FillRectangle** 绘制实心矩形
 - 内部参数
 			int x, int y, int width, int height,
                  Gdiplus::Color color

15. **void DrawCircle** 绘制空心圆形
 - 内部参数 
 			int x, int y, int radius,
               Gdiplus::Color color, float penWidth = 1.0f
			   
16. **void FillCircle** 绘制实心圆形
 - 内部参数
 			int x, int y, int radius, Gdiplus::Color color

17. **void DrawPolygon** 绘制空心多边形
 - 内部参数
 			const std::vector<Gdiplus::Point>& points,
                Gdiplus::Color color, float penWidth = 1.0f
				
18. **void FillPolygon** 绘制实心多边形
 - 内部参数
 			const std::vector<Gdiplus::Point>& points,
                Gdiplus::Color color
				
19. *void DrawText** 绘制文本
 - 内部参数
 			const std::wstring& text, int x, int y,
             const std::wstring& fontName = L"Arial",
             float fontSize = 12.0f,
             Gdiplus::Color color = Gdiplus::Color(255, 0, 0, 0)

20. **void SetAntiAlias(bool enabled)** 设置抗锯齿,true为开启，默认开启

21. **Gdiplus::LinearGradientBrush* CreateGradientBrush** 创建线性渐变画刷
 - 内部参数
 			int x1, int y1, int x2, int y2,
   			 Gdiplus::Color startColor, Gdiplus::Color endColor
			
22. **void FillRectangleWithBrush** 使用自定义画刷填充矩形
 - 内部参数
 			int x, int y, int width, int height,
                          Gdiplus::Brush* brush

23. **void FillCircleWithBrush** 使用自定义画刷填充圆形
 - 内部参数 
 			int x, int y, int radius,
                       Gdiplus::Brush* brush

24. **void FillPolygonWithBrush** 使用自定义画刷填充多边形
 - 内部参数
 			const std::vector<Gdiplus::Point>& points,
                        Gdiplus::Brush* brush


<br></br>
---
# ---------------OtterWebView2Renderer.h 调用详解-----------------
## 详解:
 **此为Web渲染控件，因为前面本身不支持视频绘制，此控件将负责视频渲染和更复杂的渲染机制**
 初始化:***OtterWebView2Renderer***类
 ```cpp
 	RECT rect{0,0,100,100}; //rect坐标结构
 	OtterWindow::OtterWebView2Renderer Web(Win.GetHWND(),rect); //创建Web控件，获取窗口句柄和对应的坐标结构
	//初始化Web
	Web.SetInitializedCallback([&](){
		Web.LoadUrl(L"https://github.com"); //打开对应的网页
	});
	
	//渲染回调
	Win.RB([&](){
		
	});
	
	Web.
 ```
#### 方法构造与调用:
1. **OtterWebView2Renderer(HWND hWnd, const RECT& rect)** 构造函数
2. **bool Initialize()** 初始化 WebView2 环境
3. **bool IsInitialized() const** 检查是否已完成初始化
4. **void SetInitializedCallback(std::function<void()> callback)** 设置初始化完成回调函数
 - 参数 ***callback***: 初始化完成后调用的函数

5. **void LoadHtml(const std::wstring& htmlContent)** 加载 HTML 字符串内容
 - 参数 htmlContent: 要加载的 HTML 内容
 
6. **void LoadUrl(const std::wstring& url)** 加载指定 URL
 - 参数 url: 要加载的网页地址
 
7. **void SetVisible(bool visible)** 设置控件可见性
  - 参数 visible: true 显示，false 隐藏

8. **bool IsVisible() const** 获取当前可见状态 
 - 返回: 可见返回true，否则返回false

9. **void SetRect(const RECT& rect)** 设置渲染区域
 - 参数 rect: 新的渲染区域
  
10. **RECT GetRect() const** 获取当前渲染区域
 - 返回: 当前渲染区域RECT结构
 
11. **void SetZOrder(HWND hWndInsertAfter)** 设置 Z 顺序
 - 参数 hWndInsertAfter: 指定窗口句柄(如HWND_TOP等)
 
12. **void Render(OtterIMG::OtterImageRenderer& imageRenderer)** 渲染兼容
 - 参数 imageRenderer: 图像渲染器引用
 
 <br></br>
 ---
 # ---------------------------OtterTCP.h详解-----------------------------
 **TCP为基础的网络协议，为此此头现支持TCP/HTTP传输协议实现前后的实时控制**\
 一个基于 **Windows Sockets** 的 TCP 端口监控和通信类，支持 HTTP 请求处理和消息记录功能。

类定义
class PortMonitor
公共类型
回调函数类型
```cpp
using MessageHandler = std::function<void(const std::string&)>;
using ParamHandler = std::function<std::string(const std::string&)>;
```
消息记录结构体
```cpp
struct MessageRecord {
    std::string content;                     // 消息内容
    bool isOutgoing;                         // 是否为发送的消息
    std::chrono::system_clock::time_point timestamp; // 时间戳
};
```
### 构造函数与析构函数
#### 构造函数
```cpp
PortMonitor()
```
-初始化 Winsock 库\
---抛出异常: 如果 WSAStartup 失败

#### 析构函数
```cpp
~PortMonitor()
```
-停止监控\
---清理 Winsock 资源

### 公共方法
#### 监控控制
startMonitoring
```cpp
bool startMonitoring(int port)
```
-开始监控指定端口\
--参数 port: 要监控的端口号\
---返回: 成功返回true，失败返回false

stopMonitoring
```cpp
void stopMonitoring()
```
-停止端口监控

#### 消息发送
sendMessage
```cpp
static bool sendMessage(const std::string& ip, int port, const std::string& message)
```
-静态方法，向指定IP和端口发送消息\
--参数 ip: 目标IP地址\
---参数 port: 目标端口\
----参数 message: 要发送的消息内容\
-----返回: 成功返回true，失败返回false

#### 参数处理
setParamHandler
```cpp
void setParamHandler(const std::string& paramName, ParamHandler handler)
```
-设置动态参数处理器\
--参数 paramName: 参数名称\
---参数 handler: 处理函数

#### 消息历史记录
getMessageHistory
```cpp
std::vector<MessageRecord> getMessageHistory() const
```
-获取所有消息记录\
--返回: 消息记录向量

getLastReceivedMessage
```cpp
std::string getLastReceivedMessage() const
```
-获取最近接收的消息\
--返回: 消息内容字符串

getLastSentMessage
```cpp
std::string getLastSentMessage() const
```
-获取最近发送的消息\
--返回: 消息内容字符串

clearMessageHistory
```cpp
void clearMessageHistory()
```
-清空消息历史记录

### OtterLamae 命名空间
extractValuePair
```cpp
std::pair<std::string, double> extractValuePair(const std::string& input)
```
-从输入字符串中提取标识符和数值对\
--参数 input: 输入字符串(格式: "identifier value")\
---返回: 包含标识符和数值的pair\
----抛出异常: 如果输入格式无效\

### 使用示例
```cpp
// 创建监控器
PortMonitor monitor;

// 设置参数处理器
monitor.setParamHandler("temp", [](const std::string& value) {
    return "Temperature: " + value + "°C";
});

// 开始监控8080端口
if (monitor.startMonitoring(8080)) {
    std::cout << "Monitoring started on port 8080" << std::endl;
}

// 发送消息
if (PortMonitor::sendMessage("127.0.0.1", 8080, "GET /?temp=25 HTTP/1.1")) {
    std::cout << "Message sent" << std::endl;
}

// 获取消息历史
auto history = monitor.getMessageHistory();
for (const auto& record : history) {
    std::cout << (record.isOutgoing ? "Sent: " : "Received: ") 
              << record.content << std::endl;
}

// OtterLamae 使用示例
try {
    auto result = OtterLamae::extractValuePair("A 12");
    std::cout << "Identifier: " << result.first << ", Value: " << result.second << std::endl;
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```
### 注意事项
使用前需要确保 Winsock 已正确初始化

消息历史记录默认限制为1000条，超过会自动删除最旧的记录

HTTP 处理目前仅支持 GET 请求

线程安全通过互斥锁保证

使用单例模式实现静态方法访问实例成员

### Json请前往 https://github.com/Yanghaopor/Simple_Json_Cpp 查看原理与教程
