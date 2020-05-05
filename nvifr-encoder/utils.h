#include <chrono>
#include <mutex>

class Timer {
 private:
  std::chrono::high_resolution_clock::time_point time;
  std::mutex mmutex;

 public:
  Timer() { this->reset(); }

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

  long getElapsedMicroseconds() {
    std::unique_lock<std::mutex> lck{mmutex};
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::high_resolution_clock::now() - this->time)
        .count();
  }
};