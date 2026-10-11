#ifndef ELF3D_CMD_ROTATE_DOCUMENT_H
#define ELF3D_CMD_ROTATE_DOCUMENT_H

#include <elf3d/model.h>

namespace elf3d::cmd {

// Degrees, positive right-handed rotations about fixed axes: X, then Y, then Z.
// Pivot: the center of the world-space bounds of all node-instanced geometry.
// Nonzero rotation requires geometry and affine node matrices.
// Failure can leave earlier roots changed; the caller must discard the document.
[[nodiscard]] Result<void> rotate_document(Document& document, Float3 degrees) noexcept;

} // namespace elf3d::cmd

#endif
