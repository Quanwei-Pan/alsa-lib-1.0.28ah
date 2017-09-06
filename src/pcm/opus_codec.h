#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPUS_CODE_H
#define OPUS_CODE_H

#define MODE_SILK_ONLY          1000
#define MODE_HYBRID             1001
#define MODE_CELT_ONLY          1002

#define OPUS_SET_FORCE_MODE_REQUEST    11002
#define OPUS_SET_FORCE_MODE(x) OPUS_SET_FORCE_MODE_REQUEST, __opus_check_int(x)

int int mi_opus(char *input, int inputsize, char *output, int *outputsize);
#endif

#ifdef __cplusplus
}
#endif
