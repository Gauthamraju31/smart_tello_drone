#include "Logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<ImGuiSink_mt> Logger::s_guiSink = nullptr;

void Logger::init() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    console_sink->set_pattern("[%^%l%$] %v");

    s_guiSink = std::make_shared<ImGuiSink_mt>();
    s_guiSink->set_level(spdlog::level::info); // GUI doesn't need all debug spam
    s_guiSink->set_pattern("[%H:%M:%S] [%l] %v"); // Include time in GUI

    spdlog::sinks_init_list sink_list = { console_sink, s_guiSink };
    
    auto logger = std::make_shared<spdlog::logger>("tello", sink_list.begin(), sink_list.end());
    logger->set_level(spdlog::level::debug);
    
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(3));
    
    spdlog::info("Logger initialized");
}

std::shared_ptr<ImGuiSink_mt> Logger::getGuiSink() {
    return s_guiSink;
}
