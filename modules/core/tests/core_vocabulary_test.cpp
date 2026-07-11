import elf.core;

#include <elf3d/core/diagnostics.h>
#include <elf3d/core/result.h>

#include <string>

namespace {

class CapturingSink final : public elf3d::LogSink {
  public:
    void write(const elf3d::LogRecord &record) noexcept override {
        latest = record;
        ++count;
    }

    elf3d::LogRecord latest;
    int count = 0;
};

} // namespace

int elf3d_core_vocabulary_test() {
    static_assert(elf3d::core::vocabulary_revision == 1U);

    const elf3d::Error error{elf3d::ErrorCode::invalid_argument, "invalid value"};
    if (error.code() != elf3d::ErrorCode::invalid_argument ||
        std::string{error.message()} != "invalid value") {
        return 1;
    }

    elf3d::Result<int> value{7};
    if (!value || value.value() != 7) {
        return 2;
    }

    const elf3d::Result<int> failed{error};
    if (failed || failed.error().code() != elf3d::ErrorCode::invalid_argument) {
        return 3;
    }

    const elf3d::Result<void> success;
    if (!success) {
        return 4;
    }

    CapturingSink sink;
    sink.write(elf3d::LogRecord{elf3d::LogLevel::warning, error.code(), "captured"});
    if (sink.count != 1 || sink.latest.level != elf3d::LogLevel::warning ||
        sink.latest.code != std::optional{elf3d::ErrorCode::invalid_argument} ||
        sink.latest.message != "captured") {
        return 5;
    }

    return 0;
}
