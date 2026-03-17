#include "los.h"

float sightdist, sightz, sighttopslope, sightbotslope;
bool sightblocked;

static bool lineofsight_col(float x1, float y1, float x2, float y2, linedef_t* line, float t)
{
    float lower, upper;
    float botslope, topslope;

    if(!line->back || !line->front)
    {
        sightblocked = true;
        return true;
    }

    lower = level_linelower(line);
    upper = level_lineupper(line);

    botslope = (lower - sightz) / (t * sightdist);
    topslope = (upper - sightz) / (t * sightdist);

    sighttopslope = MIN(sighttopslope, topslope);
    sightbotslope = MAX(sightbotslope, botslope);

    if(sightbotslope >= sighttopslope)
    {
        sightblocked = true;
        return true;
    }

    return false;
}

bool lineofsight(object_t* a, object_t* b)
{
    sightz = a->info.z + a->info.height / 2.0;
    sightdist = magnitude(b->info.x - a->info.x, b->info.y - a->info.y);

    sightbotslope = (b->info.z - sightz) / sightdist;
    sighttopslope = (b->info.z + b->info.height - sightz) / sightdist;

    sightblocked = false;
    level_traverseline(a->info.x, a->info.y, b->info.x, b->info.y, false, lineofsight_col, NULL);

    return !sightblocked;
}