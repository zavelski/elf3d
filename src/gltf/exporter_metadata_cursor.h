#pragma once

#include <model/document_write_source.h>
#include <optional>

namespace elf3d::gltf::exporter_detail {
enum class ExportMetadataInput { record, text, links, complete };
struct ExportMetadataRequest {
    ExportMetadataInput input = ExportMetadataInput::record;
    model::detail::WriteSourceAddress address;
    model::detail::WriteSourceTextRequest text;
    model::detail::WriteSourceDataRequest data;
};

class ExportSourceMetadata;
class ExportMetadataCursor final {
  public:
    [[nodiscard]] static Result<ExportMetadataCursor>
    at(model::detail::WriteSourceDescription source, ExportMetadataRequest request,
       model::detail::WriteSourceRecord record);
    [[nodiscard]] ExportMetadataRequest request() const noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] Result<void>
    validate_record(const model::detail::WriteSourceRecord& record) const;
    [[nodiscard]] Result<void> accept_record(const model::detail::WriteSourceRecord& record);
    [[nodiscard]] Result<void> accept_text(model::detail::WriteSourceChunk chunk);
    [[nodiscard]] Result<void> accept_links(model::detail::WriteSourceChunk chunk);

  private:
    friend class ExportSourceMetadata;
    enum class Phase {
        record,
        name,
        extras,
        extension_name,
        extension_data,
        links,
        required_extension,
        complete
    };
    [[nodiscard]] model::detail::WriteSourceAddress address() const noexcept;
    void next_field(Phase after);
    void next_record();
    void finish_text();
    [[nodiscard]] Result<void> validate_text_sequence(model::detail::WriteSourceChunk chunk);
    model::detail::WriteSourceDescription description_;
    model::detail::WriteSourceRecord current_;
    Phase phase_ = Phase::record;
    std::size_t category_ = 0, index_ = 0, item_ = 0, offset_ = 0;
    std::optional<std::size_t> text_size_;
};
} // namespace elf3d::gltf::exporter_detail
