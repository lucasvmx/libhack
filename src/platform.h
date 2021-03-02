#ifndef PLATFORM_H
#define PLATFORM_H


#if defined(__MINGW64__) || defined(_WIN64)
#define __x64__
#elif __MINGW32__ || defined(_WIN32)
#define __x86__
#endif

#endif // PLATFORM_H
