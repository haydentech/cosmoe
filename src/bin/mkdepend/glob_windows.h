/*
 * Minimal glob.h implementation for Windows
 * This is a simplified version that only supports basic wildcard expansion
 */
#ifndef GLOB_WINDOWS_H
#define GLOB_WINDOWS_H

#ifdef _WIN32

#include <stddef.h>

/* Error return values */
#define GLOB_NOSPACE    1
#define GLOB_ABORTED    2
#define GLOB_NOMATCH    3

/* Flags */
#define GLOB_NOSORT     0x01
#define GLOB_BRACE      0x02
#define GLOB_TILDE      0x04

typedef struct {
    size_t   gl_pathc;    /* Count of paths matched by pattern */
    char   **gl_pathv;    /* List of matched pathnames */
    size_t   gl_offs;     /* Slots to reserve in gl_pathv */
} glob_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Simple glob implementation for Windows */
static inline int glob(const char *pattern, int flags, 
                      int (*errfunc)(const char *, int),
                      glob_t *pglob)
{
    /* For Windows, we'll just add the pattern itself without expansion
     * This is a simplified approach - mkdepend usually gets explicit filenames anyway */
    (void)flags;
    (void)errfunc;
    
    if (!pattern || !pglob)
        return GLOB_ABORTED;
    
    /* Check if pattern contains wildcards */
    if (strchr(pattern, '*') || strchr(pattern, '?')) {
        /* For now, return NOMATCH for wildcards on Windows
         * A full implementation would use FindFirstFile/FindNextFile */
        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
        return GLOB_NOMATCH;
    }
    
    /* No wildcards - just add the pattern as-is */
    pglob->gl_pathc = 1;
    pglob->gl_pathv = (char **)malloc(2 * sizeof(char *));
    if (!pglob->gl_pathv)
        return GLOB_NOSPACE;
    
    pglob->gl_pathv[0] = _strdup(pattern);
    pglob->gl_pathv[1] = NULL;
    
    if (!pglob->gl_pathv[0]) {
        free(pglob->gl_pathv);
        return GLOB_NOSPACE;
    }
    
    return 0;
}

static inline void globfree(glob_t *pglob)
{
    if (pglob && pglob->gl_pathv) {
        for (size_t i = 0; i < pglob->gl_pathc; i++) {
            free(pglob->gl_pathv[i]);
        }
        free(pglob->gl_pathv);
        pglob->gl_pathv = NULL;
        pglob->gl_pathc = 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* GLOB_WINDOWS_H */
