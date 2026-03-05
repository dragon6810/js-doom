#include "stbar.h"

#include <string.h>

#include "draw.h"
#include "screen.h"
#include "wad.h"

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
#define ST_KEY0Y			4
#define ST_KEY1WIDTH		ST_KEY0WIDTH
#define ST_KEY1X			239
#define ST_KEY1Y			14
#define ST_KEY2WIDTH		ST_KEY0WIDTH
#define ST_KEY2X			239
#define ST_KEY2Y			24

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
}

// y from top of stbar
static void stbar_drawpic(pic_t* pic, int x, int y)
{
    fixed_t texmid;
    fixed_t iscale, scale, s;
    post_t *post;

    y += sttop;

    if(y >= screenheight)
        return;

    texmid = -((y << FIXEDSHIFT) - (screenheight << (FIXEDSHIFT - 1)));
    iscale = fixeddiv(200 << FIXEDSHIFT, screenheight << FIXEDSHIFT);
    scale = fixeddiv(1 << FIXEDSHIFT, iscale);

    if(y + (fixedmul(pic->h << FIXEDSHIFT, scale) >> FIXEDSHIFT) <= sttop)
        return;

    y = MAX(y, sttop);

    for(s=0; x<screenwidth && (s>>FIXEDSHIFT)<pic->w; x++, s+=iscale)
    {
        if(x < 0)
            continue;
        if(x >= screenwidth)
            break;

        post = (post_t*) ((uint8_t*) pic + pic->postoffs[s >> FIXEDSHIFT]);
        draw_postcolumn(post, colormap->maps[0], x, y, screenheight-1, texmid, iscale, scale); 
    }
}

// right-aligned
// percent can be NULL
void stbar_drawnumber(int x, int y, int num, int width, lumpinfo_t* digits[10], lumpinfo_t* percent)
{
    int i;
    
    pic_t *pic;
    fixed_t scale;
    int dig;

    for(i=0; i<10; i++)
        if(!digits[i])
            return;

    scale = fixeddiv(screenheight << FIXEDSHIFT, 200 << FIXEDSHIFT);
    
    if(percent)
    {
        wad_cache(percent);
        pic = percent->cache;
        stbar_drawpic(pic, x, y);
    }

    if(!num)
    {
        wad_cache(digits[0]);
        pic = digits[0]->cache;
        x -= fixedmul(pic->w << FIXEDSHIFT, scale) >> FIXEDSHIFT;
        stbar_drawpic(pic, x, y);
        return;
    }

    for(i=0; i<width&&num; i++, num/=10)
    {
        dig = num % 10;
        wad_cache(digits[dig]);
        pic = digits[dig]->cache;
        x -= fixedmul(pic->w << FIXEDSHIFT, scale) >> FIXEDSHIFT;
        stbar_drawpic(pic, x, y);
    }
}

void stbar_draw(void)
{
    int x;

    if(!stbarlump)
        return;
    wad_cache(stbarlump);

    stbar_drawpic(stbarlump->cache, 0, 0);

    stbar_drawnumber(ST_AMMOX, ST_AMMOY, 50, ST_AMMOWIDTH, bignums, NULL);
    stbar_drawnumber(ST_HEALTHX, ST_HEALTHY, 100, ST_HEALTHWIDTH, bignums, bigpercent);
    stbar_drawnumber(ST_FRAGSX, ST_FRAGSY, 0, ST_FRAGSWIDTH, bignums, NULL);
    stbar_drawnumber(ST_ARMORX, ST_ARMORY, 100, ST_ARMORWIDTH, bignums, bigpercent);
    stbar_drawnumber(ST_AMMO0X, ST_AMMO0Y, 50, ST_AMMO0WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_AMMO1X, ST_AMMO1Y, 0, ST_AMMO1WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_AMMO2X, ST_AMMO2Y, 0, ST_AMMO2WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_AMMO3X, ST_AMMO3Y, 0, ST_AMMO3WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_MAXAMMO0X, ST_MAXAMMO0Y, 200, ST_MAXAMMO0WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_MAXAMMO1X, ST_MAXAMMO1Y, 50, ST_MAXAMMO1WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_MAXAMMO2X, ST_MAXAMMO2Y, 50, ST_MAXAMMO2WIDTH, yellownums, NULL);
    stbar_drawnumber(ST_MAXAMMO3X, ST_MAXAMMO3Y, 300, ST_MAXAMMO3WIDTH, yellownums, NULL);

    if(keys[0] && keys[1] && keys[2])
    {
        wad_cache(keys[0]);
        wad_cache(keys[1]);
        wad_cache(keys[2]);

        stbar_drawpic(keys[0]->cache, ST_KEY0X, ST_KEY0Y);
        stbar_drawpic(keys[1]->cache, ST_KEY1X, ST_KEY1Y);
        stbar_drawpic(keys[2]->cache, ST_KEY2X, ST_KEY2Y);
    }
}