#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
class ThreadPool {
private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queueMutex;
  std::condition_variable condition;
  bool stop;
  void worker();

public:
  ThreadPool(size_t numOfThreads);
  ~ThreadPool();
  void enqueue(std::function<void()> &&func);
};
