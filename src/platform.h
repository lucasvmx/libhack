#ifndef PLATFORM_H
#define PLATFORM_H


#if defined(__MINGW64__) || defined(_WIN64)
#define __x64__ 1
#define __win64 1
#define __windows__ 1
#elif defined(__MINGW32__) || defined(_WIN32)
#define __x86__ 1
#define __win32 1
#define __windows__ 1
#elif defined(__linux__) || defined(__x86_64__)
#define __linux64 1
#elif defined(__linux__)
#define __linux32 1
#else
#error "Unknown platform detected"
#endif

#endif // PLATFORM_H
