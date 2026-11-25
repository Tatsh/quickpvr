#include "logging.h"

#include <memory>
#include <string_view>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

namespace Pvr2Png {

namespace {

constexpr char kLevelFlag = '*';

// The formatter writes Python's level names. spdlog writes some differently (`warning` in lower
// case, for one).
class LevelNameFormatter : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg &message,
                const std::tm &,
                spdlog::memory_buf_t &destination) override {
        std::string_view name;
        switch (message.level) {
        case spdlog::level::trace:
        case spdlog::level::debug:
            name = "DEBUG";
            break;
        case spdlog::level::info:
            name = "INFO";
            break;
        case spdlog::level::warn:
            name = "WARNING";
            break;
        case spdlog::level::err:
            name = "ERROR";
            break;
        default:
            name = "CRITICAL";
            break;
        }
        destination.append(name.data(), name.data() + name.size());
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<LevelNameFormatter>();
    }
};

} // namespace

void setupLogging(bool debug) {
    auto logger = spdlog::stderr_logger_st("pvr2png");
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<LevelNameFormatter>(kLevelFlag).set_pattern("%*: %v");
    logger->set_formatter(std::move(formatter));
    logger->set_level(debug ? spdlog::level::debug : spdlog::level::info);
    spdlog::set_default_logger(std::move(logger));
}

} // namespace Pvr2Png
