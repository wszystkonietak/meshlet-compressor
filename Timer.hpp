#pragma once

#include <chrono>
#include <iostream>
#include <string>

class Timer {
 public:
  Timer() = default;

  Timer(const std::string& msg, const std::string& start_msg) : message_(msg) {
    std::cout << start_msg << '\n';
    start_time_ = std::chrono::steady_clock::now();
  }

  ~Timer() {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time_);
    if (!message_.empty()) {
      std::cout << message_ << ": " << duration.count() << " ms\n";
    }
  }

  void Start() { start_time_ = std::chrono::steady_clock::now(); }

  void Step(const std::string& msg) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time_);
    std::cout << msg << ": " << duration.count() << " ms\n";
  }

 private:
  std::chrono::steady_clock::time_point start_time_;
  std::string message_;
};