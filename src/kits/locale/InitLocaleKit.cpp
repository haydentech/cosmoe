/*
 * This file contains library initialization code.
 * The required mimetypes and attribute-indices are created here.
 */


#include <DefaultCatalog.h>
#include <MutableLocaleRoster.h>
#include <SystemCatalog.h>


namespace BPrivate {

BCatalog gSystemCatalog;

}


using BPrivate::DefaultCatalog;
using BPrivate::MutableLocaleRoster;
using BPrivate::gSystemCatalog;


void
__initialize_locale_kit()
{
	MutableLocaleRoster::Default()->LoadSystemCatalog(&gSystemCatalog);
}
