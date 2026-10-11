#include "rotate_document.h"

#include <elf3d/core/assert.h>
#include <elf3d/model.h>

#include <exception>
#include <iostream>

int main(int count, char** arguments)
{
    try {
        if (count != 3) {
            return 2;
        }
        auto loaded = elf3d::load_document(arguments[1]);
        if (!loaded) {
            std::cerr << loaded.error().message() << '\n';
            return 1;
        }
        auto& document = loaded.value().document;
        if (!elf3d::cmd::rotate_document(document, {0.0F, 0.0F, 90.0F}) ||
            elf3d::validate_document(document.view()).has_errors()) {
            return 1;
        }
        const auto saved = elf3d::save_document(arguments[2], document.view());
        if (!saved) {
            std::cerr << saved.error().message() << '\n';
            return 1;
        }
        const auto reloaded = elf3d::load_document(arguments[2]);
        return reloaded && reloaded.value().document.statistics() == document.statistics() ? 0 : 1;
    } catch (...) {
        elf3d::fatal_error("Unexpected failure at external model process boundary");
    }
}
