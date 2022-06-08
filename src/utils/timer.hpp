#pragma once

#include <chrono>

struct Timer {
  Timer();
  explicit Timer(std::string_view str);
  ~Timer();

  Timer(const Timer &) = delete;
  Timer(Timer &&other) noexcept = delete;
  auto operator=(const Timer &) -> const Timer & = delete;
  auto operator=(Timer &&other) noexcept -> Timer & = delete;

  void stop();

private:
  std::chrono::time_point<std::chrono::steady_clock> start_time_;
  std::string str_;
};