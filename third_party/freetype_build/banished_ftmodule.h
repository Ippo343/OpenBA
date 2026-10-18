/* third_party/freetype_build/banished_ftmodule.h -- OpenBA's FreeType module list
 * (FT_CONFIG_MODULES_H). The minimal set for rasterising one TrueType face:
 * sfnt + truetype load it, smooth renders the exact-coverage anti-aliasing,
 * raster1 the monochrome bitmap the GDI-like 8x8 supersample is counted from.
 * No Type 1 / CFF / CID / PFR / Type 42 / Windows FNT / PCF / BDF drivers, no
 * sdf / svg renderers, no psnames / pshinter / psaux, no autofit. Keep in step
 * with CMakeLists.txt's source list. */
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_raster1_renderer_class )
