#include <elf3d/model.h>

#include <array>
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

namespace {
using namespace elf3d;
static_assert(!std::is_copy_constructible_v<DocumentHierarchySnapshot>);
static_assert(std::is_nothrow_move_constructible_v<DocumentHierarchySnapshot>);

[[nodiscard]] bool require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL hierarchy: %s\n", message);
    }
    return condition;
}

struct Fixture {
    Document document;
    NodeId root;
    NodeId detached;
    MeshId shared;
    MeshId unused;
    PrimitiveId primitive;
};

[[nodiscard]] bool has_position(const Result<std::size_t>& result, std::size_t expected)
{
    return result && result.value() == expected;
}

[[nodiscard]] bool resource_positions(const Fixture& value)
{
    const auto& document = value.document;
    const auto view = document.view();
    if (!has_position(document.index_of(value.shared), 0) ||
        !has_position(view.index_of(value.unused), 1))
        return false;
    if (!has_position(view.index_of(value.primitive), 0) ||
        !has_position(document.index_of(document.primitive_at(1).value().id), 1))
        return false;
    const auto material = document.material_at(0).value().id;
    if (!has_position(document.index_of(material), 0) || !has_position(view.index_of(material), 0))
        return false;
    return !document.index_of(MeshId{}) && !view.index_of(PrimitiveId{}) &&
           !view.index_of(MaterialId{});
}

[[nodiscard]] bool foreign_positions(const Fixture& value)
{
    const auto& document = value.document;
    Document foreign;
    const auto other = foreign.create_node("Same table slot").value();
    if (document.index_of(other) || foreign.index_of(value.root) || foreign.index_of(value.shared))
        return false;
    Document moved = std::move(foreign);
    return !foreign.index_of(other) && has_position(moved.view().index_of(other), 0);
}

[[nodiscard]] bool table_positions(const Fixture& value)
{
    const auto& document = value.document;
    const auto revision = document.revision();
    if (!has_position(document.index_of(value.root), 0) ||
        !has_position(document.view().index_of(value.detached), 1))
        return false;
    if (!resource_positions(value) || document.index_of(NodeId{}) ||
        DocumentView{}.index_of(value.root))
        return false;
    return foreign_positions(value) && document.revision() == revision;
}

[[nodiscard]] Result<Fixture> fixture()
{
    Fixture result;
    auto& doc = result.document;
    const auto first = doc.create_scene("First").value();
    const auto second = doc.create_scene("Second").value();
    result.root = doc.create_node(std::string(10000, 'x')).value();
    result.detached = doc.create_node("Outside Scenes").value();
    result.shared = doc.create_mesh("Mesh \xE2\x98\x83").value();
    result.unused = doc.create_mesh().value();
    const auto material = doc.create_material().value();
    const std::array<Float3, 3> positions{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    PrimitiveDataView geometry;
    geometry.positions = positions;
    geometry.indices = indices;
    result.primitive = doc.create_primitive(result.shared, material, geometry).value();
    if (!doc.create_primitive(result.unused, material, geometry)) {
        return Error{ErrorCode::invalid_argument, "Cannot build unused primitive fixture"};
    }
    if (!doc.clear_default_scene() || !doc.add_scene_root(first, result.root) ||
        !doc.add_scene_root(second, result.root) ||
        !doc.set_node_mesh(result.root, result.shared)) {
        return Error{ErrorCode::invalid_argument, "Cannot build shared-scene fixture"};
    }
    for (unsigned index = 0; index < 200; ++index) {
        const auto child = doc.create_node(index % 2 == 0 ? "Duplicate" : "").value();
        if (!doc.set_parent(child, result.root) || !doc.set_node_mesh(child, result.shared)) {
            return Error{ErrorCode::invalid_argument, "Cannot build hierarchy fanout"};
        }
    }
    return result;
}

[[nodiscard]] bool inspect_tables(const DocumentHierarchySnapshot& index)
{
    return require(index.ready() && index.scene_count() == 2 && index.node_count() == 202 &&
                       index.mesh_count() == 2 && index.primitive_count() == 2 &&
                       !index.default_scene(),
                   "retained tables and absent default scene");
}

[[nodiscard]] bool inspect_objects(const DocumentHierarchySnapshot& index, const Fixture& source)
{
    const auto root = index.node(source.root);
    return require(root && root.value().name.size() == 10000 &&
                       root.value().children.size() == 200 && root.value().in_authored_scene,
                   "long name and wide child list are complete") &&
           require(!index.node(source.detached).value().in_authored_scene,
                   "detached forest stays outside authored scenes") &&
           require(index.scene_at(0).value().roots.front() == source.root &&
                       index.scene_at(1).value().roots.front() == source.root,
                   "two scene occurrences use the same canonical node identity");
}

[[nodiscard]] bool inspect_resources(const DocumentHierarchySnapshot& index, const Fixture& source)
{
    const auto mesh = index.mesh(source.shared);
    return require(mesh && mesh.value().users.size() == 201 &&
                       mesh.value().primitives.size() == 1 &&
                       mesh.value().name == "Mesh \xE2\x98\x83",
                   "shared mesh names and canonical usage") &&
           require(index.mesh(source.unused).value().users.empty() &&
                       index.mesh(source.unused).value().primitives.size() == 1,
                   "unused mesh and its primitive remain in table") &&
           require(index.primitive(source.primitive).value().triangles == 1,
                   "primitive metadata has canonical ownership and scalar counts");
}

[[nodiscard]] bool inspect(const DocumentHierarchySnapshot& index, const Fixture& source)
{
    return inspect_tables(index) && inspect_objects(index, source) &&
           inspect_resources(index, source);
}

[[nodiscard]] bool advance_single_units(DocumentHierarchySnapshot& index, DocumentView source)
{
    std::size_t steps = 0;
    while (!index.ready() && steps < 5000) {
        if (!index.advance(source, 1)) {
            return false;
        }
        ++steps;
    }
    return require(steps > 800 && steps < 5000,
                   "wide names, links and usage advance in bounded units");
}

[[nodiscard]] bool source_independent(Fixture& source, const DocumentHierarchySnapshot& index)
{
    auto synchronous = source.document.hierarchy_snapshot();
    if (!synchronous || !inspect(synchronous.value(), source)) {
        return false;
    }
    source.document = Document{};
    return inspect(index, source) && inspect(synchronous.value(), source) &&
           require(!index.matches_document(source.document.view()),
                   "owned metadata survives source destruction without reviving identity");
}

[[nodiscard]] bool bounded_and_owned()
{
    auto created = fixture();
    if (!created) {
        return false;
    }
    auto source = std::move(created).value();
    if (!require(table_positions(source), "typed positions preserve ownership and revision"))
        return false;
    const auto statistics = source.document.statistics();
    auto built = DocumentHierarchySnapshot::create(source.document.view());
    if (!built || built.value().ready() || built.value().node_at(0) ||
        built.value().advance(source.document.view(), 0)) {
        return require(false, "partial snapshot and zero budget are rejected");
    }
    auto index = std::move(built).value();
    return advance_single_units(index, source.document.view()) && inspect(index, source) &&
           require(source.document.statistics() == statistics,
                   "index leaves canonical payload intact") &&
           source_independent(source, index);
}

[[nodiscard]] bool empty_owners(Document& document, const Document& foreign)
{
    auto empty = document.hierarchy_snapshot();
    return require(empty && empty.value().ready() && empty.value().node_count() == 0 &&
                       !empty.value().matches_document(foreign.view()),
                   "empty owners keep distinct identities");
}

[[nodiscard]] bool invalid_ids(const DocumentHierarchySnapshot& index, NodeId foreign)
{
    return require(!index.node(foreign) && !index.mesh(MeshId{}) &&
                       !index.primitive(PrimitiveId{}) && !index.node_at(1),
                   "foreign, empty and out-of-range IDs are rejected");
}

[[nodiscard]] bool moved_owner(Document document, const DocumentHierarchySnapshot& index,
                               NodeId node)
{
    return require(index.matches_document(document.view()) &&
                       index.node(node).value().name == "Duplicate" &&
                       !DocumentHierarchySnapshot::create(DocumentView{}),
                   "moving the owner preserves identity and empty views fail");
}

[[nodiscard]] bool source_validation()
{
    Document document;
    Document foreign;
    if (!empty_owners(document, foreign)) {
        return false;
    }
    auto pending = DocumentHierarchySnapshot::create(document.view());
    const auto captured_revision = document.revision();
    const auto node = document.create_node("Duplicate").value();
    if (!require(document.revision() > captured_revision &&
                     !pending.value().advance(document.view()),
                 "mutation invalidates a captured revision")) {
        return false;
    }
    auto current = document.hierarchy_snapshot();
    const auto foreign_node = foreign.create_node("Duplicate").value();
    if (!invalid_ids(current.value(), foreign_node)) {
        return false;
    }
    const bool retained = moved_owner(std::move(document), current.value(), node);
    return retained && require(document.revision() == 0, "moved-from owner has revision zero");
}

[[nodiscard]] bool persistent_revision()
{
    auto source = std::move(fixture()).value();
    const auto structural = source.document.revision();
    ModelDocumentMetadata metadata;
    metadata.root.extras_json = "{\"label\":\"Retained\"}";
    if (!source.document.attach_verified_metadata(std::move(metadata)) ||
        source.document.revision() <= structural) {
        return require(false, "metadata attachment advances canonical revision");
    }
    const auto annotated = source.document.revision();
    const auto geometry = source.document.primitive(source.primitive).value().data;
    const auto replaced = source.document.replace_primitive(
        source.primitive, geometry, PrimitiveMetadataPolicy::preserve_verified_references);
    return require(replaced && source.document.revision() > annotated &&
                       !source.document.preserved_metadata_stale(),
                   "verified geometry changes advance revision without invalidating metadata");
}
} // namespace

int elf3d_model_hierarchy_test()
{
    return bounded_and_owned() && source_validation() && persistent_revision() ? 0 : 1;
}
