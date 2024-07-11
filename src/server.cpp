#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>



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


// DEFINING MACROS
#ifdef _WIN32
#define CLOSE_SOCKET(fd) do { closesocket(fd); WSACleanup(); } while(0)
#else
#define CLOSE_SOCKET(fd) close(fd)
#endif


int main(int argc, char **argv) {
#ifdef _WIN32
  WSADATA wsaData;
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    std::cerr << "WSAStartup failed: " << iResult << "\n";
    return 1;
  }
#endif

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    std::cerr << "Failed to create server socket\n";
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    CLOSE_SOCKET(server_fd);
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);

  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    CLOSE_SOCKET(server_fd);
    return 1;
  }

  if (listen(server_fd, 5) != 0) {
    std::cerr << "listen failed\n";
    CLOSE_SOCKET(server_fd);
    return 1;
  }

  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";


  int client_socket = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);

  if (client_socket == -1) {
    std::cerr << "accept failed\n";
    CLOSE_SOCKET(server_fd);
    return 1;
  }
  std::cout << "Client connected\n";

  std::string client_message(1024, '\0');
  ssize_t bytes_received = recv(client_socket, (char *)&client_message[0], sizeof(client_message), 0);

  if(bytes_received < 0){
    std::cerr << "error receiving message from client\n";
    CLOSE_SOCKET(client_socket);
    CLOSE_SOCKET(server_fd);
    return 1;
  }

  std::cout << "Received " << client_message << "\n";
  std::cerr << "Client Message (length: " << client_message.size() << ")" << std::endl;
  std::clog << client_message << std::endl;

  bool starts_with_get = client_message.find("GET / HTTP/1.1\r\n") == 0;
  std::string response = starts_with_get ? "HTTP/1.1 200 OK\r\n\r\n" : "HTTP/1.1 404 Not Found\r\n\r\n";
  send(client_socket, response.c_str(), response.size(), 0);

  CLOSE_SOCKET(server_fd);

  return 0;
}