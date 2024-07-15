#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define CLOSE_SOCKET(fd) do { closesocket(fd); WSACleanup(); } while(0)
#else
#define CLOSE_SOCKET(fd) close(fd)
#endif

const int PORT = 4221;
const int BUFFER_SIZE = 1024;
const std::string OK_MESSAGE = "HTTP/1.1 200 OK\r\n\r\n";
const std::string ERROR_MESSAGE = "HTTP/1.1 404 Not Found\r\n\r\n";
const std::string CONTENT_TYPE = "Content-Type: text/plain\r\n";

void initializeWinsock() {
#ifdef _WIN32
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        exit(1);
    }
#endif
}

int createServerSocket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create server socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
    return server_fd;
}

void setSocketOptions(int server_fd) {
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        CLOSE_SOCKET(server_fd);
        exit(1);
    }
}

void bindSocket(int server_fd, struct sockaddr_in& server_addr) {
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port " << PORT << "\n";
        CLOSE_SOCKET(server_fd);
        exit(1);
    }
}

void startListening(int server_fd) {
    if (listen(server_fd, 5) != 0) {
        std::cerr << "listen failed\n";
        CLOSE_SOCKET(server_fd);
        exit(1);
    }
}

int acceptClient(int server_fd, struct sockaddr_in& client_addr, socklen_t& client_addr_len) {
    int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        std::cerr << "accept failed\n";
        CLOSE_SOCKET(server_fd);
        exit(1);
    }
    return client_socket;
}

std::string receiveMessage(int client_socket) {
    std::string client_message(BUFFER_SIZE, '\0');
    ssize_t bytes_received = recv(client_socket, (char*)&client_message[0], BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        std::cerr << "error receiving message from client\n";
        CLOSE_SOCKET(client_socket);
        exit(1);
    }
    return client_message;
}

std::string extractHeader(const std::string& request, const std::string& header_name) {
    size_t start = request.find(header_name);
    if (start == std::string::npos) return "";
    start += header_name.length();
    size_t end = request.find("\r\n", start);
    return request.substr(start, end - start);
}

void handleRequest(int client_socket, const std::string& client_message) {
    std::string method = client_message.substr(0, client_message.find(' '));
    std::string path = client_message.substr(client_message.find(' ') + 1, client_message.find(' ', client_message.find(' ') + 1) - client_message.find(' ') - 1);

    if (client_message.find("GET /echo/") == 0) {
        int endOfStr = client_message.find("HTTP/1.1");
        std::string contentStr = client_message.substr(10, endOfStr - 11);
        std::string message = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(contentStr.size()) + "\r\n\r\n" + contentStr;
        send(client_socket, message.c_str(), message.length(), 0);
    } else if (client_message.find("GET / HTTP/1.1\r\n") == 0) {
        send(client_socket, OK_MESSAGE.c_str(), OK_MESSAGE.length(), 0);
    } else if (method == "GET" && path == "/user-agent") {
        std::string user_agent = extractHeader(client_message, "User-Agent: ");
        if (!user_agent.empty()) {
            int user_length = user_agent.length();
            std::string user_agent_response = OK_MESSAGE + CONTENT_TYPE + "Content-Length: " + std::to_string(user_length) + "\r\n\r\n" + user_agent;
            send(client_socket, user_agent_response.c_str(), user_agent_response.size(), 0);
        } else {
            std::string error_response = ERROR_MESSAGE + CONTENT_TYPE + "\r\n";
            send(client_socket, error_response.c_str(), error_response.size(), 0);
        }
    } else {
        send(client_socket, ERROR_MESSAGE.c_str(), ERROR_MESSAGE.length(), 0);
    }
    CLOSE_SOCKET(client_socket);
}

void clientHandler(int client_socket) {
    std::string client_message = receiveMessage(client_socket);
    std::cout << "Received " << client_message << "\n";
    handleRequest(client_socket, client_message);
}

int main(int argc, char** argv) {
    initializeWinsock();

    int server_fd = createServerSocket();
    setSocketOptions(server_fd);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bindSocket(server_fd, server_addr);
    startListening(server_fd);

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for clients to connect...\n";

    std::vector<std::thread> threads;

    while (true) {
        int client_socket = acceptClient(server_fd, client_addr, client_addr_len);
        std::cout << "Client connected\n";
        threads.emplace_back(clientHandler, client_socket);
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    CLOSE_SOCKET(server_fd);
    return 0;
}