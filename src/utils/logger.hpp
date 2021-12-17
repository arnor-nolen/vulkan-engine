#pragma once

#include <memory>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

class Logger {
public:
  Logger();
  void dump(const std::string_view log_entry);
  auto get_logs() -> std::vector<std::string>;

private:
  std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> ringbuffer_sink_;
  std::shared_ptr<spdlog::logger> logger_;
};