//Those functions are here just to compile BString.
//They will be removed as soon as our libc compiles.

#include <ctype.h>
#include <string.h>
#include "string_helper.h"

#ifdef _WIN32
// strcasestr is a standard POSIX function on Linux/Mac, only define on Windows
char *
strcasestr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return NULL;
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return (char *)s;
}
#endif // _WIN32

