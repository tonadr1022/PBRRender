#include "ThreadPool.hpp"

ThreadPool* ThreadPool::instance_ = nullptr;

ThreadPool& ThreadPool::Get() { return *instance_; }

void ThreadPool::Init() { instance_ = new ThreadPool; }

void ThreadPool::Shutdown() {
  ZoneScoped;
  delete instance_;
}

ThreadPool::ThreadPool() { instance_ = this; }
