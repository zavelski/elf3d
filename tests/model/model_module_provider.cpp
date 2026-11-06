#include "model_module_provider.h"

namespace elf3d::test {

Result<Document> make_module_document() noexcept
{
    Document document;
    const auto node = document.create_node("A DLL-owned node name long enough to allocate storage");
    if (!node) {
        return node.error();
    }
    return document;
}

Result<void> check_module_node(const Document& document, NodeId node) noexcept
{
    const auto found = document.node(node);
    if (!found) {
        return found.error();
    }
    return {};
}

Result<Document> clone_module_document(DocumentView document) noexcept
{
    return clone_document(document);
}

Result<NodeId> append_module_node(Document& document) noexcept
{
    return document.create_node("A node allocated by DLL code into caller-owned Document storage");
}

void replace_module_document(Document& document) noexcept
{
    document = Document{};
}

} // namespace elf3d::test
