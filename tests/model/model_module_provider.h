#ifndef ELF3D_TEST_MODEL_MODULE_PROVIDER_H
#define ELF3D_TEST_MODEL_MODULE_PROVIDER_H

#include <elf3d/model.h>

#if defined(_WIN32)
#if defined(ELF3D_TEST_BUILDING_MODEL_PROVIDER)
#define ELF3D_TEST_MODEL_PROVIDER_API __declspec(dllexport)
#else
#define ELF3D_TEST_MODEL_PROVIDER_API __declspec(dllimport)
#endif
#else
#define ELF3D_TEST_MODEL_PROVIDER_API __attribute__((visibility("default")))
#endif

namespace elf3d::test {

// Test-only source-integrated boundary, built with the same toolchain and CRT.
// This deliberately moves Model storage across two compiled copies; it is not
// a production runtime DLL API or a promise of a stable STL ABI.
[[nodiscard]] ELF3D_TEST_MODEL_PROVIDER_API Result<Document> make_module_document() noexcept;
[[nodiscard]] ELF3D_TEST_MODEL_PROVIDER_API Result<Document>
clone_module_document(DocumentView document) noexcept;
[[nodiscard]] ELF3D_TEST_MODEL_PROVIDER_API Result<void> check_module_node(const Document& document,
                                                                           NodeId node) noexcept;
[[nodiscard]] ELF3D_TEST_MODEL_PROVIDER_API Result<NodeId>
append_module_node(Document& document) noexcept;
ELF3D_TEST_MODEL_PROVIDER_API void replace_module_document(Document& document) noexcept;

} // namespace elf3d::test

#endif
