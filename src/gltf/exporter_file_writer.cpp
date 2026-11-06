#include "exporter_file_writer.h"

#include "file_path.hpp"
#include <elf3d/core/assert.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <utility>

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;
namespace {
[[nodiscard]] Result<void> validate_options(const OutputPath& path, ModelWriteOptions options)
{
    const auto extension = path.extension().string();
    if (extension != ".glb" && extension != ".gltf")
        return Error{ErrorCode::unsupported_model_format,
                     "Document output paths must use a .gltf or .glb extension"};
    if (options.numeric_policy != ModelNumericWritePolicy::round_trip &&
        options.numeric_policy != ModelNumericWritePolicy::clean_rotation)
        return Error{ErrorCode::invalid_argument, "Invalid model numeric write policy"};
    if (options.image_policy != ModelImageWritePolicy::automatic &&
        options.image_policy != ModelImageWritePolicy::embedded &&
        options.image_policy != ModelImageWritePolicy::external)
        return Error{ErrorCode::invalid_argument, "Invalid model image write policy"};
    return {};
}
[[nodiscard]] std::string output_stem(const OutputPath& path)
{
    const auto utf8 = path.stem().u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t c : utf8)
        result.push_back(std::bit_cast<char>(c));
    return result;
}
} // namespace

ExportFileWriter::~ExportFileWriter() noexcept = default;
Result<std::unique_ptr<ExportFileWriter>>
ExportFileWriter::create(OutputPath path, WriteSourceDescription source, ModelWriteOptions options)
{
    if (const auto valid = validate_options(path, options); !valid)
        return valid.error();
    auto metadata = ExportSourceMetadata::create(source);
    if (!metadata)
        return metadata.error();
    auto result = std::make_unique<ExportFileWriter>();
    result->path_ = std::move(path);
    result->options_ = options;
    result->package_.document.metadata_owner = std::move(metadata).value();
    result->package_.document.preserved_metadata_dropped = source.preserved_metadata_stale;
    result->package_.numeric_policy = options.numeric_policy;
    result->package_.glb = result->path_.extension() == ".glb";
    result->package_.embedded =
        options.image_policy == ModelImageWritePolicy::embedded ||
        (options.image_policy == ModelImageWritePolicy::automatic && result->package_.glb);
    result->stem_ = output_stem(result->path_);
    result->package_.buffer_uri = result->stem_ + ".bin";
    return result;
}
std::optional<ExportFileRequest> ExportFileWriter::request() const noexcept
{
    ExportFileRequest result;
    if (phase_ == ExportFilePhase::metadata) {
        const auto metadata = package_.document.metadata_owner->request();
        if (metadata.input == ExportMetadataInput::complete)
            return std::nullopt;
        result.metadata = metadata;
        if (metadata.input == ExportMetadataInput::text)
            result.input = ExportFileInput::text;
        else if (metadata.input == ExportMetadataInput::links)
            result.input = ExportFileInput::links;
        return result;
    }
    if (phase_ != ExportFilePhase::geometry && phase_ != ExportFilePhase::images)
        return std::nullopt;
    const auto data = payload_request();
    if (!data)
        return std::nullopt;
    result.input = ExportFileInput::payload;
    result.data = *data;
    return result;
}
std::optional<WriteSourceDataRequest> ExportFileWriter::payload_request() const noexcept
{
    if (ordinary_)
        return ordinary_->request();
    return image_ ? image_->request() : std::nullopt;
}
Result<void> ExportFileWriter::accept_record(const WriteSourceRecord& record)
{
    const auto expected = request();
    if (!expected || expected->input != ExportFileInput::record)
        return Error{ErrorCode::invalid_argument, "File writer is not expecting a metadata record"};
    return package_.document.metadata_owner->accept_record(record);
}
Result<void> ExportFileWriter::accept_text(WriteSourceChunk chunk, std::span<const char> text)
{
    const auto expected = request();
    if (!expected || expected->input != ExportFileInput::text)
        return Error{ErrorCode::invalid_argument, "File writer is not expecting metadata text"};
    return package_.document.metadata_owner->accept_text(chunk, text);
}
Result<void> ExportFileWriter::accept_data(WriteSourceChunk chunk, std::span<const std::byte> bytes)
{
    const auto expected = request();
    if (!expected)
        return Error{ErrorCode::invalid_argument, "File writer is not expecting source bytes"};
    if (expected->input == ExportFileInput::links)
        return package_.document.metadata_owner->accept_links(chunk, bytes);
    if (expected->input != ExportFileInput::payload)
        return Error{ErrorCode::invalid_argument, "File writer expects another input type"};
    if (ordinary_)
        return ordinary_->accept(expected->data, chunk, bytes, package_);
    ELF3D_ASSERT(image_);
    return image_->accept(expected->data, chunk, bytes);
}
Result<void> ExportFileWriter::validate_metadata()
{
    if (const auto bound = package_.document.metadata_owner->bind(package_.document); !bound)
        return bound.error();
    if (const auto valid = validate_owned_export(package_.document); !valid)
        return valid.error();
    apply_export_metadata_policy(package_.document);
    if (const auto opened = package_.binary.open(path_); !opened)
        return opened.error();
    phase_ = ExportFilePhase::geometry;
    return {};
}
Result<void> ExportFileWriter::begin_geometry()
{
    const auto source = package_.document.metadata_owner->primitive_source(index_);
    if (!source)
        return source.error();
    auto ordinary = ExportGeometryInput::create(source.value(), package_.document);
    if (!ordinary)
        return ordinary.error();
    ordinary_ = std::move(ordinary).value();
    return {};
}
Result<void> ExportFileWriter::finish_geometry(PrimitiveOutput output)
{
    package_.primitives.push_back(output);
    ordinary_.reset();
    ++index_;
    return {};
}
Result<void> ExportFileWriter::advance_geometry()
{
    if (index_ == package_.document.primitives.size()) {
        index_ = 0;
        phase_ = ExportFilePhase::images;
        return {};
    }
    if (ordinary_)
        return finish_geometry(ordinary_->output().value());
    return begin_geometry();
}
bool ExportFileWriter::ready() const noexcept
{
    return phase_ == ExportFilePhase::completed || phase_ == ExportFilePhase::cancelled ||
           phase_ == ExportFilePhase::failed;
}
ExportFileProgress ExportFileWriter::progress() const noexcept
{
    if (phase_ == ExportFilePhase::geometry)
        return {phase_, index_, package_.document.primitives.size()};
    if (phase_ == ExportFilePhase::images || phase_ == ExportFilePhase::image_output)
        return {phase_, index_, package_.document.images.size()};
    return {phase_};
}
Result<void> ExportFileWriter::advance_work()
{
    switch (phase_) {
    case ExportFilePhase::metadata:
        phase_ = ExportFilePhase::validating;
        return {};
    case ExportFilePhase::validating:
        return validate_metadata();
    case ExportFilePhase::geometry:
        return advance_geometry();
    case ExportFilePhase::images:
        return advance_images();
    case ExportFilePhase::image_output:
        return append_image_output();
    case ExportFilePhase::json: {
        auto json = build_json(package_);
        if (!json)
            return json.error();
        json_ = std::move(json).value();
        outcome_.report = build_write_report(package_.encoded_images,
                                             package_.document.preserved_metadata_dropped,
                                             package_.cleaned_matrices);
        phase_ = ExportFilePhase::staging;
        return {};
    }
    default:
        return advance_staging();
    }
}
void ExportFileWriter::reject_input(Error error) noexcept
{
    ELF3D_ASSERT(!ready());
    if (!outcome_.error)
        outcome_.error = std::move(error);
    phase_ = ExportFilePhase::cleanup;
}
Result<void> ExportFileWriter::advance(ExportFileControl control)
{
    if (ready())
        return Error{ErrorCode::invalid_argument, "File writer is already complete"};
    if (control.cancel_requested && !outcome_.committed && phase_ != ExportFilePhase::cleanup) {
        outcome_.cancelled = true;
        phase_ = ExportFilePhase::cleanup;
    }
    if (phase_ == ExportFilePhase::cleanup) {
        finish_cleanup();
        return {};
    }
    if (request())
        return Error{ErrorCode::invalid_argument, "File writer needs source input"};
    if (const auto advanced = advance_work(); !advanced)
        reject_input(advanced.error());
    return {};
}
Result<ExportFileOutcome> ExportFileWriter::take_outcome()
{
    if (!ready() || outcome_taken_)
        return Error{ErrorCode::invalid_argument, "File writer outcome is not available"};
    outcome_taken_ = true;
    return std::move(outcome_);
}
} // namespace elf3d::gltf::exporter_detail
