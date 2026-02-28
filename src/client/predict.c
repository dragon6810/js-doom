#include "predict.h"

#include "connection.h"

playercmd_t inputwindow[PRED_WINDOW] = {};
object_t predplayer;

void predictplayer(void)
{
    int i;

    int start, end;

    predplayer = mobjs[serverconn.edict];
    predplayer.ssector = NULL;
    level_placemobj(&predplayer);

    start = serverconn.chan.inack + 1;
    end = serverconn.chan.outseq;

    if(end - start >= PRED_WINDOW)
        return;

    for(i=start; i<=end; i++)
    {
        player_docmd(&predplayer, &inputwindow[i % PRED_WINDOW]);
    }
}