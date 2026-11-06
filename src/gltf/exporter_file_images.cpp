#include "exporter_file_writer.h"

#include "file_path.hpp"
#include <elf3d/core/assert.h>

#include <algorithm>
#include <array>
#include <utility>

namespace elf3d::gltf::exporter_detail {
namespace {
[[nodiscard]] std::string_view image_extension(ModelImageMimeType mime) noexcept
{
    if (mime == ModelImageMimeType::tiff)
        return ".tiff";
    if (mime == ModelImageMimeType::exr)
        return ".exr";
    return mime == ModelImageMimeType::jpeg ? ".jpg" : ".png";
}
} // namespace
Result<void> ExportFileWriter::advance_images()
{
    if (index_ == package_.document.images.size()) {
        phase_ = ExportFilePhase::json;
        return {};
    }
    if (!image_) {
        const auto source = package_.document.metadata_owner->image_source(index_);
        if (!source)
            return source.error();
        auto image = ExportImageInput::create(source.value(), package_.document, path_);
        if (!image)
            return image.error();
        image_ = std::move(image).value();
        return {};
    }
    if (image_->needs_advance())
        return image_->advance();
    ELF3D_ASSERT(image_->ready());
    return begin_image_output();
}
Result<void> ExportFileWriter::begin_image_output()
{
    auto output = image_->take_output();
    if (!output)
        return output.error();
    image_output_ = std::move(output).value();
    package_.encoded_images.push_back({image_output_->mime_type, image_output_->reencoded});
    package_.image_mime_types.push_back(image_output_->mime_type);
    package_.image_uris.push_back(stem_ + ".image_" + std::to_string(index_) +
                                  std::string{image_extension(image_output_->mime_type)});
    if (image_output_->data.size() == 0)
        return Error{ErrorCode::image_encode_failed, "Encoded image contains no bytes"};
    image_offset_ = 0;
    image_range_.reset();
    phase_ = ExportFilePhase::image_output;
    return {};
}
Result<void> ExportFileWriter::append_image_output()
{
    ELF3D_ASSERT(image_output_);
    if (!package_.embedded) {
        const auto image_path = file_path::from_utf8(package_.image_uris.back());
        if (!image_path)
            return image_path.error();
        auto path = image_output_->data.take_path();
        if (!path)
            return path.error();
        files_.adopt(path_.parent_path() / image_path.value(), std::move(path).value());
        package_.image_views.push_back(std::nullopt);
        return finish_image_output();
    }
    auto& source = image_output_->data;
    std::array<std::byte, 64 * 1024> bytes;
    const auto count = std::min(bytes.size(), source.size() - image_offset_);
    const auto chunk = std::span{bytes}.first(count);
    if (const auto read = source.read(image_offset_, chunk); !read)
        return read.error();
    const auto appended = package_.binary.append_bytes(chunk);
    if (!appended)
        return appended.error();
    if (!image_range_)
        image_range_ = appended.value();
    else {
        ELF3D_ASSERT(appended.value().offset == image_range_->offset + image_range_->length);
        image_range_->length += appended.value().length;
    }
    image_offset_ += count;
    if (image_offset_ != source.size())
        return {};
    const auto view = append_buffer_view(package_.views, *image_range_);
    if (!view)
        return view.error();
    package_.image_views.push_back(view.value());
    return finish_image_output();
}
Result<void> ExportFileWriter::finish_image_output()
{
    if (const auto discarded = image_output_->data.discard(); !discarded)
        return discarded.error();
    image_output_.reset();
    if (const auto discarded = image_->discard(); !discarded)
        return discarded.error();
    image_.reset();
    ++index_;
    phase_ = ExportFilePhase::images;
    return {};
}
} // namespace elf3d::gltf::exporter_detail
