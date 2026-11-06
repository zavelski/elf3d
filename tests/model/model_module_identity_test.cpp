#include "model_module_provider.h"

#include <elf3d/model.h>

#include <cstdio>
#include <utility>

namespace {

int verify_foreign_ids(const elf3d::Document& local, elf3d::NodeId local_id,
                       const elf3d::Document& remote, elf3d::NodeId remote_id)
{
    const auto local_foreign = local.node(remote_id);
    const auto remote_foreign = elf3d::test::check_module_node(remote, local_id);
    if (local_foreign || remote_foreign ||
        local_foreign.error().code() != elf3d::ErrorCode::invalid_node_id ||
        remote_foreign.error().code() != elf3d::ErrorCode::invalid_node_id) {
        return 4;
    }
    return 0;
}

int verify_transferred_storage(elf3d::Document& local, elf3d::NodeId local_id,
                               elf3d::Document remote, elf3d::NodeId remote_id);

int verify_module_clones(const elf3d::Document& local, const elf3d::Document& remote)
{
    auto from_exe = elf3d::test::clone_module_document(local.view());
    auto from_dll = elf3d::clone_document(remote.view());
    if (!from_exe || !from_dll)
        return 7;
    const auto local_node = local.node_at(0).value();
    const auto remote_node = remote.node_at(0).value();
    const auto cloned_local = from_exe.value().node_at(0).value();
    const auto cloned_remote = from_dll.value().node_at(0).value();
    if (local_node.name != cloned_local.name || remote_node.name != cloned_remote.name)
        return 8;
    if (verify_foreign_ids(local, local_node.id, from_exe.value(), cloned_local.id) != 0)
        return 9;
    if (verify_foreign_ids(remote, remote_node.id, from_dll.value(), cloned_remote.id) != 0)
        return 10;
    // Each clone is destroyed through the other implementation's matching CRT.
    elf3d::test::replace_module_document(from_dll.value());
    from_exe.value() = elf3d::Document{};
    return local.node(local_node.id) && remote.node(remote_node.id) ? 0 : 11;
}

int verify_module_owners()
{
    elf3d::Document local;
    const auto local_node =
        local.create_node("An EXE-owned node name long enough to allocate storage");
    auto created = elf3d::test::make_module_document();
    if (!local_node || !created) {
        return 1;
    }
    auto remote = std::move(created).value();
    const auto remote_node = remote.node_at(0);
    if (!remote_node) {
        return 2;
    }
    const auto remote_id = remote_node.value().id;
    if (local_node.value().debug_value() != remote_id.debug_value() ||
        local_node.value() == remote_id) {
        std::fprintf(stderr, "Document identities collide between executable and provider DLL\n");
        return 3;
    }
    const int foreign_result = verify_foreign_ids(local, local_node.value(), remote, remote_id);
    if (foreign_result != 0) {
        return foreign_result;
    }
    if (const int clone_result = verify_module_clones(local, remote); clone_result != 0) {
        return clone_result;
    }
    return verify_transferred_storage(local, local_node.value(), std::move(remote), remote_id);
}

int verify_transferred_storage(elf3d::Document& local, elf3d::NodeId local_id,
                               elf3d::Document remote, elf3d::NodeId remote_id)
{
    auto moved = std::move(remote);
    const auto appended = elf3d::test::append_module_node(local);
    if (!moved.node(remote_id) || !appended || !local.node(appended.value()) ||
        !elf3d::test::check_module_node(local, local_id)) {
        return 5;
    }
    // Release DLL allocations through the EXE copy, and EXE allocations through
    // the DLL copy, using the required shared CRT in both directions.
    moved = elf3d::Document{};
    elf3d::test::replace_module_document(local);
    if (moved.node(remote_id) || local.node(local_id) || local.node(appended.value()) ||
        elf3d::validate_document(moved.view()).has_errors() ||
        elf3d::validate_document(local.view()).has_errors()) {
        return 6;
    }
    return 0;
}

} // namespace

int main()
{
    for (int iteration = 0; iteration < 8; ++iteration) {
        if (const int result = verify_module_owners(); result != 0) {
            return result;
        }
    }
    return 0;
}
