#include "lineatk.h"

float linez;
float topslope, botslope;
float lineslope;
float aimdst;
object_t *atkmobj;
bool dealtdamage;
int linedmg;

static bool aimlinecol(float x1, float y1, float x2, float y2, linedef_t* line, float t)
{
    float dist, bottom, top, lbotslope, ltopslope;

    dist = t * aimdst;

    if(!line->back || !line->back->sector)
        return true;

    bottom = level_linelower(line) - linez;
    top = level_lineupper(line) - linez;

    if(bottom >= top)
        return true;

    lbotslope = bottom / dist;
    ltopslope = top / dist;

    topslope = MIN(topslope, ltopslope);
    botslope = MAX(botslope, lbotslope);

    if(botslope >= topslope)
        return true;

    return false;
}

static bool aimmobjcol(float x1, float y1, float x2, float y2, object_t* mobj, float t)
{
    float dist;
    float mobjtop, mobjbottom, mtopslope, mbotslope;

    if(mobj == atkmobj)
        return false;

    if(!(mobj->info.flags & MF_SHOOTABLE))
        return false;

    dist = aimdst * t;

    mobjbottom = mobj->info.z - linez;
    mobjtop = mobjbottom + mobjinfo[mobj->info.type].height;

    mbotslope = mobjbottom / dist;
    mtopslope = mobjtop / dist;

    if(mbotslope >= mtopslope)
        return false;

    if(mbotslope >= topslope)
        return false;

    if(mtopslope <= botslope)
        return false;

    lineslope = LERP(mbotslope, mtopslope, 0.5);

    return true;
}

float lineatk_findslope(object_t* mobj, angle_t ang)
{
    float x1, x2, y1, y2;
    float cosang, sinang;

    atkmobj = mobj;

    cosang = ANGCOS(ang);
    sinang = ANGSIN(ang);

    aimdst = 1024;

    x1 = mobj->info.x;
    y1 = mobj->info.y;
    x2 = x1 + cosang * aimdst;
    y2 = y1 + sinang * aimdst;

    linez = mobj->info.z + (mobjinfo[mobj->info.type].height / 2.0) + 8;
    topslope = 0.625;
    botslope = -0.625;
    lineslope = 0;

    level_traverseline(x1, y1, x2, y2, false, aimlinecol, aimmobjcol);

    return lineslope;
}

static bool atklinecol(float x1, float y1, float x2, float y2, linedef_t* line, float t)
{
    float dist, bottom, top, lbotslope, ltopslope;

    dist = t * aimdst;

    if(!line->back || !line->back->sector)
        return true;

    bottom = level_linelower(line) - linez;
    top = level_lineupper(line) - linez;

    if(bottom >= top)
        return true;

    lbotslope = bottom / dist;
    ltopslope = top / dist;

    if(lbotslope >= lineslope)
        return true;
    if(ltopslope <= lineslope)
        return true;

    return false;
}

static bool atkmobjcol(float x1, float y1, float x2, float y2, object_t* mobj, float t)
{
    float dist;
    float mobjtop, mobjbottom, mtopslope, mbotslope;

    if(mobj == atkmobj)
        return false;

    if(!(mobj->info.flags & MF_SHOOTABLE))
        return false;

    dist = aimdst * t;

    mobjbottom = mobj->info.z - linez;
    mobjtop = mobjbottom + mobjinfo[mobj->info.type].height;

    mbotslope = mobjbottom / dist;
    mtopslope = mobjtop / dist;

    if(mbotslope >= mtopslope)
        return false;

    if(mbotslope > lineslope)
        return false;

    if(mtopslope < lineslope)
        return false;

    dealtdamage = true;
    level_damagemobj(mobj, linedmg);
    return true;
}

bool lineatk(int dmg, object_t* mobj, angle_t ang, float dist, float slope)
{
    float x1, x2, y1, y2;
    float cosang, sinang;

    atkmobj = mobj;
    linedmg = dmg;

    cosang = ANGCOS(ang);
    sinang = ANGSIN(ang);

    aimdst = 1024;

    x1 = mobj->info.x;
    y1 = mobj->info.y;
    x2 = x1 + cosang * aimdst;
    y2 = y1 + sinang * aimdst;

    linez = mobj->info.z + (mobjinfo[mobj->info.type].height / 2.0) + 8;
    lineslope = slope;

    dealtdamage = false;
    level_traverseline(x1, y1, x2, y2, false, atklinecol, atkmobjcol);

    return dealtdamage;
}
