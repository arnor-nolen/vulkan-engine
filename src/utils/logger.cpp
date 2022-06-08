#include "logger.hpp"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

Logger logger{};

Logger::Logger() {
  ringbuffer_sink_ = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(100);

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(
      std::make_shared<spdlog::sinks::basic_file_sink_mt>("output.log"));
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  sinks.push_back(ringbuffer_sink_);

  logger_ =
      std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());
}

void Logger::dump(const std::string_view log_entry,
                  spdlog::level::level_enum level) {
  logger_->log(level, log_entry);
}

auto Logger::get_logs() -> std::vector<std::string> {
  return ringbuffer_sink_->last_formatted();
}
