#include <elf3d/elf3d.h>

#include <string_view>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdlib>

namespace {

// The seven parameters are fixed by the MSVC Debug CRT allocation-hook ABI.
int fail_allocation_once(int allocation_type, void*, std::size_t size, int, long,
                         const unsigned char*, int)
{
    // Skip the STL's small iterator proxies, allocated in noexcept constructors.
    if (allocation_type == _HOOK_ALLOC && size > 2 * sizeof(void*)) {
        _CrtSetAllocHook(nullptr);
        return 0;
    }
    return 1;
}

} // namespace

int run_scene_allocation_failure(elf3d::Engine& engine, elf3d::Scene& scene, elf3d::EntityId entity,
                                 std::string_view scenario)
{
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetAllocHook(fail_allocation_once);
    if (scenario == "scene_create") {
        static_cast<void>(engine.create_scene());
    } else if (scenario == "scene_snapshot") {
        static_cast<void>(scene.hierarchy_snapshot());
    } else if (scenario == "scene_name") {
        static_cast<void>(scene.set_entity_name(
            entity, "A deliberately long scene entity name that requires owned allocation"));
    } else if (scenario == "scene_visibility") {
        static_cast<void>(scene.set_entity_local_visibility(entity, false));
    } else if (scenario == "scene_bounds") {
        static_cast<void>(scene.world_bounds());
    }
    _CrtSetAllocHook(nullptr);
    return 90;
}
#endif
