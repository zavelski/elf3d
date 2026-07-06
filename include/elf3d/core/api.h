#ifndef ELF3D_CORE_API_H
#define ELF3D_CORE_API_H

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(ELF3D_BUILDING_LIBRARY)
#define ELF3D_API __declspec(dllexport)
#else
#define ELF3D_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define ELF3D_API __attribute__((visibility("default")))
#else
#define ELF3D_API
#endif

#endif
