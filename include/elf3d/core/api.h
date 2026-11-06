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

// Core diagnostics are supplied by either the source-integrated Model archive
// or the runtime DLL. Function calls must not force a DLL import when both are
// linked. Runtime classes still use ELF3D_API and retain their DLL ownership.
#if defined(ELF3D_BUILDING_LIBRARY)
#define ELF3D_CORE_API ELF3D_API
#else
#define ELF3D_CORE_API
#endif

#endif
