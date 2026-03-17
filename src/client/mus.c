#include "mus.h"

#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "clsnd.h"
#include "wad.h"
#include "../../Nuked-OPL3/opl3.h"

#define OPL_VOICES   9
#define MUS_NCHANS   16
#define MUS_SRATE    44100
#define MUS_TICRATE  140
#define SPT          (MUS_SRATE / MUS_TICRATE)
#define OPL_GENRATE  (MUS_SRATE / 2)   /* synthesise at 22050 Hz, hold for 2 output samples */
#define OPL_STEP     (MUS_SRATE / OPL_GENRATE)

/* OPL2 operator slot offsets for channels 0-8 */
static const int modoff[9] = {  0,  1,  2,  8,  9, 10, 16, 17, 18 };
static const int caroff[9] = {  3,  4,  5, 11, 12, 13, 19, 20, 21 };

typedef struct __attribute__((packed))
{
    uint8_t tremolo;
    uint8_t attack;
    uint8_t sustain_r;
    uint8_t waveform;
    uint8_t scale;
    uint8_t level;
} gm_op_t;

typedef struct __attribute__((packed))
{
    gm_op_t  mod;
    uint8_t  feedback;
    gm_op_t  car;
    uint8_t  unused;
    int16_t  base_note;
} gm_voice_t;

typedef struct __attribute__((packed))
{
    uint16_t    flags;
    uint8_t     finetune;
    uint8_t     note;
    gm_voice_t  voice[2];
} gm_inst_t;

typedef struct
{
    int instrument;
    int volume;
} mus_chan_t;

typedef struct
{
    int     active;
    int     mschan;
    int     note;
    uint8_t b0;
} oplchan_t;

/* ================================================================
   Module state
   ================================================================ */

static gm_inst_t    genmidi[175];
static int          genmidi_ok = 0;

static opl3_chip    chip;
static oplchan_t    oplchans[OPL_VOICES];
static mus_chan_t    mchans[MUS_NCHANS];

static uint8_t     *mus_buf        = NULL;
static int          mus_bufsize    = 0;
static int          mus_pos        = 0;
static int          mus_scorestart = 0;
static int          mus_active     = 0;
static int          mus_delay      = 0;
static int          mus_samp       = 0;

static void wreg(uint16_t reg, uint8_t val)
{
    OPL3_WriteReg(&chip, reg, val);
}

static void freq_to_block(float freq, int *fnum_out, int *block_out)
{
    float f;
    int   b = 0;
    /* Fnum = freq * 2^21 / Fmaster, Fmaster ≈ 49716 Hz */
    f = freq * (float)(1 << 21) / 49716.0f;
    while(f > 1023.5f && b < 7) { f /= 2.0f; b++; }
    *block_out = b;
    *fnum_out  = (int)(f + 0.5f);
    if(*fnum_out > 1023) *fnum_out = 1023;
}

static int alloc_oplchan(int mschan, int note)
{
    int i, best, bestpri;

    for(i = 0; i < OPL_VOICES; i++)
        if(!oplchans[i].active) return i;

    for(i = 0; i < OPL_VOICES; i++)
        if(oplchans[i].mschan == mschan && oplchans[i].note == note) return i;

    /* steal lowest-priority: prefer different channel, then just take first */
    best    = 0;
    bestpri = oplchans[0].mschan == mschan ? 1 : 0;
    for(i = 1; i < OPL_VOICES; i++)
    {
        int pri = oplchans[i].mschan == mschan ? 1 : 0;
        if(pri < bestpri) { best = i; bestpri = pri; }
    }
    return best;
}

static void opl_noteon(int ci, int mschan, int note_opl, int vol,
                       const gm_voice_t *gv)
{
    int     fnum, block, mo, co, raw_tl, tl;
    uint8_t b0;

    mo = modoff[ci];
    co = caroff[ci];

    /* key off first */
    wreg(0xB0 + ci, oplchans[ci].b0 & ~0x20);

    freq_to_block(440.0f * powf(2.0f, (note_opl - 69) / 12.0f), &fnum, &block);

    /* carrier TL scaled by velocity: at vol=127 use raw TL, at vol=0 use 63 */
    raw_tl = gv->car.level & 0x3F;
    tl     = raw_tl + ((63 - raw_tl) * (127 - vol) / 127);
    if(tl > 63) tl = 63;

    wreg(0x20 + mo, gv->mod.tremolo);
    wreg(0x40 + mo, gv->mod.scale);
    wreg(0x60 + mo, gv->mod.attack);
    wreg(0x80 + mo, gv->mod.sustain_r);
    wreg(0xE0 + mo, gv->mod.waveform);

    wreg(0x20 + co, gv->car.tremolo);
    wreg(0x40 + co, (gv->car.scale & 0xC0) | (uint8_t)tl);
    wreg(0x60 + co, gv->car.attack);
    wreg(0x80 + co, gv->car.sustain_r);
    wreg(0xE0 + co, gv->car.waveform);

    wreg(0xC0 + ci, gv->feedback);

    wreg(0xA0 + ci, fnum & 0xFF);
    b0 = 0x20 | ((block & 7) << 2) | ((fnum >> 8) & 3);
    wreg(0xB0 + ci, b0);

    oplchans[ci].active = 1;
    oplchans[ci].mschan = mschan;
    oplchans[ci].b0     = b0;
}

static void mus_noteon(int chan, int note, int vol)
{
    int              gi, ci, actual_note;
    const gm_inst_t *inst;
    const gm_voice_t *gv;

    if(!genmidi_ok) return;

    if(chan == 15)
    {
        gi = 128 + (note % 47);
    }
    else
    {
        gi = mchans[chan].instrument;
        if(gi < 0 || gi > 127) gi = 0;
    }
    inst = &genmidi[gi];

    if(vol < 0)   vol = 0;
    if(vol > 127) vol = 127;

    gv          = &inst->voice[0];
    actual_note = note + (int)gv->base_note;
    if(actual_note < 0)   actual_note = 0;
    if(actual_note > 127) actual_note = 127;
    if(inst->flags & 1)   actual_note = inst->note;

    ci = alloc_oplchan(chan, note);
    opl_noteon(ci, chan, actual_note, vol, gv);
    oplchans[ci].note = note;

    if(inst->flags & 2)
    {
        gv          = &inst->voice[1];
        actual_note = note + (int)gv->base_note;
        if(actual_note < 0)   actual_note = 0;
        if(actual_note > 127) actual_note = 127;
        if(inst->flags & 1)   actual_note = inst->note;

        ci = alloc_oplchan(chan, note + 128);
        opl_noteon(ci, chan, actual_note, vol, gv);
        oplchans[ci].note = note + 128;
    }
}

static void mus_noteoff(int chan, int note)
{
    int i;
    for(i = 0; i < OPL_VOICES; i++)
    {
        if(oplchans[i].active && oplchans[i].mschan == chan &&
           (oplchans[i].note == note || oplchans[i].note == note + 128))
        {
            wreg(0xB0 + i, oplchans[i].b0 & ~0x20);
            oplchans[i].active = 0;
        }
    }
}

static void mus_process_events(void)
{
    uint8_t ev, etype, echan, last, b;
    int     note, vol, ctrl, val, delay;
    int     i, safety;

    safety = 10000;
    while(mus_active && mus_pos < mus_bufsize && safety-- > 0)
    {
        ev    = mus_buf[mus_pos++];
        last  = (ev >> 7) & 1;
        etype = (ev >> 4) & 7;
        echan = ev & 0xF;

        if(mus_pos > mus_bufsize - 2 && etype < 5) break;

        switch(etype)
        {
        case 0:
            note = mus_buf[mus_pos++] & 0x7F;
            mus_noteoff(echan, note);
            break;
        case 1:
            b    = mus_buf[mus_pos++];
            note = b & 0x7F;
            if(b & 0x80) mchans[echan].volume = mus_buf[mus_pos++];
            mus_noteon(echan, note, mchans[echan].volume);
            break;
        case 2:
            mus_pos++;
            break;
        case 3:
            b = mus_buf[mus_pos++];
            if(b == 11 || b == 14)
            {
                for(i = 0; i < OPL_VOICES; i++)
                {
                    if(oplchans[i].active && oplchans[i].mschan == echan)
                    {
                        wreg(0xB0 + i, oplchans[i].b0 & ~0x20);
                        oplchans[i].active = 0;
                    }
                }
            }
            break;
        case 4:
            ctrl = mus_buf[mus_pos++];
            val  = mus_buf[mus_pos++];
            if(ctrl == 0)      mchans[echan].instrument = val;
            else if(ctrl == 3) mchans[echan].volume      = val;
            break;
        default:
            mus_pos = mus_scorestart;
            break;
        }

        if(last)
        {
            delay = 0;
            do {
                if(mus_pos >= mus_bufsize) break;
                b     = mus_buf[mus_pos++];
                delay = delay * 128 + (b & 0x7F);
            } while(b & 0x80);
            mus_delay = delay;
            return;
        }
    }
}

void mus_init(void)
{
    lumpinfo_t *lump;
    int         i;

    lump = wad_findlump("GENMIDI", true);
    if(!lump || lump->size < 8 + 175 * (int)sizeof(gm_inst_t))
    {
        printf("[mus] GENMIDI not found\n");
        return;
    }
    if(memcmp(lump->cache, "#OPL_II#", 8) != 0)
    {
        printf("[mus] GENMIDI bad magic\n");
        wad_decache(lump);
        return;
    }
    memcpy(genmidi, (uint8_t*)lump->cache + 8, 175 * sizeof(gm_inst_t));
    wad_decache(lump);
    genmidi_ok = 1;
    printf("[mus] GENMIDI loaded\n");

    OPL3_Reset(&chip, OPL_GENRATE);
    wreg(0x01, 0x20); /* enable OPL2 waveform select */

    for(i = 0; i < OPL_VOICES; i++)
        oplchans[i].active = 0;
}

void mus_start(int episode, int map)
{
    char        name[9];
    lumpinfo_t *lump;
    int         i;
    uint16_t    scorestart;

    if(!genmidi_ok) return;

    SDL_LockAudioDevice(audiodev);

    free(mus_buf);
    mus_buf    = NULL;
    mus_active = 0;

    for(i = 0; i < OPL_VOICES; i++)
    {
        wreg(0xB0 + i, 0);
        oplchans[i].active = 0;
        oplchans[i].b0     = 0;
    }

    snprintf(name, sizeof(name), "D_E%dM%d", episode, map);
    lump = wad_findlump(name, true);
    if(!lump)
    {
        printf("[mus] lump %s not found\n", name);
        SDL_UnlockAudioDevice(audiodev);
        return;
    }

    mus_buf = malloc(lump->size);
    if(!mus_buf)
    {
        wad_decache(lump);
        SDL_UnlockAudioDevice(audiodev);
        return;
    }
    memcpy(mus_buf, lump->cache, lump->size);
    mus_bufsize = lump->size;
    wad_decache(lump);

    if(mus_bufsize < 8 || memcmp(mus_buf, "MUS\x1a", 4) != 0)
    {
        printf("[mus] bad MUS header for %s\n", name);
        free(mus_buf);
        mus_buf = NULL;
        SDL_UnlockAudioDevice(audiodev);
        return;
    }

    scorestart    = *(uint16_t*)(mus_buf + 6);
    mus_scorestart = scorestart;
    mus_pos        = scorestart;
    mus_delay      = 0;
    mus_samp       = 0;

    for(i = 0; i < MUS_NCHANS; i++)
    {
        mchans[i].instrument = 0;
        mchans[i].volume     = 100;
    }

    mus_active = 1;
    mus_process_events();

    SDL_UnlockAudioDevice(audiodev);
    printf("[mus] playing %s\n", name);
}

void mus_mixbuf(int32_t *lbuf, int32_t *rbuf, int n)
{
    static int16_t oplsamp[2];
    static int     oplphase = 0;
    int i;

    if(!mus_active) return;

    for(i = 0; i < n; i++)
    {
        mus_samp++;
        if(mus_samp >= SPT)
        {
            mus_samp = 0;
            if(mus_delay > 0) mus_delay--;
            if(mus_delay == 0) mus_process_events();
        }

        if(oplphase == 0)
            OPL3_GenerateResampled(&chip, oplsamp);
        oplphase = (oplphase + 1) % OPL_STEP;

        lbuf[i] += oplsamp[0];
        rbuf[i] += oplsamp[1];
    }
}
