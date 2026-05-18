#include "host/logging/logger.hpp"

namespace host {
    void logger_t::init() {
        spdlog::set_pattern("%^[%T] %n: %v%$");

        s_logger = spdlog::stderr_color_mt("Server");
        s_logger->set_level(spdlog::level::trace);
    }

    std::shared_ptr<spdlog::logger>& logger_t::get_logger() {
        return s_logger;
    }
}