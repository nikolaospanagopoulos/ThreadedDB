#include "ThreadPool.hpp"
#include <cstddef>
#include <mutex>
ThreadPool::ThreadPool(size_t numOfThreads) : stop(false) {
  for (size_t i = 0; i < numOfThreads; i++) {
    workers.emplace_back([this] { worker(); });
  }
}

void ThreadPool::worker() {

  while (true) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(this->queueMutex);
      condition.wait(lock, [this] { return !tasks.empty() || stop; });
      if (stop && tasks.empty()) {
        return;
      }
      task = std::move(tasks.front());
      tasks.pop();
    }
    task();
  }
}

void ThreadPool::enqueue(std::function<void()> &&func) {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    tasks.push(func);
  }
  condition.notify_one();
}
ThreadPool::~ThreadPool() {

  {
    std::unique_lock<std::mutex> lock(this->queueMutex);
    stop = true;
  }
  condition.notify_all();
  for (auto &thr : workers) {
    thr.join();
  }
}
