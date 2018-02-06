#ifndef ROOT_THcTrigger
#define ROOT_THcTrigger

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Trigger                                                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "TClonesArray.h"
#include "THaNonTrackingDetector.h"
#include "THcHitList.h"
#include "THcTriggerHit.h"

class THcHodoscope;
class THcTrigger : public THaNonTrackingDetector, public THcHitList {

 public:
  THcTrigger(const char* name, const char* description = "", THaApparatus* a = NULL);
  virtual ~THcTrigger();

  virtual void    Clear(Option_t* opt="");
  virtual Int_t   ReadDatabase(const TDatime& date);
  virtual Int_t   DefineVariables(EMode mode = kDefine);
  virtual Int_t   Decode(const THaEvData&);
  //virtual Int_t   CoarseProcess(TClonesArray& tracks);
  virtual EStatus Init(const TDatime& run_time);
  virtual void SetSpectName( const char* name);
  virtual void AddEvtType(int evtype);
  virtual void SetEvtType(int evtype);
  virtual Bool_t IsIgnoreType(Int_t evtype) const;
  virtual Bool_t HaveIgnoreList() const;  

  // Vector/TClonesArray length parameters
  static const Int_t MaxNumPmt      = 200;
  static const Int_t MaxNumAdcPulse = 4;

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
  
  /* Int_t     fDebugAdc; */
  /* Double_t* fWidth; */

  vector<Int_t>    eventtypes;
  vector<string>   fAdcNames;
  vector<string>   fTdcNames;
  vector<Double_t> fGoodAdcPed;
  vector<Double_t> fGoodAdcPulseInt;
  vector<Double_t> fGoodAdcPulseIntRaw;
  vector<Double_t> fGoodAdcPulseAmp;
  vector<Double_t> fGoodAdcPulseTime;
  vector<Double_t> fGoodAdcTdcDiffTime; 
 
  // 12 Gev FADC variables
  TClonesArray* frAdcPedRaw;
  TClonesArray* frAdcPulseIntRaw;
  TClonesArray* frAdcPulseAmpRaw;
  TClonesArray* frAdcPulseTimeRaw;
  TClonesArray* frAdcPed;
  TClonesArray* frAdcPulseInt;
  TClonesArray* frAdcPulseAmp;
  TClonesArray* frAdcPulseTime;
  TClonesArray* fAdcErrorFlag;

  /* void Setup(const char* name, const char* description); */
  THcHodoscope* fglHod;	// Hodoscope to get start time

  ClassDef(THcTrigger,0)        // Generic cherenkov class
};

#endif
