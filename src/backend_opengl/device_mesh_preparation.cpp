#include "device_mesh.h"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <utility>

namespace elf3d::backend::opengl::device_detail {
namespace {
using Phase = graphics::MeshPreparationPhase;
struct Attribute {
    GLint components;
    std::size_t float_offset;
};
constexpr std::array<Attribute, 6> attributes{{{3, 0}, {3, 3}, {2, 6}, {2, 8}, {4, 10}, {4, 14}}};
[[nodiscard]] std::size_t attribute_count(graphics::VertexLayout layout) noexcept
{
    constexpr auto tangent =
        graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4_tangent_float4;
    switch (layout) {
    case graphics::VertexLayout::position_normal_float3:
        return 2;
    case graphics::VertexLayout::position_normal_float3_texcoord_float2:
        return 3;
    case graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4:
        return 5;
    case tangent:
        return 6;
    }
    return 0;
}
class CopyBufferBinding final {
  public:
    explicit CopyBufferBinding(GLuint buffer) noexcept
    {
        // OpenGL 4.1 names this binding query GL_COPY_WRITE_BUFFER.
        glGetIntegerv(GL_COPY_WRITE_BUFFER, &previous_);
        glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
    }
    ~CopyBufferBinding() noexcept
    {
        glBindBuffer(GL_COPY_WRITE_BUFFER, static_cast<GLuint>(previous_));
    }

  private:
    GLint previous_ = 0;
};
class VertexArrayBinding final {
  public:
    explicit VertexArrayBinding(GLuint array) noexcept
    {
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_);
        glBindVertexArray(array);
    }
    ~VertexArrayBinding() noexcept
    {
        glBindVertexArray(static_cast<GLuint>(previous_));
    }

  private:
    GLint previous_ = 0;
};
class VertexBufferBinding final {
  public:
    explicit VertexBufferBinding(GLuint buffer) noexcept
    {
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
    }
    ~VertexBufferBinding() noexcept
    {
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_));
    }

  private:
    GLint previous_ = 0;
};
[[nodiscard]] Result<void> checked_gl_error(const char* message) noexcept
{
    if (glGetError() != GL_NO_ERROR)
        return Error{ErrorCode::gpu_buffer_creation_failed, message};
    return {};
}
[[nodiscard]] std::uint32_t call_limit(Phase phase) noexcept
{
    if (phase == Phase::attribute_pointer)
        return 8;
    if (phase == Phase::vertex_storage || phase == Phase::index_storage ||
        phase == Phase::index_binding || phase == Phase::vertices || phase == Phase::indices)
        return 6;
    if (phase == Phase::objects || phase == Phase::attribute_enable)
        return 5;
    return phase == Phase::validation ? 2 : 0;
}
class OpenGLMeshPreparation final : public graphics::StaticMeshPreparation {
  public:
    OpenGLMeshPreparation(std::unique_ptr<OpenGLStaticMesh> mesh, std::size_t stride) noexcept
        : mesh_(std::move(mesh)), stride_(stride)
    {
    }
    [[nodiscard]] Result<graphics::MeshUploadRequest>
    request(std::size_t maximum_bytes) const noexcept override;
    [[nodiscard]] Result<graphics::MeshUploadStep>
    advance(std::span<const std::byte> payload) noexcept override;
    [[nodiscard]] Result<graphics::MeshUploadStep>
    advance_setup(std::uint32_t maximum_calls) noexcept override;
    [[nodiscard]] std::uint64_t driver_calls() const noexcept override
    {
        return driver_calls_;
    }
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept override
    {
        if (!mesh_)
            return graphics::ResourceRetirementStep{0, 0, true, false};
        auto step = mesh_->advance_retirement(budget);
        if (step && (step.value().progressed || step.value().complete))
            phase_ = Phase::failed; // A retired upload cannot be resumed or adopted.
        return step;
    }
    [[nodiscard]] Result<std::unique_ptr<graphics::StaticMesh>> take_mesh() noexcept override;

  private:
    [[nodiscard]] Result<graphics::MeshUploadRequest>
    data_request(std::size_t maximum_bytes) const noexcept;
    [[nodiscard]] Result<void> validate_payload(std::span<const std::byte> payload) const noexcept;
    [[nodiscard]] Result<void> native_step(std::span<const std::byte> payload) noexcept;
    [[nodiscard]] Result<void> data_step(std::span<const std::byte> payload) noexcept;
    [[nodiscard]] Result<void> allocate_objects() noexcept;
    [[nodiscard]] Result<void> allocate_buffer(GLuint buffer, std::size_t bytes) noexcept;
    [[nodiscard]] Result<void> bind_indices() noexcept;
    void attribute_pointer() noexcept;
    void attribute_enable() noexcept;
    [[nodiscard]] Result<void> upload(GLuint buffer, std::size_t offset,
                                      std::span<const std::byte> bytes) noexcept;
    [[nodiscard]] std::size_t total_vertices_bytes() const noexcept
    {
        return mesh_->vertex_count() * stride_;
    }
    [[nodiscard]] std::size_t total_indices_bytes() const noexcept
    {
        return mesh_->index_count() * sizeof(std::uint32_t);
    }
    void add_calls(std::uint32_t count) noexcept;
    std::unique_ptr<OpenGLStaticMesh> mesh_;
    std::size_t stride_ = 0;
    Phase phase_ = Phase::objects;
    std::size_t attribute_ = 0;
    std::size_t vertex_offset_ = 0;
    std::size_t index_offset_ = 0;
    std::uint64_t driver_calls_ = 1; // Successful factory context validation.
};
void OpenGLMeshPreparation::add_calls(std::uint32_t count) noexcept
{
    if (count > std::numeric_limits<std::uint64_t>::max() - driver_calls_)
        fatal_error("OpenGL mesh preparation driver-call count exhausted");
    driver_calls_ += count;
}
Result<graphics::MeshUploadRequest>
OpenGLMeshPreparation::request(std::size_t maximum_bytes) const noexcept
{
    if (phase_ == Phase::failed || phase_ == Phase::consumed)
        return Error{ErrorCode::invalid_argument, "Mesh preparation has failed or was consumed"};
    if (const auto valid = mesh_->validate_thread(); !valid)
        return valid.error();
    if (maximum_bytes == 0)
        return Error{ErrorCode::invalid_argument, "Mesh upload byte allowance must be positive"};
    graphics::MeshUploadRequest result{phase_, 0, 0, call_limit(phase_)};
    if (phase_ != Phase::vertices && phase_ != Phase::indices)
        return result;
    return data_request(maximum_bytes);
}
Result<graphics::MeshUploadRequest>
OpenGLMeshPreparation::data_request(std::size_t maximum_bytes) const noexcept
{
    graphics::MeshUploadRequest result{phase_, 0, 0, call_limit(phase_)};
    const bool vertices = phase_ == Phase::vertices;
    result.offset_bytes = vertices ? vertex_offset_ : index_offset_;
    const auto total = vertices ? total_vertices_bytes() : total_indices_bytes();
    const auto unit = vertices ? stride_ : sizeof(std::uint32_t);
    const auto allowance = std::min(maximum_bytes, graphics::maximum_mesh_upload_chunk);
    result.bytes = std::min(total - result.offset_bytes, allowance / unit * unit);
    if (result.bytes == 0)
        return Error{ErrorCode::invalid_argument,
                     "Mesh upload allowance is smaller than one record"};
    return result;
}
Result<void>
OpenGLMeshPreparation::validate_payload(std::span<const std::byte> payload) const noexcept
{
    const auto next = request(graphics::maximum_mesh_upload_chunk);
    if (!next)
        return next.error();
    if (next.value().bytes == 0) {
        if (!payload.empty())
            return Error{ErrorCode::invalid_argument,
                         "Mesh setup steps do not accept source bytes"};
        return {};
    }
    const auto unit = phase_ == Phase::vertices ? stride_ : sizeof(std::uint32_t);
    if (payload.empty() || payload.size() > next.value().bytes || payload.size() % unit != 0)
        return Error{ErrorCode::invalid_argument, "Mesh upload chunk size or alignment is invalid"};
    return {};
}
Result<graphics::MeshUploadStep>
OpenGLMeshPreparation::advance(std::span<const std::byte> payload) noexcept
{
    if (const auto valid = validate_payload(payload); !valid)
        return valid.error();
    if (phase_ == Phase::ready)
        return graphics::MeshUploadStep{0, 0, true};
    const auto before = driver_calls_;
    const auto context = mesh_->validate_operation();
    if (context || context.error().code() == ErrorCode::graphics_context_unavailable)
        add_calls(1);
    if (!context)
        return context.error();
    const auto previous_phase = phase_;
    if (const auto executed = native_step(payload); !executed) {
        phase_ = Phase::failed;
        return executed.error();
    }
    // Each native step's explicit bound is at most eight calls, including validation.
    const auto allocated = previous_phase == Phase::vertex_storage  ? total_vertices_bytes()
                           : previous_phase == Phase::index_storage ? total_indices_bytes()
                                                                    : 0;
    return graphics::MeshUploadStep{payload.size(),
                                    static_cast<std::uint32_t>(driver_calls_ - before),
                                    phase_ == Phase::ready, allocated};
}
Result<graphics::MeshUploadStep>
OpenGLMeshPreparation::advance_setup(std::uint32_t maximum_calls) noexcept
{
    if (phase_ != Phase::attribute_pointer)
        return graphics::MeshUploadStep{};
    const auto count = attribute_count(mesh_->vertex_layout()) - attribute_;
    const auto calls = static_cast<std::uint32_t>(8 + 2 * count); // <= 22
    if (maximum_calls < calls)
        return graphics::MeshUploadStep{};
    if (const auto context = mesh_->validate_operation(); !context)
        return context.error();
    const VertexArrayBinding array{mesh_->vertex_array};
    const VertexBufferBinding buffer{mesh_->vertex_buffer};
    for (; attribute_ < attribute_count(mesh_->vertex_layout()); ++attribute_) {
        const auto attribute = attributes[attribute_];
        glVertexAttribPointer(
            static_cast<GLuint>(attribute_), attribute.components, GL_FLOAT, GL_FALSE,
            static_cast<GLsizei>(stride_),
            reinterpret_cast<const void*>(attribute.float_offset * sizeof(float)));
        glEnableVertexAttribArray(static_cast<GLuint>(attribute_));
    }
    add_calls(calls);
    if (const auto error = checked_gl_error("OpenGL mesh attribute group failed"); !error) {
        phase_ = Phase::failed;
        return error.error();
    }
    phase_ = Phase::vertices;
    return graphics::MeshUploadStep{0, calls};
}
Result<void> OpenGLMeshPreparation::allocate_objects() noexcept
{
    glGenVertexArrays(1, &mesh_->vertex_array);
    glGenBuffers(1, &mesh_->vertex_buffer);
    glGenBuffers(1, &mesh_->index_buffer);
    const auto error = checked_gl_error("OpenGL could not allocate mesh objects");
    add_calls(4);
    if (!error)
        return error.error();
    if (mesh_->vertex_array == 0 || mesh_->vertex_buffer == 0 || mesh_->index_buffer == 0)
        return Error{ErrorCode::gpu_buffer_creation_failed, "OpenGL returned an empty mesh object"};
    phase_ = Phase::vertex_storage;
    return {};
}
Result<void> OpenGLMeshPreparation::allocate_buffer(GLuint buffer, std::size_t bytes) noexcept
{
    const CopyBufferBinding binding{buffer};
    // Factory validation proves all storage sizes fit the native signed byte range.
    glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(bytes), nullptr, GL_STATIC_DRAW);
    auto result = checked_gl_error("OpenGL could not allocate mesh buffer storage");
    add_calls(5); // Capture, bind, allocate, error query, restore.
    return result;
}
Result<void> OpenGLMeshPreparation::bind_indices() noexcept
{
    const VertexArrayBinding binding{mesh_->vertex_array};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_->index_buffer);
    auto result = checked_gl_error("OpenGL could not bind mesh index storage");
    add_calls(5);
    if (result)
        phase_ = Phase::attribute_pointer;
    return result;
}
void OpenGLMeshPreparation::attribute_pointer() noexcept
{
    const VertexArrayBinding array{mesh_->vertex_array};
    const VertexBufferBinding buffer{mesh_->vertex_buffer};
    const auto attribute = attributes[attribute_];
    // Attribute count <= 6, stride <= 72; GL interprets this pointer as a buffer offset.
    glVertexAttribPointer(static_cast<GLuint>(attribute_), attribute.components, GL_FLOAT, GL_FALSE,
                          static_cast<GLsizei>(stride_),
                          reinterpret_cast<const void*>(attribute.float_offset * sizeof(float)));
    add_calls(7);
    phase_ = Phase::attribute_enable;
}
void OpenGLMeshPreparation::attribute_enable() noexcept
{
    const VertexArrayBinding array{mesh_->vertex_array};
    glEnableVertexAttribArray(static_cast<GLuint>(attribute_)); // attribute_ < 6
    add_calls(4);
    ++attribute_;
    phase_ = attribute_ == attribute_count(mesh_->vertex_layout()) ? Phase::vertices
                                                                   : Phase::attribute_pointer;
}
Result<void> OpenGLMeshPreparation::upload(GLuint buffer, std::size_t offset,
                                           std::span<const std::byte> bytes) noexcept
{
    const CopyBufferBinding binding{buffer};
    // The validated payload lies inside factory-checked native storage sizes.
    glBufferSubData(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(bytes.size()), bytes.data());
    auto result = checked_gl_error("OpenGL could not upload a mesh buffer chunk");
    add_calls(5);
    return result;
}
Result<void> OpenGLMeshPreparation::data_step(std::span<const std::byte> payload) noexcept
{
    if (phase_ == Phase::vertices) {
        if (const auto result = upload(mesh_->vertex_buffer, vertex_offset_, payload); !result)
            return result.error();
        vertex_offset_ += payload.size();
        if (vertex_offset_ == total_vertices_bytes())
            phase_ = Phase::indices;
        return {};
    }
    if (phase_ == Phase::indices) {
        if (const auto result = upload(mesh_->index_buffer, index_offset_, payload); !result)
            return result.error();
        index_offset_ += payload.size();
        if (index_offset_ == total_indices_bytes())
            phase_ = Phase::validation;
        return {};
    }
    auto result = checked_gl_error("OpenGL mesh attribute preparation failed");
    add_calls(1);
    if (result) {
        phase_ = Phase::ready;
        mesh_->ready = true;
    }
    return result;
}
Result<void> OpenGLMeshPreparation::native_step(std::span<const std::byte> payload) noexcept
{
    switch (phase_) {
    case Phase::objects:
        return allocate_objects();
    case Phase::vertex_storage:
        if (const auto result = allocate_buffer(mesh_->vertex_buffer, total_vertices_bytes());
            !result)
            return result.error();
        phase_ = Phase::index_storage;
        return {};
    case Phase::index_storage:
        if (const auto result = allocate_buffer(mesh_->index_buffer, total_indices_bytes());
            !result)
            return result.error();
        phase_ = Phase::index_binding;
        return {};
    case Phase::index_binding:
        return bind_indices();
    case Phase::attribute_pointer:
        attribute_pointer();
        return {};
    case Phase::attribute_enable:
        attribute_enable();
        return {};
    default:
        return data_step(payload);
    }
}
Result<std::unique_ptr<graphics::StaticMesh>> OpenGLMeshPreparation::take_mesh() noexcept
{
    if (phase_ != Phase::ready)
        return Error{ErrorCode::invalid_argument, "The mesh upload is not ready for transfer"};
    if (const auto thread = mesh_->validate_thread(); !thread)
        return thread.error();
    phase_ = Phase::consumed;
    return std::unique_ptr<graphics::StaticMesh>{std::move(mesh_)};
}
[[nodiscard]] Result<std::size_t>
validate_storage(const graphics::MeshStorageDescription& description) noexcept
{
    const auto stride = graphics::mesh_vertex_stride(description.vertex_layout);
    if (!stride)
        return stride.error();
    if (description.vertex_count == 0 || description.index_count == 0 ||
        description.index_count > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()))
        return Error{ErrorCode::gpu_buffer_creation_failed, "Invalid native mesh element counts"};
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max());
    if (description.vertex_count > maximum / stride.value() ||
        description.index_count > maximum / sizeof(std::uint32_t))
        return Error{ErrorCode::size_overflow, "Mesh storage exceeds the native byte range"};
    return stride.value();
}
} // namespace
Result<std::unique_ptr<graphics::StaticMeshPreparation>>
begin_static_mesh(std::shared_ptr<OpenGLDeviceState> state,
                  const graphics::MeshStorageDescription& description) noexcept
{
    if (const auto context = state->validate_operation(); !context)
        return context.error();
    const auto stride = validate_storage(description);
    if (!stride)
        return stride.error();
    try {
        auto mesh = std::make_unique<OpenGLStaticMesh>(std::move(state), description);
        return std::unique_ptr<graphics::StaticMeshPreparation>{
            std::make_unique<OpenGLMeshPreparation>(std::move(mesh), stride.value())};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}
} // namespace elf3d::backend::opengl::device_detail
