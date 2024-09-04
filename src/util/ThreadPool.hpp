#pragma once
#include <BS_thread_pool.hpp>

class ThreadPool {
 public:
  static void Init();
  static void Shutdown();
  static ThreadPool& Get();

  BS::thread_pool thread_pool;

 private:
  static ThreadPool* instance_;
  ThreadPool();
};
