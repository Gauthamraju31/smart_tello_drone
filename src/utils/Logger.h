#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <deque>
#include <string>

// Structure to hold log messages for the GUI
struct LogMessage {
    spdlog::level::level_enum level;
    std::string text;
};

// Custom spdlog sink that stores messages in a deque
template<typename Mutex>
class ImGuiSink : public spdlog::sinks::base_sink<Mutex> {
public:
    ImGuiSink() = default;

    std::deque<LogMessage> getMessages() {
        std::lock_guard<Mutex> lock(this->mutex_);
        return m_messages;
    }

    void clear() {
        std::lock_guard<Mutex> lock(this->mutex_);
        m_messages.clear();
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        
        m_messages.push_back({msg.level, fmt::to_string(formatted)});
        if (m_messages.size() > 1000) {
            m_messages.pop_front();
        }
    }

    void flush_() override {}

private:
    std::deque<LogMessage> m_messages;
};

using ImGuiSink_mt = ImGuiSink<std::mutex>;

class Logger {
public:
    static void init();
    static std::shared_ptr<ImGuiSink_mt> getGuiSink();

private:
    static std::shared_ptr<ImGuiSink_mt> s_guiSink;
};
