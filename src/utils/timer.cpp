#include "timer.hpp"
#include "utils/logger.hpp"
#include <format>

namespace utils {

Timer::Timer() : Timer("") {}
Timer::Timer(const std::string_view str)
    : start_time_(std::chrono::steady_clock::now()), str_(str) {}
Timer::~Timer() { stop(); }

void Timer::stop() {
  auto end_time = std::chrono::steady_clock::now();
  auto duration =
      std::chrono::floor<std::chrono::milliseconds>(end_time - start_time_);
  logger.dump(std::format("{} {}ms", str_, duration.count()));
}

} // namespace utils