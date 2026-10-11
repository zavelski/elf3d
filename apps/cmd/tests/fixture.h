#ifndef ELF3D_CMD_TEST_FIXTURE_H
#define ELF3D_CMD_TEST_FIXTURE_H

#include <elf3d/model.h>

[[nodiscard]] elf3d::Result<elf3d::Document> make_fixture();
[[nodiscard]] int round_trip_test(std::string_view input, std::string_view output, bool zero);

#endif
