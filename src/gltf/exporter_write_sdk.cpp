#include "exporter_write_sdk.h"

#include "file_path.hpp"
#include <core/exception_boundary.h>

#include <array>
#include <cstring>
#include <type_traits>
#include <utility>

namespace elf3d {
using namespace gltf::exporter_detail;
namespace model::detail {
ModelWriteSource WriteSdkAccess::capture(DocumentWriteSource source) noexcept
{
    ModelWriteSource result;
    const auto description = source.description();
    const auto owner = source.owner();
    result.owner_domain_ = owner.domain;
    result.owner_sequence_ = owner.sequence;
    result.revision_ = source.revision();
    result.counts_ = description.counts;
    result.default_scene_ = description.default_scene;
    result.required_extensions_ = description.required_extensions;
    result.metadata_stale_ = description.preserved_metadata_stale;
    return result;
}
DocumentWriteSource WriteSdkAccess::reader(const ModelWriteSource& source) noexcept
{
    return DocumentWriteSource::restore({source.owner_domain_, source.owner_sequence_},
                                        source.revision_,
                                        {source.counts_, source.default_scene_,
                                         source.required_extensions_, source.metadata_stale_});
}
bool WriteSdkAccess::valid(const ModelWriteSource& source) noexcept
{
    return source.owner_domain_ != 0 && source.owner_sequence_ != 0;
}
ModelWriteInputRequest WriteSdkAccess::encode(ExportFileRequest request,
                                              const ModelWriteSource& source) noexcept
{
    ModelWriteInputRequest result;
    auto& f = result.fields_;
    f[0] = source.owner_domain_;
    f[1] = source.owner_sequence_;
    f[2] = source.revision_;
    f[3] = static_cast<std::uint64_t>(request.input);
    const auto address =
        request.input == ExportFileInput::payload ? request.data.address : request.metadata.address;
    f[4] = static_cast<std::uint64_t>(address.object);
    f[5] = address.index;
    if (request.input == ExportFileInput::text) {
        f[6] = static_cast<std::uint64_t>(request.metadata.text.text);
        f[7] = request.metadata.text.item;
        f[8] = request.metadata.text.offset;
    } else if (request.input == ExportFileInput::links ||
               request.input == ExportFileInput::payload) {
        const auto data =
            request.input == ExportFileInput::links ? request.metadata.data : request.data;
        f[6] = static_cast<std::uint64_t>(data.data);
        f[8] = data.offset;
    }
    return result;
}
Result<ExportFileRequest> WriteSdkAccess::decode(ModelWriteInputRequest request,
                                                 const ModelWriteSource& source) noexcept
{
    const auto& f = request.fields_;
    if (f[0] != source.owner_domain_ || f[1] != source.owner_sequence_ || f[2] != source.revision_)
        return Error{ErrorCode::invalid_argument,
                     "Write input request belongs to another source or revision"};
    if (f[3] > static_cast<std::uint64_t>(ExportFileInput::payload) ||
        f[4] >= write_source_object_count)
        return Error{ErrorCode::invalid_argument,
                     "Write input request has an invalid kind or address"};
    ExportFileRequest result;
    result.input = static_cast<ExportFileInput>(f[3]);
    const WriteSourceAddress address{static_cast<WriteSourceObject>(f[4]),
                                     static_cast<std::size_t>(f[5])};
    result.metadata.address = address;
    result.metadata.input =
        result.input == ExportFileInput::text
            ? ExportMetadataInput::text
            : (result.input == ExportFileInput::links ? ExportMetadataInput::links
                                                      : ExportMetadataInput::record);
    result.metadata.text = {address, static_cast<WriteSourceText>(f[6]),
                            static_cast<std::size_t>(f[7]), static_cast<std::size_t>(f[8])};
    result.data = {address, static_cast<WriteSourceData>(f[6]), static_cast<std::size_t>(f[8])};
    result.metadata.data = result.data;
    return result;
}
} // namespace model::detail

Result<ModelWriteSource> ModelWriteSource::create(DocumentView document) noexcept
{
    const auto source = model::detail::DocumentWriteSource::create(document);
    return source ? Result<ModelWriteSource>{model::detail::WriteSdkAccess::capture(source.value())}
                  : Result<ModelWriteSource>{source.error()};
}
bool ModelWriteSource::matches_document(DocumentView document) const noexcept
{
    return model::detail::WriteSdkAccess::reader(*this).matches(document);
}
std::uint64_t ModelWriteSource::document_revision() const noexcept
{
    return revision_;
}
Result<std::size_t> ModelWriteSource::read(DocumentView document, ModelWriteInputRequest request,
                                           std::span<std::byte> destination) const noexcept
{
    return detail::fatal_exception_boundary(
        [&] { return model::detail::WriteSdkAccess::read(*this, document, request, destination); });
}

class ModelFileWriter::Impl {
  public:
    ModelWriteSource source;
    std::unique_ptr<ExportFileWriter> writer;
};
ModelFileWriter::ModelFileWriter() noexcept = default;
ModelFileWriter::~ModelFileWriter() noexcept = default;
ModelFileWriter::ModelFileWriter(ModelFileWriter&&) noexcept = default;
ModelFileWriter& ModelFileWriter::operator=(ModelFileWriter&&) noexcept = default;
Result<ModelFileWriter> ModelFileWriter::create(std::string_view path_utf8, ModelWriteSource source,
                                                const ModelWriteOptions& options) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<ModelFileWriter> {
        if (!model::detail::WriteSdkAccess::valid(source))
            return Error{ErrorCode::invalid_argument,
                         "File writer requires a captured Document source"};
        const auto path = gltf::file_path::from_utf8(path_utf8);
        if (!path)
            return path.error();
        auto writer = ExportFileWriter::create(
            path.value(), model::detail::WriteSdkAccess::reader(source).description(), options);
        if (!writer)
            return writer.error();
        ModelFileWriter result;
        result.impl_ = std::make_unique<Impl>();
        result.impl_->source = source;
        result.impl_->writer = std::move(writer).value();
        return result;
    });
}
std::optional<ModelWriteInputRequest> ModelFileWriter::input_request() const noexcept
{
    if (!impl_)
        return std::nullopt;
    const auto request = impl_->writer->request();
    return request ? std::optional{model::detail::WriteSdkAccess::encode(*request, impl_->source)}
                   : std::nullopt;
}
Result<void> ModelFileWriter::provide(std::span<const std::byte> packet) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        if (!impl_)
            return Error{ErrorCode::invalid_argument, "File writer is empty"};
        return model::detail::WriteSdkAccess::provide(*impl_->writer, impl_->source, packet);
    });
}
Result<void> ModelFileWriter::reject_input(Error error) noexcept
{
    if (!impl_ || impl_->writer->ready() || !impl_->writer->request())
        return Error{ErrorCode::invalid_argument, "File writer has no pending source input"};
    impl_->writer->reject_input(std::move(error));
    return {};
}
Result<void> ModelFileWriter::advance(const ModelWriteAdvanceOptions& options) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        if (!impl_)
            return Error{ErrorCode::invalid_argument, "File writer is empty"};
        return impl_->writer->advance({options.cancel_requested});
    });
}
bool ModelFileWriter::ready() const noexcept
{
    return impl_ && impl_->writer->ready();
}
ModelWriteProgress ModelFileWriter::progress() const noexcept
{
    if (!impl_)
        return {};
    const auto value = impl_->writer->progress();
    return value;
}
Result<ModelWriteOutcome> ModelFileWriter::take_outcome() noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<ModelWriteOutcome> {
        if (!impl_)
            return Error{ErrorCode::invalid_argument, "File writer is empty"};
        return impl_->writer->take_outcome();
    });
}
Result<void> ModelFileWriter::discard() noexcept
{
    return detail::fatal_exception_boundary(
        [&]() -> Result<void> { return impl_ ? impl_->writer->discard() : Result<void>{}; });
}
} // namespace elf3d
