#pragma once

#include "exporter_file_writer.h"
#include <elf3d/model_write.h>
#include <model/document_write_source.h>

#include <span>

namespace elf3d::model::detail {
struct WriteSdkPacketHeader {
    ModelWriteInputRequest request;
    WriteSourceChunk chunk;
};
static_assert(sizeof(WriteSdkPacketHeader) <= 256);
class WriteSdkAccess final {
  public:
    [[nodiscard]] static ModelWriteSource capture(DocumentWriteSource source) noexcept;
    [[nodiscard]] static DocumentWriteSource reader(const ModelWriteSource& source) noexcept;
    [[nodiscard]] static bool valid(const ModelWriteSource& source) noexcept;
    [[nodiscard]] static ModelWriteInputRequest
    encode(gltf::exporter_detail::ExportFileRequest request,
           const ModelWriteSource& source) noexcept;
    [[nodiscard]] static Result<gltf::exporter_detail::ExportFileRequest>
    decode(ModelWriteInputRequest request, const ModelWriteSource& source) noexcept;
    [[nodiscard]] static Result<void> provide(gltf::exporter_detail::ExportFileWriter& writer,
                                              const ModelWriteSource& source,
                                              std::span<const std::byte> packet);
    [[nodiscard]] static Result<std::size_t> read(const ModelWriteSource& source,
                                                  DocumentView document,
                                                  ModelWriteInputRequest request,
                                                  std::span<std::byte> destination);
};
} // namespace elf3d::model::detail
