#ifndef ELF3D_IMGUI_CONTEXT_OWNER_H
#define ELF3D_IMGUI_CONTEXT_OWNER_H

#include <elf3d/core/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct GLFWwindow;
struct ImGuiContext;

namespace elf3d::imgui::detail {

struct ContextOptions {
    std::optional<std::string> font_path_utf8;
    float font_size_pixels = 20.0F;
};

class ContextOwner final {
  private:
    struct ConstructionKey final {};

  public:
    ~ContextOwner() noexcept;

    ContextOwner(const ContextOwner&) = delete;
    ContextOwner& operator=(const ContextOwner&) = delete;
    ContextOwner(ContextOwner&&) = delete;
    ContextOwner& operator=(ContextOwner&&) = delete;

    [[nodiscard]] static Result<std::unique_ptr<ContextOwner>>
    create(GLFWwindow* window, std::string_view glsl_version,
           const ContextOptions& options = {}) noexcept;

    void begin_frame() noexcept;
    void discard_frame() noexcept;
    void render() noexcept;

    explicit ContextOwner(ConstructionKey) noexcept {}

  private:
    [[nodiscard]] Result<void> initialize(GLFWwindow* window, const char* glsl_version,
                                          const ContextOptions& options);

    ImGuiContext* context_ = nullptr;
    bool glfw_backend_initialized_ = false;
    bool opengl_backend_initialized_ = false;
};

} // namespace elf3d::imgui::detail

#endif
