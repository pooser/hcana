#ifndef ROOT_THcTrigger
#define ROOT_THcTrigger

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Trigger                                                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "TClonesArray.h"
#include "THaNonTrackingDetector.h"
#include "THaDetector.h"
#include "THcHitList.h"
#include "THcTriggerHit.h"

class THcHodoscope;
class TDatime;
class THaApparatus;
class THaEvData;

class THcTrigger : public THaNonTrackingDetector, public THcHitList {
 public:
  THcTrigger(const char* name, const char* description = "", THaApparatus* a = NULL);
  virtual ~THcTrigger();

  virtual EStatus Init(const TDatime& run_time);

  virtual void    Clear(Option_t* opt="");
  virtual Int_t   ReadDatabase(const TDatime& date);
  virtual Int_t   DefineVariables(EMode mode = kDefine);
  virtual Int_t   Decode(const THaEvData&);
  virtual Int_t   CoarseProcess(TClonesArray& tracks);
  virtual Int_t   FineProcess(TClonesArray& tracks);

  virtual void   SetSpectName( const char* name);
  virtual void   AddEvtType(int evtype);
  virtual void   SetEvtType(int evtype);
  virtual Bool_t IsIgnoreType(Int_t evtype) const;
  virtual Bool_t HaveIgnoreList() const;  

  // Vector/TClonesArray length parameters
  static const Int_t MaxNumPmt      = 200;
  static const Int_t MaxNumAdcPulse = 4;
  static const Int_t MaxNumTdcHits  = 128;

  THcTrigger();  // for ROOT I/O
 protected:
  void Setup(const char* name, const char* description);

  Int_t    fNumAdc;
  Int_t    fNumTdc;
  string   fKwPrefix;    
  Bool_t*  fPresentP;
  TString  fSpectName;
  Double_t fAdcOffset;
  Double_t fTdcOffset;
  Double_t fAdcTdcOffset;
  Double_t fTdcChanPerNs;
  Double_t fAdcTimeWindowMin;
  Double_t fAdcTimeWindowMax;
  
  // Various data objects
  vector<Int_t>    eventtypes;
  vector<string>   fAdcNames;
  vector<string>   fTdcNames;
 
  // 12 Gev FADC variables
  // Raw ADC variables
  vector<TClonesArray*> fAdcPulsePedRaw;
  vector<TClonesArray*> fAdcPulseIntRaw;
  vector<TClonesArray*> fAdcPulseAmpRaw;
  vector<TClonesArray*> fAdcPulseTimeRaw;
  // ADC veriables
  vector<TClonesArray*> fAdcPulsePed;
  vector<TClonesArray*> fAdcPulseInt;
  vector<TClonesArray*> fAdcPulseAmp;
  vector<TClonesArray*> fAdcPulseTime;
  vector<TClonesArray*> fAdcErrorFlag;
  vector<TClonesArray*> fAdcMultiplicity;
  // 12 GeV TDC variables
  vector<TClonesArray*> fTdcTimeRaw;
  vector<TClonesArray*> fTdcTime;
  vector<TClonesArray*> fTdcMultiplicity;

  // Good ADC & TDC variables
  vector<Double_t> fGoodAdcPed;
  vector<Double_t> fGoodAdcPulseInt;
  vector<Double_t> fGoodAdcPulseAmp;
  vector<Double_t> fGoodAdcPulseTime;
  vector<Double_t> fGoodAdcTdcDiffTime; 
  vector<Double_t> fGoodTdcTime;

  /* void Setup(const char* name, const char* description); */
  THcHodoscope* fglHod;	// Hodoscope to get start time

  ClassDef(THcTrigger,0)        // Generic cherenkov class
};

#endif
