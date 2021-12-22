#include "timer.hpp"

Timer::Timer() : Timer("") {}
Timer::Timer(const std::string_view str)
    : start_time_(std::chrono::steady_clock::now()), str_(str) {}
Timer::~Timer() { stop(); }

void Timer::stop() {
  auto end_time = std::chrono::steady_clock::now();
  auto duration =
      std::chrono::floor<std::chrono::milliseconds>(end_time - start_time_);
  std::cout << str_ << duration.count() << "ms" << '\n';
}
