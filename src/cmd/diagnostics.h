#ifndef ELF3D_CMD_DIAGNOSTICS_H
#define ELF3D_CMD_DIAGNOSTICS_H

#include <elf3d/model.h>

#include <string_view>

namespace elf3d::cmd {

void print_error(std::string_view stage, const Error& error);
void print_report(const ModelLoadReport& report);
void print_report(const DocumentValidationReport& report);
void print_report(const ModelWriteReport& report);
void print_usage();

} // namespace elf3d::cmd

#endif
