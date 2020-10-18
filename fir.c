// Part of dump1090
//
// fir.c: FIR Implementation
//
// Copyright (c) 2020 George Joseph (g.devel@wxy78.net)
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "fir.h"

/*!
 * Context for int16 samples
 *
 */
struct FIRFilterContextInt16 {
    /*! The filter template to use */
    struct FIRFilterInt16 *filter;
    /*! Working variable to keep track of current history slot */
    uint32_t index;
    /*! If decimation factor is 0, no decimation is done */
    uint32_t decimation_factor;
    /*! Pointer to a function that will process a buffer */
    void (*processBuffer)(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count);
    /*! Pointer to a function that will process one sample */
    void (*processOne)(struct FIRFilterContextInt16 *ctx, int16_t *in);
    /*! The sample history */
    int16_t history[];
};



struct FIRFilterContextInt16 *FIRFilterCreateInt16Ctx(struct FIRFilterInt16 *filter, uint32_t decimation_factor)
{
    struct FIRFilterContextInt16 *ctx = calloc(1, sizeof(*ctx) + (sizeof(int16_t) * filter->tapCount));

    ctx->filter = filter;
    ctx->index = 0;
    ctx->decimation_factor = decimation_factor;
    /*
     * The functions are determined here to remove tests during actual processing
     */
    if (decimation_factor) {
        ctx->processBuffer = filter->processDecimateBuffer;
    } else {
        ctx->processBuffer = filter->processBuffer;
    }

    return ctx;
}

void FIRFilterResetInt16(struct FIRFilterContextInt16 *ctx)
{
    uint32_t i;

    ctx->index = 0;
    for (i = 0; i < ctx->filter->tapCount; i++) {
        ctx->history[i] = 0;
    }
}

void FIRFilterFreeInt16(struct FIRFilterContextInt16 *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

/*
 * These 2 macros can be used for the 4 fast/slow functions.
 */
#define __FIRFilterProcessInt16Slow(__ctx, __in, __tapTable) \
({\
    uint32_t __i; \
    uint32_t __index = __ctx->index; \
    int64_t __sum = 0; \
    __ctx->history[__index] = ABS(__in); \
    for(__i = 0; __i < __ctx->filter->tapCount; ++__i) { \
      __index = __index != 0 ? __index-1 : __ctx->filter->tapCount - 1; \
      __sum += (int64_t)(__ctx->history[__index] * __tapTable[__i]); \
    }; \
    if (++__ctx->index == __ctx->filter->tapCount) { \
        __ctx->index = 0; \
    } \
    (__sum >> __ctx->filter->shiftCount); \
})

#define __FIRFilterProcessInt16Fast(__ctx, __in, __tapTable) \
({ \
    uint32_t __i; \
    uint32_t __index = __ctx->index++; \
    int64_t __sum = 0; \
    __ctx->history[(__index) & __ctx->filter->indexMask] = ABS(__in); \
    for(__i = 0; __i < __ctx->filter->tapCount; ++__i) { \
      __sum += (int64_t)__ctx->history[(__index--) & __ctx->filter->indexMask] * __tapTable[__i]; \
    }; \
    (__sum >> __ctx->filter->shiftCount); \
})

inline int16_t FIRFilterProcessInt16Int16TapsSlow(struct FIRFilterContextInt16 *ctx, int16_t in)
{
    return __FIRFilterProcessInt16Slow(ctx, in, ctx->filter->intTaps);
}

inline int16_t FIRFilterProcessInt16Int16TapsFast(struct FIRFilterContextInt16 *ctx, int16_t in)
{
    return __FIRFilterProcessInt16Fast(ctx, in, ctx->filter->intTaps);
}

inline int16_t FIRFilterProcessInt16FloatTapsSlow(struct FIRFilterContextInt16 *ctx, int16_t in)
{
    return __FIRFilterProcessInt16Slow(ctx, in, ctx->filter->floatTaps);
}

inline int16_t FIRFilterProcessInt16FloatTapsFast(struct FIRFilterContextInt16 *ctx, int16_t in)
{
    return __FIRFilterProcessInt16Fast(ctx, in, ctx->filter->floatTaps);
}

/*
 * This is the shift-only implementation inspired by @prog
 */
inline int16_t FIRFilterProcessInt16Int16TapsShifter(struct FIRFilterContextInt16 *__ctx, int16_t __in)
{
    uint32_t __i;
    uint32_t __index = __ctx->index;
    int64_t __sum = 0;
    __ctx->history[__index] = ABS(__in);
    for(__i = 0; __i < __ctx->filter->tapCount; ++__i) {
      __index = __index != 0 ? __index-1 : __ctx->filter->tapCount - 1;
      __sum += (int64_t)(__ctx->history[__ctx->filter->shiftIndexes[__index]] << __ctx->filter->intTaps[__i]);
    };
    if (++__ctx->index == __ctx->filter->tapCount) {
        __ctx->index = 0;
    }
    return (__sum >> __ctx->filter->shiftCount);

}

/*
 * To decimate or not to decimate, that is the question.
 */
void FIRFilterNoDecProcessInt16Buffer(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count)
{
    uint32_t i;

    for (i = 0; i < sample_count; i++) {
        in[i] = ctx->filter->processOne(ctx, in[i]);
    }
}

void FIRFilterDecimatorProcessInt16Buffer(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count)
{
    uint32_t i, j, k;
    int16_t temp;

    for (i = 0, j = 0, k = (ctx->decimation_factor - 1); i < sample_count; i++) {
        temp = ctx->filter->processOne(ctx, in[i]);
        /*
         * We do the decimation in-line so we don't have to iterate over the entire buffer
         * again to pick out teh samples we want to keep.
         */
        if (k == (ctx->decimation_factor - 1)) {
            in[j++] = temp;
        } else if (k == 0) {
            k = ctx->decimation_factor;
        }
        k--;
    }
}

/*
 * The public function for processing a buffer
 */
void FIRFilterProcessInt16Buffer(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count)
{
    ctx->processBuffer(ctx, in, sample_count);
}

/*
 * Standalone decimators.
 */
void DecimateInt16(int16_t *in, size_t sample_count, uint32_t decimation_factor)
{
    size_t i, j;
    for (i=0, j=0; i < sample_count; i++, j+=decimation_factor) {
        in[i] = in[j];
    }
}

void DecimateInt16MAG(int16_t *in, size_t sample_count, uint32_t decimation_factor)
{
    size_t i, j;
    for (i=0, j=0; i < sample_count; i++, j+=decimation_factor) {
        in[i] = ABS(in[j]);
    }
}

void DecimateInt16MaxMAG(int16_t *in, size_t sample_count, uint32_t decimation_factor)
{
    uint32_t i, j, k;
    int16_t itemp, jtemp;
    for (i=0, j=0; i < sample_count && j < sample_count; i++) {
        itemp = ABS(in[i]);
        for (k = 0; k < decimation_factor; k++) {
            jtemp = ABS(in[j++]);
            in[i] = jtemp > itemp ? jtemp : itemp;
        }
    }
}

static char __attribute__((unused)) *print16(int16_t val, char *buf)
{
    for (int i = 0; i<16; i++) {
        unsigned int m = 1 << i;
        buf[15-i] = val & m ? '1' : '0';
    }
    buf[16] = '\0';
    return buf;
}


void DecimateInt16AverageMAGThreshold(int16_t *in, size_t sample_count, uint32_t decimation_factor,
    int16_t threshold)
{
    uint32_t i, j, k;
    int16_t temp;

    for (i = 0, j = 0; i < sample_count && j < sample_count; i++) {
        int sc = 0;
        for (k = 0; k < decimation_factor; k++) {
            temp = ABS(in[j]);
            if (temp > threshold) {
                in[i] += temp;
                sc++;
            }
            j++;
        } \
        if (sc <= 0) {
            in[i] = 0;
        } else {
            in[i] /= sc;
        }
    }
}

void DecimateInt16IQ(struct iq_int_sample *in, size_t sample_count, uint32_t decimation_factor)
{
    uint32_t i, j;

    for (i=0, j=0; i < sample_count; i++, j+=decimation_factor) {
        in[i] = in[j];
    }
}
