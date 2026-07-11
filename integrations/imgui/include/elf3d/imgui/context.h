#ifndef ELF3D_IMGUI_CONTEXT_H
#define ELF3D_IMGUI_CONTEXT_H

#include <elf3d/core/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct GLFWwindow;
struct ImGuiContext;

namespace elf3d::imgui {

struct ContextOptions {
    std::optional<std::string> font_path_utf8;
    float font_size_pixels = 20.0F;
};

class Context final {
  private:
    struct ConstructionKey final {};

  public:
    ~Context() noexcept;

    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&) = delete;
    Context &operator=(Context &&) = delete;

    [[nodiscard]] static Result<std::unique_ptr<Context>>
    create(GLFWwindow *window, std::string_view glsl_version,
           const ContextOptions &options = {}) noexcept;

    void begin_frame() noexcept;
    void render() noexcept;

    explicit Context(ConstructionKey) noexcept {}

  private:
    [[nodiscard]] Result<void> initialize(GLFWwindow *window, const char *glsl_version,
                                          const ContextOptions &options);

    ImGuiContext *context_ = nullptr;
    bool glfw_backend_initialized_ = false;
    bool opengl_backend_initialized_ = false;
};

} // namespace elf3d::imgui

#endif
