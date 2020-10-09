// Part of dump1090, a Mode S message intrface for AirSpy.
//
// sdr_airspy.c: AirSpy support
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

#include "dump1090.h"
#include "sdr_airspy.h"

#include <libairspy/airspy.h>
#include <inttypes.h>

#define CONVERTER_SAMPLE_TYPE INPUT_SC16
#define AIRSPY_SAMPLE_TYPE AIRSPY_SAMPLE_INT16_IQ
int sample_size = sizeof(int16_t) * 2;

struct iq_sample {
    int16_t sample[2];
};

static struct {
    struct airspy_device *device;
    uint64_t serial;
    uint64_t freq;
    int lna_gain;
    int mixer_gain;
    int vga_gain;
    int linearity_gain;
    int sensitivity_gain;
    int individual_gains_set;
    int preset_gains_set;
    int lna_agc;
    int mixer_agc;
    int agcs_set;
    int rf_bias;
    int packing;
    int samplerate;

    iq_convert_fn converter;
    struct converter_state *converter_state;
    enum airspy_sample_type sample_type;
} AirSpy;

void airspyInitConfig()
{
    AirSpy.device = NULL;
    AirSpy.serial = 0;
    AirSpy.freq = 1090000000;
    AirSpy.lna_gain = -1;
    AirSpy.mixer_gain = -1;
    AirSpy.vga_gain = -1;
    AirSpy.linearity_gain = -1;
    AirSpy.sensitivity_gain = -1;
    AirSpy.individual_gains_set = 0;
    AirSpy.preset_gains_set = 0;
    AirSpy.lna_agc = 0;
    AirSpy.mixer_agc = 0;
    AirSpy.agcs_set = 0;
    AirSpy.rf_bias = 0;
    AirSpy.samplerate = 12000000;
    AirSpy.converter = NULL;
    AirSpy.converter_state = NULL;
    AirSpy.sample_type = AIRSPY_SAMPLE_TYPE;
}

bool airspyHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j+1 < argc);
    if (!strcmp(argv[j], "--lna-gain") && more) {
        AirSpy.lna_gain = atoi(argv[++j]);

        if (AirSpy.lna_gain > 15 || AirSpy.lna_gain < 0) {
            fprintf(stderr, "Error: --lna-gain range is 0 - 15\n");
            return false;
        }
        AirSpy.individual_gains_set++;
    } else if (!strcmp(argv[j], "--vga-gain") && more) {
        AirSpy.vga_gain = atoi(argv[++j]);

        if (AirSpy.vga_gain > 15 || AirSpy.vga_gain < 0) {
            fprintf(stderr, "Error: --vga-gain range is 0 - 15\n");
            return false;
        }
        AirSpy.individual_gains_set++;
    } else if (!strcmp(argv[j], "--mixer-gain") && more) {
        AirSpy.mixer_gain = atoi(argv[++j]);

        if (AirSpy.mixer_gain > 15 || AirSpy.mixer_gain < 0) {
            fprintf(stderr, "Error: --mixer-gain range is 0 - 15\n");
            return false;
        }
        AirSpy.individual_gains_set++;
    } else if (!strcmp(argv[j], "--linearity-gain") && more) {
        AirSpy.linearity_gain = atoi(argv[++j]);

        if (AirSpy.linearity_gain > 21 || AirSpy.linearity_gain < 0) {
            fprintf(stderr, "Error: --linearity-gain range is 0 - 21\n");
            return false;
        }
        AirSpy.preset_gains_set++;
    } else if (!strcmp(argv[j], "--sensitivity-gain") && more) {
        AirSpy.sensitivity_gain = atoi(argv[++j]);

        if (AirSpy.sensitivity_gain > 21 || AirSpy.sensitivity_gain < 0) {
            fprintf(stderr, "Error: --sensitivity-gain range is 0 - 21\n");
            return false;
        }
        AirSpy.preset_gains_set++;
    } else if (!strcmp(argv[j], "--sample-rate") && more) {
        AirSpy.samplerate = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--enable-lna-agc")) {
        AirSpy.lna_agc = 1;
        AirSpy.agcs_set++;
    } else if (!strcmp(argv[j], "--enable-mixer-agc")) {
        AirSpy.mixer_agc = 1;
        AirSpy.agcs_set++;
    } else if (!strcmp(argv[j], "--enable-packing")) {
        AirSpy.packing = 1;
    } else if (!strcmp(argv[j], "--enable-rf-bias")) {
        AirSpy.rf_bias = 1;
    } else {
        return false;
    }

    *jptr = j;

    return true;
}

void airspyShowHelp()
{
    printf("      AirSpy-specific options (use with --device-type airspy)\n");
    printf("\n");
    printf("--device <serial>         select device by hex serial number\n");
    printf("--lna-gain <gain>         set lna gain (Range 0-15)\n");
    printf("--mixer-gain <gain>       set mixer gain (Range 0-15)\n");
    printf("--vga-gain <gain>         set vga gain (Range 0-15)\n");
    printf("--linearity-gain <gain>   set linearity gain presets (Range 0-21) (default 21)\n");
    printf("                          emphasizes vga gains over lna and mixer gains\n");
    printf("                          mutually exclusive with all other gain settings\n");
    printf("                          same as setting --gain\n");
    printf("--sensitivity-gain <gain> set sensitivity gain presets (Range 0-21)\n");
    printf("                          emphasizes lna and mixer gains over vga gain\n");
    printf("                          mutually exclusive with all other gain settings\n");
    printf("--sample-rate             set sample rate in Hz (default 12000000 samples /sec\n");
    printf("--enable-lna-agc          enable on lna agc\n");
    printf("--enable-mixer-agc        enable mixer agc\n");
    printf("--enable-packing          enable packing on the usb interface\n");
    printf("--enable-rf-bias          enable the bias-tee for external LNA\n");
    printf("\n");
}

static void show_config()
{
    fprintf(stderr, "serial      : 0x%" PRIx64 "\n", AirSpy.serial);
    fprintf(stderr, "freq        : %" PRIu64 "\n", AirSpy.freq);
    fprintf(stderr, "sample-rate : %d\n", AirSpy.samplerate);
    fprintf(stderr, "\n");
    fprintf(stderr, "lna_gain         : %d %s\n", AirSpy.lna_gain, AirSpy.lna_gain < 0 ? "(not set)": "");
    fprintf(stderr, "mixer_gain       : %d %s\n", AirSpy.mixer_gain, AirSpy.mixer_gain < 0 ? "(not set)": "");
    fprintf(stderr, "vga_gain         : %d %s\n", AirSpy.vga_gain, AirSpy.vga_gain < 0 ? "(not set)": "");
    fprintf(stderr, "linearity_gain   : %d %s\n", AirSpy.linearity_gain, AirSpy.linearity_gain < 0 ? "(not set)": "");
    fprintf(stderr, "sensitivity_gain : %d %s\n", AirSpy.sensitivity_gain, AirSpy.sensitivity_gain < 0 ? "(not set)": "");
    fprintf(stderr, "\n");
    fprintf(stderr, "lna_agc    : %s\n", AirSpy.lna_agc ? "on" : "off");
    fprintf(stderr, "mixer_agc  : %s\n", AirSpy.mixer_agc ? "on" : "off");
    fprintf(stderr, "packing    : %s\n", AirSpy.packing ? "on" : "off");
    fprintf(stderr, "rf_bias    : %s\n", AirSpy.rf_bias ? "on" : "off");
}

#define SET_PARAM(__name) \
    status = airspy_set_ ## __name(AirSpy.device, AirSpy.__name); \
    if (status != 0) { \
        fprintf(stderr, "AirSpy: airspy_set_" #__name " failed with code %d\n", status); \
        airspy_close(AirSpy.device); \
        airspy_exit(); \
        return false; \
    }

#define SET_PARAM_GAIN(__name) \
    if (AirSpy.__name >= 0) { \
        SET_PARAM(__name); \
    }

bool airspyOpen()
{
    int status;

    if (AirSpy.device) {
        return true;
    }

    if (Modes.dev_name) {
        char *ptr = Modes.dev_name;
        if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
            ptr += 2;
        }
        if (sscanf(ptr, "%" PRIx64, &AirSpy.serial) != 1) {
            fprintf(stderr, "AirSpy: invalid device '%s'\n", Modes.dev_name);
            return false;
        }
    }

    if (Modes.gain != MODES_MAX_GAIN) {
        if (AirSpy.individual_gains_set || AirSpy.preset_gains_set || AirSpy.agcs_set) {
            fprintf(stderr, "AirSpy: --gain can't be combined with AirSpy specific gain or agc settings\n");
            return false;
        }
        AirSpy.linearity_gain = Modes.gain / 10;
        if (AirSpy.linearity_gain < 0 || AirSpy.linearity_gain > 21) {
            fprintf(stderr, "Error: --linearity-gain (or --gain) range is 0 - 21\n");
            return false;
        }
        AirSpy.preset_gains_set++;
        fprintf(stderr, "AirSpy: --linearity-gain set to %d from --gain\n", AirSpy.linearity_gain);
    }


    if (AirSpy.individual_gains_set && AirSpy.preset_gains_set) {
        fprintf(stderr, "AirSpy: Individual gains can't be combined with preset gains\n");
        return false;
    }

    if ((AirSpy.lna_gain >= 0 || AirSpy.preset_gains_set) && AirSpy.lna_agc) {
        fprintf(stderr, "AirSpy: Options that alter lna-gain can't be combined with lna-agc\n");
        return false;
    }

    if ((AirSpy.mixer_gain >= 0 || AirSpy.preset_gains_set) && AirSpy.mixer_agc) {
        fprintf(stderr, "AirSpy: Options that alter mixer-gain can't be combined with mixer-agc\n");
        return false;
    }

    if (AirSpy.preset_gains_set > 1) {
        fprintf(stderr, "AirSpy: linearity-gain and sensitivity-gain are mutually exclusive\n");
        return false;
    }

    status = airspy_init();
    if (status != 0) {
        fprintf(stderr, "AirSpy: airspy_init failed with code %d\n", status);
        return false;
    }

    if (AirSpy.serial) {
        status = airspy_open_sn(&AirSpy.device, AirSpy.serial);
    } else {
        status = airspy_open(&AirSpy.device);
    }

    if (status != AIRSPY_SUCCESS) {
        fprintf(stderr, "AirSpy: airspy_open failed with code %d\n", status);
        airspy_exit();
        return false;
    }

    SET_PARAM(freq);

    SET_PARAM_GAIN(lna_gain);
    SET_PARAM_GAIN(mixer_gain);
    SET_PARAM_GAIN(vga_gain);
    SET_PARAM_GAIN(linearity_gain);
    SET_PARAM_GAIN(sensitivity_gain);

    SET_PARAM(lna_agc);
    SET_PARAM(mixer_agc);

    SET_PARAM(rf_bias);
    SET_PARAM(packing);

    SET_PARAM(sample_type);
    SET_PARAM(samplerate);

    show_config();

    AirSpy.converter = init_converter(CONVERTER_SAMPLE_TYPE,
                                      Modes.sample_rate,
                                      Modes.dc_filter,
                                      &AirSpy.converter_state);
    if (!AirSpy.converter) {
        fprintf(stderr, "AirSpy: can't initialize sample converter\n");
        return false;
    }

    return true;
}

static int handle_airspy_samples(airspy_transfer *transfer)
{
    static uint64_t dropped = 0;
    static uint64_t sampleCounter = 0;

    sdrMonitor();

    if (Modes.exit || transfer->sample_count <= 0)
        return -1;

    struct iq_sample *buf = transfer->samples;
    int sample_ratio = AirSpy.samplerate / Modes.sample_rate;
    uint64_t sample_count = transfer->sample_count / sample_ratio;
    dropped += transfer->dropped_samples;

//    printf("Sample Count: %d  Sample ratio: %d  Dropped: %lu\n", (int)sample_count, sample_ratio, transfer->dropped_samples);

    for (uint32_t i=0, j=0; i < sample_count; i++, j+=sample_ratio) {
        buf[i] = buf[j];
    }

    struct mag_buf *outbuf = fifo_acquire(0 /* don't wait */);
    if (!outbuf) {
        // FIFO is full. Drop this block.
        dropped += sample_count;
        sampleCounter += sample_count;
        return 0;
    }

    outbuf->flags = 0;

    if (dropped) {
        // We previously dropped some samples due to no buffers being available
        outbuf->flags |= MAGBUF_DISCONTINUOUS;
        outbuf->dropped = dropped;
    }

    dropped = 0;

    // Compute the sample timestamp and system timestamp for the start of the block
    outbuf->sampleTimestamp = sampleCounter * 12e6 / Modes.sample_rate;
    sampleCounter += sample_count;

    // Get the approx system time for the start of this block
    uint64_t block_duration = 1e3 * sample_count / Modes.sample_rate;
    outbuf->sysTimestamp = mstime() - block_duration;

    // Convert the new data
    unsigned to_convert = sample_count;
    if (to_convert + outbuf->overlap > outbuf->totalLength) {
        // how did that happen?
        printf("WTF\n");
        to_convert = outbuf->totalLength - outbuf->overlap;
        dropped = sample_count - to_convert;
    }

    AirSpy.converter(buf, &outbuf->data[outbuf->overlap], to_convert,
        AirSpy.converter_state, &outbuf->mean_level, &outbuf->mean_power);
    outbuf->validLength = outbuf->overlap + to_convert;

    // Push to the demodulation thread
    fifo_enqueue(outbuf);

    return 0;
}


void airspyRun()
{
    if (!AirSpy.device) {
        fprintf(stderr, "airspyRun: AirSpy.device = NULL\n");
        return;
    }

    int status = airspy_start_rx(AirSpy.device, &handle_airspy_samples, NULL);

    if (status != 0) { 
        fprintf(stderr, "airspy_start_rx failed\n");
        airspy_close(AirSpy.device);
        airspy_exit();
        exit (1); 
    }

    // airspy_start_rx does not block so we need to wait until the streaming is finished
    // before returning from the hackRFRun function
    while (airspy_is_streaming(AirSpy.device) == AIRSPY_TRUE && !Modes.exit) {
        struct timespec slp = { 0, 100 * 1000 * 1000};
        nanosleep(&slp, NULL);
    }

    airspyClose();
    fprintf(stderr, "AirSpy stopped streaming\n");
}

void airspyClose()
{
    if (AirSpy.device) {
        airspy_stop_rx(AirSpy.device);
        airspy_close(AirSpy.device);
        airspy_exit();
        AirSpy.device = NULL;
    }
}
