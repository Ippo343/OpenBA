/* Banished reimplementation, third_party/wmadec: only the WMA v2 decoder is built
 * (Banished's xWMA is WAVE_FORMAT_WMAUDIO2 = 0x0161 = wmav2). Not an FFmpeg file. */
#ifndef WMADEC_SHIM_CONFIG_COMPONENTS_H
#define WMADEC_SHIM_CONFIG_COMPONENTS_H
#define CONFIG_WMAV1_DECODER 0
#define CONFIG_WMAV2_DECODER 1
#endif
