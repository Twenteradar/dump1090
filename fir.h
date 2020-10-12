
#ifndef __FIR_H__
#define __FIR_H__

#include "dump1090.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct iq_int_sample {
    int16_t sample[2];
};

struct iq_float_sample {
    float sample[2];
};


#define ABS(__i) (__i < 0 ? -__i : __i)
#define FABS(__i) (__i < 0.0f ? -__i : __i)


typedef struct {
    const char *description;
    uint64_t sampleRateHz;
    uint64_t cutoffStartFreqHz;
    uint64_t cutoffEndFreqHz;
    uint32_t tapCount;
    int32_t taps[];
} FIRFilterInt16;

static FIRFilterInt16 __attribute__((unused)) fir_12_19_int = {
    .description = "http://t-filter.engineerjs.com/",
    .sampleRateHz = 12000000,
    .cutoffStartFreqHz = 1100000,
    .cutoffEndFreqHz = 1200000,
    .tapCount = 19,
    .taps = {
             -341,
             -830,
             -1250,
             -965,
             750,
             4296,
             9359,
             14805,
             19030,
             20625,
             19030,
             14805,
             9359,
             4296,
             750,
             -965,
             -1250,
             -830,
             -341
    }
};

static FIRFilterInt16 __attribute__((unused)) fir_12_19_ml1_int = {
    .description = "MatLab Least-Squares",
    .sampleRateHz = 12000000,
    .cutoffStartFreqHz = 1100000,
    .cutoffEndFreqHz = 1200000,
    .tapCount = 9,
    .taps = {
        -52,   -943,  -1409,  -1203,   -251,   1324,   3213,   5005,   6284,
       6747,   6284,   5005,   3213,   1324,   -251,  -1203,  -1409,   -943,
        -52
    }
};



typedef struct {
    FIRFilterInt16 *filter;
    uint32_t index;
    int16_t *history;
} FIRFilterContextInt16;


void FIRFilterInitInt16(FIRFilterContextInt16 *ctx);
int16_t  FIRFilterProcessInt16(FIRFilterContextInt16 *ctx, int64_t in);
void FIRFilterProcessInt16Buffer(FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count,
    uint32_t decimation_factor);
void FIRFilterFreeInt16(FIRFilterContextInt16 *ctx);
void DecimateInt16(int16_t *in, size_t sample_count, uint32_t decimation_factor);
void DecimateInt16MAG(int16_t *in, size_t sample_count, uint32_t decimation_factor);
void DecimateInt16MaxMAG(int16_t *in, size_t sample_count, uint32_t decimation_factor);

char *print16(int16_t val, char *buf);
void DecimateInt16AverageMAGThreshold(int16_t *in, size_t sample_count, uint32_t decimation_factor,
    int16_t threshold);
void DecimateInt16IQ(struct iq_int_sample *in, size_t sample_count, uint32_t decimation_factor);

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif//__FIR_H__



