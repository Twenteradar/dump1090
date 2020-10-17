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


void FIRFilterInitInt16(FIRFilterContextInt16 *ctx)
{
  ctx->history = calloc(ctx->filter->tapCount, sizeof(*ctx->history));
  ctx->index = 0;
}

int16_t  FIRFilterProcessInt16(FIRFilterContextInt16 *ctx, int64_t in)
{
    uint32_t i;
    uint32_t index = ctx->index;
    int64_t sum = 0;

    ctx->history[index] = ABS(in);

    for(i = 0; i < ctx->filter->tapCount; ++i) {
      index = index != 0 ? index-1 : ctx->filter->tapCount - 1;
      sum += (int64_t)ctx->history[index] * ctx->filter->taps[i];
    };

    if (++ctx->index == ctx->filter->tapCount) {
        ctx->index = 0;
    }

    return sum >> 16;
}

void FIRFilterProcessInt16Buffer(FIRFilterContextInt16 *ctx, int16_t *in, size_t sample_count,
    uint32_t decimation_factor)
{
    uint32_t i, j, k;
    int16_t temp;

    for (i = 0, j = 0, k = (decimation_factor - 1); i < sample_count; i++) {
        temp = FIRFilterProcessInt16(ctx, in[i]);
        if (decimation_factor) {
            if (k == (decimation_factor - 1)) {
                in[j++] = temp;
            } else if (k == 0) {
                k = decimation_factor;
            }
            k--;
        } else {
            in[i] = temp;
        }

    }
}

void FIRFilterFreeInt16(FIRFilterContextInt16 *ctx)
{
    free(ctx->history);
}

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

char *print16(int16_t val, char *buf)
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
