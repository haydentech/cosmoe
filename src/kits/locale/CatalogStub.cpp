/*
 * Copyright 2010-2016, Adrien Destugues <pulkomandy@pulkomandy.tk>.
 * Distributed under the terms of the MIT License.
 */


#include <Catalog.h>
#include <LocaleRoster.h>

#include <locks.h>


static int32 sCatalogInitOnce = INIT_ONCE_UNINITIALIZED;


extern const char kGetCatalogAsmSymbol[] asm("_ZN13BLocaleRoster10GetCatalogEv");
extern const char kGetCatalogHiddenAsmSymbol[]
	asm(".hidden _ZN13BLocaleRoster10GetCatalogEv");
extern const char kGetCatalogImplAsmSymbol[]
	asm("_ZN13BLocaleRoster11_GetCatalogEP8BCatalogPi");
extern const char kGetCatalogImplHiddenAsmSymbol[]
	asm(".hidden _ZN13BLocaleRoster11_GetCatalogEP8BCatalogPi");


__attribute__((visibility("hidden"))) BCatalog*
BLocaleRoster::GetCatalog()
{
	static BCatalog sCatalog;

	return _GetCatalog(&sCatalog, &sCatalogInitOnce);
}


namespace BPrivate{
	void ForceUnloadCatalog()
	{
		sCatalogInitOnce = INIT_ONCE_UNINITIALIZED;
	}
}

