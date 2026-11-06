#ifndef ELF3D_MODEL_IDS_H
#define ELF3D_MODEL_IDS_H

#include <cstdint>

namespace elf3d {

namespace model::detail {
class DocumentHandleAccess;
class DocumentMetadataAccess;
class DocumentValidation;

// Opaque, process-local identity. The domain distinguishes simultaneously loaded
// copies of the source-integrated Model SDK; the sequence prevents storage reuse
// from reviving a stale ID. The creating module must remain loaded while IDs live.
struct DocumentOwnerToken final {
    std::uintptr_t domain = 0;
    std::uint64_t sequence = 0;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return domain != 0 && sequence != 0;
    }

    bool operator==(const DocumentOwnerToken&) const = default;
};
} // namespace model::detail

class DocumentSceneId final {
  public:
    constexpr DocumentSceneId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return document_token_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept
    {
        return value_;
    }

    bool operator==(const DocumentSceneId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr DocumentSceneId(model::detail::DocumentOwnerToken document_token,
                              std::uint64_t value) noexcept
        : document_token_(document_token), value_(value)
    {
    }

    model::detail::DocumentOwnerToken document_token_;
    std::uint64_t value_ = 0;
};

class NodeId final {
  public:
    constexpr NodeId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return document_token_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept
    {
        return value_;
    }

    bool operator==(const NodeId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr NodeId(model::detail::DocumentOwnerToken document_token, std::uint64_t value) noexcept
        : document_token_(document_token), value_(value)
    {
    }

    model::detail::DocumentOwnerToken document_token_;
    std::uint64_t value_ = 0;
};

class MeshId final {
  public:
    constexpr MeshId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return document_token_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept
    {
        return value_;
    }

    bool operator==(const MeshId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr MeshId(model::detail::DocumentOwnerToken document_token, std::uint64_t value) noexcept
        : document_token_(document_token), value_(value)
    {
    }

    model::detail::DocumentOwnerToken document_token_;
    std::uint64_t value_ = 0;
};

class PrimitiveId final {
  public:
    constexpr PrimitiveId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return document_token_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept
    {
        return value_;
    }

    bool operator==(const PrimitiveId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr PrimitiveId(model::detail::DocumentOwnerToken document_token,
                          std::uint64_t value) noexcept
        : document_token_(document_token), value_(value)
    {
    }

    model::detail::DocumentOwnerToken document_token_;
    std::uint64_t value_ = 0;
};

class MaterialId final {
  public:
    constexpr MaterialId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return document_token_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept
    {
        return value_;
    }

    bool operator==(const MaterialId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr MaterialId(model::detail::DocumentOwnerToken document_token,
                         std::uint64_t value) noexcept
        : document_token_(document_token), value_(value)
    {
    }

    model::detail::DocumentOwnerToken document_token_;
    std::uint64_t value_ = 0;
};

class ImageId final {
  public:
    constexpr ImageId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return document_token_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept
    {
        return value_;
    }

    bool operator==(const ImageId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr ImageId(model::detail::DocumentOwnerToken document_token,
                      std::uint64_t value) noexcept
        : document_token_(document_token), value_(value)
    {
    }

    model::detail::DocumentOwnerToken document_token_;
    std::uint64_t value_ = 0;
};

class TextureId final {
  public:
    constexpr TextureId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return document_token_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept
    {
        return value_;
    }

    bool operator==(const TextureId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr TextureId(model::detail::DocumentOwnerToken document_token,
                        std::uint64_t value) noexcept
        : document_token_(document_token), value_(value)
    {
    }

    model::detail::DocumentOwnerToken document_token_;
    std::uint64_t value_ = 0;
};

class SamplerId final {
  public:
    constexpr SamplerId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return document_token_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept
    {
        return value_;
    }

    bool operator==(const SamplerId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr SamplerId(model::detail::DocumentOwnerToken document_token,
                        std::uint64_t value) noexcept
        : document_token_(document_token), value_(value)
    {
    }

    model::detail::DocumentOwnerToken document_token_;
    std::uint64_t value_ = 0;
};

} // namespace elf3d

#endif
