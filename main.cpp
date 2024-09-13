#include "ThreadPool.hpp"
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>

// ThreadPool class to handle concurrent clients

// Redis-like in-memory key-value store
class RedisClone {
public:
  void set(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lock(dataMutex);
    data[key] = value;
  }

  std::string get(const std::string &key) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (data.find(key) != data.end()) {
      return data[key];
    } else {
      return "Key not found";
    }
  }

private:
  std::unordered_map<std::string, std::string> data;
  std::mutex dataMutex;
};

// Function to handle each client connection
void handleClient(int clientSocket, RedisClone &store) {
  char buffer[1024];
  try {
    while (true) {
      memset(buffer, 0, sizeof(buffer));
      ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
      if (bytesReceived <= 0) {
        std::cerr << "Client disconnected" << std::endl;
        close(clientSocket);
        return;
      }

      std::string command(buffer, bytesReceived);
      std::istringstream ss(command);
      std::string action, key, value;

      ss >> action;
      if (action == "SET") {
        ss >> key >> value;
        store.set(key, value);
        std::string response = "OK\n";
        send(clientSocket, response.c_str(), response.length(), 0);
      } else if (action == "GET") {
        ss >> key;
        std::string result = store.get(key);
        result += "\n";
        send(clientSocket, result.c_str(), result.length(), 0);
      } else {
        std::string response = "Unknown command\n";
        send(clientSocket, response.c_str(), response.length(), 0);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    close(clientSocket);
  }
}

void startServer(int port, ThreadPool &threadPool, RedisClone &store) {
  // AF_INET = ipv4 , SOCK_STREAM = tcp, protocol 0 -> default for tcp
  // returns file descriptor for the socket
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == -1) {
    std::cerr << "Failed to create socket" << std::endl;
    return;
  }

  // holds servers ip address and port
  sockaddr_in serverAddr;
  // ipv4
  serverAddr.sin_family = AF_INET;
  // INADDR_ANY means the server will accept connections on any network
  // interface on the machine
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  // converts the port number from host byte order to network byte order (Big
  // Endian).
  serverAddr.sin_port = htons(port);

  // binds server socker to specified ip and port
  if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) <
      0) {
    std::cerr << "Bind failed" << std::endl;
    // probably port already used
    close(serverSocket);
    return;
  }

  // passive socket
  // 10 connections can be queued
  if (listen(serverSocket, 10) < 0) {
    std::cerr << "Failed to listen" << std::endl;
    close(serverSocket);
    return;
  }

  std::cout << "Server is listening on port " << port << std::endl;

  while (true) {
    sockaddr_in clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);
    // When a connection is received, it creates a new socket
    int clientSocket =
        accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrSize);

    if (clientSocket >= 0) {
      threadPool.enqueue(
          [clientSocket, &store]() { handleClient(clientSocket, store); });
    } else {
      std::cerr << "Failed to accept connection" << std::endl;
    }
  }

  close(serverSocket);
}

int main() {
  RedisClone store;
  ThreadPool pool(4); // Create 4 worker threads

  startServer(6379, pool, store); // Start server on port 6379

  return 0;
}
