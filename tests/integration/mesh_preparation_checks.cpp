#include "mesh_preparation_checks.h"
#include "texture_preparation_checks.h"
#include <backend_opengl/device_mesh.h>
#include <graphics/mesh_preparation.h>

#include <algorithm>
#include <array>
#include <new>
#include <vector>

namespace {
using namespace elf3d;
using namespace elf3d::graphics;
using Phase = MeshPreparationPhase;
[[nodiscard]] Error failure(const char* message)
{
    return {ErrorCode::gpu_buffer_creation_failed, message};
}
struct Bindings {
    GLint array = 0;
    GLint vertex = 0;
    GLint copy_write = 0;
    GLint copy_read = 0;
    bool operator==(const Bindings&) const = default;
};
[[nodiscard]] Bindings bindings()
{
    Bindings result;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &result.array);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &result.vertex);
    glGetIntegerv(GL_COPY_WRITE_BUFFER, &result.copy_write);
    glGetIntegerv(GL_COPY_READ_BUFFER, &result.copy_read);
    return result;
}
void restore(const Bindings& state)
{
    glBindVertexArray(static_cast<GLuint>(state.array));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(state.vertex));
    glBindBuffer(GL_COPY_WRITE_BUFFER, static_cast<GLuint>(state.copy_write));
    glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(state.copy_read));
}
class ForeignBuffers final {
  public:
    [[nodiscard]] Result<void> prepare()
    {
        original_ = bindings();
        captured_ = true;
        glGenVertexArrays(1, &array_);
        glGenBuffers(3, buffers_.data());
        if (array_ == 0 || std::find(buffers_.begin(), buffers_.end(), 0U) != buffers_.end())
            return failure("Cannot create foreign buffer binding probes");
        glBindVertexArray(array_);
        glBindBuffer(GL_ARRAY_BUFFER, buffers_[0]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers_[1]);
        glBindBuffer(GL_COPY_WRITE_BUFFER, buffers_[2]);
        return {};
    }
    ~ForeignBuffers() noexcept
    {
        if (!captured_)
            return;
        restore(original_);
        glDeleteVertexArrays(1, &array_);
        glDeleteBuffers(3, buffers_.data());
    }

  private:
    Bindings original_;
    bool captured_ = false;
    GLuint array_ = 0;
    std::array<GLuint, 3> buffers_{};
};
struct Payload {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    MeshStorageDescription storage;
};
[[nodiscard]] Payload payload()
{
    Payload result;
    result.storage = {
        35000, 120000,
        VertexLayout::position_normal_float3_texcoord2_float2_color_float4_tangent_float4};
    result.vertices.resize(static_cast<std::size_t>(result.storage.vertex_count) * 18);
    result.indices.resize(result.storage.index_count);
    for (std::size_t i = 0; i < result.vertices.size(); ++i)
        result.vertices[i] = static_cast<float>(i % 97) / 97.0F; // remainder < 97
    for (std::size_t i = 0; i < result.indices.size(); ++i)
        result.indices[i] = static_cast<std::uint32_t>(i % result.storage.vertex_count);
    return result;
}
[[nodiscard]] Result<void> reject_early(StaticMeshPreparation& prepared)
{
    const auto before = prepared.driver_calls();
    const std::array<std::byte, 1> invalid{};
    if (prepared.request(0) || prepared.take_mesh() || prepared.advance(invalid) ||
        prepared.driver_calls() != before)
        return failure("Invalid mesh setup input performed graphics work or transferred its owner");
    return {};
}
[[nodiscard]] Result<void> reject_incomplete_record(StaticMeshPreparation& prepared)
{
    const auto before = prepared.driver_calls();
    const std::array<std::byte, 1> invalid{};
    if (prepared.advance(invalid) || prepared.driver_calls() != before)
        return failure("An incomplete vertex record changed mesh upload state");
    return {};
}
[[nodiscard]] Result<MeshUploadStep> advance_checked(StaticMeshPreparation& prepared,
                                                     const MeshUploadRequest& request,
                                                     std::span<const std::byte> bytes)
{
    if (request.bytes > maximum_mesh_upload_chunk || request.driver_call_limit > 8)
        return failure("A mesh preparation step exceeds its declared upload/call bounds");
    const auto state = bindings();
    const auto before = prepared.driver_calls();
    auto step = prepared.advance(bytes);
    if (!step)
        return step.error();
    if (step.value().driver_calls > request.driver_call_limit ||
        step.value().driver_calls != prepared.driver_calls() - before ||
        step.value().uploaded_bytes != bytes.size() || bindings() != state)
        return failure("Mesh preparation violated its step report or foreign OpenGL state");
    return std::move(step).value();
}
[[nodiscard]] Result<void> grouped_setup(StaticMeshPreparation& prepared)
{
    const auto state = bindings();
    const auto before = prepared.driver_calls();
    const auto small = prepared.advance_setup(1);
    if (!small || small.value().driver_calls != 0 || prepared.driver_calls() != before)
        return failure("Insufficient mesh setup allowance changed native state");
    const auto bulk = prepared.advance_setup(32);
    if (!bulk || bulk.value().driver_calls == 0 || bulk.value().driver_calls > 32 ||
        prepared.driver_calls() - before != bulk.value().driver_calls || bindings() != state)
        return failure("Grouped mesh setup changed foreign bindings or call accounting");
    return {};
}
struct MeshUploadProgress {
    std::span<const std::byte> vertices;
    std::span<const std::byte> indices;
    std::size_t vertex_chunks = 0;
};
[[nodiscard]] Result<std::span<const std::byte>> mesh_packet(StaticMeshPreparation& prepared,
                                                             const MeshUploadRequest& next,
                                                             MeshUploadProgress& progress)
{
    if (next.phase == Phase::vertices) {
        if (const auto rejected = reject_incomplete_record(prepared); !rejected)
            return rejected.error();
        ++progress.vertex_chunks;
        return progress.vertices.subspan(next.offset_bytes, next.bytes);
    }
    if (next.phase == Phase::indices)
        return progress.indices.subspan(next.offset_bytes, next.bytes);
    return std::span<const std::byte>{};
}
[[nodiscard]] Result<std::unique_ptr<StaticMesh>> upload(StaticMeshPreparation& prepared,
                                                         const Payload& source)
{
    MeshUploadProgress progress{std::as_bytes(std::span{source.vertices}),
                                std::as_bytes(std::span{source.indices})};
    for (std::size_t i = 0; i < 128; ++i) {
        const auto request =
            prepared.request(progress.vertex_chunks == 0 ? 128 : 2U * 1024U * 1024U);
        if (!request)
            return request.error();
        const auto& next = request.value();
        if (next.phase == Phase::ready) {
            if (progress.vertex_chunks < 3)
                return failure("The large mesh was not subdivided into complete-record chunks");
            return prepared.take_mesh();
        }
        if (next.phase == Phase::attribute_pointer) {
            if (const auto setup = grouped_setup(prepared); !setup)
                return setup.error();
            continue;
        }
        const auto bytes = mesh_packet(prepared, next, progress);
        if (!bytes)
            return bytes.error();
        if (const auto step = advance_checked(prepared, next, bytes.value()); !step)
            return step.error();
    }
    return failure("Mesh preparation failed to reach a complete resource");
}
[[nodiscard]] Result<void> buffer_matches(GLuint object, std::span<const std::byte> source)
{
    const auto state = bindings();
    glBindBuffer(GL_COPY_READ_BUFFER, object);
    std::array<std::byte, 64U * 1024U> read{};
    for (std::size_t offset = 0; offset < source.size();) {
        const auto count = std::min(read.size(), source.size() - offset);
        // This fixture has less than 3 MiB of native storage, fitting GLintptr.
        glGetBufferSubData(GL_COPY_READ_BUFFER, static_cast<GLintptr>(offset),
                           static_cast<GLsizeiptr>(count), read.data());
        const auto actual = std::span{read}.first(count);
        if (!std::equal(actual.begin(), actual.end(), source.subspan(offset, count).begin())) {
            restore(state);
            return failure("Chunked mesh upload changed vertex/index payload bytes");
        }
        offset += count;
    }
    restore(state);
    return {};
}
[[nodiscard]] Result<void> verify_buffers(StaticMesh& mesh, const Payload& source)
{
    const auto view = backend::opengl::device_detail::mesh_view(mesh);
    if (!view)
        return view.error();
    const auto state = bindings();
    glBindVertexArray(view.value().vertex_array);
    GLint vertices = 0, indices = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vertices);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &indices);
    restore(state);
    if (const auto r = buffer_matches(static_cast<GLuint>(vertices),
                                      std::as_bytes(std::span{source.vertices}));
        !r)
        return r.error();
    return buffer_matches(static_cast<GLuint>(indices), std::as_bytes(std::span{source.indices}));
}
[[nodiscard]] Result<void> verify_abandon(Device& device, const MeshStorageDescription& description)
{
    const auto before = bindings();
    auto prepared = device.begin_static_mesh(description);
    if (!prepared)
        return prepared.error();
    if (!prepared.value()->advance())
        return failure("Cannot create the abandoned mesh preparation probe");
    std::uint32_t objects = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        const auto step = prepared.value()->advance_retirement({1, 3});
        if (!step)
            return step.error();
        objects += step.value().deleted_objects;
        if (step.value().complete)
            break;
    }
    if (objects != 3 || prepared.value()->request())
        return failure("Incomplete mesh retirement did not release its native objects");
    prepared.value().reset();
    if (bindings() != before || glGetError() != GL_NO_ERROR)
        return failure("Abandoning an incomplete mesh damaged foreign OpenGL state");
    return {};
}
[[nodiscard]] Result<void> verify_absent_mesh(Device& device, StaticMesh& mesh,
                                              const std::array<GLuint, 3>& objects)
{
    const auto repeated = device.retire_static_mesh(mesh, {0, 0});
    if (!repeated || !repeated.value().complete || repeated.value().driver_calls != 0)
        return failure("Mesh retirement repeated native work");
    if (glIsVertexArray(objects[0]) || glIsBuffer(objects[1]) || glIsBuffer(objects[2]))
        return failure("Mesh native objects survived complete retirement");
    return {};
}
[[nodiscard]] Result<void> verify_retirement(Device& device, StaticMesh& mesh)
{
    const auto& native = static_cast<const backend::opengl::device_detail::OpenGLStaticMesh&>(mesh);
    const std::array objects{native.vertex_array, native.vertex_buffer, native.index_buffer};
    const auto before = bindings();
    const auto zero = device.retire_static_mesh(mesh, {0, 0});
    if (!zero || zero.value().progressed || !backend::opengl::device_detail::mesh_view(mesh))
        return failure("A zero mesh retirement allowance changed readiness");
    for (std::size_t i = 0; i < 3; ++i) {
        const auto step = device.retire_static_mesh(mesh, {1, 3});
        if (!step || step.value().deleted_objects != 1 || step.value().driver_calls > 3 ||
            bindings() != before)
            return failure("Mesh retirement exceeded one native object or changed foreign state");
    }
    return verify_absent_mesh(device, mesh, objects);
}
[[nodiscard]] Result<void> run(backend::opengl::GraphicsProcedureLoader loader)
{
    auto device = backend::opengl::create_device({loader});
    if (!device)
        return device.error();
    ForeignBuffers foreign;
    if (const auto created = foreign.prepare(); !created)
        return created.error();
    const auto source = payload();
    auto prepared = device.value()->begin_static_mesh(source.storage);
    if (!prepared)
        return prepared.error();
    if (const auto rejected = reject_early(*prepared.value()); !rejected)
        return rejected.error();
    auto mesh = upload(*prepared.value(), source);
    if (!mesh)
        return mesh.error();
    if (prepared.value()->take_mesh() || prepared.value()->request())
        return failure("A consumed mesh preparation owner remained reusable");
    if (const auto checked = verify_buffers(*mesh.value(), source); !checked)
        return checked.error();
    if (const auto retired = verify_retirement(*device.value(), *mesh.value()); !retired)
        return retired.error();
    return verify_abandon(*device.value(), source.storage);
}
} // namespace
elf3d::Result<void>
verify_staged_mesh_upload(elf3d::backend::opengl::GraphicsProcedureLoader loader) noexcept
{
    try {
        return run(loader);
    } catch (const std::bad_alloc&) {
        elf3d::fatal_error("Mesh preparation probe allocation failed");
    } catch (...) {
        elf3d::fatal_error("Unexpected mesh preparation probe failure");
    }
}
elf3d::Result<void>
verify_staged_resource_uploads(elf3d::backend::opengl::GraphicsProcedureLoader loader) noexcept
{
    if (const auto mesh = verify_staged_mesh_upload(loader); !mesh)
        return mesh.error();
    return verify_staged_texture_upload(loader);
}
