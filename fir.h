// Part of dump1090
//
// fir.h: Header for FIR Implementation
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

struct FIRFilterInt16;


struct FIRFilterContextInt16;

enum FIRFilterTapType {
    TAP_TYPE_INT,
    TAP_TYPE_FLOAT,
};

#define FILTER_MAX_NAME_LEN 33

/*! int16 Sample Filter Definition */
struct FIRFilterInt16 {
    const char name[FILTER_MAX_NAME_LEN];
    /*! The following fields are informational only */
    const char *description;
    uint64_t sampleRateHz;
    uint64_t startFreqHz;
    uint64_t cutoffStartFreqHz;
    uint64_t cutoffEndFreqHz;
    uint64_t endFreqHz;
    /*! How many taps for this filter */
    uint32_t tapCount;
    /*! If the processOne function is used is "fast", this is the mask for the index */
    uint32_t indexMask;
    /*! The processOne will shift its result by this many bits before returning */
    uint32_t shiftCount;
    /*! If the processOne function is used is "shifter", these are the sample indexes to use for each shift */
    uint16_t shiftIndexes[32];
    /*! A function that processes a buffer without decimation */
    void (*processBuffer)(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count);
    /*! A function that processes a buffer while decimatiing */
    void (*processDecimateBuffer)(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count);
    /*! A function that proceses one sample */
    int16_t (*processOne)(struct FIRFilterContextInt16 *ctx, int16_t in);
    /*! TAP_TYPE_INT or TAP_TYPE_FLOAT */
    enum FIRFilterTapType tapType;
    /*! The taps */
    union {
        int16_t intTaps[32];
        float floatTaps[32];
    };
};

/*!
 * @brief Create a context for filtering int16 samples
 *
 * @param filter A FIRFilterInt16 structure
 * @param decimation_factor If decimating/downsampling, the factor.
 * @return struct FIRFilterContextInt16 *
 */
struct FIRFilterContextInt16 *FIRFilterCreateInt16Ctx(const char *filterName, uint32_t decimation_factor);

/*!
 * @brief Resets the context history and index
 * @param ctx struct FIRFilterContextInt16 *
 *
 * You shouldn't call this unless you relly need to start over.  Calling it
 * before processing a buffer will cause that buffer to have to start from scratch again.
 */
void FIRFilterResetInt16(struct FIRFilterContextInt16 *ctx);

/*!
 * @brief Frees context and its history
 * @param ctx struct FIRFilterContextInt16 *
 */
void FIRFilterFreeInt16(struct FIRFilterContextInt16 *ctx);

/*
 * Functions for processing a single byte.
 * These should not be called directly but instead usaed to set a filter's processOne function.
 */
/*!
 * @brief Processes one int16 sample using int16 taps and the arbitrary tap count algorithmm
 * @param ctx
 * @param in
 * @return The new int16 sample
 */
int16_t FIRFilterProcessInt16Int16TapsSlow(struct FIRFilterContextInt16 *ctx, int16_t in);

/*!
 * @brief Processes one int16 sample using int16 taps and "fast" algorithmm
 * @param ctx
 * @param in
 * @return The new int16 sample
 *
 * The "fast" algorithm requires that the filter's tap count be a power of 2 and that
 * shiftCount == tabCount && indexMask == (shiftCount - 1)
 *
 * Example
 *    .tapCount = 16,
 *    .shiftCount = 16,
 *   .indexMask = 0xf,
 *
 */
int16_t FIRFilterProcessInt16Int16TapsFast(struct FIRFilterContextInt16 *ctx, int16_t in);

/*!
 * @brief Processes one int16 sample using floattaps and the arbitrary tap count algorithmm
 * @param ctx
 * @param in
 * @return The new int16 sample
 */
int16_t FIRFilterProcessInt16FloatTapsSlow(struct FIRFilterContextInt16 *ctx, int16_t in);

/*!
 * @brief Processes one int16 sample using float taps and "fast" algorithmm
 * @param ctx
 * @param in
 * @return The new int16 sample
 *
 * The "fast" algorithm requires that the filter's tap count be a power of 2 and that
 * shiftCount == tabCount && indexMask == (shiftCount - 1)
 *
 * Example
 *    .tapCount = 16,
 *    .shiftCount = 16,
 *   .indexMask = 0xf,
 *
 */
int16_t FIRFilterProcessInt16FloatTapsFast(struct FIRFilterContextInt16 *ctx, int16_t in);

/*!
 * @brief Processes one int16 sample using int16 taps and the "shift" algorithmm
 * @param ctx
 * @param in
 * @return The new int16 sample
 *
 * The "shift" algorithm accumulates the samples after shifting them a number of bits to the left.
 * .tapCount should be set to the number of shift operations to perform.
 * .intTaps contains the bits to shift by at each sample.
 * .shiftIndexes contains the history index to shift.
 * Example:
 *   .tapCount = 6,
 *   .intTaps = {
 *       1, 3, 3, 1, 3, 1
 *   },
 *   .shiftIndexes = {
 *       0, 1, 2, 2, 3, 4
 *   },
 *   .shiftCount = 5,
 *
 * This implents a 5 tap filter using 2 8 10 8 2 as the coefficients but instead of
 * using multiplication, it uses bit shifts.
 *
 * You'll notice that the 3rd coefficient is not a power of 2 so the 3rd sample
 * has to be shifted twice, by 3 to get an 8, then by 1 to get a 2.  That's why
 * there are actually 6 taps and 6 shifts...  The 3rd sampe is shiftes twice.
 *
 */
int16_t FIRFilterProcessInt16Int16TapsShifter(struct FIRFilterContextInt16 *ctx, int16_t in);

/*
 * Function for processing a buffer.
 * These should not be called directly but instead usaed to set a filter's buffer processing functions.
 */
/*!
 * @brief Process a buffer of int16 samples without decimation.
 *
 * @param ctx struct FIRFilterContextInt16 *ctx
 * @param in int16_t * buffer
 * @param sample_count number of samples (not bytes) in the buffer
 */
void FIRFilterNoDecProcessInt16Buffer(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count);

/*!
 * @brief Process a buffer of int16 samples with decimation.
 *
 * @param ctx struct FIRFilterContextInt16 *ctx
 * @param in int16_t * buffer
 * @param sample_count number of samples (not bytes) in the buffer
 *
 * The decimation factor should have been supplied when FIRFilterCreateInt16Ctx was called
 */
void FIRFilterDecimatorProcessInt16Buffer(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count);

void FIRFilterAverageDecimatorInt16Buffer(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count);

/*
 * The public buffer processing function
 */
/*!
 * @brief Process a buffer of int16 samples according to the context configfuration
 *
 * @param ctx struct FIRFilterContextInt16 *ctx
 * @param in int16_t * buffer
 * @param sample_count number of samples (not bytes) in the buffer
 *
 * This is the only buffer processing function that should be called directly.
 */
void FIRFilterProcessInt16Buffer(struct FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count);

/*
 * Standalone decimators.   Not all are useful.
 */
/*!
 * @brief Simple keep the first out of every decimation_factor int16 samples
 *
 * @param in
 * @param sample_count
 * @param decimation_factor
 */
void DecimateInt16(int16_t *in, size_t sample_count, uint32_t decimation_factor);

/*!
 * @brief Simple keep absolute value of the first out of every decimation_factor int16 samples
 *
 * @param in
 * @param sample_count
 * @param decimation_factor
 */
void DecimateInt16MAG(int16_t *in, size_t sample_count, uint32_t decimation_factor);

/*!
 * @brief Keep the LARGEST of the absolute value of decimation_factor int16 samples
 *
 * @param in
 * @param sample_count
 * @param decimation_factor
 */
void DecimateInt16MaxMAG(int16_t *in, size_t sample_count, uint32_t decimation_factor);

/*!
 * @brief Simple keep the first of every INT16_IQ decimation_factor samples
 *
 * @param in
 * @param sample_count
 * @param decimation_factor
 */
void DecimateInt16IQ(struct iq_int_sample *in, size_t sample_count, uint32_t decimation_factor);

/*!
 * @brief Keep the AVERAGE of the absolute value of decimation_factor int16 samples that exceed the threshold
 *
 * @param in
 * @param sample_count
 * @param decimation_factor
 */
void DecimateInt16AverageMAGThreshold(int16_t *in, size_t sample_count, uint32_t decimation_factor,
    int16_t threshold);

/*
 * Filter definitions
 */

static struct FIRFilterInt16 __attribute__((unused)) fir_12_19_tf1_int = {
    .name = "t-filter-19",
    .description = "http://t-filter.engineerjs.com/",
    .sampleRateHz = 12000000,
    .cutoffStartFreqHz = 1100000,
    .cutoffEndFreqHz = 1200000,
    .tapCount = 19,
    .shiftCount = 16,
    .processBuffer = FIRFilterProcessInt16Buffer,
    .processDecimateBuffer = FIRFilterDecimatorProcessInt16Buffer,
    .processOne = FIRFilterProcessInt16Int16TapsSlow,
    .tapType = TAP_TYPE_INT,
    .intTaps = {
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
    },
};

static struct FIRFilterInt16 __attribute__((unused)) fir_const_5_int = {
    .name = "5tap-constant",
    .description = "custom",
    .sampleRateHz = 0,
    .cutoffStartFreqHz = 0,
    .cutoffEndFreqHz = 0,
    .tapCount = 5,
    .shiftCount = 0,
    .indexMask = 0x7,
    .processBuffer = FIRFilterProcessInt16Buffer,
    .processDecimateBuffer = FIRFilterDecimatorProcessInt16Buffer,
    .processOne = FIRFilterProcessInt16FloatTapsSlow,
    .tapType = TAP_TYPE_FLOAT,
    .floatTaps = {
        0.2,
        0.2,
        0.2,
        0.2,
        0.2,
    }
};

static struct FIRFilterInt16 __attribute__((unused)) fir_shifter_5_int = {
    .name = "5tap-shifter",
    .description = "custom",
    .sampleRateHz = 0,
    .cutoffStartFreqHz = 0,
    .cutoffEndFreqHz = 0,
    .tapCount = 6,
    .shiftCount = 5,
    .processBuffer = FIRFilterProcessInt16Buffer,
    .processDecimateBuffer = FIRFilterDecimatorProcessInt16Buffer,
    .processOne = FIRFilterProcessInt16Int16TapsShifter,
    .tapType = TAP_TYPE_INT,
    .intTaps = {
        1, 3, 3, 1, 3, 1
    },
    .shiftIndexes = {
        0, 1, 2, 2, 3, 4
    }
};

static struct FIRFilterInt16 __attribute__((unused)) fir_12_16_tf3_int = {
    .name = "t-filter-16",
    .description = "http://t-filter.engineerjs.com/",
    .sampleRateHz = 12000000,
    .cutoffStartFreqHz = 1200000,
    .cutoffEndFreqHz = 1500000,
    .tapCount = 16,
    .shiftCount = 16,
    .processBuffer = FIRFilterProcessInt16Buffer,
    .processDecimateBuffer = FIRFilterDecimatorProcessInt16Buffer,
    .processOne = FIRFilterProcessInt16Int16TapsFast,
    .tapType = TAP_TYPE_INT,
    .indexMask = 0xf,
    .intTaps = {
        -4057,
        -5724,
        2956,
        -1250,
        -238,
        2154,
        -5688,
        20432,
        20432,
        -5688,
        2154,
        -238,
        -1250,
        2956,
        -5724,
        -4057
    }
};

static struct FIRFilterInt16 __attribute__((unused)) fir_average_5_int = {
    .name = "5tap-average",
    .description = "custom",
    .sampleRateHz = 0,
    .cutoffStartFreqHz = 0,
    .cutoffEndFreqHz = 0,
    .processDecimateBuffer = FIRFilterAverageDecimatorInt16Buffer,
};


#if 0
/* These are crap but kept to keep track of what I've already tried. */

static struct FIRFilterInt16 __attribute__((unused)) fir_12_8_int = {
    .description = "http://t-filter.engineerjs.com/",
    .sampleRateHz = 12000000,
    .cutoffStartFreqHz = 1200000,
    .cutoffEndFreqHz = 1500000,
    .tapCount = 8,
    .shiftCount = 8,
    .indexMask = 0x7,
    .taps = {
        -93,
        398,
        -629,
        332,
        332,
        -629,
        398,
        -93
    }
};

static struct FIRFilterInt16 __attribute__((unused)) fir_12_19_ml1_int = {
    .description = "MatLab Least-Squares",
    .sampleRateHz = 12000000,
    .cutoffStartFreqHz = 1200000,
    .cutoffEndFreqHz = 1200000,
    .tapCount = 19,
    .taps = {
        -52,   -943,  -1409,  -1203,   -251,   1324,   3213,   5005,   6284,
       6747,   6284,   5005,   3213,   1324,   -251,  -1203,  -1409,   -943,
        -52
    }
};

static struct FIRFilterInt16 __attribute__((unused)) fir_12_16_ml2_int = {
    .description = "MatLab Equiripple",
    .sampleRateHz = 12000000,
    .cutoffStartFreqHz = 1200000,
    .cutoffEndFreqHz = 1500000,
    .tapCount = 16,
    .taps = {
-966,  -4538,   -679,   -226,   1882,   4092,   6073,   7219,   7219,
6073,   4092,   1882,   -226,   -679,  -4538,   -966
    }
};

#endif


static struct FIRFilterInt16 __attribute__((unused)) *FIRFilters[] = {
    &fir_average_5_int,
    &fir_const_5_int,
    &fir_12_19_tf1_int,
    &fir_shifter_5_int,
    &fir_12_16_tf3_int,
};

struct FIRFilterInt16 *FIRFindFilter(const char *name);
size_t FIRFiltersGetCount();

/*!
 * @brief Get a comma separate list of available filters
 * @return
 *
 * You must call free() on the returned string.
 */
char *FIRFiltersGetNames();



#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif//__FIR_H__



