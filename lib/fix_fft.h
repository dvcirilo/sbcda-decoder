#ifndef FIX_FFT_H
#define FIX_FFT_H

int fix_fft(short fr[], short fi[], short m, short inverse);
int parallel_fix_fft(short fr[], short fi[], short m);
int fix_fftr(short*,int,int);

#endif
