/*******************************************************************
 * resample_48khz.c
 *
 * Includes the following resampling combinations
 * 48 kHz -> 16 kHz
 * 16 kHz -> 48 kHz
 * 48 kHz ->  8 kHz
 *  8 kHz -> 48 kHz
 *
 ******************************************************************/

typedef struct {
        int S_48_48[16];
        int S_48_32[8];
        int S_32_16[8];
} WebRtcSpl_State48khzTo16khz;

void WebRtcSpl_Resample48khzTo16khz(const short* in, short* out,
                                    WebRtcSpl_State48khzTo16khz* state,
                                    int* tmpmem,
                                    int input_len,
                                    int output_len);

void WebRtcSpl_ResetResample48khzTo16khz(WebRtcSpl_State48khzTo16khz* state);

void WebRtcSpl_LPBy2ShortToInt(const short* in, int len,
                               int* out, int* state);
void WebRtcSpl_LPBy2ShortToInt(const short* in, int len, int* out,
                               int* state);
void WebRtcSpl_DownBy2IntToShort(int *in, int len, short *out,
                                 int *state);
