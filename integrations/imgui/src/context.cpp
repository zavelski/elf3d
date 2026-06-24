#include <elf3d/imgui/context.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <exception>
#include <memory>
#include <string>

namespace elf3d::imgui {
namespace {

void apply_elf3d_style(GLFWwindow *window, const ContextOptions &options) noexcept {
    float x_scale = 1.0F;
    float y_scale = 1.0F;
    glfwGetWindowContentScale(window, &x_scale, &y_scale);
    const float scale = std::max(1.0F, std::min(std::max(x_scale, y_scale), 3.0F));

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    const float requested_font_size = std::max(1.0F, options.font_size_pixels) * scale;
    const bool loaded_font =
        options.font_path_utf8 != nullptr && options.font_path_utf8[0] != '\0' &&
        io.Fonts->AddFontFromFileTTF(options.font_path_utf8, requested_font_size, nullptr,
                                     io.Fonts->GetGlyphRangesCyrillic()) != nullptr;
    if (!loaded_font) {
        ImFontConfig font_config;
        font_config.SizePixels = requested_font_size;
        io.Fonts->AddFontDefault(&font_config);
    }

    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::StyleColorsLight(&style);
    style.WindowRounding = 0.0F;
    style.ChildRounding = 0.0F;
    style.FrameRounding = 0.0F;
    style.PopupRounding = 0.0F;
    style.ScrollbarRounding = 3.0F;
    style.GrabRounding = 2.0F;
    style.TabRounding = 0.0F;
    style.WindowBorderSize = 0.0F;
    style.FrameBorderSize = 0.0F;
    style.ScrollbarSize = 12.0F;
    style.WindowPadding = ImVec2{8.0F, 8.0F};
    style.FramePadding = ImVec2{4.0F, 3.0F};
    style.ItemSpacing = ImVec2{6.0F, 4.0F};
    style.TabBorderSize = 0.0F;
    style.ScaleAllSizes(scale);

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4{0.00F, 0.00F, 0.00F, 1.00F};
    colors[ImGuiCol_TextDisabled] = ImVec4{0.60F, 0.60F, 0.60F, 1.00F};
    colors[ImGuiCol_WindowBg] = ImVec4{0.74F, 0.81F, 0.81F, 1.00F};
    colors[ImGuiCol_ChildBg] = ImVec4{0.94F, 0.94F, 0.94F, 1.00F};
    colors[ImGuiCol_PopupBg] = ImVec4{0.94F, 0.94F, 0.94F, 0.98F};
    colors[ImGuiCol_Border] = ImVec4{0.70F, 0.70F, 0.70F, 0.65F};
    colors[ImGuiCol_BorderShadow] = ImVec4{0.00F, 0.00F, 0.00F, 0.00F};
    colors[ImGuiCol_FrameBg] = ImVec4{0.71F, 0.71F, 0.71F, 1.00F};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.69F, 0.69F, 0.69F, 1.00F};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.00F, 0.00F, 0.00F, 0.32F};
    colors[ImGuiCol_TitleBg] = ImVec4{0.86F, 0.86F, 0.86F, 1.00F};
    colors[ImGuiCol_TitleBgActive] = ImVec4{0.86F, 0.86F, 0.86F, 1.00F};
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.40F, 0.40F, 0.80F, 0.20F};
    colors[ImGuiCol_MenuBarBg] = ImVec4{0.94F, 0.94F, 0.94F, 1.00F};
    colors[ImGuiCol_ScrollbarBg] = ImVec4{0.48F, 0.48F, 0.48F, 0.60F};
    colors[ImGuiCol_ScrollbarGrab] = ImVec4{0.00F, 0.00F, 0.00F, 0.27F};
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{0.00F, 0.00F, 0.00F, 0.40F};
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{0.00F, 0.00F, 0.00F, 0.51F};
    colors[ImGuiCol_CheckMark] = ImVec4{1.00F, 1.00F, 0.95F, 1.00F};
    colors[ImGuiCol_SliderGrab] = ImVec4{1.00F, 1.00F, 1.00F, 0.30F};
    colors[ImGuiCol_SliderGrabActive] = ImVec4{0.75F, 1.00F, 1.00F, 1.00F};
    colors[ImGuiCol_Button] = ImVec4{0.74F, 0.81F, 0.81F, 1.00F};
    colors[ImGuiCol_ButtonHovered] = ImVec4{0.79F, 0.89F, 0.89F, 1.00F};
    colors[ImGuiCol_ButtonActive] = ImVec4{0.75F, 1.00F, 1.00F, 1.00F};
    colors[ImGuiCol_Header] = ImVec4{0.00F, 0.00F, 0.00F, 0.24F};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.00F, 0.00F, 0.00F, 0.37F};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.00F, 0.00F, 0.00F, 0.30F};
    colors[ImGuiCol_Separator] = ImVec4{0.50F, 0.50F, 0.50F, 1.00F};
    colors[ImGuiCol_SeparatorHovered] = ImVec4{0.70F, 0.60F, 0.60F, 1.00F};
    colors[ImGuiCol_SeparatorActive] = ImVec4{0.90F, 0.70F, 0.70F, 1.00F};
    colors[ImGuiCol_ResizeGrip] = ImVec4{1.00F, 1.00F, 1.00F, 0.30F};
    colors[ImGuiCol_ResizeGripHovered] = ImVec4{1.00F, 1.00F, 1.00F, 0.60F};
    colors[ImGuiCol_ResizeGripActive] = ImVec4{1.00F, 1.00F, 1.00F, 0.90F};
    colors[ImGuiCol_Tab] = ImVec4{0.88F, 0.88F, 0.88F, 1.00F};
    colors[ImGuiCol_TabHovered] = ImVec4{1.00F, 1.00F, 1.00F, 1.00F};
    colors[ImGuiCol_TabActive] = ImVec4{1.00F, 1.00F, 1.00F, 1.00F};
    colors[ImGuiCol_TabUnfocused] = ImVec4{0.88F, 0.88F, 0.88F, 1.00F};
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.91F, 0.91F, 0.91F, 1.00F};
    colors[ImGuiCol_DockingPreview] = ImVec4{0.75F, 1.00F, 1.00F, 0.55F};
    colors[ImGuiCol_DockingEmptyBg] = ImVec4{1.00F, 1.00F, 1.00F, 1.00F};
    colors[ImGuiCol_TextSelectedBg] = ImVec4{1.00F, 1.00F, 1.00F, 1.00F};
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4{0.20F, 0.20F, 0.20F, 0.35F};
}

} // namespace

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
    return create(window, glsl_version, ContextOptions{}, error_message);
}

std::unique_ptr<Context> Context::create(GLFWwindow *window, const char *glsl_version,
                                         const ContextOptions &options,
                                         std::string &error_message) noexcept {
    error_message.clear();

    try {
        std::unique_ptr<Context> context{new Context()};
        if (!context->initialize(window, glsl_version, options, error_message)) {
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

bool Context::initialize(GLFWwindow *window, const char *glsl_version,
                         const ContextOptions &options, std::string &error_message) {
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
    apply_elf3d_style(window, options);

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
