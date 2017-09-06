#include "resample.h"
#include <string.h>

// allpass filter coefficients.
static const short kResampleAllpass[2][3] = {
        { 821, 6110, 12382 },
        { 3050, 9368, 15063 }
};
// interpolation coefficients
static const short kCoefficients48To32[2][8] = {
        { 778, -2050, 1087, 23285, 12903, -3783, 441, 222 },
        { 222, 441, -3783, 12903, 23285, 1087, -2050, 778 }
};
// initialize state of 48 -> 16 resampler
void WebRtcSpl_ResetResample48khzTo16khz(WebRtcSpl_State48khzTo16khz *state)
{
        memset(state->S_48_48, 0, 16 * sizeof(int));
        memset(state->S_48_32, 0, 8 * sizeof(int));
        memset(state->S_32_16, 0, 8 * sizeof(int));
}

//   lowpass filter
// input:  short
// output: int (normalized, not saturated)
// state:  filter state array; length = 8
void WebRtcSpl_LPBy2ShortToInt(const short* in, int len, int* out,
                               int* state)
{
        int tmp0, tmp1, diff;
        int i;

        len >>= 1;

        // lower allpass filter: odd input -> even output samples
        in++;
        // initial state of polyphase delay element
        tmp0 = state[12];
        for (i = 0; i < len; i++)
        {
                diff = tmp0 - state[1];
                // scale down and round
                diff = (diff + (1 << 13)) >> 14;
                tmp1 = state[0] + diff * kResampleAllpass[1][0];
                state[0] = tmp0;
                diff = tmp1 - state[2];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                tmp0 = state[1] + diff * kResampleAllpass[1][1];
                state[1] = tmp1;
                diff = tmp0 - state[3];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                state[3] = state[2] + diff * kResampleAllpass[1][2];
                state[2] = tmp0;

                // scale down, round and store
                out[i << 1] = state[3] >> 1;
                tmp0 = ((int)in[i << 1] << 15) + (1 << 14);
        }
        in--;

        // upper allpass filter: even input -> even output samples
        for (i = 0; i < len; i++)
        {
                tmp0 = ((int)in[i << 1] << 15) + (1 << 14);
                diff = tmp0 - state[5];
                // scale down and round
                diff = (diff + (1 << 13)) >> 14;
                tmp1 = state[4] + diff * kResampleAllpass[0][0];
                state[4] = tmp0;
                diff = tmp1 - state[6];
                // scale down and round
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                tmp0 = state[5] + diff * kResampleAllpass[0][1];
                state[5] = tmp1;
                diff = tmp0 - state[7];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                state[7] = state[6] + diff * kResampleAllpass[0][2];
                state[6] = tmp0;

                // average the two allpass outputs, scale down and store
                out[i << 1] = (out[i << 1] + (state[7] >> 1)) >> 15;
        }

        // switch to odd output samples
        out++;

        // lower allpass filter: even input -> odd output samples
        for (i = 0; i < len; i++)
        {
                tmp0 = ((int)in[i << 1] << 15) + (1 << 14);
                diff = tmp0 - state[9];
                // scale down and round
                diff = (diff + (1 << 13)) >> 14;
                tmp1 = state[8] + diff * kResampleAllpass[1][0];
                state[8] = tmp0;
                diff = tmp1 - state[10];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                tmp0 = state[9] + diff * kResampleAllpass[1][1];
                state[9] = tmp1;
                diff = tmp0 - state[11];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                state[11] = state[10] + diff * kResampleAllpass[1][2];
                state[10] = tmp0;

                // scale down, round and store
                out[i << 1] = state[11] >> 1;
        }

        // upper allpass filter: odd input -> odd output samples
        in++;
        for (i = 0; i < len; i++)
        {
                tmp0 = ((int)in[i << 1] << 15) + (1 << 14);
                diff = tmp0 - state[13];
                // scale down and round
                diff = (diff + (1 << 13)) >> 14;
                tmp1 = state[12] + diff * kResampleAllpass[0][0];
                state[12] = tmp0;
                diff = tmp1 - state[14];
                // scale down and round
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                tmp0 = state[13] + diff * kResampleAllpass[0][1];
                state[13] = tmp1;
                diff = tmp0 - state[15];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                state[15] = state[14] + diff * kResampleAllpass[0][2];
                state[14] = tmp0;

                // average the two allpass outputs, scale down and store
                out[i << 1] = (out[i << 1] + (state[15] >> 1)) >> 15;
        }
}

void WebRtcSpl_DownBy2IntToShort(int *in, int len, short *out,
                                 int *state)
{
        int tmp0, tmp1, diff;
        int i;

        len >>= 1;

        // lower allpass filter (operates on even input samples)
        for (i = 0; i < len; i++)
        {
                tmp0 = in[i << 1];
                diff = tmp0 - state[1];
                // scale down and round
                diff = (diff + (1 << 13)) >> 14;
                tmp1 = state[0] + diff * kResampleAllpass[1][0];
                state[0] = tmp0;
                diff = tmp1 - state[2];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                tmp0 = state[1] + diff * kResampleAllpass[1][1];
                state[1] = tmp1;
                diff = tmp0 - state[3];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                state[3] = state[2] + diff * kResampleAllpass[1][2];
                state[2] = tmp0;

                // divide by two and store temporarily
                in[i << 1] = (state[3] >> 1);
        }

        in++;

        // upper allpass filter (operates on odd input samples)
        for (i = 0; i < len; i++)
        {
                tmp0 = in[i << 1];
                diff = tmp0 - state[5];
                // scale down and round
                diff = (diff + (1 << 13)) >> 14;
                tmp1 = state[4] + diff * kResampleAllpass[0][0];
                state[4] = tmp0;
                diff = tmp1 - state[6];
                // scale down and round
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                tmp0 = state[5] + diff * kResampleAllpass[0][1];
                state[5] = tmp1;
                diff = tmp0 - state[7];
                // scale down and truncate
                diff = diff >> 14;
                if (diff < 0)
                        diff += 1;
                state[7] = state[6] + diff * kResampleAllpass[0][2];
                state[6] = tmp0;

                // divide by two and store temporarily
                in[i << 1] = (state[7] >> 1);
        }

        in--;

        // combine allpass outputs
        for (i = 0; i < len; i += 2)
        {
                // divide by two, add both allpass outputs and round
                tmp0 = (in[i << 1] + in[(i << 1) + 1]) >> 15;
                tmp1 = (in[(i << 1) + 2] + in[(i << 1) + 3]) >> 15;
                if (tmp0 >(int)0x00007FFF)
                        tmp0 = 0x00007FFF;
                if (tmp0 < (int)0xFFFF8000)
                        tmp0 = 0xFFFF8000;
                out[i] = (short)tmp0;
                if (tmp1 >(int)0x00007FFF)
                        tmp1 = 0x00007FFF;
                if (tmp1 < (int)0xFFFF8000)
                        tmp1 = 0xFFFF8000;
                out[i + 1] = (short)tmp1;
        }
}
void WebRtcSpl_Resample48khzTo32khz(const int *In, int *Out,
                                    int K)
{
        /////////////////////////////////////////////////////////////
        // Filter operation:
        //
        // Perform resampling (3 input samples -> 2 output samples);
        // process in sub blocks of size 3 samples.
        int tmp;
        int m;

        for (m = 0; m < K; m++)
        {
                tmp = 1 << 14;
                tmp += kCoefficients48To32[0][0] * In[0];
                tmp += kCoefficients48To32[0][1] * In[1];
                tmp += kCoefficients48To32[0][2] * In[2];
                tmp += kCoefficients48To32[0][3] * In[3];
                tmp += kCoefficients48To32[0][4] * In[4];
                tmp += kCoefficients48To32[0][5] * In[5];
                tmp += kCoefficients48To32[0][6] * In[6];
                tmp += kCoefficients48To32[0][7] * In[7];
                Out[0] = tmp;

                tmp = 1 << 14;
                tmp += kCoefficients48To32[1][0] * In[1];
                tmp += kCoefficients48To32[1][1] * In[2];
                tmp += kCoefficients48To32[1][2] * In[3];
                tmp += kCoefficients48To32[1][3] * In[4];
                tmp += kCoefficients48To32[1][4] * In[5];
                tmp += kCoefficients48To32[1][5] * In[6];
                tmp += kCoefficients48To32[1][6] * In[7];
                tmp += kCoefficients48To32[1][7] * In[8];
                Out[1] = tmp;

                // update pointers
                In += 3;
                Out += 2;
        }
}
void WebRtcSpl_Resample48khzTo16khz(const short* in, short* out,
                                    WebRtcSpl_State48khzTo16khz* state, int* tmpmem, int input_len, int output_len)
{
        ///// 48 --> 48(LP) /////
        // short  in[480]
        // int out[480]
        /////
        WebRtcSpl_LPBy2ShortToInt(in, input_len, tmpmem + 16, state->S_48_48);

        ///// 48 --> 32 /////
        // int  in[480]
        // int out[320]
        /////
        // copy state to and from input array
        memcpy(tmpmem + 8, state->S_48_32, 8 * sizeof(int));
        memcpy(state->S_48_32, tmpmem + input_len + 8, 8 * sizeof(int));
        WebRtcSpl_Resample48khzTo32khz(tmpmem + 8, tmpmem, output_len);

        ///// 32 --> 16 /////
        // int  in[320]
        // short out[160]
        /////
        WebRtcSpl_DownBy2IntToShort(tmpmem, output_len*2, out, state->S_32_16);
}
