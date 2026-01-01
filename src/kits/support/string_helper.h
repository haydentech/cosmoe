//Those functions are here just to compile BString.
//They will be removed as soon as our libc compiles.

#ifndef __stringhelper_h
#define __stringhelper_h

#ifdef _WIN32
  #ifdef BUILDING_LIBBE
    #define LIBBE_EXPORT __declspec(dllexport)
  #else
    #define LIBBE_EXPORT __declspec(dllimport)
  #endif

  // strcasestr is a standard POSIX function on Linux/Mac, only declare on Windows
  extern "C" LIBBE_EXPORT char *strcasestr(const char *s, const char *find);
#else
  #define LIBBE_EXPORT
#endif

#endif //__stringhelper_h
