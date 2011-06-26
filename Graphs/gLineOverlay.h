/********************************************************************
 gLineOverlayBar Header
 Copyright (c)2011 Mark Watkins <jedimark@users.sourceforge.net>
 License: GPL
*********************************************************************/

#ifndef GLINEOVERLAY_H
#define GLINEOVERLAY_H

#include "graphlayer.h"
#include "graphdata.h"

enum LO_Type { LOT_Bar, LOT_Dot };
class gLineOverlayBar:public gLayer
{
    public:
        gLineOverlayBar(gPointData *d=NULL,QColor col=QColor("black"),QString _label="",LO_Type _lot=LOT_Bar);
        virtual ~gLineOverlayBar();

        virtual void Plot(gGraphWindow & w,float scrx,float scry);

    protected:
        QString label;
        LO_Type lo_type;
};

#endif // GLINEOVERLAY_H
