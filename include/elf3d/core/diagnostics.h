#ifndef ELF3D_CORE_DIAGNOSTICS_H
#define ELF3D_CORE_DIAGNOSTICS_H

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>

#include <optional>
#include <string_view>

namespace elf3d {

enum class LogLevel {
    trace,
    debug,
    info,
    warning,
    error,
    fatal,
};

struct LogRecord {
    LogLevel level = LogLevel::info;
    std::optional<ErrorCode> code;
    std::string_view message;
};

class LogSink {
  public:
    virtual ~LogSink() noexcept = default;

    LogSink(const LogSink &) = delete;
    LogSink &operator=(const LogSink &) = delete;
    LogSink(LogSink &&) = delete;
    LogSink &operator=(LogSink &&) = delete;

    virtual void write(const LogRecord &record) noexcept = 0;

  protected:
    LogSink() = default;
};

} // namespace elf3d

#endif
