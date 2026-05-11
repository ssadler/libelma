#ifndef MAIN_H
#define MAIN_H

#include <string>

constexpr double STOPWATCH_MULTIPLIER = 0.182;
extern bool ErrorGraphicsLoaded;

[[noreturn]] void quit();

[[noreturn]] void internal_error(const std::string& message);
[[noreturn]] void external_error(const std::string& message);

#endif
