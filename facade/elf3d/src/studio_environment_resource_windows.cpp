#include "studio_environment_resource.h"

#include <elf3d/core/result.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstddef>
#include <memory>
#include <span>

import elf.renderer;

#include "resource_ids.h"

namespace elf3d::detail {
namespace {

const int studio_environment_module_anchor = 0;

class WindowsStudioEnvironmentSource final : public renderer::StudioEnvironmentSource {
  public:
    [[nodiscard]] Result<std::span<const std::byte>> bytes() noexcept override {
        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&studio_environment_module_anchor),
                               &module) == FALSE ||
            module == nullptr) {
            return Error{ErrorCode::graphics_initialization_failed,
                         "Could not locate the Elf3D module for its studio environment"};
        }
        const HRSRC resource = FindResourceW(
            module, MAKEINTRESOURCEW(ELF3D_STUDIO_ENVIRONMENT_RESOURCE_ID), MAKEINTRESOURCEW(10));
        if (resource == nullptr) {
            return Error{ErrorCode::graphics_initialization_failed,
                         "The built-in studio environment resource is missing"};
        }
        const DWORD byte_count = SizeofResource(module, resource);
        const HGLOBAL loaded = LoadResource(module, resource);
        const void* data = loaded != nullptr ? LockResource(loaded) : nullptr;
        if (byte_count == 0 || data == nullptr) {
            return Error{ErrorCode::graphics_initialization_failed,
                         "The built-in studio environment resource could not be loaded"};
        }
        return std::span<const std::byte>{static_cast<const std::byte*>(data),
                                          static_cast<std::size_t>(byte_count)};
    }
};

} // namespace

std::unique_ptr<renderer::StudioEnvironmentSource> create_studio_environment_source() {
    return std::make_unique<WindowsStudioEnvironmentSource>();
}

} // namespace elf3d::detail
