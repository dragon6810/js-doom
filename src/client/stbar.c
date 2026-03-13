#include "stbar.h"

#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "draw.h"
#include "rand.h"
#include "screen.h"
#include "wad.h"

// Number of status faces.
#define ST_NUMPAINFACES		5
#define ST_NUMSTRAIGHTFACES	3
#define ST_NUMTURNFACES		2
#define ST_NUMSPECIALFACES		3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES		2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET		(ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET		(ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET		(ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET		(ST_EVILGRINOFFSET + 1)
#define ST_GODFACE			(ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE			(ST_GODFACE+1)

#define ST_FACESX			143
#define ST_FACESY			0

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT		(1*TICRATE)
#define ST_OUCHCOUNT		(1*TICRATE)
#define ST_RAMPAGEDELAY		(2*TICRATE)

#define ST_MUCHPAIN			20

// AMMO number pos.
#define ST_AMMOWIDTH		3	
#define ST_AMMOX			44
#define ST_AMMOY			3

// HEALTH number pos.
#define ST_HEALTHWIDTH		3	
#define ST_HEALTHX			90
#define ST_HEALTHY			3

// Weapon pos.
#define ST_ARMSX			111
#define ST_ARMSY			4
#define ST_ARMSBGX			104
#define ST_ARMSBGY			0
#define ST_ARMSXSPACE		12
#define ST_ARMSYSPACE		10

// Frags pos.
#define ST_FRAGSX			138
#define ST_FRAGSY			3	
#define ST_FRAGSWIDTH		2

// ARMOR number pos.
#define ST_ARMORWIDTH		3
#define ST_ARMORX			221
#define ST_ARMORY			3

// Key icon positions.
#define ST_KEY0WIDTH		8
#define ST_KEY0HEIGHT		5
#define ST_KEY0X			239
#define ST_KEY0Y			3
#define ST_KEY1WIDTH		ST_KEY0WIDTH
#define ST_KEY1X			239
#define ST_KEY1Y			13
#define ST_KEY2WIDTH		ST_KEY0WIDTH
#define ST_KEY2X			239
#define ST_KEY2Y			23

// Ammunition counter.
#define ST_AMMO0WIDTH 3
#define ST_AMMO0HEIGHT 6
#define ST_AMMO0X 288
#define ST_AMMO0Y 5
#define ST_AMMO1WIDTH ST_AMMO0WIDTH
#define ST_AMMO1X 288
#define ST_AMMO1Y 11
#define ST_AMMO2WIDTH ST_AMMO0WIDTH
#define ST_AMMO2X 288
#define ST_AMMO2Y 23
#define ST_AMMO3WIDTH ST_AMMO0WIDTH
#define ST_AMMO3X 288
#define ST_AMMO3Y 17

#define ST_MAXAMMO0WIDTH 3
#define ST_MAXAMMO0HEIGHT 5
#define ST_MAXAMMO0X 314
#define ST_MAXAMMO0Y 5
#define ST_MAXAMMO1WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X 314
#define ST_MAXAMMO1Y 11
#define ST_MAXAMMO2WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X 314
#define ST_MAXAMMO2Y 23
#define ST_MAXAMMO3WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X 314
#define ST_MAXAMMO3Y 17

lumpinfo_t *stbarlump;
int sttop;
lumpinfo_t *bignums[10];
lumpinfo_t *graynums[10];
lumpinfo_t *yellownums[10];
lumpinfo_t *bigpercent;
lumpinfo_t *keys[6];
lumpinfo_t *facestart;
lumpinfo_t *facebg;

lumpinfo_t *curface;

void stbar_init(void)
{
    int i;

    char bigname[9], grayname[9], yellowname[9], keyname[9];

    stbarlump = wad_findlump("STBAR", true);
    if(!stbarlump)
        fprintf(stderr, "stbar_init: no STBAR lump\n");

    sttop = screenheight - 32.0 * (screenheight / 200.0);

    strcpy(bigname, "STTNUM ");
    strcpy(grayname, "STGNUM ");
    strcpy(yellowname, "STYSNUM ");
    for(i=0; i<10; i++)
    {
        bigname[6] = i + '0';
        grayname[6] = i + '0';
        yellowname[7] = i + '0';
        bignums[i] = wad_findlump(bigname, true);
        graynums[i] = wad_findlump(grayname, true);
        yellownums[i] = wad_findlump(yellowname, true);

        if(!bignums[i])
            fprintf(stderr, "stbar_init no %s lump\n", bigname);
        if(!graynums[i])
            fprintf(stderr, "stbar_init no %s lump\n", grayname);
        if(!yellownums[i])
            fprintf(stderr, "stbar_init no %s lump\n", yellowname);
    }

    bigpercent = wad_findlump("STTPRCNT", true);
    if(!bigpercent)
        fprintf(stderr, "stbar_init no STTPRCNT lump\n");

    strcpy(keyname, "STKEYS ");
    for(i=0; i<6; i++)
    {
        keyname[6] = i + '0';
        keys[i] = wad_findlump(keyname, true);
        if(!keys[i])
            fprintf(stderr, "stbar_init no %s lump\n", keyname);
    }

    facestart = wad_findlump("STFST01", false);
    if(!facestart)
        fprintf(stderr, "stbar_init no STFST01 lump\n");

    facebg = wad_findlump("STFB0", false);
    if(!facebg)
        fprintf(stderr, "stbar_init no STFB0 lump\n");
}

typedef struct
{
    thinker_t thinker;
    int lasthealth;
    float dmgfade;
    float lastglance;
    object_t *lastmobj;
} stthink_t;

int glanceface = 0;

static void stbar_calcface(stthink_t* thinker, float ft, float progt)
{
    int offs;

    if(!facestart)
        curface = NULL;

    if(progt - thinker->lastglance > 0.5)
    {
        glanceface = mrand() % 3;
        thinker->lastglance = progt;
    }

    curface = facestart + glanceface;
}

bool stbar_think(stthink_t* thinker, float ft, float progt)
{
    int palindex;
    int dmg;
    bool spawned;

    if(!player.mobj)
    {
        thinker->lastmobj = NULL;
        return false;
    }

    spawned = thinker->lastmobj != player.mobj;

    stbar_calcface(thinker, ft, progt);

    if(!spawned && thinker->lasthealth >= 0)
        dmg = CLAMP(thinker->lasthealth - player.mobj->info.health, 0, 100);
    else
        dmg = 0;
    thinker->dmgfade += dmg;

    palindex = 0;
    if(thinker->dmgfade > 0)
    {
        palindex = ((int) thinker->dmgfade + 7) / 8;
        if(palindex >= 8)
            palindex = 7;
        palindex++;

        thinker->dmgfade -= ft * 35;
        if(thinker->dmgfade < 0)
            thinker->dmgfade = 0;
    }
    else if(player.pickupcnt >= 1)
    {
        palindex = ((int) player.pickupcnt + 7) / 8;
        if(palindex >= 4)
            palindex = 3;
        palindex += 9;
    }

    player.pickupcnt -= ft * 35;
    if(player.pickupcnt < 0)
        player.pickupcnt = 0;

    screen_setpal(palettes[palindex]);

    thinker->lasthealth = player.mobj->info.health;
    thinker->lastmobj = player.mobj;

    return false;
}

void stbar_makethink(void)
{
    stthink_t *thinker;

    thinker = calloc(1, sizeof(stthink_t));
    thinker->thinker.func = (thinkfunc_t) stbar_think;
    thinker->lasthealth = -1;
    addthinker(thinker);
}

// right-aligned
// percent can be NULL
void stbar_drawnumber(int x, int y, int num, int width, lumpinfo_t* digits[10], lumpinfo_t* percent)
{
    int i;
    
    pic_t *pic;
    int dig;

    for(i=0; i<10; i++)
        if(!digits[i])
            return;
    
    if(percent)
    {
        wad_cache(percent);
        pic = percent->cache;
        draw_scaledpic(pic, NULL, colormap->maps[0], x << FIXEDSHIFT, y << FIXEDSHIFT);
    }

    if(!num)
    {
        wad_cache(digits[0]);
        pic = digits[0]->cache;
        x -= pic->w;
        draw_scaledpic(pic, NULL, colormap->maps[0], x << FIXEDSHIFT, y << FIXEDSHIFT);
        return;
    }

    for(i=0; i<width&&num; i++, num/=10)
    {
        dig = num % 10;
        wad_cache(digits[dig]);
        pic = digits[dig]->cache;
        x -= pic->w;
        draw_scaledpic(pic, NULL, colormap->maps[0], x << FIXEDSHIFT, y << FIXEDSHIFT);
    }
}

static void stbar_drawfacebg(void)
{
    if(!player.mobj->info.color)
        return;
    if(!facebg)
        return;
    
    wad_cache(facebg);
    draw_scaledpic(facebg->cache, transtbls[player.mobj->info.color], colormap->maps[0], ST_FACESX << FIXEDSHIFT, ST_FACESY << FIXEDSHIFT);
}

void stbar_draw(void)
{
    int x;

    int maxbul, maxshel, maxrock, maxcell;

    drawscreen = &screens[SCR_ST];

    if(!stbarlump)
        return;
    wad_cache(stbarlump);

    maxbul = 200;
    maxshel = 50;
    maxrock = 50;
    maxcell = 300;

    if(player.info.flags & PFLAG_BACKPACK)
    {
        maxbul *= 2;
        maxshel *= 2;
        maxrock *= 2;
        maxcell *= 2;
    }

    draw_scaledpic(stbarlump->cache, NULL, colormap->maps[0], 0, 0);

    // stbar_drawnumber(ST_AMMOX, ST_AMMOY, 50, ST_AMMOWIDTH, bignums, NULL);
    stbar_drawnumber(ST_HEALTHX, ST_HEALTHY, player.mobj->info.health, ST_HEALTHWIDTH, bignums, bigpercent);
    stbar_drawnumber(ST_FRAGSX, ST_FRAGSY, player.info.frags, ST_FRAGSWIDTH, bignums, NULL);
    stbar_drawnumber(ST_ARMORX, ST_ARMORY, player.info.armor, ST_ARMORWIDTH, bignums, bigpercent);
    stbar_drawnumber(ST_AMMO0X, ST_AMMO0Y, player.info.ammo[AMMO_BUL], ST_AMMO0WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_AMMO1X, ST_AMMO1Y, player.info.ammo[AMMO_SHEL], ST_AMMO1WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_AMMO2X, ST_AMMO2Y, player.info.ammo[AMMO_ROCK], ST_AMMO2WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_AMMO3X, ST_AMMO3Y, player.info.ammo[AMMO_CELL], ST_AMMO3WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_MAXAMMO0X, ST_MAXAMMO0Y, maxbul, ST_MAXAMMO0WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_MAXAMMO1X, ST_MAXAMMO1Y, maxshel, ST_MAXAMMO1WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_MAXAMMO2X, ST_MAXAMMO2Y, maxrock, ST_MAXAMMO2WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_MAXAMMO3X, ST_MAXAMMO3Y, maxcell, ST_MAXAMMO3WIDTH, yellownums, NULL);

    if(keys[0] && keys[1] && keys[2])
    {
        wad_cache(keys[0]);
        wad_cache(keys[1]);
        wad_cache(keys[2]);

        draw_scaledpic(keys[0]->cache, NULL, colormap->maps[0], ST_KEY0X << FIXEDSHIFT, ST_KEY0Y << FIXEDSHIFT);
        draw_scaledpic(keys[1]->cache, NULL, colormap->maps[0], ST_KEY1X << FIXEDSHIFT, ST_KEY1Y << FIXEDSHIFT);
        draw_scaledpic(keys[2]->cache, NULL, colormap->maps[0], ST_KEY2X << FIXEDSHIFT, ST_KEY2Y << FIXEDSHIFT);
    }

    stbar_drawfacebg();
    if(curface)
    {
        wad_cache(curface);
        draw_scaledpic(curface->cache, NULL, colormap->maps[0], ST_FACESX << FIXEDSHIFT, ST_FACESY << FIXEDSHIFT);
    }
}