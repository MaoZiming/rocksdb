#include "thread_pool.hpp"

#include <stdexcept>

// Constructor definition
ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this]() {
      for (;;) {
        std::function<void()> task;

        {
          std::unique_lock<std::mutex> lock(this->queue_mutex_);
          this->condition_.wait(
              lock, [this]() { return this->stop_ || !this->tasks_.empty(); });
          if (this->stop_ && this->tasks_.empty()) return;
          task = std::move(this->tasks_.front());
          this->tasks_.pop();
        }

        task();
      }
    });
  }
}

// Destructor definition
ThreadPool::~ThreadPool() {
  stop_ = true;
  condition_.notify_all();
  for (std::thread &worker : workers_) worker.join();
}
