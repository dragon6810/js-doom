#include "snd.h"

#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "connection.h"
#include "../doommath.h"
#include "../level.h"
#include "../packets.h"
#include "../wad.h"

#define SND_CHANNELS   16
#define SND_FREQ       44100
#define SND_CLIPDIST   1200.0f
#define SND_CLOSEDIST  160.0f

static const char *sfxnames[NUM_SFX] =
{
    "DSPISTOL",
    "DSSHOTGN",
    "DSDOROPN",
    "DSDORCLS",
};

typedef struct
{
    int16_t *samples;
    int nsamples;
} sndcache_t;

typedef struct
{
    bool active;
    const int16_t *samples;
    int nsamples;
    int pos;
    float leftvol;
    float rightvol;
    bool hasedict;
    int edict;
    float srcx, srcy;
} sndchan_t;

static sndcache_t cache[NUM_SFX];
static sndchan_t channels[SND_CHANNELS];

static float listenerx, listenery;
static angle_t listenerang;

static SDL_AudioDeviceID audiodev;

static void audiocallback(void *udata, uint8_t *stream, int len)
{
    int i, s, n;
    int32_t l, r;
    int16_t *out = (int16_t*)stream;
    sndchan_t *ch;

    n = len / 4; // stereo int16 pairs

    for(s=0; s<n; s++)
    {
        l = r = 0;
        for(i=0; i<SND_CHANNELS; i++)
        {
            ch = &channels[i];
            if(!ch->active)
                continue;
            if(ch->pos >= ch->nsamples)
            {
                ch->active = false;
                continue;
            }
            l += (int32_t)(ch->samples[ch->pos] * ch->leftvol);
            r += (int32_t)(ch->samples[ch->pos] * ch->rightvol);
            ch->pos++;
        }
        if(l >  32767) l =  32767;
        if(l < -32768) l = -32768;
        if(r >  32767) r =  32767;
        if(r < -32768) r = -32768;
        out[s*2+0] = (int16_t)l;
        out[s*2+1] = (int16_t)r;
    }
}

static void decodesound(int sfxid)
{
    lumpinfo_t *lump;
    uint8_t *data;
    int i, rawcount, outcount;
    int srcfreq;
    float ratio;
    int16_t *out;

    if(cache[sfxid].samples)
        return;

    lump = wad_findlump(sfxnames[sfxid], true);
    if(!lump || lump->size < 25)
        return;

    data = (uint8_t*)lump->cache;

    srcfreq = *(uint16_t*)(data + 2);
    rawcount = *(uint32_t*)(data + 4);
    rawcount = MIN(rawcount, lump->size - 24);

    // upsample to SND_FREQ using nearest-neighbour
    ratio = (float)SND_FREQ / (float)srcfreq;
    outcount = (int)(rawcount * ratio);

    out = malloc(outcount * sizeof(int16_t));
    for(i=0; i<outcount; i++)
    {
        int src = (int)(i / ratio);
        if(src >= rawcount) src = rawcount - 1;
        out[i] = ((int16_t)data[24 + src] - 128) * 256;
    }

    cache[sfxid].samples = out;
    cache[sfxid].nsamples = outcount;
}

void snd_init(void)
{
    SDL_AudioSpec want, got;

    memset(&want, 0, sizeof(want));
    want.freq     = SND_FREQ;
    want.format   = AUDIO_S16;
    want.channels = 2;
    want.samples  = 512;
    want.callback = audiocallback;

    audiodev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    SDL_PauseAudioDevice(audiodev, 0);
}

static void calcsepvol(float srcx, float srcy, float *lv, float *rv)
{
    float dx, dy, dist, vol, sep;
    angle_t ang;

    dx = srcx - listenerx;
    dy = srcy - listenery;
    dist = sqrtf(dx*dx + dy*dy);

    if(dist >= SND_CLIPDIST)
    {
        *lv = *rv = 0;
        return;
    }

    vol = (dist <= SND_CLOSEDIST) ? 1.0f
        : (SND_CLIPDIST - dist) / (SND_CLIPDIST - SND_CLOSEDIST);

    ang = ANGATAN2(dy, dx) - listenerang;
    sep = 0.5f - ANGSIN(ang) * 0.375f;

    *lv = vol * (1.0f - sep);
    *rv = vol * sep;
}

static void startchannel(int sfxid, bool hasedict, int edict, float x, float y)
{
    int i;
    sndchan_t *ch, *oldest;
    float lv, rv;

    if(!cache[sfxid].samples)
        return;

    // find a free channel; if none, steal the one furthest through playback
    ch = NULL;
    oldest = &channels[0];
    for(i=0; i<SND_CHANNELS; i++)
    {
        if(!channels[i].active) { ch = &channels[i]; break; }
        if(channels[i].pos > oldest->pos) oldest = &channels[i];
    }
    if(!ch) ch = oldest;

    if(hasedict && edict == serverconn.edict)
    {
        // own sounds: full volume, center pan
        lv = rv = 1.0f;
    }
    else
    {
        if(hasedict)
        {
            x = mobjs[edict].info.x;
            y = mobjs[edict].info.y;
        }
        calcsepvol(x, y, &lv, &rv);
    }

    SDL_LockAudioDevice(audiodev);
    ch->active   = true;
    ch->samples  = cache[sfxid].samples;
    ch->nsamples = cache[sfxid].nsamples;
    ch->pos      = 0;
    ch->leftvol  = lv;
    ch->rightvol = rv;
    ch->hasedict = hasedict;
    ch->edict    = edict;
    ch->srcx     = x;
    ch->srcy     = y;
    SDL_UnlockAudioDevice(audiodev);
}

void snd_playsoundedict(int sfxid, int edict)
{
    decodesound(sfxid);
    startchannel(sfxid, true, edict, 0, 0);
}

void snd_playsoundpos(int sfxid, float x, float y)
{
    decodesound(sfxid);
    startchannel(sfxid, false, -1, x, y);
}

void snd_update(float lx, float ly, angle_t lang)
{
    int i;
    sndchan_t *ch;
    float x, y, lv, rv;

    listenerx  = lx;
    listenery  = ly;
    listenerang = lang;

    SDL_LockAudioDevice(audiodev);
    for(i=0; i<SND_CHANNELS; i++)
    {
        ch = &channels[i];
        if(!ch->active || ch->edict == serverconn.edict)
            continue;

        x = ch->srcx;
        y = ch->srcy;
        if(ch->hasedict)
        {
            x = mobjs[ch->edict].info.x;
            y = mobjs[ch->edict].info.y;
        }
        calcsepvol(x, y, &lv, &rv);
        ch->leftvol  = lv;
        ch->rightvol = rv;
    }
    SDL_UnlockAudioDevice(audiodev);
}

// stubs — special.c is compiled client-side but never triggers door sounds there
void snd_queueedict(int sfxid, int edict) {}
void snd_queuepos(int sfxid, float x, float y) {}
