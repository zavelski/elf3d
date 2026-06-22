#ifndef ELF3D_IMGUI_CONTEXT_H
#define ELF3D_IMGUI_CONTEXT_H

#include <memory>
#include <string>

struct GLFWwindow;
struct ImGuiContext;

namespace elf3d::imgui {

class Context final {
  public:
    ~Context();

    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&) = delete;
    Context &operator=(Context &&) = delete;

    [[nodiscard]] static std::unique_ptr<Context>
    create(GLFWwindow *window, const char *glsl_version, std::string &error_message) noexcept;

    void begin_frame() noexcept;
    void render() noexcept;

  private:
    Context() noexcept = default;

    [[nodiscard]] bool initialize(GLFWwindow *window, const char *glsl_version,
                                  std::string &error_message);

    ImGuiContext *context_ = nullptr;
    bool glfw_backend_initialized_ = false;
    bool opengl_backend_initialized_ = false;
};

} // namespace elf3d::imgui

#endif
