//#include "../config.h"


#ifndef __VCR_RESAMPLE__
#define __VCR_RESAMPLE__

int VCR_resample(short** dst, int dst_freq,
                 const short* src, int src_freq, int src_bitrate, int src_len);


int VCR_getResampleLen(int dst_freq, int src_freq, int src_bitrate,
                       int src_len);

#endif // __VCR_RESAMPLE__

