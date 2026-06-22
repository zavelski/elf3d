#include <elf3d/imgui/context.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <exception>
#include <memory>
#include <string>

namespace elf3d::imgui {

Context::~Context() {
    if (context_ == nullptr) {
        return;
    }

    ImGui::SetCurrentContext(context_);

    if (opengl_backend_initialized_) {
        ImGui_ImplOpenGL3_Shutdown();
    }
    if (glfw_backend_initialized_) {
        ImGui_ImplGlfw_Shutdown();
    }

    ImGui::DestroyContext(context_);
}

std::unique_ptr<Context> Context::create(GLFWwindow *window, const char *glsl_version,
                                         std::string &error_message) noexcept {
    error_message.clear();

    try {
        std::unique_ptr<Context> context{new Context()};
        if (!context->initialize(window, glsl_version, error_message)) {
            return nullptr;
        }
        return context;
    } catch (const std::exception &exception) {
        error_message =
            std::string{"Dear ImGui initialization threw an exception: "} + exception.what();
    } catch (...) {
        error_message = "Dear ImGui initialization failed with an unknown exception";
    }

    return nullptr;
}

bool Context::initialize(GLFWwindow *window, const char *glsl_version, std::string &error_message) {
    if (window == nullptr) {
        error_message = "Dear ImGui initialization requires a valid GLFW window";
        return false;
    }
    if (glsl_version == nullptr) {
        error_message = "Dear ImGui initialization requires a GLSL version string";
        return false;
    }

    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    if (context_ == nullptr) {
        error_message = "Dear ImGui context creation failed";
        return false;
    }

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        error_message = "Dear ImGui GLFW backend initialization failed";
        return false;
    }
    glfw_backend_initialized_ = true;

    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        error_message = "Dear ImGui OpenGL3 backend initialization failed";
        return false;
    }
    opengl_backend_initialized_ = true;

    return true;
}

void Context::begin_frame() noexcept {
    ImGui::SetCurrentContext(context_);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Context::render() noexcept {
    ImGui::SetCurrentContext(context_);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace elf3d::imgui
