/********************************************************************
 SleepLib Session Implementation
 This stuff contains the base calculation smarts
 Copyright (c)2011 Mark Watkins <jedimark@users.sourceforge.net>
 License: GPL
*********************************************************************/

#include "session.h"
#include "math.h"
#include <QDir>
#include <SleepLib/binary_file.h>
#include <vector>
#include <algorithm>

using namespace std;

Session::Session(Machine * m,SessionID session)
{
    if (!session) {
        session=m->CreateSessionID();
    }
    s_machine=m;
    s_session=session;
    s_changed=false;
    s_events_loaded=false;
    s_waves_loaded=false;
    _first_session=true;

    s_wavefile="";
    s_eventfile="";
}
Session::~Session()
{
    TrashEvents();
    TrashWaveforms();
}
double Session::min_event_field(MachineCode mc,int field)
{
    if (events.find(mc)==events.end()) return 0;

    bool first=true;
    double min=0;
    vector<Event *>::iterator i;
    for (i=events[mc].begin(); i!=events[mc].end(); i++) {
        if (field>(*i)->e_fields) throw BoundsError();
        if (first) {
            first=false;
            min=(*(*i))[field];
        } else {
            if (min>(*(*i))[field]) min=(*(*i))[field];
        }
    }
    return min;
}
double Session::max_event_field(MachineCode mc,int field)
{
    if (events.find(mc)==events.end()) return 0;

    bool first=true;
    double max=0;
    vector<Event *>::iterator i;
    for (i=events[mc].begin(); i!=events[mc].end(); i++) {
        if (field>(*i)->e_fields) throw BoundsError();
        if (first) {
            first=false;
            max=(*(*i))[field];
        } else {
            if (max<(*(*i))[field]) max=(*(*i))[field];
        }
    }
    return max;
}

double Session::sum_event_field(MachineCode mc,int field)
{
    if (events.find(mc)==events.end()) return 0;

    double sum=0;
    vector<Event *>::iterator i;

    for (i=events[mc].begin(); i!=events[mc].end(); i++) {
        if (field>(*i)->e_fields) throw BoundsError();
        sum+=(*(*i))[field];
    }
    return sum;
}
double Session::avg_event_field(MachineCode mc,int field)
{
    if (events.find(mc)==events.end()) return 0;

    double sum=0;
    int cnt=0;
    vector<Event *>::iterator i;

    for (i=events[mc].begin(); i!=events[mc].end(); i++) {
        if (field>(*i)->e_fields) throw BoundsError();
        sum+=(*(*i))[field];
        cnt++;
    }

    return sum/cnt;
}

bool sortfunction (double i,double j) { return (i<j); }

double Session::percentile(MachineCode mc,int field,double percent)
{
    if (events.find(mc)==events.end()) return 0;

    vector<EventDataType> array;

    vector<Event *>::iterator e;

    for (e=events[mc].begin(); e!=events[mc].end(); e++) {
        Event & ev = *(*e);
        array.push_back(ev[0]);
    }
    std::sort(array.begin(),array.end(),sortfunction);
    int size=array.size();
    double i=size*percent;
    double t;
    double q=modf(i,&t);
    int j=t;
    if (j>=size-1) return array[j];

    double a=array[j-1];
    double b=array[j];
    if (a==b) return a;
    //double c=(b-a);
    //double d=c*q;
    return array[j]+q;
}

double Session::weighted_avg_event_field(MachineCode mc,int field)
{
    if (events.find(mc)==events.end()) return 0;

    int cnt=0;

    bool first=true;
    QDateTime last;
    int lastval=0,val;
    const int max_slots=2600;
    double vtime[max_slots]={0};


    double mult;
    if ((mc==CPAP_Pressure) || (mc==CPAP_EAP) ||  (mc==CPAP_IAP)) {
        mult=10.0;

    } else mult=10.0;

    vector<Event *>::iterator i;

    for (i=events[mc].begin(); i!=events[mc].end(); i++) {
        Event & e =(*(*i));
        val=e[field]*mult;
        if (field > e.e_fields) throw BoundsError();
        if (first) {
            first=false;
        } else {
            int d=e.e_time.toTime_t()-last.toTime_t();
            if (lastval>max_slots) {
                qWarning("max_slots to small in Session::weighted_avg_event_fied()");
                return 0;
            }

            vtime[lastval]+=d;

        }
        cnt++;
        last=e.e_time;
        lastval=val;
    }

    double total;
    for (int i=0; i<max_slots; i++) total+=vtime[i];
    //double hours=total.GetSeconds().GetLo()/3600.0;

    double s0=0,s1=0,s2=0;

    for (int i=0; i<max_slots; i++) {
        if (vtime[i] > 0) {
            s0=(vtime[i]/3600.0);
            s1+=i*s0;
            s2+=s0;
        }
    }

    return (s1/hours())/mult;
}

void Session::AddEvent(Event * e)
{
    events[e->code()].push_back(e);
    if (s_first.isValid()) {
        if (e->time()<s_first) s_first=e->time();
        if (e->time()>s_last) s_last=e->time();
    } else {
        s_first=s_last=e->time();
        //_first_session=false;
    }
    //qDebug((e->time().toString("yyyy-MM-dd HH:mm:ss")+" %04i %02i").toLatin1(),e->code(),e->fields());
}
void Session::AddWaveform(Waveform *w)
{
    waveforms[w->code()].push_back(w);
  /*  if (s_last.isValid()) {
        if (w->start()<s_first) s_first=w->start();
        if (w->end()>s_last) s_last=w->end();
    } else {
        s_first=w->start();
        s_last=w->end();
    } */

    // Could parse the flow data for Breaths per minute info here..
}
void Session::TrashEvents()
// Trash this sessions Events and release memory.
{
    map<MachineCode,vector<Event *> >::iterator i;
    vector<Event *>:: iterator j;
    for (i=events.begin(); i!=events.end(); i++) {
        for (j=i->second.begin(); j!=i->second.end(); j++) {
            delete *j;
        }
    }
    events.clear();
}
void Session::TrashWaveforms()
// Trash this sessions Waveforms and release memory.
{
    map<MachineCode,vector<Waveform *> >::iterator i;
    vector<Waveform *>:: iterator j;
    for (i=waveforms.begin(); i!=waveforms.end(); i++) {
        for (j=i->second.begin(); j!=i->second.end(); j++) {
            delete *j;
        }
    }
    waveforms.clear();
}


const int max_pack_size=128;
/*template <class T>
int pack(char * buf,T * var)
{
    int c=0;
    int s=sizeof(T);
    if (s>max_pack_size) return 0;

    char * dat=(char *)var;
    if (IsPlatformLittleEndian()) {
        for (int i=0; i<s; i++) {
            buf[i]=dat[i];
        }
    } else {
        for (int i=0; i<s; i++) {
            buf[s-i]=dat[i];
        }
    }
    return s;
} */

bool Session::Store(QString path)
// Storing Session Data in our format
// {DataDir}/{MachineID}/{SessionID}.{ext}
{

    QDir dir(path);
    if (!dir.exists(path))
        dir.mkpath(path);

    QString base;
    base.sprintf("%08lx",s_session);
    base=path+"/"+base;
    qDebug(("Storing Session: "+base).toLatin1());
    bool a,b,c;
    a=StoreSummary(base+".000"); // if actually has events
    if (events.size()>0) b=StoreEvents(base+".001");
    if (waveforms.size()>0) c=StoreWaveforms(base+".002");
    if (a) {
        s_changed=false;
    }
    return a;
}

const quint32 magic=0xC73216AB;

bool Session::StoreSummary(QString filename)
{
    BinaryFile f;
    f.Open(filename,BF_WRITE);

    f.Pack((quint32)magic); 			// Magic Number
    f.Pack((quint32)s_machine->id());	// Machine ID
    f.Pack((quint32)s_session);		// Session ID
    f.Pack((quint16)0);				// File Type 0 == Summary File
    f.Pack((quint16)0);				// File Version

    time_t starttime=s_first.toTime_t();
    time_t duration=s_last.toTime_t()-starttime;

    f.Pack(s_first);					// Session Start Time
    f.Pack((quint16)duration);			// Duration of sesion in seconds.
    f.Pack((quint16)summary.size());

    map<MachineCode,MCDataType> mctype;

    // First output the Machine Code and type for each summary record
    map<MachineCode,QVariant>::iterator i;
    for (i=summary.begin(); i!=summary.end(); i++) {
        MachineCode mc=i->first;

        QVariant::Type type=i->second.type(); // Urkk.. this is a mess.
        if (type==QVariant::Bool) {
            mctype[mc]=MC_bool;
        } else if (type==QVariant::Int) {
            mctype[mc]=MC_int;
        } else if (type==QVariant::LongLong) {
            mctype[mc]=MC_long;
        } else if (type==QVariant::Double) {
            mctype[mc]=MC_double;
        } else if (type==QVariant::String) {
            mctype[mc]=MC_string;
        } else if (type==QVariant::DateTime) {
            mctype[mc]=MC_datetime;
        } else {
            QString t=i->second.typeToName(type);
            qWarning(("Error in Session->StoreSummary: Can't pack variant type "+t).toLatin1());
            exit(1);
        }
        f.Pack((qint16)mc);
        f.Pack((qint8)mctype[mc]);
    }
    // Then dump out the actual data, according to format.
    for (i=summary.begin(); i!=summary.end(); i++) {
        MachineCode mc=i->first;
        if (mctype[mc]==MC_bool) {
            f.Pack((qint8)i->second.toBool());
        } else if (mctype[mc]==MC_int) {
            f.Pack((qint32)i->second.toInt());
        } else if (mctype[mc]==MC_long) {
            f.Pack((qint64)i->second.toLongLong());
        } else if (mctype[mc]==MC_double) {
            f.Pack((double)i->second.toDouble());
        } else if (mctype[mc]==MC_string) {
            f.Pack(i->second.toString());
        } else if (mctype[mc]==MC_datetime) {
            f.Pack(i->second.toDateTime());
        }
    }
    f.Close();
//    wxFFile j;
    return true;
}
bool Session::LoadSummary(QString filename)
{
    qDebug(("Loading Summary "+filename).toLatin1());
    BinaryFile f;
    if (!f.Open(filename,BF_READ)) {
        qDebug(("Couldn't open file"+filename).toLatin1());
        return false;
    }

    quint64 t64;
    quint32 t32;
    quint16 t16;
    quint8 t8;

    qint16 sumsize;
    map<MachineCode,MCDataType> mctype;
    vector<MachineCode> mcorder;

    if (!f.Unpack(t32)) throw UnpackError();		// Magic Number
    if (t32!=magic) throw UnpackError();

    if (!f.Unpack(t32)) throw UnpackError();		// MachineID

    if (!f.Unpack(t32)) throw UnpackError();		// Sessionid;
    s_session=t32;

    if (!f.Unpack(t16)) throw UnpackError();		// File Type
    if (t16!=0) throw UnpackError(); 				//wrong file type

    if (!f.Unpack(t16)) throw UnpackError();		// File Version
    // dont care yet

    if (!f.Unpack(s_first)) throw UnpackError();	// Start time
    if (!f.Unpack(t16)) throw UnpackError();		// Duration // (16bit==Limited to 18 hours)

    s_last=s_first.addSecs(t16);
    s_hours=t16/3600.0;

    if (!f.Unpack(sumsize)) throw UnpackError();	// Summary size (number of Machine Code lists)

    for (int i=0; i<sumsize; i++) {
        if (!f.Unpack(t16)) throw UnpackError();	// Machine Code
        MachineCode mc=(MachineCode)t16;
        if (!f.Unpack(t8)) throw UnpackError();		// Data Type
        mctype[mc]=(MCDataType)t8;
        mcorder.push_back(mc);
    }

    for (int i=0; i<sumsize; i++) {					// Load each summary entry according to type
        MachineCode mc=mcorder[i];
        if (mctype[mc]==MC_bool) {
            bool b;
            if (!f.Unpack(b)) throw UnpackError();
            summary[mc]=b;
        } else if (mctype[mc]==MC_int) {
            if (!f.Unpack(t32)) throw UnpackError();
            summary[mc]=(qint32)t32;
        } else if (mctype[mc]==MC_long) {
            if (!f.Unpack(t64)) throw UnpackError();
            summary[mc]=(qint64)t64;
        } else if (mctype[mc]==MC_double) {
            double dl;
            if (!f.Unpack(dl)) throw UnpackError();
            summary[mc]=(double)dl;
        } else if (mctype[mc]==MC_string) {
            QString s;
            if (!f.Unpack(s)) throw UnpackError();
            summary[mc]=s;
        } else if (mctype[mc]==MC_datetime) {
            QDateTime dt;
            if (!f.Unpack(dt)) throw UnpackError();
            summary[mc]=dt;
        }

    }
    return true;
}

bool Session::StoreEvents(QString filename)
{
    BinaryFile f;
    f.Open(filename,BF_WRITE);

    f.Pack((quint32)magic); 			// Magic Number
    f.Pack((quint32)s_machine->id());  // Machine ID
    f.Pack((quint32)s_session);		// This session's ID
    f.Pack((quint16)1);				// File type 1 == Event
    f.Pack((quint16)0);				// File Version

    time_t starttime=s_first.toTime_t();
    time_t duration=s_last.toTime_t()-starttime;

    f.Pack(s_first);
    f.Pack((qint16)duration);

    f.Pack((qint16)events.size()); // Number of event categories


    map<MachineCode,vector<Event *> >::iterator i;
    vector<Event *>::iterator j;

    for (i=events.begin(); i!=events.end(); i++) {
        f.Pack((qint16)i->first); // MachineID
        f.Pack((qint16)i->second.size()); // count of events in this category
        j=i->second.begin();
        f.Pack((qint8)(*j)->fields()); 	// number of data fields in this event type
    }
    bool first;
    float tf;
    time_t last=0,eventtime,delta;

    for (i=events.begin(); i!=events.end(); i++) {
        first=true;
        for (j=i->second.begin(); j!=i->second.end(); j++) {
            eventtime=(*j)->time().toTime_t();
            if (first) {
                f.Pack((*j)->time());
                first=false;
            } else {
                delta=eventtime-last;
                if (delta>0xffff) {
                    qDebug("StoreEvent: Delta too big.. needed to use bigger value");
                    exit(1);
                }
                f.Pack((quint16)delta);
            }
            for (int k=0; k<(*j)->fields(); k++) {
                tf=(*(*j))[k];
                f.Pack((float)tf);
            }
            last=eventtime;
        }
    }
    f.Close();
    return true;
}
bool Session::LoadEvents(QString filename)
{
    BinaryFile f;
    if (!f.Open(filename,BF_READ)) {
        qDebug(("Couldn't open file"+filename).toLatin1());
        return false;
    }

    quint32 t32;
    quint16 t16;
    quint8 t8;
    qint16 i16;

//    qint16 sumsize;

    if (!f.Unpack(t32)) throw UnpackError();		// Magic Number
    if (t32!=magic) throw UnpackError();

    if (!f.Unpack(t32)) throw UnpackError();		// MachineID

    if (!f.Unpack(t32)) throw UnpackError();		// Sessionid;
    s_session=t32;

    if (!f.Unpack(t16)) throw UnpackError();		// File Type
    if (t16!=1) throw UnpackError(); 				//wrong file type

    if (!f.Unpack(t16)) throw UnpackError();		// File Version
    // dont give a crap yet..

    if (!f.Unpack(s_first)) throw UnpackError();	// Start time
    if (!f.Unpack(t16)) throw UnpackError();		// Duration // (16bit==Limited to 18 hours)

    s_last=s_first.addSecs(t16);
    s_hours=t16/3600.0;

    qint16 evsize;
    if (!f.Unpack(evsize)) throw UnpackError();	// Summary size (number of Machine Code lists)

    map<MachineCode,qint16> mcsize;
    map<MachineCode,qint16> mcfields;
    vector<MachineCode> mcorder;

    MachineCode mc;
    for (int i=0; i<evsize; i++) {
        if (!f.Unpack(t16)) throw UnpackError();  // MachineID
        mc=(MachineCode)t16;
        mcorder.push_back(mc);
        if (!f.Unpack(t16)) throw UnpackError();  // Count of this Event
        mcsize[mc]=t16;
        if (!f.Unpack(t8)) throw UnpackError();  // Count of this Event
        mcfields[mc]=t8;
    }

    for (int i=0; i<evsize; i++) {
        mc=mcorder[i];
        bool first=true;
        QDateTime d;
        float fl;
        events[mc].clear();
        for (int e=0; e<mcsize[mc]; e++) {
            if (first) {
                if (!f.Unpack(d)) throw UnpackError();  // Timestamp
                first=false;
            } else {
                if (!f.Unpack(t16)) throw UnpackError();  // Time Delta
                d=d.addSecs(t16);
            }
            EventDataType ED[max_number_event_fields];
            for (int c=0; c<mcfields[mc]; c++) {
                if (!f.Unpack(fl)); //throw UnpackError();  // Data Fields in float format
                ED[c]=fl;
            }
            Event *ev=new Event(d,mc,ED,mcfields[mc]);

            AddEvent(ev);
        }
    }
    return true;
}


bool Session::StoreWaveforms(QString filename)
{
    BinaryFile f;
    f.Open(filename,BF_WRITE);

    quint16 t16;

    f.Pack((quint32)magic); 			// Magic Number
    f.Pack((quint32)s_machine->id());  // Machine ID
    f.Pack((quint32)s_session);		// This session's ID
    f.Pack((quint16)2);				// File type 2 == Waveform
    f.Pack((quint16)0);				// File Version

    time_t starttime=s_first.toTime_t();
    time_t duration=s_last.toTime_t()-starttime;

    f.Pack(s_first);
    f.Pack((qint16)duration);

    f.Pack((qint16)waveforms.size());	// Number of different waveforms

    map<MachineCode,vector<Waveform *> >::iterator i;
    vector<Waveform *>::iterator j;
    for (i=waveforms.begin(); i!=waveforms.end(); i++) {
        f.Pack((qint16)i->first); 		// Machine Code
        t16=i->second.size();
        f.Pack(t16); 					// Number of (hopefully non-linear) waveform chunks

        for (j=i->second.begin(); j!=i->second.end(); j++) {

            Waveform &w=*(*j);
            f.Pack(w.start()); 			// Start time of first waveform chunk

            //qint32 samples;
            //double seconds;

            f.Pack((qint32)w.samples());					// Total number of samples
            f.Pack((float)w.duration());            // Total number of seconds
            f.Pack((qint16)w.min());
            f.Pack((qint16)w.max());
            f.Pack((qint8)sizeof(SampleFormat));  	// Bytes per sample
            f.Pack((qint8)0); // signed.. all samples for now are signed 16bit.
            //t8=0; // 0=signed, 1=unsigned, 2=float

            // followed by sample data.
            if (IsPlatformLittleEndian()) {
                f.Write((const char *)w.GetBuffer(),w.samples()*sizeof(SampleFormat));
            } else {
                for (int k=0; k<(*j)->samples(); k++) f.Pack((qint16)w[k]);
            }
        }
    }
    return true;
}


bool Session::LoadWaveforms(QString filename)
{
    BinaryFile f;
    if (!f.Open(filename,BF_READ)) {
        qDebug(("Couldn't open file "+filename).toLatin1());
        return false;
    }

    quint32 t32;
    quint16 t16;
    quint8 t8;

    if (!f.Unpack(t32)) throw UnpackError();		// Magic Number
    if (t32!=magic) throw UnpackError();

    if (!f.Unpack(t32)) throw UnpackError();		// MachineID

    if (!f.Unpack(t32)) throw UnpackError();		// Sessionid;
    s_session=t32;

    if (!f.Unpack(t16)) throw UnpackError();		// File Type
    if (t16!=2) throw UnpackError(); 				//wrong file type?

    if (!f.Unpack(t16)) throw UnpackError();		// File Version
    // dont give a crap yet..

    if (!f.Unpack(s_first)) throw UnpackError();	// Start time
    if (!f.Unpack(t16)) throw UnpackError();		// Duration // (16bit==Limited to 18 hours)

    s_last=s_first.addSecs(t16);
    s_hours=t16/3600.0;

    qint16 wvsize;
    if (!f.Unpack(wvsize)) throw UnpackError();	// Summary size (number of Machine Code lists)

    MachineCode mc;
    QDateTime date;
    float seconds;
    qint32 samples;
    int chunks;
    SampleFormat min,max;
    for (int i=0; i<wvsize; i++) {
        if (!f.Unpack(t16)) throw UnpackError();		// Machine Code
        mc=(MachineCode)t16;
        if (!f.Unpack(t16)) throw UnpackError();		// Machine Code
        chunks=t16;

        for (int i=0; i<chunks; i++) {

            if (!f.Unpack(date)) throw UnpackError();		// Waveform DateTime
            if (!f.Unpack(samples)) throw UnpackError();	// Number of samples
            if (!f.Unpack(seconds)) throw UnpackError();	// Duration in seconds
            if (!f.Unpack(min)) throw UnpackError();		// Min value
            if (!f.Unpack(max)) throw UnpackError();		// Min value
            if (!f.Unpack(t8)) throw UnpackError();			// sample size in bytes
            if (!f.Unpack(t8)) throw UnpackError();			// Number of samples

            SampleFormat *data=new SampleFormat [samples];

            for (int k=0; k<samples; k++) {
                if (!f.Unpack(t16)) throw UnpackError();			// Number of samples
                data[k]=t16;
            }
            Waveform *w=new Waveform(date,mc,data,samples,seconds,min,max);
            AddWaveform(w);
        }
    }
    return true;
}
