#include "exporter_file_writer.h"

#include "file_path.hpp"
#include <elf3d/core/assert.h>

#include <algorithm>
#include <utility>

namespace elf3d::gltf::exporter_detail {
Result<void> ExportFileWriter::advance_json_staging()
{
    if (!json_stream_.is_open()) {
        if (package_.binary.size() != 0) {
            const auto uri = file_path::from_utf8(package_.buffer_uri);
            if (!uri)
                return uri.error();
            auto path = package_.binary.take_path();
            if (!path)
                return path.error();
            files_.adopt(path_.parent_path() / uri.value(), std::move(path).value());
        }
        const auto path = files_.stage(path_);
        if (!path)
            return path.error();
        json_stream_.open(path.value(), std::ios::binary | std::ios::trunc);
        if (!json_stream_)
            return Error{ErrorCode::source_file_write_failed,
                         "Cannot open staged glTF JSON output"};
    }
    const auto count = std::min<std::size_t>(64 * 1024, json_.size() - json_offset_);
    const auto bytes =
        std::as_bytes(std::span{json_.data(), json_.size()}).subspan(json_offset_, count);
    if (const auto written = exporter_output::write_bytes(json_stream_, bytes); !written)
        return written.error();
    json_offset_ += count;
    if (json_offset_ != json_.size())
        return {};
    if (const auto closed = exporter_output::close_file(json_stream_); !closed)
        return closed.error();
    phase_ = ExportFilePhase::commit;
    return {};
}
Result<void> ExportFileWriter::advance_staging()
{
    if (phase_ == ExportFilePhase::commit) {
        if (const auto committed = files_.publish(); !committed)
            return committed.error();
        outcome_.committed = true;
        phase_ = ExportFilePhase::cleanup;
        return {};
    }
    ELF3D_ASSERT(phase_ == ExportFilePhase::staging);
    if (!package_.glb)
        return advance_json_staging();
    if (!glb_) {
        auto output = ExportGlbOutput::create(path_, json_.size(), package_.binary.size(), files_);
        if (!output)
            return output.error();
        glb_ = std::move(output).value();
        return {};
    }
    if (!glb_->ready())
        return glb_->advance(json_, package_.binary);
    phase_ = ExportFilePhase::commit;
    return {};
}
Result<void> ExportFileWriter::release_working_data()
{
    std::optional<Error> failure;
    const auto collect = [&](Result<void> result) {
        if (!result && !failure)
            failure = result.error();
    };
    if (glb_)
        collect(glb_->discard());
    if (json_stream_.is_open())
        collect(exporter_output::close_file(json_stream_));
    if (image_)
        collect(image_->discard());
    if (image_output_)
        collect(image_output_->data.discard());
    collect(files_.discard());
    collect(package_.binary.discard());
    return failure ? Result<void>{*failure} : Result<void>{};
}
void ExportFileWriter::finish_cleanup()
{
    if (const auto released = release_working_data(); !released)
        outcome_.cleanup_error = released.error();
    ordinary_.reset();
    image_.reset();
    image_output_.reset();
    glb_.reset();
    package_ = {};
    std::string{}.swap(json_);
    if (outcome_.error || outcome_.cleanup_error)
        phase_ = ExportFilePhase::failed;
    else
        phase_ = outcome_.cancelled ? ExportFilePhase::cancelled : ExportFilePhase::completed;
}
Result<void> ExportFileWriter::discard()
{
    if (ready())
        return release_working_data();
    if (!outcome_.committed && !outcome_.error)
        outcome_.cancelled = true;
    phase_ = ExportFilePhase::cleanup;
    finish_cleanup();
    return outcome_.cleanup_error ? Result<void>{*outcome_.cleanup_error} : Result<void>{};
}
} // namespace elf3d::gltf::exporter_detail
