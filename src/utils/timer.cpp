#include "timer.hpp"

Timer::Timer() : Timer("") {}
Timer::Timer(const std::string_view str)
    : start_time_(std::chrono::steady_clock::now()), str_(str) {}
Timer::~Timer() { stop(); }

void Timer::stop() {
  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time_);
  const float ms_in_sec = 0.001F;
  std::cout << str_ << duration.count() * ms_in_sec << " ms.\n";
}
