/* third_party/freetype_build/ft2build.h -- OpenBA's FreeType entry header.
 *
 * FreeType's documented customisation point: a build's own ft2build.h names the
 * option and module files before the real header, so no compile definition has
 * to reach every consumer (the `-DFT_CONFIG_OPTIONS_H=<...>` form broke windres
 * on src/app/icon.rc). This directory precedes the submodule's include/ on the
 * include path for the library and for every target that links it.
 */
#ifndef BANISHED_FT2BUILD_H_
#define BANISHED_FT2BUILD_H_

#ifndef FT_CONFIG_OPTIONS_H
#define FT_CONFIG_OPTIONS_H <banished_ftoption.h>
#endif
#ifndef FT_CONFIG_MODULES_H
#define FT_CONFIG_MODULES_H <banished_ftmodule.h>
#endif

#include <freetype/config/ftheader.h>

#endif /* BANISHED_FT2BUILD_H_ */
