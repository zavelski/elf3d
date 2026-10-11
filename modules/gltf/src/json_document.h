#ifndef ELF3D_GLTF_JSON_DOCUMENT_H
#define ELF3D_GLTF_JSON_DOCUMENT_H

#include <elf3d/core/result.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::gltf::json {

enum class Kind { object, array, string, number, literal };

// Flat, owned tokens avoid recursion and preserve opaque numeric spellings.
struct Value {
    Kind kind = Kind::literal;
    std::string key;
    std::string token;
    std::vector<std::size_t> children;
};

struct Document {
    std::vector<Value> values;
};

inline constexpr std::size_t maximum_json_bytes = 64U * 1024U * 1024U;
inline constexpr std::size_t maximum_json_values = 2U * 1024U * 1024U;
inline constexpr std::size_t maximum_json_depth = 256U;

[[nodiscard]] Result<Document> parse(std::string_view source);
[[nodiscard]] Result<std::string> write(const Document& document, std::size_t root = 0U);
[[nodiscard]] std::optional<std::size_t> member(const Document& document, std::size_t object,
                                                std::string_view key) noexcept;
void erase(Document& document, std::size_t object, std::string_view key);
[[nodiscard]] Result<std::string> string_value(std::string_view token);
[[nodiscard]] Result<std::size_t> unsigned_value(const Document& document, std::size_t value);
void omit_defaults(Document& document);
void order_sections(Document& document);
void reconcile_extensions(Document& document);
[[nodiscard]] std::size_t clean_rotation_matrices(Document& document);

} // namespace elf3d::gltf::json

#endif
