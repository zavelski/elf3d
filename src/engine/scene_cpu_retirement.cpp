#include "scene_cpu_retirement.h"

#include <utility>

namespace elf3d {
SceneCpuRetirement::SceneCpuRetirement() noexcept = default;
SceneCpuRetirement::SceneCpuRetirement(std::unique_ptr<Impl> impl) noexcept : impl_{std::move(impl)}
{
}
SceneCpuRetirement::~SceneCpuRetirement() noexcept = default;
SceneCpuRetirement::SceneCpuRetirement(SceneCpuRetirement&&) noexcept = default;
SceneCpuRetirement& SceneCpuRetirement::operator=(SceneCpuRetirement&&) noexcept = default;
bool SceneCpuRetirement::empty() const noexcept
{
    return impl_ == nullptr;
}
bool SceneCpuRetirement::has_document() const noexcept
{
    return impl_ && impl_->storage->has_document();
}

Result<Document> SceneCpuRetirement::take_document() noexcept
{
    if (!has_document()) {
        return Error{ErrorCode::invalid_argument,
                     "The retired CPU state has no canonical Document"};
    }
    auto document = impl_->storage->release_document();
    impl_.reset();
    return document;
}
} // namespace elf3d
