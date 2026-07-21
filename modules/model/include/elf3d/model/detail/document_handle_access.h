#ifndef ELF3D_MODEL_DETAIL_DOCUMENT_HANDLE_ACCESS_H
#define ELF3D_MODEL_DETAIL_DOCUMENT_HANDLE_ACCESS_H

#include <elf3d/model_ids.h>

#include <cstdint>

namespace elf3d::model::detail {

class DocumentHandleAccess final {
  public:
    [[nodiscard]] static constexpr DocumentSceneId create_scene(std::uint64_t document_token,
                                                                std::uint64_t value) noexcept {
        return DocumentSceneId{document_token, value};
    }

    [[nodiscard]] static constexpr NodeId create_node(std::uint64_t document_token,
                                                      std::uint64_t value) noexcept {
        return NodeId{document_token, value};
    }

    [[nodiscard]] static constexpr MeshId create_mesh(std::uint64_t document_token,
                                                      std::uint64_t value) noexcept {
        return MeshId{document_token, value};
    }

    [[nodiscard]] static constexpr PrimitiveId create_primitive(std::uint64_t document_token,
                                                                std::uint64_t value) noexcept {
        return PrimitiveId{document_token, value};
    }

    [[nodiscard]] static constexpr MaterialId create_material(std::uint64_t document_token,
                                                              std::uint64_t value) noexcept {
        return MaterialId{document_token, value};
    }

    [[nodiscard]] static constexpr ImageId create_image(std::uint64_t document_token,
                                                        std::uint64_t value) noexcept {
        return ImageId{document_token, value};
    }

    [[nodiscard]] static constexpr TextureId create_texture(std::uint64_t document_token,
                                                            std::uint64_t value) noexcept {
        return TextureId{document_token, value};
    }

    [[nodiscard]] static constexpr SamplerId create_sampler(std::uint64_t document_token,
                                                            std::uint64_t value) noexcept {
        return SamplerId{document_token, value};
    }

    [[nodiscard]] static constexpr std::uint64_t document_token(DocumentSceneId id) noexcept {
        return id.document_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t document_token(NodeId id) noexcept {
        return id.document_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t document_token(MeshId id) noexcept {
        return id.document_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t document_token(PrimitiveId id) noexcept {
        return id.document_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t document_token(MaterialId id) noexcept {
        return id.document_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t document_token(ImageId id) noexcept {
        return id.document_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t document_token(TextureId id) noexcept {
        return id.document_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t document_token(SamplerId id) noexcept {
        return id.document_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(DocumentSceneId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(NodeId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(MeshId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(PrimitiveId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(MaterialId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(ImageId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(TextureId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(SamplerId id) noexcept {
        return id.value_;
    }
};

} // namespace elf3d::model::detail

#endif
