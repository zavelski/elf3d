#ifndef ELF3D_MODEL_IDS_H
#define ELF3D_MODEL_IDS_H

#include <cstdint>

namespace elf3d {

namespace model::detail {
class DocumentHandleAccess;
class DocumentMetadataAccess;
class DocumentValidation;
} // namespace model::detail

class DocumentSceneId final {
  public:
    constexpr DocumentSceneId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return document_ != 0 && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const DocumentSceneId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr DocumentSceneId(std::uintptr_t document, std::uint64_t value) noexcept
        : document_(document), value_(value) {}

    std::uintptr_t document_ = 0;
    std::uint64_t value_ = 0;
};

class NodeId final {
  public:
    constexpr NodeId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return document_ != 0 && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const NodeId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr NodeId(std::uintptr_t document, std::uint64_t value) noexcept
        : document_(document), value_(value) {}

    std::uintptr_t document_ = 0;
    std::uint64_t value_ = 0;
};

class MeshId final {
  public:
    constexpr MeshId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return document_ != 0 && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const MeshId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr MeshId(std::uintptr_t document, std::uint64_t value) noexcept
        : document_(document), value_(value) {}

    std::uintptr_t document_ = 0;
    std::uint64_t value_ = 0;
};

class PrimitiveId final {
  public:
    constexpr PrimitiveId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return document_ != 0 && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const PrimitiveId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr PrimitiveId(std::uintptr_t document, std::uint64_t value) noexcept
        : document_(document), value_(value) {}

    std::uintptr_t document_ = 0;
    std::uint64_t value_ = 0;
};

class MaterialId final {
  public:
    constexpr MaterialId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return document_ != 0 && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const MaterialId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr MaterialId(std::uintptr_t document, std::uint64_t value) noexcept
        : document_(document), value_(value) {}

    std::uintptr_t document_ = 0;
    std::uint64_t value_ = 0;
};

class ImageId final {
  public:
    constexpr ImageId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return document_ != 0 && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const ImageId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr ImageId(std::uintptr_t document, std::uint64_t value) noexcept
        : document_(document), value_(value) {}

    std::uintptr_t document_ = 0;
    std::uint64_t value_ = 0;
};

class TextureId final {
  public:
    constexpr TextureId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return document_ != 0 && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const TextureId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr TextureId(std::uintptr_t document, std::uint64_t value) noexcept
        : document_(document), value_(value) {}

    std::uintptr_t document_ = 0;
    std::uint64_t value_ = 0;
};

class SamplerId final {
  public:
    constexpr SamplerId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return document_ != 0 && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const SamplerId&) const = default;

  private:
    friend class model::detail::DocumentHandleAccess;

    constexpr SamplerId(std::uintptr_t document, std::uint64_t value) noexcept
        : document_(document), value_(value) {}

    std::uintptr_t document_ = 0;
    std::uint64_t value_ = 0;
};

} // namespace elf3d

#endif
