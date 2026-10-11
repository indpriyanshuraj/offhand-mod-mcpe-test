#pragma once

// Use Android's system logger directly so these messages are visible in the
// launcher's logcat, independent of the PreLoader logger configuration.
#include <android/log.h>

namespace offhand_qol::log {

inline constexpr char Tag[] = "OffhandQoL";

template <class... Args>
inline void verbose(const char* format, Args... args) noexcept {
    __android_log_print(ANDROID_LOG_VERBOSE, Tag, format, args...);
}

template <class... Args>
inline void debug(const char* format, Args... args) noexcept {
    __android_log_print(ANDROID_LOG_DEBUG, Tag, format, args...);
}

template <class... Args>
inline void info(const char* format, Args... args) noexcept {
    __android_log_print(ANDROID_LOG_INFO, Tag, format, args...);
}

template <class... Args>
inline void warn(const char* format, Args... args) noexcept {
    __android_log_print(ANDROID_LOG_WARN, Tag, format, args...);
}

template <class... Args>
inline void error(const char* format, Args... args) noexcept {
    __android_log_print(ANDROID_LOG_ERROR, Tag, format, args...);
}

} // namespace offhand_qol::log