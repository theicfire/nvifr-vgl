#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>

// Thread safe!
class MTimer {
 private:
  std::chrono::high_resolution_clock::time_point time;
  std::mutex mmutex;

 public:
  MTimer() { this->reset(); }

  void reset() {
    std::unique_lock<std::mutex> lck{mmutex};
    this->time = std::chrono::high_resolution_clock::now();
  }

  double getElapsedSeconds() {
    std::unique_lock<std::mutex> lck{mmutex};
    return 1.0e-6 * std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now() - this->time)
                        .count();
  }

  double getElapsedMilliseconds() {
    std::unique_lock<std::mutex> lck{mmutex};
    return 1.0e-3 * std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now() - this->time)
                        .count();
  }

  unsigned long long getElapsedMicroseconds() {
    std::unique_lock<std::mutex> lck{mmutex};
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::high_resolution_clock::now() - this->time)
        .count();
  }
};
