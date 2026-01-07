/*
 * Dummy resource symbols for libraries that don't have embedded resources.
 * These are needed because MinGW doesn't properly support weak symbols.
 * Applications with actual embedded resources will override these with
 * strong symbols from the objcopy-generated object file.
 */

const unsigned char _binary_app_rsrc_start[1] = {0};
const unsigned char _binary_app_rsrc_end[1] = {0};
const unsigned char _binary_app_rsrc_size[1] = {0};
