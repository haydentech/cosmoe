/*
 * Copyright 2003-2012, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */
#ifndef _CATALOG_H_
#define _CATALOG_H_



// Translation macros which may be used to shorten translation requests:
#undef B_TRANSLATE
#define B_TRANSLATE(string) string

#undef B_TRANSLATE_CONTEXT
#define B_TRANSLATE_CONTEXT(string, context) string

#undef B_TRANSLATE_COMMENT
#define B_TRANSLATE_COMMENT(string, comment) string

#undef B_TRANSLATE_ALL
#define B_TRANSLATE_ALL(string, context, comment) string

#undef B_TRANSLATE_SYSTEM_NAME
#define B_TRANSLATE_SYSTEM_NAME(string) string

#undef B_TRANSLATE_MARK
#define B_TRANSLATE_MARK(string) (string)

#undef B_TRANSLATE_MARK_CONTEXT
#define B_TRANSLATE_MARK_CONTEXT(string, context) (string)

#undef B_TRANSLATE_MARK_COMMENT
#define B_TRANSLATE_MARK_COMMENT(string, comment) (string)

#undef B_TRANSLATE_MARK_ALL
#define B_TRANSLATE_MARK_ALL(string, context, comment) (string)

#undef B_TRANSLATE_MARK_ID
#define B_TRANSLATE_MARK_ID(id) (id)

#undef B_TRANSLATE_MARK_SYSTEM_NAME
#define B_TRANSLATE_MARK_SYSTEM_NAME(string) (string)

// the same for void contexts:
#undef B_TRANSLATE_MARK_VOID
#define B_TRANSLATE_MARK_VOID(string)

#undef B_TRANSLATE_MARK_CONTEXT_VOID
#define B_TRANSLATE_MARK_CONTEXT_VOID(string, context)

#undef B_TRANSLATE_MARK_COMMENT_VOID
#define B_TRANSLATE_MARK_COMMENT_VOID(string, comment)

#undef B_TRANSLATE_MARK_ALL_VOID
#define B_TRANSLATE_MARK_ALL_VOID(string, context, comment)

#undef B_TRANSLATE_MARK_ID_VOID
#define B_TRANSLATE_MARK_ID_VOID(id)

#undef B_TRANSLATE_MARK_SYSTEM_NAME_VOID
#define B_TRANSLATE_MARK_SYSTEM_NAME_VOID(string)

// Translation macros which cause collectcatkeys to ignore this key
// (useful in combination with the marking macros above):
#undef B_TRANSLATE_NOCOLLECT
#define B_TRANSLATE_NOCOLLECT(string) \
	B_TRANSLATE(string)

#undef B_TRANSLATE_NOCOLLECT_COMMENT
#define B_TRANSLATE_NOCOLLECT_COMMENT(string, comment) \
	B_TRANSLATE_COMMENT(string, comment)

#undef B_TRANSLATE_NOCOLLECT_ALL
#define B_TRANSLATE_NOCOLLECT_ALL(string, context, comment) \
	B_TRANSLATE_ALL(string, context, comment)

#undef B_TRANSLATE_NOCOLLECT_ID
#define B_TRANSLATE_NOCOLLECT_ID(id) \
	B_TRANSLATE_ID(id)

#undef B_TRANSLATE_NOCOLLECT_SYSTEM_NAME
#define B_TRANSLATE_NOCOLLECT_SYSTEM_NAME(string) \
	B_TRANSLATE_SYSTEM_NAME(string)


#endif /* _CATALOG_H_ */
