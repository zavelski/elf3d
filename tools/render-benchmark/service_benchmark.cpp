#include "service_benchmark.h"
#include "service_fixture.h"

#include <array>
#include <charconv>
#include <chrono>
#include <elf3d/app/application.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <optional>
#include <string>

namespace elf3d::tools {
namespace {
using Clock = std::chrono::steady_clock;
struct Options {
    ServiceFixture fixture;
    std::string name;
    std::uint32_t calls = 32;
    std::uint32_t cycles = 1;
    std::filesystem::path report;
};
[[nodiscard]] std::optional<std::uint32_t> number(std::string_view text)
{
    std::uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return std::nullopt;
    return value;
}
[[nodiscard]] std::optional<ServiceFixture> fixture_kind(std::string_view text)
{
    if (text == "large-mesh")
        return ServiceFixture::large_mesh;
    if (text == "many-meshes")
        return ServiceFixture::many_meshes;
    if (text == "large-image")
        return ServiceFixture::large_image;
    if (text == "many-images")
        return ServiceFixture::many_images;
    return std::nullopt;
}
[[nodiscard]] std::filesystem::path utf8_path(std::string_view text)
{
    std::u8string bytes;
    for (const unsigned char byte : text)
        bytes.push_back(static_cast<char8_t>(byte));
    return std::filesystem::path{bytes};
}
[[nodiscard]] std::optional<Options> options(int count, char** values)
{
    if (count != 6)
        return std::nullopt;
    const auto fixture = fixture_kind(values[2]);
    const auto calls = number(values[3]);
    const auto cycles = number(values[4]);
    if (!fixture || !calls || !cycles)
        return std::nullopt;
    if (*calls < 32 || *calls > 4096 || *cycles == 0 || *cycles > 10)
        return std::nullopt;
    return Options{*fixture, values[2], *calls, *cycles, utf8_path(values[5])};
}
[[nodiscard]] double elapsed(Clock::time_point begin)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
}
enum class SamplePhase { setup, preparation, detach, retirement };
[[nodiscard]] std::string_view phase_name(SamplePhase phase)
{
    constexpr std::array<std::string_view, 4> names{"setup", "preparation", "detach", "retirement"};
    return names[static_cast<std::size_t>(phase)];
}
struct Sample {
    SamplePhase phase = SamplePhase::setup;
    double wall_ms = 0;
    double reported_ms = 0;
    double phase_ms = 0;
    std::uint32_t units = 0;
    std::uint32_t calls = 0;
    std::uint64_t uploaded = 0;
    std::uint64_t allocated = 0;
    double longest_ms = 0;
    int longest_phase = -1;
    int shader_kind = -1;
    int shader_step = -1;
    std::uint64_t remaining_entities = 0;
    std::uint64_t retired_entries = 0;
    std::uint64_t model_base_bytes = 0;
    std::uint64_t retired_bytes = 0;
    std::uint64_t staging_bytes = 0;
};
[[nodiscard]] Sample preparation_sample(const SceneGraphicsPreparationReport& report)
{
    const auto& activity = report.activity;
    Sample sample;
    sample.phase = SamplePhase::preparation;
    sample.reported_ms = activity.milliseconds + report.retirement.activity.milliseconds;
    sample.units = activity.work_units + report.retirement.activity.work_units;
    sample.calls = activity.driver_calls + report.retirement.activity.driver_calls;
    sample.uploaded = activity.uploaded_bytes;
    sample.allocated = activity.allocated_bytes;
    sample.longest_ms = activity.longest_step.milliseconds;
    sample.longest_phase = static_cast<int>(activity.longest_step.phase);
    if (activity.longest_step.shader_pipeline)
        sample.shader_kind = static_cast<int>(*activity.longest_step.shader_pipeline);
    if (activity.longest_step.shader_step)
        sample.shader_step = static_cast<int>(*activity.longest_step.shader_step);
    sample.remaining_entities = report.status.entity_slots - report.status.completed_entity_slots;
    sample.model_base_bytes = report.status.allocated_bytes;
    return sample;
}
[[nodiscard]] Sample retirement_sample(const GraphicsRetirementReport& report)
{
    Sample sample;
    sample.phase = SamplePhase::retirement;
    sample.reported_ms = report.activity.milliseconds;
    sample.units = report.activity.work_units;
    sample.calls = report.activity.driver_calls;
    sample.longest_ms = report.activity.longest_step.milliseconds;
    sample.longest_phase = static_cast<int>(report.activity.longest_step.phase);
    sample.retired_entries = report.status.mesh_entries + report.status.texture_entries;
    sample.retired_bytes =
        report.status.estimated_geometry_bytes + report.status.estimated_texture_bytes;
    sample.staging_bytes = report.status.staging_bytes;
    return sample;
}
class ServiceBenchmark final : public Application {
  public:
    explicit ServiceBenchmark(Options options) : options_(std::move(options))
    {
    }
    Result<void> start(ApplicationContext& context) noexcept override
    {
        try {
            stream_.open(options_.report, std::ios::binary | std::ios::trunc);
            if (!stream_)
                return Error{ErrorCode::source_file_write_failed,
                             "Cannot open graphics service report"};
            stream_ << "scope,fixture,calls,cycle,phase,frame,wall_ms,reported_ms,phase_ms,work_"
                       "units,driver_calls,uploaded_bytes,allocated_bytes,longest_ms,longest_phase,"
                       "shader_kind,shader_step,remaining_entity_slots,retired_resource_entries,"
                       "model_allocated_base_bytes,retired_estimated_bytes,staging_bytes,shared_"
                       "environment_bytes\n";
            budget_.driver_calls = options_.calls;
            return create(context.engine());
        } catch (...) {
            fatal_error("Graphics service benchmark startup failed unexpectedly");
        }
    }
    Result<void> update(ApplicationUpdateContext& context) noexcept override
    {
        try {
            if (++frames_ > 200000)
                return Error{ErrorCode::invalid_argument,
                             "Graphics service benchmark exceeded its frame bound"};
            switch (phase_) {
            case Phase::creating:
                return create(context.engine());
            case Phase::preparing:
                return prepare(context.engine());
            case Phase::detaching:
                return detach();
            case Phase::retiring:
                return retire(context.engine());
            case Phase::finished:
                stream_.flush();
                if (!stream_)
                    return Error{ErrorCode::source_file_write_failed,
                                 "Graphics service report commit failed"};
                context.request_exit();
                return {};
            }
            return Error{ErrorCode::invalid_argument, "Invalid graphics service benchmark phase"};
        } catch (...) {
            fatal_error("Graphics service benchmark update failed unexpectedly");
        }
    }
    Result<void> build_ui(ApplicationUiContext&) noexcept override
    {
        return {};
    }
    void stop(ApplicationContext&) noexcept override
    {
        scene_.reset();
    }

  private:
    enum class Phase { creating, preparing, detaching, retiring, finished };
    [[nodiscard]] Result<void> write(Sample sample)
    {
        stream_ << "explicit_service_procedural_runtime," << options_.name << ',' << options_.calls
                << ',' << cycle_ << ',' << phase_name(sample.phase) << ',' << frames_ << ','
                << sample.wall_ms << ',' << sample.reported_ms << ',' << sample.phase_ms << ','
                << sample.units << ',' << sample.calls << ',' << sample.uploaded << ','
                << sample.allocated << ',' << sample.longest_ms << ',' << sample.longest_phase
                << ',' << sample.shader_kind << ',' << sample.shader_step << ','
                << sample.remaining_entities << ',' << sample.retired_entries << ','
                << sample.model_base_bytes << ',' << sample.retired_bytes << ','
                << sample.staging_bytes << ',' << shared_bytes_ << '\n';
        if (!stream_)
            return Error{ErrorCode::source_file_write_failed,
                         "Graphics service report write failed"};
        return {};
    }
    [[nodiscard]] Result<void> create(Engine& engine)
    {
        const auto begin = Clock::now();
        auto created = make_service_fixture(engine, options_.fixture);
        if (!created)
            return created.error();
        scene_ = std::move(created).value();
        Sample sample;
        sample.phase = SamplePhase::setup;
        sample.wall_ms = elapsed(begin);
        phase_ = Phase::preparing;
        phase_begin_ = Clock::now();
        return write(sample);
    }
    [[nodiscard]] Result<void> prepare(Engine& engine)
    {
        const auto begin = Clock::now();
        const auto report = engine.advance_scene_graphics(*scene_, budget_);
        const auto wall = elapsed(begin);
        if (!report)
            return report.error();
        auto sample = preparation_sample(report.value());
        sample.wall_ms = wall;
        sample.phase_ms = elapsed(phase_begin_);
        shared_bytes_ = report.value().status.shared_environment_bytes;
        if (report.value().status.state == GraphicsPreparationState::ready)
            phase_ = Phase::detaching;
        return write(sample);
    }
    [[nodiscard]] Result<void> detach()
    {
        const auto begin = Clock::now();
        scene_.reset();
        Sample sample;
        sample.phase = SamplePhase::detach;
        sample.wall_ms = elapsed(begin);
        phase_ = Phase::retiring;
        phase_begin_ = Clock::now();
        return write(sample);
    }
    [[nodiscard]] Result<void> retire(Engine& engine)
    {
        const auto begin = Clock::now();
        const auto report =
            engine.advance_graphics_retirement({budget_.milliseconds, budget_.work_units,
                                                budget_.driver_calls, budget_.retired_objects});
        const auto wall = elapsed(begin);
        if (!report)
            return report.error();
        auto sample = retirement_sample(report.value());
        sample.wall_ms = wall;
        sample.phase_ms = elapsed(phase_begin_);
        if (report.value().status.empty()) {
            if (sample.retired_bytes != 0 || sample.staging_bytes != 0 ||
                sample.retired_entries != 0)
                return Error{ErrorCode::invalid_argument,
                             "Graphics service did not return to its model-resource baseline"};
        }
        const auto written = write(sample);
        if (!written)
            return written.error();
        if (report.value().status.empty()) {
            ++cycle_;
            phase_ = cycle_ == options_.cycles ? Phase::finished : Phase::creating;
        }
        return {};
    }
    Options options_;
    GraphicsPreparationBudget budget_;
    std::ofstream stream_;
    std::unique_ptr<Scene> scene_;
    Clock::time_point phase_begin_;
    Phase phase_ = Phase::creating;
    std::uint32_t cycle_ = 0;
    std::uint32_t frames_ = 0;
    std::uint64_t shared_bytes_ = 0;
};
} // namespace
int run_service_benchmark(int count, char** values)
{
    try {
        const auto parsed = options(count, values);
        if (!parsed) {
            std::cerr << "Usage: elf3d_render_benchmark --service "
                         "<large-mesh|many-meshes|large-image|many-images> <calls:32..4096> "
                         "<cycles:1..10> <csv>\n";
            return 2;
        }
        ServiceBenchmark application{*parsed};
        ApplicationOptions configuration;
        configuration.title = "Elf3D graphics service benchmark";
        configuration.initial_window_extent = {96, 96};
        configuration.initial_visibility = ApplicationWindowVisibility::hidden;
        configuration.presentation_mode = PresentationMode::immediate;
        const auto result = run_application(configuration, application);
        if (!result) {
            std::cerr << result.error().message() << '\n';
            return 7;
        }
        return result.value();
    } catch (const std::bad_alloc&) {
        fatal_error("Graphics service benchmark allocation failed");
    } catch (...) {
        fatal_error("Graphics service benchmark failed unexpectedly");
    }
}
} // namespace elf3d::tools
