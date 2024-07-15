#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <zlib.h>

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
const std::string CREATED_MESSAGE = "HTTP/1.1 201 Created\r\n\r\n";
const std::string ERROR_MESSAGE = "HTTP/1.1 404 Not Found\r\n\r\n";
const std::string CONTENT_TYPE = "Content-Type: text/plain\r\n";

class Server {
public:
  Server(const std::string& directory) : file_directory(directory) {
    initializeWinsock();
    server_fd = createServerSocket();
    setSocketOptions();
    bindSocket();
    startListening();
  }

  ~Server() {
    CLOSE_SOCKET(server_fd);
  }

  void run() {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for clients to connect...\n";

    std::vector<std::thread> threads;

    while (true) {
      int client_socket = acceptClient(client_addr, client_addr_len);
      std::cout << "Client connected\n";
      threads.emplace_back(&Server::clientHandler, this, client_socket);
    }

    for (auto& thread : threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

private:
  int server_fd;
  std::string file_directory;

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
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
      std::cerr << "Failed to create server socket\n";
    #ifdef _WIN32
      WSACleanup();
    #endif
      exit(1);
    }
    return fd;
  }

  void setSocketOptions() {
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
      std::cerr << "setsockopt failed\n";
      CLOSE_SOCKET(server_fd);
      exit(1);
    }
  }

  void bindSocket() {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
      std::cerr << "Failed to bind to port " << PORT << "\n";
      CLOSE_SOCKET(server_fd);
      exit(1);
    }
  }

  void startListening() {
    if (listen(server_fd, 5) != 0) {
      std::cerr << "listen failed\n";
      CLOSE_SOCKET(server_fd);
      exit(1);
    }
  }

  int acceptClient(struct sockaddr_in& client_addr, socklen_t& client_addr_len) {
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

  bool supportsGzip(const std::string& accept_encoding) {
    std::istringstream ss(accept_encoding);
    std::string token;
    while (std::getline(ss, token, ',')) {
      token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
      if (token == "gzip") {
        return true;
      }
    }
    return false;
  }

  std::string gzipCompress(const std::string& data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
      throw std::runtime_error("deflateInit2 failed while compressing.");
    }
    zs.next_in = (Bytef*)data.data();
    zs.avail_in = data.size();
    int ret;
    char outbuffer[32768];
    std::string outstring;
    do {
      zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
      zs.avail_out = sizeof(outbuffer);
      ret = deflate(&zs, Z_FINISH);
      if (outstring.size() < zs.total_out) {
        outstring.append(outbuffer, zs.total_out - outstring.size());
      }
    } while (ret == Z_OK);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END) {
      throw std::runtime_error("Exception during zlib compression: " + std::to_string(ret));
    }
    return outstring;
  }

  void sendFileResponse(int client_socket, const std::string& file_path, bool gzip) {
    std::ifstream file(file_path, std::ios::binary);
    if (file) {
      std::ostringstream ss;
      ss << file.rdbuf();
      std::string file_content = ss.str();
      std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(file_content.size()) + "\r\n";
      if (gzip) {
        response += "Content-Encoding: gzip\r\n";
      }
      response += "\r\n" + file_content;
      send(client_socket, response.c_str(), response.size(), 0);
    } else {
      send(client_socket, ERROR_MESSAGE.c_str(), ERROR_MESSAGE.length(), 0);
    }
  }

  void handleGetRequest(int client_socket, const std::string& path, bool gzip) {
    if (path == "/") {
      std::string response = OK_MESSAGE;
      if (gzip) {
        response = "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n";
      }
      send(client_socket, response.c_str(), response.length(), 0);
    } else if (path.find("/files/") == 0) {
      if (path.length() > 7) {
        std::string filename = path.substr(7); // Extract filename from path
        std::string file_path = file_directory + "/" + filename;
        sendFileResponse(client_socket, file_path, gzip);
      } else {
        send(client_socket, ERROR_MESSAGE.c_str(), ERROR_MESSAGE.length(), 0);
      }
    } else {
      send(client_socket, ERROR_MESSAGE.c_str(), ERROR_MESSAGE.length(), 0);
    }
  }

  void handlePostRequest(int client_socket, const std::string& path, const std::string& body) {
    if (path.find("/files/") == 0) {
      if (path.length() > 7) {
        std::string filename = path.substr(7);
        std::string file_path = file_directory + "/" + filename;
        std::ofstream file(file_path, std::ios::binary);
        if (file) {
          file << body;
          file.close();
          send(client_socket, CREATED_MESSAGE.c_str(), CREATED_MESSAGE.length(), 0);
        } else {
          send(client_socket, ERROR_MESSAGE.c_str(), ERROR_MESSAGE.length(), 0);
        }
      } else {
        send(client_socket, ERROR_MESSAGE.c_str(), ERROR_MESSAGE.length(), 0);
      }
    } else {
      send(client_socket, ERROR_MESSAGE.c_str(), ERROR_MESSAGE.length(), 0);
    }
  }

  void handleRequest(int client_socket, const std::string& client_message) {
    std::string method = client_message.substr(0, client_message.find(' '));
    std::string path = client_message.substr(client_message.find(' ') + 1, client_message.find(' ', client_message.find(' ') + 1) - client_message.find(' ') - 1);
    std::string accept_encoding = extractHeader(client_message, "Accept-Encoding: ");
    bool gzip = supportsGzip(accept_encoding);
    if (method == "GET") {
      if (client_message.find("GET /echo/") == 0) {
        int endOfStr = client_message.find("HTTP/1.1");
        std::string contentStr = client_message.substr(10, endOfStr - 11);
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n";
        if (gzip) {
          std::string compressedContent = gzipCompress(contentStr);
          response += "Content-Encoding: gzip\r\nContent-Length: " + std::to_string(compressedContent.size()) + "\r\n\r\n" + compressedContent;
        } else {
          response += "Content-Length: " + std::to_string(contentStr.size()) + "\r\n\r\n" + contentStr;
        }
        send(client_socket, response.c_str(), response.length(), 0);
      } else if (path == "/") {
        std::string response = OK_MESSAGE;
        if (gzip) {
          response = "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n";
        }
        send(client_socket, response.c_str(), response.length(), 0);
      } else if (path == "/user-agent") {
        std::string user_agent = extractHeader(client_message, "User-Agent: ");
        if (!user_agent.empty()) {
          int user_length = user_agent.length();
          std::string response = OK_MESSAGE + CONTENT_TYPE + "Content-Length: " + std::to_string(user_length) + "\r\n";
          if (gzip) {
            std::string compressedContent = gzipCompress(user_agent);
            response += "Content-Encoding: gzip\r\nContent-Length: " + std::to_string(compressedContent.size()) + "\r\n\r\n" + compressedContent;
          } else {
            response += "\r\n" + user_agent;
          }
          send(client_socket, response.c_str(), response.size(), 0);
        } else {
          std::string error_response = ERROR_MESSAGE + CONTENT_TYPE + "\r\n";
          send(client_socket, error_response.c_str(), error_response.size(), 0);
        }
      } else {
        handleGetRequest(client_socket, path, gzip);
      }
    } else if (method == "POST") {
      std::string content_length_str = extractHeader(client_message, "Content-Length: ");
      int content_length = std::stoi(content_length_str);
      std::string body = client_message.substr(client_message.find("\r\n\r\n") + 4, content_length);
      handlePostRequest(client_socket, path, body);
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
};

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--directory") {
    std::cerr << "Usage: " << argv[0] << " --directory <path>\n";
    return 1;
  }
  std::string file_directory = argv[2];

  Server server(file_directory);
  server.run();

  return 0;
}


/**
Explanation:

  1.) Encapsulation: The server logic is encapsulated within the Server class. This includes initialization, socket setup, request handling, and client communication.
  2.) Modularity: Each function within the Server class is responsible for a specific task, promoting modularity and readability.
  3.) Lifecycle Management: The Server class constructor and destructor manage the server's lifecycle, including resource allocation and cleanup.
  4.) Thread Management: The run method handles client connections in separate threads, ensuring concurrent request handling.

This approach ensures that the server is modular, maintainable, and scalable, adhering to best practices for building robust and efficient servers.
*/
