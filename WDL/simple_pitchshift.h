#ifndef _WDL_SIMPLEPITCHSHIFT_H_
#define _WDL_SIMPLEPITCHSHIFT_H_


#include "queue.h"

#ifndef WDL_SIMPLEPITCHSHIFT_SAMPLETYPE
#define WDL_SIMPLEPITCHSHIFT_SAMPLETYPE double
#endif


#ifdef WDL_SIMPLEPITCHSHIFT_PARENTCLASS
class WDL_SimplePitchShifter : public WDL_SIMPLEPITCHSHIFT_PARENTCLASS
#else
class WDL_SimplePitchShifter
#endif
{
public:
  WDL_SimplePitchShifter()
  {
    m_last_nch=1;
    m_srate=44100.0;
    m_last_tempo=1.0;
    m_last_shift=1.0;
    m_qual=0;

    Reset();
  }
  ~WDL_SimplePitchShifter() {   }

  void Reset()
  {
    m_hadinput=0;
    m_pspos=0.0;
    m_pswritepos=0;
    m_tempo_fracpos=0.0;
    m_queue.Clear();
    m_rsbuf.Resize(0);
  }

  bool IsReset()
  {
    return !m_queue.Available() && !m_hadinput;
  }



  void set_srate(double srate) { m_srate=srate; }
  void set_nch(int nch) { if (m_last_nch!=nch) { m_queue.Clear(); m_last_nch=nch; m_tempo_fracpos=0.0; } }
  void set_shift(double shift) { m_last_shift=shift; }
  void set_tempo(double tempo) { m_last_tempo=tempo; }
  void set_formant_shift(double shift)
  {
  }

  WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *GetBuffer(int size)
  {
    return m_inbuf.Resize(size*m_last_nch);
  }
  void BufferDone(int input_filled);
  void FlushSamples() {}

  static char *enumQual(int q);
 static bool GetSizes(int qv, int *ws, int *os);

  int GetSamples(int requested_output, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *buffer);


 void SetQualityParameter(int parm)
 {
   m_qual=parm;
 }


  void PitchShift(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *inputs, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *outputs, int nch, int length, double pitch, double srate, int ws_ms, int os_ms);

private:
  void PitchShiftBlock(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *inputs, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *outputs, int nch, int length, double pitch, int bsize, int olsize, double srate);


private:
  double m_pspos WDL_FIXALIGN;
  double m_tempo_fracpos;
  double m_srate,m_last_tempo,m_last_shift;

  WDL_TypedBuf<WDL_SIMPLEPITCHSHIFT_SAMPLETYPE> m_psbuf;
  WDL_Queue m_queue;
  WDL_TypedBuf<WDL_SIMPLEPITCHSHIFT_SAMPLETYPE> m_inbuf;
  WDL_TypedBuf<WDL_SIMPLEPITCHSHIFT_SAMPLETYPE> m_rsbuf;

  int m_pswritepos;
  int m_last_nch;
  int m_qual;
  int m_hadinput;

};


#ifdef WDL_SIMPLEPITCHSHIFT_IMPLEMENT
void WDL_SimplePitchShifter::BufferDone(int input_filled)
{
  // perform pitch shifting
  if (input_filled>0)
  {
    m_hadinput=1;
    int ws,os;
    GetSizes(m_qual,&ws,&os);
    if (fabs(m_last_tempo-1.0)<0.0000000001)
    {
      PitchShift(m_inbuf.Get(),(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *)m_queue.Add(NULL,input_filled*m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)),m_last_nch,input_filled,m_last_shift,m_srate,ws,os);
    }
    else
    {
      int needclear=0;
      if (m_rsbuf.GetSize()<m_last_nch) needclear=1;
      WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *bufi=m_rsbuf.Resize((input_filled+1)*m_last_nch,false);
      if (needclear)
        memset(bufi,0,m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE));
      double tempo=m_last_tempo;
      double itempo=1.0/tempo;
      PitchShift(m_inbuf.Get(),bufi+m_last_nch,m_last_nch,input_filled,m_last_shift*itempo,m_srate,ws,os);
//        memcpy(bufi+m_last_nch,m_inbuf.Get(),input_filled*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)*m_last_nch);

      double fp=m_tempo_fracpos;
      int outlen = (int) (input_filled*itempo-fp);
      WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *bufo = (WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *)m_queue.Add(NULL,outlen*m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE));
      // resample bufi to bufo
      int i,nch=m_last_nch;
      for (i = 0; i < outlen; i ++)
      {
        double rdpos=floor(fp);
        int idx=((int)rdpos);
        if (idx>=input_filled) idx=input_filled-1;
        rdpos = (fp-rdpos);
        int a;
        idx*=nch;
        for (a = 0; a < nch; a ++)
        {
          *bufo++ = bufi[idx+a]*(1.0-rdpos)+bufi[idx+nch+a]*rdpos;
        }
        fp += tempo;
      }
        
      memcpy(bufi,bufi+m_last_nch*input_filled,m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)); // save last sample for interpolation later
      //
      m_tempo_fracpos=fp-floor(fp);
    }
  }    
}

char *WDL_SimplePitchShifter::enumQual(int q)
{
  int ws,os;
  if (!GetSizes(q,&ws,&os)) return NULL;
  static char buf[128];
  sprintf(buf,"%dms window, %dms fade",ws,os);
  return buf;
}

bool WDL_SimplePitchShifter::GetSizes(int qv, int *ws, int *os)
{
  int windows[]={50,75,100,150,225,300,40,30,20,10,5,3};
  int divs[]={2,3,5,7};

  int wd=qv/(sizeof(divs)/sizeof(divs[0]));
  if (wd >= sizeof(windows)/sizeof(windows[0])) wd=-1;

  *ws=windows[wd>=0?wd:0];
  *os = *ws / divs[qv%(sizeof(divs)/sizeof(divs[0]))];
  if (*os<1) *os=1;

  return wd>=0;
}

int WDL_SimplePitchShifter::GetSamples(int requested_output, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *buffer)
{
  if (!m_last_nch||requested_output<1) return 0;

  int l=m_queue.Available()/sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)/m_last_nch;
  if (requested_output>l) requested_output=l;
  int sz=requested_output*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)*m_last_nch;
  memcpy(buffer,m_queue.Get(),sz);
  m_queue.Advance(sz);
  m_queue.Compact();
  return requested_output;
}

void WDL_SimplePitchShifter::PitchShift(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *inputs, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *outputs, int nch, int length, double pitch, double srate, int ws_ms, int os_ms)
{
  int bsize=(int) (ws_ms * 0.001 * srate);
  if (bsize<16) bsize=16;
  else if (bsize>128*1024)bsize=128*1024;

  int olsize=(int) (os_ms * 0.001 * srate);
  if (olsize > bsize/2) olsize=bsize/2;
  if (olsize<1)olsize=1;
  if (m_psbuf.GetSize() != bsize*nch)
  {
    memset(m_psbuf.Resize(bsize*nch,false),0,sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)*bsize*nch);
    m_pspos=(double) (bsize/2);
    m_pswritepos=0;
  }

  PitchShiftBlock(inputs,outputs,nch,length,pitch,bsize,olsize,srate);
}

void WDL_SimplePitchShifter::PitchShiftBlock(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *inputs, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *outputs, int nch, int length, double pitch, int bsize, int olsize, double srate)
{
  double iolsize=1.0/olsize;

  WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *psbuf=m_psbuf.Get();

  double dpi=pitch;

  double pspos=m_pspos;
  int writepos=m_pswritepos;
  int writeposnch = writepos*nch;
  int bsizench = bsize*nch;
  int olsizench = olsize*nch;

  int i=length;
  while (i--)
  {
    int ipos1=(int)pspos;
    double frac0=pspos-ipos1;

    ipos1*=nch;

    int ipos2=ipos1+nch;
    
    if (ipos2 >= bsizench) ipos2=0;

    int a;
    for(a=0;a<nch;a++) outputs[a]=(psbuf[ipos1+a]*(1-frac0)+psbuf[ipos2+a]*frac0);      

    double tv=pspos;
    if (dpi >= 1.0)
    {
      if (tv > writepos) tv-=bsize;

      if (tv >= writepos-olsize && tv < writepos)
      {
        double tfrac=(writepos-tv)*iolsize;
        int tmp=ipos1+olsizench;
        if (tmp>=bsizench) tmp-=bsizench;
        int tmp2=tmp+nch;
        if (tmp2 >= bsizench) tmp2=0;

        for(a=0;a<nch;a++) outputs[a]= outputs[a]*tfrac + (1-tfrac)*(psbuf[tmp+a]*(1-frac0)+psbuf[tmp2+a]*frac0);

        if (tv+pitch > writepos) pspos+=olsize;
      }

    }
    else
    {
      if (tv<writepos) tv+=bsize;

      if (tv >= writepos && tv < writepos+olsize)
      {
        double tfrac=(tv-writepos)*iolsize;
        int tmp=ipos1+olsizench;
        if (tmp>=bsizench) tmp -= bsizench;
        int tmp2= tmp+nch;
        if (tmp2 >= bsizench) tmp2=0;
        for(a=0;a<nch;a++) outputs[a] = outputs[a]*tfrac + (1-tfrac)*(psbuf[tmp+a]*(1-frac0)+psbuf[tmp2+a]*frac0);
        if (tv+pitch < writepos+1) pspos += olsize;
      }
    }


    if ((pspos+=pitch) >= bsize) pspos -= bsize;


    memcpy(psbuf+writeposnch,inputs,nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE));

    writeposnch += nch;
    if (++writepos >= bsize) writeposnch = writepos=0;

    inputs += nch;
    outputs += nch;
  } // sample loop
  m_pspos=pspos;
  m_pswritepos=writepos;
}

#endif

#endif