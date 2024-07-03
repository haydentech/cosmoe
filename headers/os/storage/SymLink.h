/*
 * Copyright 2002-2007 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYM_LINK_H
#define _SYM_LINK_H


#include <Node.h>
#include <StorageDefs.h>


class BDirectory;
class BPath;

class BSymLink : public BNode {
public:
								BSymLink();
								BSymLink(const BSymLink& other);
								BSymLink(const entry_ref* ref);
								BSymLink(const BEntry* entry);
								BSymLink(const char* path);
								BSymLink(const BDirectory* dir,
								         const char* path);
	virtual						~BSymLink();

	// WORKAROUND
	// SetTo() methods: Part of a work around until someone has an idea how to
	// get BPrivate::Storage::read_link(FileDescriptor,...) to work.
	status_t SetTo(const entry_ref *ref);
	status_t SetTo(const BEntry *entry);
	status_t SetTo(const char *path);
	status_t SetTo(const BDirectory *dir, const char *path);
	void Unset();

			ssize_t				ReadLink(char* buf, size_t size);

			ssize_t				MakeLinkedPath(const char* dirPath,
									BPath* path);
			ssize_t				MakeLinkedPath(const BDirectory* dir,
									BPath* path);

			bool				IsAbsolute();

	// WORKAROUND
	// operator=(): Part of a work around until someone has an idea how to
	// get BPrivate::Storage::read_link(FileDescriptor,...) to work.
	BSymLink &operator=(const BSymLink &link);

private:
	virtual	void				_MissingSymLink1();
	virtual	void				_MissingSymLink2();
	virtual	void				_MissingSymLink3();
	virtual	void				_MissingSymLink4();
	virtual	void				_MissingSymLink5();
	virtual	void				_MissingSymLink6();

			uint32				_reservedData[4];
			BEntry*				fSecretEntry;

private:
			int					get_fd() const;
};


#endif	// _SYM_LINK_H
