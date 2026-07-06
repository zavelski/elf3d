#ifndef ELF3D_MATH_GLM_CONFIG_H
#define ELF3D_MATH_GLM_CONFIG_H

#if defined(GLM_FORCE_ALIGNED_GENTYPES) || defined(GLM_FORCE_INTRINSICS) ||                        \
    defined(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
#error "Elf3D does not permit aligned or forced-SIMD GLM types"
#endif

#ifndef GLM_FORCE_CXX20
#define GLM_FORCE_CXX20
#endif

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#endif
