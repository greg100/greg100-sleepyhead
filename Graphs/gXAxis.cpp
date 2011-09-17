/*
 gXAxis Implementation
 Copyright (c)2011 Mark Watkins <jedimark@users.sourceforge.net>
 License: GPL
*/

#include <math.h>
#include <QDebug>
#include "gXAxis.h"

const quint64 divisors[]={
    15552000000LL, 7776000000LL, 5184000000LL, 2419200000L, 1814400000L, 1209600000L, 604800000L, 259200000L,
    172800000L, 86400000,2880000,14400000,7200000,3600000,2700000,
    1800000,1200000,900000,600000,300000,120000,60000,45000,30000,
    20000,15000,10000,5000,2000,1000,100,50,10
};
const int divcnt=sizeof(divisors)/sizeof(quint64);

gXAxis::gXAxis(QColor col,bool fadeout)
:Layer("")
{
    m_line_color=col;
    m_text_color=col;
    m_major_color=Qt::darkGray;
    m_minor_color=Qt::lightGray;
    m_show_major_lines=false;
    m_show_minor_lines=false;
    m_show_minor_ticks=true;
    m_show_major_ticks=true;
    m_utcfix=false;
    m_fadeout=fadeout;
    QDateTime d=QDateTime::currentDateTime();
    QTime t1=d.time();
    QTime t2=d.toUTC().time();

    tz_offset=t2.secsTo(t1);
    tz_hours=tz_offset/3600.0;
    tz_offset*=1000L;

}
gXAxis::~gXAxis()
{
}
void gXAxis::paint(gGraph & w,int left,int top, int width, int height)
{
    double px,py;

    int start_px=left;
    //int start_py=top;
    //int width=scrx-(w.GetLeftMargin()+w.GetRightMargin());
//    float height=scry-(w.GetTopMargin()+w.GetBottomMargin());

    if (width<40)
        return;
    qint64 minx;
    qint64 maxx;
    if (w.blockZoom()) {
        minx=w.rmin_x;
        maxx=w.rmax_x;
    } else {
        minx=w.min_x;
        maxx=w.max_x;
    }
    qint64 xx=maxx-minx;
    if (xx<=0) return;

    //Most of this could be precalculated when min/max is set..
    QString fd,tmpstr;
    int divmax,dividx;
    int fitmode;
    if (xx>=86400000L) {         // Day
        fd="Mjj 00";
        dividx=0;
        divmax=10;
        fitmode=0;
    } else if (xx>600000) {    // Minutes
        fd="00:00";
        dividx=10;
        divmax=27;
        fitmode=1;
    } else if (xx>5000) {      // Seconds
        fd="00:00:00";
        dividx=16;
        divmax=27;
        fitmode=2;
    } else {                   // Microseconds
        fd="00:00:00:000";
        dividx=28;
        divmax=divcnt;
        fitmode=3;
    }
    //if (divmax>divcnt) divmax=divcnt;

    int x,y;
    GetTextExtent(fd,x,y);

    if (x<=0) {
        qWarning() << "gXAxis::Plot() x<=0 font size bug";
        return;
    }

    int max_ticks=width/(x+15);  // Max number of ticks that will fit

    int fit_ticks=0;
    int div=-1;
    qint64 closest=0,tmp,tmpft;
    for (int i=dividx;i<divmax;i++){
        tmpft=xx/divisors[i];
        tmp=max_ticks-tmpft;
        if (tmp<0) continue;
        if (tmpft>closest) {     // Find the closest scale to the number
            closest=tmpft;       // that will fit
            div=i;
            fit_ticks=tmpft;
        }
    }
    if (fit_ticks==0) {
        qDebug() << "gXAxis::Plot() Couldn't fit ticks.. Too short?" << minx << maxx << xx;
        return;
    }
    if ((div<0) || (div>divcnt)) {
        qDebug() << "gXAxis::Plot() div out of bounds";
        return;
    }
    qint64 step=divisors[div];

    //Align left minimum to divisor by losing precision
    qint64 aligned_start=minx/step;
    aligned_start*=step;

    while (aligned_start<minx) {
        aligned_start+=step;
    }

    QColor linecol=Qt::black;
    GLBuffer *lines=w.backlines();


    int utcoff=m_utcfix ? -tz_hours : 0;

    //utcoff=0;
    int num_minor_ticks;

    if (step>=86400000) {
        qint64 i=step/86400000L; // number of days
        if (i>14) i/=2;
        if (i<0) i=1;
        num_minor_ticks=i;
    } else num_minor_ticks=10;

    float xmult=double(width)/double(xx);
    float step_pixels=double(step/float(num_minor_ticks))*xmult;

    py=left+float(aligned_start-minx)*xmult;


    for (int i=0;i<num_minor_ticks;i++) {
        py-=step_pixels;
        if (py<start_px) continue;
        lines->add(py,top,py,top+4,linecol);
    }

    for (qint64 i=aligned_start;i<maxx;i+=step) {
        px=(i-minx)*xmult;
        px+=left;
        lines->add(px,top,px,top+6,linecol);
        qint64 j=i;
        if (!m_utcfix) j+=tz_offset;
        int ms=j % 1000;
        int m=(j/60000L) % 60L;
        int h=((j/3600000L)-utcoff) % 24L;
        int s=(j/1000L) % 60L;
        static QString dow[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        int d=(j/86400000) % 7;

        if (fitmode==0) {
            int d=(j/1000);
            QDateTime dt=QDateTime::fromTime_t(d);
            tmpstr=dt.toString("MMM dd");
        //} else if (fitmode==0) {
//            tmpstr=QString("%1 %2:%3").arg(dow[d]).arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0'));
        } else if (fitmode==1) { // minute
            tmpstr=QString("%1:%2").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0'));
        } else if (fitmode==2) { // second
            tmpstr=QString("%1:%2:%3").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
        } else if (fitmode==3) { // milli
            tmpstr=QString("%1:%2:%3:%4").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0')).arg(ms,3,10,QChar('0'));
        }

        int tx=px-x/2.0;
        GetTextExtent(tmpstr,x,y); // this only really needs running once :(
        if (m_utcfix)
            tx+=step_pixels/2.0;
        if ((tx+x)<(left+width))
            w.renderText(tmpstr,tx,top+18);
        py=px;
        for (int j=1;j<num_minor_ticks;j++) {
            py+=step_pixels;
            if (py>=left+width) break;
            lines->add(py,top,py,top+4,linecol);
        }

        if (lines->full()) {
            qWarning() << "maxverts exceeded in gXAxis::Plot()";
            break;
        }
    }
}

