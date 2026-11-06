#pragma once

#include "exporter_container.hpp"
#include "exporter_geometry_input.h"
#include "exporter_image_input.h"
#include "exporter_output.hpp"
#include <elf3d/model_write.h>

#include <cstddef>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace elf3d::gltf::exporter_detail {
using ExportFilePhase = ModelWritePhase;
using ExportFileProgress = ModelWriteProgress;
using ExportFileControl = ModelWriteAdvanceOptions;
using ExportFileOutcome = ModelWriteOutcome;
enum class ExportFileInput { record, text, links, payload };
struct ExportFileRequest {
    ExportFileInput input = ExportFileInput::record;
    ExportMetadataRequest metadata;
    model::detail::WriteSourceDataRequest data;
};
// Explicitly driven file operation. The owner supplies copied source records
// and bounded bytes; no Document, Scene, callback, thread or input view survives
// a call. All allocation, validation, codecs, file IO and disposal belong to the
// calling CPU owner. An entered commit finishes before late Stop is considered.
class ExportFileWriter final {
  public:
    ~ExportFileWriter() noexcept;
    [[nodiscard]] static Result<std::unique_ptr<ExportFileWriter>>
    create(OutputPath path, model::detail::WriteSourceDescription source,
           ModelWriteOptions options);
    [[nodiscard]] std::optional<ExportFileRequest> request() const noexcept;
    [[nodiscard]] Result<void> accept_record(const model::detail::WriteSourceRecord& record);
    [[nodiscard]] Result<void> accept_text(model::detail::WriteSourceChunk chunk,
                                           std::span<const char> text);
    [[nodiscard]] Result<void> accept_data(model::detail::WriteSourceChunk chunk,
                                           std::span<const std::byte> bytes);
    [[nodiscard]] Result<void> advance(ExportFileControl control = {});
    void reject_input(Error error) noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] ExportFileProgress progress() const noexcept;
    [[nodiscard]] Result<ExportFileOutcome> take_outcome();
    [[nodiscard]] Result<void> discard();

  private:
    [[nodiscard]] std::optional<model::detail::WriteSourceDataRequest>
    payload_request() const noexcept;
    [[nodiscard]] Result<void> validate_metadata();
    [[nodiscard]] Result<void> advance_geometry();
    [[nodiscard]] Result<void> begin_geometry();
    [[nodiscard]] Result<void> finish_geometry(PrimitiveOutput output);
    [[nodiscard]] Result<void> advance_images();
    [[nodiscard]] Result<void> begin_image_output();
    [[nodiscard]] Result<void> append_image_output();
    [[nodiscard]] Result<void> finish_image_output();
    [[nodiscard]] Result<void> advance_staging();
    [[nodiscard]] Result<void> advance_json_staging();
    [[nodiscard]] Result<void> advance_work();
    void finish_cleanup();
    [[nodiscard]] Result<void> release_working_data();
    OutputPath path_;
    ModelWriteOptions options_;
    ExportPackage package_;
    exporter_output::OutputFiles files_;
    ExportFilePhase phase_ = ExportFilePhase::metadata;
    ExportFileOutcome outcome_;
    std::optional<ExportGeometryInput> ordinary_;
    std::optional<ExportImageInput> image_;
    std::optional<EncodedImageOutput> image_output_;
    std::optional<ByteRange> image_range_;
    std::size_t index_ = 0;
    std::size_t image_offset_ = 0;
    std::string stem_;
    std::string json_;
    std::optional<ExportGlbOutput> glb_;
    std::ofstream json_stream_;
    std::size_t json_offset_ = 0;
    bool outcome_taken_ = false;
};
[[nodiscard]] Result<void> validate_owned_export(const ExportData& document);
void apply_export_metadata_policy(ExportData& document) noexcept;
[[nodiscard]] ModelWriteReport build_write_report(std::span<const EncodedImageInfo> images,
                                                  bool metadata_dropped,
                                                  std::size_t cleaned_matrices);
} // namespace elf3d::gltf::exporter_detail
