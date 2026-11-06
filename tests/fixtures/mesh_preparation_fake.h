#pragma once
#include "resource_retirement_fake.h"

#include <algorithm>
#include <graphics/mesh_preparation.h>
#include <utility>

namespace elf3d::graphics::tests {
class FakeMeshPreparation final : public StaticMeshPreparation {
  public:
    FakeMeshPreparation(MeshStorageDescription storage, std::unique_ptr<StaticMesh> mesh) noexcept
        : storage_(storage), mesh_(std::move(mesh))
    {
    }
    [[nodiscard]] Result<MeshUploadRequest>
    request(std::size_t maximum_bytes) const noexcept override
    {
        if (!mesh_ || retiring_)
            return Error{ErrorCode::invalid_argument, "Fake mesh preparation was consumed"};
        if (maximum_bytes == 0)
            return Error{ErrorCode::invalid_argument,
                         "Fake mesh upload allowance must be positive"};
        if (phase_ == MeshPreparationPhase::ready)
            return MeshUploadRequest{phase_, 0, 0, 0};
        const auto stride = mesh_vertex_stride(storage_.vertex_layout);
        if (!stride)
            return stride.error();
        const auto unit =
            phase_ == MeshPreparationPhase::vertices ? stride.value() : sizeof(std::uint32_t);
        const auto total = phase_ == MeshPreparationPhase::vertices ? storage_.vertex_count * unit
                                                                    : storage_.index_count * unit;
        const auto bytes = std::min(
            total - offset_, std::min(maximum_bytes, maximum_mesh_upload_chunk) / unit * unit);
        if (bytes == 0)
            return Error{ErrorCode::invalid_argument,
                         "Fake mesh upload requires a complete record"};
        return MeshUploadRequest{phase_, offset_, bytes, 1};
    }
    [[nodiscard]] Result<MeshUploadStep>
    advance(std::span<const std::byte> payload) noexcept override
    {
        const auto next = request(maximum_mesh_upload_chunk);
        if (!next)
            return next.error();
        if (const auto valid = validate_payload(next.value(), payload); !valid)
            return valid.error();
        if (phase_ == MeshPreparationPhase::ready)
            return MeshUploadStep{0, 0, true};
        const auto stride = mesh_vertex_stride(storage_.vertex_layout).value();
        offset_ += payload.size();
        auto& digest = phase_ == MeshPreparationPhase::vertices ? vertex_digest_ : index_digest_;
        for (const auto byte : payload) {
            digest ^= std::to_integer<std::uint8_t>(byte);
            digest *= 1099511628211ULL; // Test-only streaming fingerprint, modulo uint64.
        }
        ++calls_;
        const auto total = phase_ == MeshPreparationPhase::vertices
                               ? storage_.vertex_count * stride
                               : storage_.index_count * sizeof(std::uint32_t);
        if (offset_ == total) {
            phase_ = phase_ == MeshPreparationPhase::vertices ? MeshPreparationPhase::indices
                                                              : MeshPreparationPhase::ready;
            offset_ = 0;
        }
        return MeshUploadStep{payload.size(), 1, phase_ == MeshPreparationPhase::ready};
    }
    [[nodiscard]] std::uint64_t driver_calls() const noexcept override
    {
        return calls_;
    }
    [[nodiscard]] Result<ResourceRetirementStep>
    advance_retirement(const ResourceRetirementBudget& budget) noexcept override
    {
        if (!mesh_)
            return ResourceRetirementStep{0, 0, true, false};
        auto step = retire_fake_objects(retirement_objects_, budget);
        if (step.value().progressed || step.value().complete)
            retiring_ = true;
        return step;
    }
    [[nodiscard]] std::uint64_t vertex_digest() const noexcept
    {
        return vertex_digest_;
    }
    [[nodiscard]] std::uint64_t index_digest() const noexcept
    {
        return index_digest_;
    }
    [[nodiscard]] Result<std::unique_ptr<StaticMesh>> take_mesh() noexcept override
    {
        if (phase_ != MeshPreparationPhase::ready || !mesh_ || retiring_)
            return Error{ErrorCode::invalid_argument, "Fake mesh upload is not ready"};
        return std::move(mesh_);
    }

  private:
    [[nodiscard]] Result<void> validate_payload(const MeshUploadRequest& next,
                                                std::span<const std::byte> payload) const noexcept
    {
        if (next.bytes == 0) {
            if (!payload.empty())
                return Error{ErrorCode::invalid_argument,
                             "Fake mesh setup/ready steps accept no payload"};
            return {};
        }
        const auto stride = mesh_vertex_stride(storage_.vertex_layout).value();
        const auto unit = phase_ == MeshPreparationPhase::vertices ? stride : sizeof(std::uint32_t);
        if (payload.empty() || payload.size() > next.bytes || payload.size() % unit != 0)
            return Error{ErrorCode::invalid_argument, "Unexpected fake mesh chunk size"};
        return {};
    }
    MeshStorageDescription storage_;
    std::uint32_t retirement_objects_ = 3;
    bool retiring_ = false;
    std::unique_ptr<StaticMesh> mesh_;
    MeshPreparationPhase phase_ = MeshPreparationPhase::vertices;
    std::size_t offset_ = 0;
    std::uint64_t calls_ = 0;
    std::uint64_t vertex_digest_ = 14695981039346656037ULL;
    std::uint64_t index_digest_ = 14695981039346656037ULL;
};
} // namespace elf3d::graphics::tests
