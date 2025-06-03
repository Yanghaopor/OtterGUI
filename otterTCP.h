#pragma once
#ifndef PORT_MONITOR_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <functional>
#include <atomic>
#include <map>
#include <sstream>
#include <algorithm>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <memory>
#include <unordered_set>

#pragma comment(lib, "ws2_32.lib")

class PortMonitor {
public:
    //MessageHandler
    using MessageHandler = std::function<std::string(const std::string&)>;
    // 设置消息处理器（新增）
    void setMessageHandler(MessageHandler handler) {
        std::lock_guard<std::mutex> lock(m_handlersMutex);
        m_messageHandler = handler;
    }

    using ParamHandler = std::function<std::string(const std::string&)>;

    // 消息记录结构体
    struct MessageRecord {
        std::string content;       // 消息内容
        bool isOutgoing;           // 是否为发送的消息
        SOCKET socket;             // 关联的套接字
        std::chrono::system_clock::time_point timestamp; // 时间戳

        MessageRecord(std::string c, bool io, SOCKET s)
            : content(std::move(c)), isOutgoing(io), socket(s),
            timestamp(std::chrono::system_clock::now()) {}
    };

    // 连接信息结构体 - 针对 C2280 错误修复
    struct ConnectionInfo {
        SOCKET socket = INVALID_SOCKET;   // 套接字
        sockaddr_in address{};             // 客户端地址
        std::thread thread;                // 处理线程
        std::atomic<bool> active{ false };   // 是否活跃
        std::atomic<bool> shouldClose{ false }; // 关闭标志

        ConnectionInfo() = default;

        // 显式删除拷贝构造函数和拷贝赋值运算符
        ConnectionInfo(const ConnectionInfo&) = delete;
        ConnectionInfo& operator=(const ConnectionInfo&) = delete;

        // 移动构造函数
        ConnectionInfo(ConnectionInfo&& other) noexcept
            : socket(other.socket), address(other.address),
            thread(std::move(other.thread)),
            active(other.active.load()),
            shouldClose(other.shouldClose.load()) {
            other.socket = INVALID_SOCKET;
            other.active = false;
            other.shouldClose = true;
        }

        // 移动赋值运算符
        ConnectionInfo& operator=(ConnectionInfo&& other) noexcept {
            if (this != &other) {
                closeSocket();
                socket = other.socket;
                other.socket = INVALID_SOCKET;
                address = other.address;
                thread = std::move(other.thread);
                active = other.active.load();
                shouldClose = other.shouldClose.load();
                other.active = false;
                other.shouldClose = true;
            }
            return *this;
        }

        ~ConnectionInfo() {
            closeSocket();
        }

        void closeSocket() {
            if (socket != INVALID_SOCKET) {
                shutdown(socket, SD_BOTH);
                closesocket(socket);
                socket = INVALID_SOCKET;
            }
        }
    };

    // 构造函数
    PortMonitor() : m_listening(false), m_serverSocket(INVALID_SOCKET) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    // 析构函数
    ~PortMonitor() {
        stopMonitoring();
        WSACleanup();
    }

    // 设置动态参数处理器
    void setParamHandler(const std::string& paramName, ParamHandler handler) {
        std::lock_guard<std::mutex> lock(m_handlersMutex);
        m_paramHandlers[paramName] = handler;
    }

    // 开始监控端口
    bool startMonitoring(int port) {
        if (m_listening) {
            return false;
        }

        // 创建监听socket
        m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_serverSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        // 设置socket选项（允许地址重用）
        int opt = 1;
        if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
            std::cerr << "Set socket option failed: " << WSAGetLastError() << std::endl;
            closesocket(m_serverSocket);
            m_serverSocket = INVALID_SOCKET;
            return false;
        }

        // 绑定地址和端口
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(m_serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Port binding failed: " << WSAGetLastError() << std::endl;
            closesocket(m_serverSocket);
            m_serverSocket = INVALID_SOCKET;
            return false;
        }

        // 开始监听
        if (listen(m_serverSocket, 256) == SOCKET_ERROR) {
            std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
            closesocket(m_serverSocket);
            m_serverSocket = INVALID_SOCKET;
            return false;
        }

        m_listening = true;
        m_monitorThread = std::thread([this] { monitorThreadFunc(); });

        return true;
    }

    // 获取所有消息记录
    std::vector<MessageRecord> getMessageHistory() const {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        return m_messageHistory;
    }

    // 获取特定连接的消息记录
    std::vector<MessageRecord> getConnectionMessages(SOCKET socket) const {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        std::vector<MessageRecord> result;

        for (const auto& record : m_messageHistory) {
            if (record.socket == socket) {
                result.push_back(record);
            }
        }

        return result;
    }

    //清除所有消息
    inline void CleatAllmsg() {
        std::vector<std::thread> threadsToJoin;

        {
            std::lock_guard<std::mutex> lock(m_connectionsMutex);
            // 标记所有连接为关闭状态并关闭套接字
            for (auto& conn : m_connections) {
                if (conn.active) {
                    conn.shouldClose = true;
                    conn.closeSocket();
                    if (conn.thread.joinable()) {
                        threadsToJoin.push_back(std::move(conn.thread));
                    }
                }
            }
            m_connections.clear();
        }

        // 等待所有线程结束
        for (auto& thread : threadsToJoin) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // 清空消息历史
        {
            std::lock_guard<std::mutex> lock(m_historyMutex);
            m_messageHistory.clear();
        }
    }

    // 获取所有活跃连接
    std::vector<SOCKET> getActiveConnections() const {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        std::vector<SOCKET> result;

        for (const auto& conn : m_connections) {
            if (conn.active) {
                result.push_back(conn.socket);
            }
        }

        return result;
    }

    // 停止监控
    void stopMonitoring() {
        if (!m_listening) {
            return;
        }

        m_listening = false;

        // 关闭服务器socket
        if (m_serverSocket != INVALID_SOCKET) {
            shutdown(m_serverSocket, SD_BOTH);
            closesocket(m_serverSocket);
            m_serverSocket = INVALID_SOCKET;
        }

        // 关闭所有活跃连接
        std::vector<std::thread> threadsToJoin;
        {
            std::lock_guard<std::mutex> lock(m_connectionsMutex);
            for (auto& conn : m_connections) {
                if (conn.active) {
                    conn.shouldClose = true;
                    conn.closeSocket();
                    if (conn.thread.joinable()) {
                        threadsToJoin.push_back(std::move(conn.thread));
                    }
                }
            }
            m_connections.clear();
        }

        // 等待线程结束
        for (auto& thread : threadsToJoin) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // 等待监听线程结束
        if (m_monitorThread.joinable()) {
            m_monitorThread.join();
        }
    }

    // 向指定IP和端口发送消息
    static bool sendMessage(const std::string& ip, int port, const std::string& message,
        int timeoutMs = 3000, std::vector<std::string>* RectMessg = nullptr) {

        // 确保Winsock已初始化
        WSADATA wsaData;
        int wsaInitResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (wsaInitResult != 0) {
            std::cerr << "WSAStartup failed in sendMessage: " << wsaInitResult << std::endl;
            return false;
        }

        SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        // 设置发送和接收超时
        DWORD timeout = timeoutMs;
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO,
            reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
        //MessageHandler
        // 设置非阻塞模式连接
        unsigned long mode = 1;
        ioctlsocket(clientSocket, FIONBIO, &mode);

        if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                closesocket(clientSocket);
                return false;
            }

            // 使用select等待连接完成或超时
            fd_set set;
            FD_ZERO(&set);
            FD_SET(clientSocket, &set);

            timeval timeoutVal{};
            timeoutVal.tv_sec = timeoutMs / 1000;
            timeoutVal.tv_usec = (timeoutMs % 1000) * 1000;

            if (select(0, nullptr, &set, nullptr, &timeoutVal) <= 0) {
                closesocket(clientSocket);
                return false;
            }
        }

        // 恢复阻塞模式
        mode = 0;
        ioctlsocket(clientSocket, FIONBIO, &mode);

        // 发送消息
        int bytesSent = send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
            closesocket(clientSocket);
            return false;
        }

        // 记录发送的消息
        getInstance().recordMessage(message, true, clientSocket);

        // 尝试接收响应
        constexpr int BUFFER_SIZE = 65536; // 64KB
        std::vector<char> buffer(BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer.data(), BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            std::string response(buffer.data(), bytesReceived);
            if (RectMessg) {
                RectMessg->push_back(response);
            }
            getInstance().recordMessage(response, false, clientSocket);
        }

        closesocket(clientSocket);
        WSACleanup();
        return true;
    }

    // 关闭特定连接
    void closeConnection(SOCKET socket) {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        auto it = std::find_if(m_connections.begin(), m_connections.end(),
            [socket](const ConnectionInfo& conn) {
                return conn.socket == socket;
            });

        if (it != m_connections.end()) {
            if (it->active) {
                it->shouldClose = true;
                it->closeSocket();
            }
            if (it->thread.joinable()) {
                it->thread.join();
            }
            m_connections.erase(it);
        }
    }

private:
   

    // 单例模式访问
    static PortMonitor& getInstance() {
        static PortMonitor instance;
        return instance;
    }



    // 监控线程函数 - 接受新连接
    void monitorThreadFunc() {
        while (m_listening) {
            sockaddr_in clientAddr{};
            int clientAddrSize = sizeof(clientAddr);

            // 接受新连接
            SOCKET clientSocket = accept(m_serverSocket,
                reinterpret_cast<sockaddr*>(&clientAddr),
                &clientAddrSize);
            if (clientSocket == INVALID_SOCKET) {
                if (!m_listening) break; // 服务器已关闭
                int error = WSAGetLastError();
                if (error == WSAEINTR || error == WSAEWOULDBLOCK) continue;
                std::cerr << "Accept failed: " << error << std::endl;
                continue;
            }

            // 设置非阻塞模式
            unsigned long mode = 1;
            ioctlsocket(clientSocket, FIONBIO, &mode);

            // 创建新连接信息
            ConnectionInfo conn;
            conn.socket = clientSocket;
            conn.address = clientAddr;
            conn.active = true;
            conn.shouldClose = false;

            // 启动处理线程
            conn.thread = std::thread([this, conn = std::move(conn)]() mutable {
                connectionThreadFunc(std::move(conn));
                });

            {
                std::lock_guard<std::mutex> lock(m_connectionsMutex);
                // 检查连接数限制
                if (m_connections.size() >= 1000) {
                    std::cerr << "Connection limit reached (1000), rejecting new connection" << std::endl;
                    closesocket(clientSocket);
                    continue;
                }

                // 存储连接信息
                m_connections.push_back(std::move(conn));
            }
        }
    }

    // 连接线程函数 - 处理单个连接
    void connectionThreadFunc(ConnectionInfo conn) {
        constexpr int BUFFER_SIZE = 8192;
        std::vector<char> buffer(BUFFER_SIZE);
        time_t lastActivityTime = time(nullptr);

        // 设置接收超时（200毫秒）
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        setsockopt(conn.socket, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&tv), sizeof(tv));

        while (!conn.shouldClose) {
            // 接收数据
            int bytesReceived = recv(conn.socket, buffer.data(), BUFFER_SIZE, 0);

            if (bytesReceived > 0) {
                lastActivityTime = time(nullptr);
                std::string request(buffer.data(), bytesReceived);

                // 记录接收到的消息
                recordMessage(request, false, conn.socket);

                // ===== 新增处理逻辑 =====
                std::string response;
                {
                    std::lock_guard<std::mutex> lock(m_handlersMutex);
                    if (m_messageHandler) {
                        response = m_messageHandler(request);
                    }
                }

                // 如果消息处理器返回了响应，则发送
                if (!response.empty()) {
                    send(conn.socket, response.c_str(), response.size(), 0);
                    recordMessage(response, true, conn.socket);
                }
                // ===== 结束新增 =====

                // 处理请求
                std::istringstream iss(request);
                std::string method, path, protocol;
                iss >> method >> path >> protocol;

                if (method == "GET") {
                    std::string response = processHttpRequest(path);
                    send(conn.socket, response.c_str(), response.size(), 0);
                    recordMessage(response, true, conn.socket);
                }
            }
            else if (bytesReceived == 0) {
                // 正常断开
                break;
            }
            else {
                int error = WSAGetLastError();
                if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
                    // 检查是否超过3分钟无活动
                    if (time(nullptr) - lastActivityTime > 180) {
                        break;
                    }
                    continue;
                }
                else {
                    // 异常断开
                    break;
                }
            }
        }

        // 标记连接为非活跃
        conn.active = false;
        conn.closeSocket();
    }

    // 处理HTTP请求
    std::string processHttpRequest(const std::string& path) {
        std::string response;

        // 解析查询参数
        size_t queryStart = path.find('?');
        if (queryStart != std::string::npos) {
            std::string query = path.substr(queryStart + 1);
            std::map<std::string, std::string> params = parseQueryParams(query);

            std::lock_guard<std::mutex> lock(m_handlersMutex);
            for (const auto& [param, value] : params) {
                if (m_paramHandlers.count(param)) {
                    response = buildHttpResponse(200, "text/plain; charset=utf-8", m_paramHandlers[param](value));
                    break;
                }
            }
        }

        if (response.empty()) {
            response = buildHttpResponse(400, "text/plain; charset=utf-8", "Error: Invalid request");
        }

        return response;
    }

    // 解析查询字符串
    std::map<std::string, std::string> parseQueryParams(const std::string& query) {
        std::map<std::string, std::string> params;
        std::istringstream iss(query);
        std::string pair;

        while (std::getline(iss, pair, '&')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = pair.substr(0, eqPos);
                std::string value = pair.substr(eqPos + 1);
                params[key] = value;
            }
        }

        return params;
    }

    // 构造 HTTP 响应
    std::string buildHttpResponse(int status, const std::string& contentType, const std::string& body) {
        std::stringstream ss;
        ss << "HTTP/1.1 " << status << " " << getStatusText(status) << "\r\n"
            << "Content-Type: " << contentType << "\r\n"
            << "Connection: close\r\n"
            << "Access-Control-Allow-Origin: *\r\n"
            << "Content-Length: " << body.size() << "\r\n\r\n"
            << body;
        return ss.str();
    }

    // 状态码描述
    std::string getStatusText(int status) {
        switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        default: return "Unknown";
        }
    }

    // 记录消息
    void recordMessage(const std::string& message, bool isOutgoing, SOCKET socket) {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_messageHistory.emplace_back(message, isOutgoing, socket);

        // 限制历史记录大小
        if (m_messageHistory.size() > 200) {
            m_messageHistory.erase(m_messageHistory.begin());
        }
    }

    // 成员变量
    std::atomic<bool> m_listening{ false };         // 监听状态标志
    SOCKET m_serverSocket{ INVALID_SOCKET };        // 服务器监听socket
    std::thread m_monitorThread;                  // 监听线程

    // 使用list存储连接，避免拷贝问题
    std::list<ConnectionInfo> m_connections;

    std::map<std::string, ParamHandler> m_paramHandlers; // 参数处理器
    std::vector<MessageRecord> m_messageHistory;  // 消息历史记录

    mutable std::mutex m_handlersMutex;           // 保护参数处理器
    mutable std::mutex m_historyMutex;            // 保护消息历史
    mutable std::mutex m_connectionsMutex;        // 保护连接列表
    MessageHandler m_messageHandler; // 消息处理器成员变量
};

// Otter数据流命名空间
namespace OtterLamae {
    // 对照格式提取(A 12)
    std::pair<std::string, double> extractValuePair(const std::string& input) {
        std::istringstream iss(input);
        std::string identifier;
        double value;

        if (!(iss >> identifier >> value)) {
            throw std::invalid_argument("Invalid input format. Expected: 'identifier value'");
        }

        std::string remaining;
        if (iss >> remaining) {
            throw std::invalid_argument("Extra characters after value");
        }

        return { identifier, value };
    }

    // U->String
    std::string unsignedCharToString(const unsigned char* data, size_t length) {
        return std::string(reinterpret_cast<const char*>(data), length);
    }

    // String->U
    std::vector<unsigned char> stringToUnsignedChar(const std::string& str) {
        return { str.begin(), str.end() };
    }

    // 传输文件初始化
    std::vector<unsigned char> InitSenndFile(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filePath);
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        // 提取文件名
        size_t lastSlash = filePath.find_last_of("/\\");
        std::string fileName = (lastSlash == std::string::npos)
            ? filePath : filePath.substr(lastSlash + 1);

        // 准备缓冲区
        std::vector<unsigned char> data(64 + fileSize);

        // 写入文件名（最多64字节）
        size_t nameLength = min(fileName.size(), static_cast<size_t>(63));
        std::copy(fileName.begin(), fileName.begin() + nameLength, data.begin());
        data[nameLength] = '\0'; // 确保以空字符结尾

        // 读取文件内容
        file.read(reinterpret_cast<char*>(data.data() + 64), fileSize);
        file.close();

        return data;
    }

    // 提取文件
    void ParseReceivedFile(const std::vector<unsigned char>& data) {
        if (data.size() < 64) {
            throw std::runtime_error("Invalid file data: too small");
        }

        // 提取文件名
        auto nullPos = std::find(data.begin(), data.begin() + 64, '\0');
        std::string fileName(data.begin(), nullPos);

        // 获取文件内容
        size_t contentSize = data.size() - 64;
        const unsigned char* content = data.data() + 64;

        // 保存文件
        std::ofstream outFile(fileName, std::ios::binary);
        outFile.write(reinterpret_cast<const char*>(content), contentSize);
        outFile.close();
    }

    // 打开网页
    void OpenWeb(std::wstring URL,
        int width = 800,
        int height = 600,
        int posX = 0,
        int posY = 0)
    {
        std::wstring currentPath = std::filesystem::current_path().wstring();

        // 构建命令行参数
        std::wstring command = L"start msedge.exe --app=\"" + currentPath + URL + L"\" "
            L"--window-size=" + std::to_wstring(width) + L","
            + std::to_wstring(height) + L" "
            L"--window-position=" + std::to_wstring(posX) + L","
            + std::to_wstring(posY);

        _wsystem(command.c_str());
    }
}

#endif // PORT_MONITOR_H