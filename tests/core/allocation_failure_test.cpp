#include <core/exception_boundary.h>
#include <elf3d/model.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <string_view>
#include <vector>

namespace {

bool fail_next_allocation = false;
std::size_t minimum_failure_size = 2 * sizeof(void*) + 1;

int run_failure(std::string_view scenario)
{
    if (scenario == "document_create") {
        fail_next_allocation = true;
        elf3d::Document document;
        return 1;
    }
    if (scenario == "unexpected") {
        elf3d::detail::fatal_exception_boundary([] { throw 1; });
        return 1;
    }

    elf3d::Document document;
    if (scenario == "document_clone") {
        const std::vector<std::byte> pixels(4096, std::byte{255});
        const auto image =
            document.create_image({1, 1024, elf3d::PixelFormat::rgba8_unorm, pixels});
        if (!image)
            return 3;
        // Skip clone validation/container bookkeeping and fail its owned pixel copy.
        minimum_failure_size = pixels.size();
        fail_next_allocation = true;
        const auto copy = elf3d::clone_document(document.view());
        return copy ? 1 : 2;
    }
    fail_next_allocation = true;
    if (scenario == "document_node") {
        const auto node = document.create_node();
        return node ? 1 : 2;
    }
    if (scenario == "document_validation") {
        const auto report = elf3d::validate_document(elf3d::DocumentView{});
        return report.has_errors() ? 1 : 2;
    }
    return 3;
}

} // namespace

// This executable links the Model SDK statically so this test-only allocation
// replacement also covers allocations inside the actual Document implementation.
void* operator new(std::size_t size)
{
    // MSVC Debug containers allocate small iterator proxies inside noexcept
    // constructors. Inject into owned data allocation, not STL debug bookkeeping.
    if (!fail_next_allocation || size < minimum_failure_size) {
        if (void* memory = std::malloc(size == 0 ? 1 : size)) {
            return memory;
        }
    }
    fail_next_allocation = false;
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

int main(int argument_count, char** arguments)
{
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    return argument_count == 2 ? run_failure(arguments[1]) : 3;
}
