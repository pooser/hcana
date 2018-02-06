/** \class THcTrigger
    \ingroup Detectors

    \brief Class for trigger apparatus related objects

*/

#include "THcTrigger.h"
#include "THcHodoscope.h"
#include "TClonesArray.h"
#include "THcSignalHit.h"
#include "THaEvData.h"
#include "THaDetMap.h"
#include "THcDetectorMap.h"
#include "THcGlobals.h"
#include "THaCutList.h"
#include "THcParmList.h"
#include "THcHitList.h"
#include "THaApparatus.h"
#include "VarDef.h"
#include "VarType.h"
#include "THaTrack.h"
#include "TClonesArray.h"
#include "TMath.h"
#include "THaTrackProj.h"
#include "THcHallCSpectrometer.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <iomanip>

using namespace std;

using std::cout;
using std::cin;
using std::endl;
using std::setw;
using std::setprecision;

//_____________________________________________________________________________
THcTrigger::THcTrigger(const char* name, const char* description, THaApparatus* apparatus) :
  THaNonTrackingDetector(name,description,apparatus)
{
  // Normal constructor with name and description
  frAdcPedRaw       = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);
  frAdcPulseIntRaw  = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);
  frAdcPulseAmpRaw  = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);
  frAdcPulseTimeRaw = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);
  frAdcPed          = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);
  frAdcPulseInt     = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);
  frAdcPulseAmp     = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);
  frAdcPulseTime    = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);
  fAdcErrorFlag     = new TClonesArray("THcSignalHit", MaxNumPmt*MaxNumAdcPulse);

  fGoodAdcPed         = vector<Double_t> (MaxNumPmt, 0.0);
  fGoodAdcPulseInt    = vector<Double_t> (MaxNumPmt, 0.0);
  fGoodAdcPulseIntRaw = vector<Double_t> (MaxNumPmt, 0.0);
  fGoodAdcPulseAmp    = vector<Double_t> (MaxNumPmt, 0.0);
  fGoodAdcPulseTime   = vector<Double_t> (MaxNumPmt, 0.0);
  fGoodAdcTdcDiffTime = vector<Double_t> (MaxNumPmt, 0.0);
}

THcTrigger::THcTrigger() : THaNonTrackingDetector()
{
  // Constructor
  frAdcPedRaw       = NULL;
  frAdcPulseIntRaw  = NULL;
  frAdcPulseAmpRaw  = NULL;
  frAdcPulseTimeRaw = NULL;
  frAdcPed          = NULL;
  frAdcPulseInt     = NULL;
  frAdcPulseAmp     = NULL;
  frAdcPulseTime    = NULL;
  fAdcErrorFlag     = NULL;
}

THcTrigger::~THcTrigger()
{
  // Destructor
  delete frAdcPedRaw;       frAdcPedRaw       = NULL;
  delete frAdcPulseIntRaw;  frAdcPulseIntRaw  = NULL;
  delete frAdcPulseAmpRaw;  frAdcPulseAmpRaw  = NULL;
  delete frAdcPulseTimeRaw; frAdcPulseTimeRaw = NULL;
  delete frAdcPed;          frAdcPed          = NULL;
  delete frAdcPulseInt;     frAdcPulseInt     = NULL;
  delete frAdcPulseAmp;     frAdcPulseAmp     = NULL;
  delete frAdcPulseTime;    frAdcPulseTime    = NULL;
  delete fAdcErrorFlag;     fAdcErrorFlag     = NULL;
}

THaAnalysisObject::EStatus THcTrigger::Init( const TDatime& date )
{
 
  // Call `Setup` before everything else.
  Setup(GetName(), GetTitle());

  // cout << "THcTrigger::Init for: " << GetName() << endl;

  string EngineDID = string(GetApparatus()->GetName()).substr(0, 1) + GetName();
  std::transform(EngineDID.begin(), EngineDID.end(), EngineDID.begin(), ::toupper);
  if(gHcDetectorMap->FillMap(fDetMap, EngineDID.c_str()) < 0) {
    static const char* const here = "Init()";
    Error(Here(here), "Error filling detectormap for %s.", EngineDID.c_str());
    return kInitError;
  }

  // Should probably put this in ReadDatabase as we will know the
  // maximum number of hits after setting up the detector map
  InitHitList(fDetMap, "THcTriggerHit", fDetMap->GetTotNumChan()+1);

  EStatus status;
  if((status = THaNonTrackingDetector::Init( date )))
    return fStatus=status;

  THcHallCSpectrometer *app = dynamic_cast <THcHallCSpectrometer*> (GetApparatus());
  if(  !app || !(fglHod = dynamic_cast <THcHodoscope*> (app->GetDetector("hod")))) {
    static const char* const here = "ReadDatabase()";
    Warning(Here(here),"Hodoscope \"%s\" not found. ","hod");
  }

  fPresentP = 0;
  THaVar* vpresent = gHaVars->Find(Form("%s.present",GetApparatus()->GetName()));
  if(vpresent)
    fPresentP = (Bool_t *) vpresent->GetValuePointer();
  
  fStatus = kOK;
  return fStatus;

}

Int_t THcTrigger::ReadDatabase(const TDatime& date) 
{
  string adcNames, tdcNames;

  DBRequest list[] = {
    {"_numAdc",         &fNumAdc,       kInt},           // Number of ADC channels.
    {"_numTdc",         &fNumTdc,       kInt},           // Number of TDC channels.
    {"_adcNames",       &adcNames,      kString},        // Names of ADC channels.
    {"_tdcNames",       &tdcNames,      kString},        // Names of TDC channels.
    {"_tdcoffset",      &fTdcOffset,    kDouble, 0, 1},  // Offset of tdc channels
    {"_adc_tdc_offset", &fAdcTdcOffset, kDouble, 0, 1},  // Offset of Adc-Tdc time (ns)
    {"_tdcchanperns",   &fTdcChanPerNs, kDouble, 0, 1},  // Convert channesl to ns
    {0}
  };

  fTdcOffset    = 300.;
  fAdcTdcOffset = 200.;
  fTdcChanPerNs = 0.1;
  gHcParms->LoadParmValues(list, fKwPrefix.c_str());
  // Split the names to vector<string>.
  fAdcNames = vsplit(adcNames);
  fTdcNames = vsplit(tdcNames);

  return kOK;
}

Int_t THcTrigger::DefineVariables(THaAnalysisObject::EMode mode) {

  if (mode == kDefine && fIsSetup) return kOK;
  fIsSetup = (mode == kDefine);

  vector<RVarDef> vars;

  // Push the variable names for ADC channels.
  vector<TString> adcPedRawTitle(fNumAdc),       adcPedRawVar(fNumAdc);
  vector<TString> adcPulseIntRawTitle(fNumAdc),  adcPulseIntRawVar(fNumAdc);
  vector<TString> adcPulseAmpRawTitle(fNumAdc),  adcPulseAmpRawVar(fNumAdc);
  vector<TString> adcPulseTimeRawTitle(fNumAdc), adcPulseTimeRawVar(fNumAdc);
  vector<TString> adcPulseTimeTitle(fNumAdc),    adcPulseTimeVar(fNumAdc);
  vector<TString> adcPedTitle(fNumAdc),          adcPedVar(fNumAdc);
  vector<TString> adcPulseIntTitle(fNumAdc),     adcPulseIntVar(fNumAdc);
  vector<TString> adcPulseAmpTitle(fNumAdc),     adcPulseAmpVar(fNumAdc);
  vector<TString> adcMultiplicityTitle(fNumAdc), adcMultiplicityVar(fNumAdc);

  for (int i = 0; i < fNumAdc; i++) {
    adcPedRawTitle.at(i) = fAdcNames.at(i) + "_adcPedRaw";
    adcPedRawVar.at(i) = TString::Format("fAdcPedRaw[%d]", i);
    RVarDef entry1 {
      adcPedRawTitle.at(i).Data(), 
      adcPedRawTitle.at(i).Data(),   
      adcPedRawVar.at(i).Data()
    };
    vars.push_back(entry1);

    adcPulseIntRawTitle.at(i) = fAdcNames.at(i) + "_adcPulseIntRaw";
    adcPulseIntRawVar.at(i) = TString::Format("fAdcPulseIntRaw[%d]", i);
    RVarDef entry2 {
      adcPulseIntRawTitle.at(i).Data(),
      adcPulseIntRawTitle.at(i).Data(),
      adcPulseIntRawVar.at(i).Data()
    };
    vars.push_back(entry2);

    adcPulseAmpRawTitle.at(i) = fAdcNames.at(i) + "_adcPulseAmpRaw";
    adcPulseAmpRawVar.at(i) = TString::Format("fAdcPulseAmpRaw[%d]", i);
    RVarDef entry3 {
      adcPulseAmpRawTitle.at(i).Data(),
      adcPulseAmpRawTitle.at(i).Data(),
      adcPulseAmpRawVar.at(i).Data()
    };
    vars.push_back(entry3);

    adcPulseTimeRawTitle.at(i) = fAdcNames.at(i) + "_adcPulseTimeRaw";
    adcPulseTimeRawVar.at(i) = TString::Format("fAdcPulseTimeRaw[%d]", i);
    RVarDef entry4 {
      adcPulseTimeRawTitle.at(i).Data(),
      adcPulseTimeRawTitle.at(i).Data(),
      adcPulseTimeRawVar.at(i).Data()
    };
    vars.push_back(entry4);

    adcPedTitle.at(i) = fAdcNames.at(i) + "_adcPed";
    adcPedVar.at(i) = TString::Format("fAdcPed[%d]", i);
    RVarDef entry5 {
      adcPedTitle.at(i).Data(),
      adcPedTitle.at(i).Data(),
      adcPedVar.at(i).Data()
    };
    vars.push_back(entry5);

    adcPulseIntTitle.at(i) = fAdcNames.at(i) + "_adcPulseInt";
    adcPulseIntVar.at(i) = TString::Format("fAdcPulseInt[%d]", i);
    RVarDef entry6 {
      adcPulseIntTitle.at(i).Data(),
      adcPulseIntTitle.at(i).Data(),
      adcPulseIntVar.at(i).Data()
    };
    vars.push_back(entry6);

    adcPulseAmpTitle.at(i) = fAdcNames.at(i) + "_adcPulseAmp";
    adcPulseAmpVar.at(i) = TString::Format("fAdcPulseAmp[%d]", i);
    RVarDef entry7 {
      adcPulseAmpTitle.at(i).Data(),
      adcPulseAmpTitle.at(i).Data(),
      adcPulseAmpVar.at(i).Data()
    };
    vars.push_back(entry7);

    adcMultiplicityTitle.at(i) = fAdcNames.at(i) + "_adcMultiplicity";
    adcMultiplicityVar.at(i) = TString::Format("fAdcMultiplicity[%d]", i);
    RVarDef entry8 {
      adcMultiplicityTitle.at(i).Data(),
      adcMultiplicityTitle.at(i).Data(),
      adcMultiplicityVar.at(i).Data()
    };
    vars.push_back(entry8);
  
    adcPulseTimeTitle.at(i) = fAdcNames.at(i) + "_adcPulseTime";
    adcPulseTimeVar.at(i) = TString::Format("fAdcPulseTime[%d]", i);
    RVarDef entry9 {
      adcPulseTimeTitle.at(i).Data(),
      adcPulseTimeTitle.at(i).Data(),
      adcPulseTimeVar.at(i).Data()
    };
    vars.push_back(entry9);
  } // fNumAdc loop

  // Push the variable names for TDC channels.
  vector<TString> tdcTimeRawTitle(fNumTdc), tdcTimeRawVar(fNumTdc);
  vector<TString> tdcTimeTitle(fNumTdc), tdcTimeVar(fNumTdc);
  vector<TString> tdcMultiplicityTitle(fNumTdc), tdcMultiplicityVar(fNumTdc);

  for (int i = 0; i < fNumTdc; i++) {
    tdcTimeRawTitle.at(i) = fTdcNames.at(i) + "_tdcTimeRaw";
    tdcTimeRawVar.at(i) = TString::Format("fTdcTimeRaw[%d]", i);
    RVarDef entry1 {
      tdcTimeRawTitle.at(i).Data(),
      tdcTimeRawTitle.at(i).Data(),
      tdcTimeRawVar.at(i).Data()
    };
    vars.push_back(entry1);

    tdcTimeTitle.at(i) = fTdcNames.at(i) + "_tdcTime";
    tdcTimeVar.at(i) = TString::Format("fTdcTime[%d]", i);
    RVarDef entry2 {
      tdcTimeTitle.at(i).Data(),
      tdcTimeTitle.at(i).Data(),
      tdcTimeVar.at(i).Data()
    };
    vars.push_back(entry2);

    tdcMultiplicityTitle.at(i) = fTdcNames.at(i) + "_tdcMultiplicity";
    tdcMultiplicityVar.at(i) = TString::Format("fTdcMultiplicity[%d]", i);
    RVarDef entry3 {
      tdcMultiplicityTitle.at(i).Data(),
      tdcMultiplicityTitle.at(i).Data(),
      tdcMultiplicityVar.at(i).Data()
    };
    vars.push_back(entry3);
  } // fNumTdc loop

  RVarDef end {0};
  vars.push_back(end);
  return DefineVarsFromList(vars.data(), mode);
}

inline
void THcTrigger::Clear(Option_t* opt)
{
  // Clear the hit lists
  frAdcPedRaw->Clear();
  frAdcPulseIntRaw->Clear();
  frAdcPulseAmpRaw->Clear();
  frAdcPulseTimeRaw->Clear();

  frAdcPed->Clear();
  frAdcPulseInt->Clear();
  frAdcPulseAmp->Clear();
  frAdcPulseTime->Clear();
  fAdcErrorFlag->Clear();

  // for (UInt_t ielem = 0; ielem < fNumAdcHits.size(); ielem++)
  //   fNumAdcHits.at(ielem) = 0;
  // for (UInt_t ielem = 0; ielem < fNumGoodAdcHits.size(); ielem++)
  //   fNumGoodAdcHits.at(ielem) = 0;
  for (UInt_t ielem = 0; ielem < fGoodAdcPed.size(); ielem++) {
    fGoodAdcPed.at(ielem)         = 0.0;
    fGoodAdcPulseInt.at(ielem)    = 0.0;
    fGoodAdcPulseIntRaw.at(ielem) = 0.0;
    fGoodAdcPulseAmp.at(ielem)    = 0.0;
    fGoodAdcPulseTime.at(ielem)   = kBig;
    fGoodAdcTdcDiffTime.at(ielem) = kBig;
  }

}

Int_t THcTrigger::Decode(const THaEvData& evData) {
  // Decode raw data for this event.
  Bool_t present = kTRUE;	// Don't suppress reference time warnings
  if(HaveIgnoreList()) {
    if(IsIgnoreType(evData.GetEvType())) {
      present = kFALSE;
    }
  } else if(fPresentP) {		// if this spectrometer not part of trigger
    present = *fPresentP;
  }
  Int_t numHits = DecodeToHitList(evData, !present);

  // Process each hit and fill variables.
  Int_t iHit     = 0;
  Int_t nAdcHits = 0;

  while (iHit < numHits) {

    THcTriggerHit* hit       = dynamic_cast <THcTriggerHit*> (fRawHitList->At(iHit));
    Int_t          npmt      = hit->fCounter;
    
    if (hit->fPlane == 1) {

      THcRawAdcHit& rawAdcHit = hit->GetRawAdcHitPos();

      for (UInt_t thit = 0; thit < rawAdcHit.GetNPulses(); thit++) {

      ((THcSignalHit*) frAdcPedRaw->ConstructedAt(nAdcHits))->Set(npmt, rawAdcHit.GetPedRaw());
      ((THcSignalHit*) frAdcPed->ConstructedAt(nAdcHits))->Set(npmt, rawAdcHit.GetPed());

      ((THcSignalHit*) frAdcPulseIntRaw->ConstructedAt(nAdcHits))->Set(npmt, rawAdcHit.GetPulseIntRaw(thit));
      ((THcSignalHit*) frAdcPulseInt->ConstructedAt(nAdcHits))->Set(npmt, rawAdcHit.GetPulseInt(thit));

      ((THcSignalHit*) frAdcPulseAmpRaw->ConstructedAt(nAdcHits))->Set(npmt, rawAdcHit.GetPulseAmpRaw(thit));
      ((THcSignalHit*) frAdcPulseAmp->ConstructedAt(nAdcHits))->Set(npmt, rawAdcHit.GetPulseAmp(thit));

      ((THcSignalHit*) frAdcPulseTimeRaw->ConstructedAt(nAdcHits))->Set(npmt, rawAdcHit.GetPulseTimeRaw(thit));
      ((THcSignalHit*) frAdcPulseTime->ConstructedAt(nAdcHits))->Set(npmt, rawAdcHit.GetPulseTime(thit)+fAdcTdcOffset);

      if (rawAdcHit.GetPulseAmpRaw(thit) > 0)  ((THcSignalHit*) fAdcErrorFlag->ConstructedAt(nAdcHits))->Set(npmt, 0);
      if (rawAdcHit.GetPulseAmpRaw(thit) <= 0) ((THcSignalHit*) fAdcErrorFlag->ConstructedAt(nAdcHits))->Set(npmt, 1);

  //     // fAdcPedRaw[cnt] = rawAdcHit.GetPedRaw();
  //     // fAdcPulseIntRaw[cnt] = rawAdcHit.GetPulseIntRaw();
  //     // fAdcPulseAmpRaw[cnt] = rawAdcHit.GetPulseAmpRaw();
  //     // fAdcPulseTimeRaw[cnt] = rawAdcHit.GetPulseTimeRaw();
  //     // fAdcPulseTime[cnt] = rawAdcHit.GetPulseTime()+fAdcTdcOffset;

  //     // fAdcPed[cnt] = rawAdcHit.GetPed();
  //     // fAdcPulseInt[cnt] = rawAdcHit.GetPulseInt();
  //     // fAdcPulseAmp[cnt] = rawAdcHit.GetPulseAmp();

  //     // fAdcMultiplicity[cnt] = rawAdcHit.GetNPulses();

      nAdcHits++;
      }  // Adc pulses

    }  // Adc hits

  //   // else if (hit->fPlane == 2) {

  //   //   THcRawTdcHit rawTdcHit = hit->GetRawTdcHit();

  //   //   fTdcTimeRaw[cnt] = rawTdcHit.GetTimeRaw();
  //   //   fTdcTime[cnt] = rawTdcHit.GetTime()*fTdcChanperNS+fTdcOffset;

  //   //   fTdcMultiplicity[cnt] = rawTdcHit.GetNHits();
  //   // } // Tdc hits
  //   // else {
  //   //   throw std::out_of_range(
  //   //     "`THcTrigDet::Decode`: only planes `1` and `2` available!"
  //   //   );
  //   // }
  //   iHit++;
  } // while hits
  return 0;
}

// //_____________________________________________________________________________
// Int_t THcTrigger::CoarseProcess( TClonesArray&  )
// {
//   Double_t StartTime = 0.0;
//   if( fglHod ) StartTime = fglHod->GetStartTime();

//   // Loop over the elements in the TClonesArray
//   for(Int_t ielem = 0; ielem < frAdcPulseInt->GetEntries(); ielem++) {

//     Int_t    npmt           = ((THcSignalHit*) frAdcPulseInt->ConstructedAt(ielem))->GetPaddleNumber() - 1;
//     Double_t pulsePed       = ((THcSignalHit*) frAdcPed->ConstructedAt(ielem))->GetData();
//     Double_t pulseInt       = ((THcSignalHit*) frAdcPulseInt->ConstructedAt(ielem))->GetData();
//     Double_t pulseIntRaw    = ((THcSignalHit*) frAdcPulseIntRaw->ConstructedAt(ielem))->GetData();
//     Double_t pulseAmp       = ((THcSignalHit*) frAdcPulseAmp->ConstructedAt(ielem))->GetData();
//     Double_t pulseTime      = ((THcSignalHit*) frAdcPulseTime->ConstructedAt(ielem))->GetData();
//     Double_t adctdcdiffTime = StartTime-pulseTime;
//     Bool_t   errorFlag      = ((THcSignalHit*) fAdcErrorFlag->ConstructedAt(ielem))->GetData();
//     Bool_t   pulseTimeCut   = adctdcdiffTime > fAdcTimeWindowMin && adctdcdiffTime < fAdcTimeWindowMax;

//     // By default, the last hit within the timing cut will be considered "good"
//     if (!errorFlag && pulseTimeCut) {
//       fGoodAdcPed.at(npmt)         = pulsePed;
//       fGoodAdcPulseInt.at(npmt)    = pulseInt;
//       fGoodAdcPulseIntRaw.at(npmt) = pulseIntRaw;
//       fGoodAdcPulseAmp.at(npmt)    = pulseAmp;
//       fGoodAdcPulseTime.at(npmt)   = pulseTime;
//       fGoodAdcTdcDiffTime.at(npmt) = adctdcdiffTime;

//       fNpe.at(npmt) = fGain[npmt]*fGoodAdcPulseInt.at(npmt);
//       fNpeSum += fNpe.at(npmt);

//       fTotNumGoodAdcHits++;
//       fNumGoodAdcHits.at(npmt) = npmt + 1;
//     }
//   }
//   return 0;
// }

void THcTrigger::Setup(const char* name, const char* description) {
  // Prefix for parameters in `param` file.
  string kwPrefix = string(GetApparatus()->GetName()) + "_" + name;
  transform(kwPrefix.begin(), kwPrefix.end(), kwPrefix.begin(), ::tolower);
  fKwPrefix = kwPrefix;
}

void THcTrigger::SetSpectName( const char* name) {
  fSpectName = name;
}

void THcTrigger::AddEvtType(int evtype) {
  eventtypes.push_back(evtype);
}
  
void THcTrigger::SetEvtType(int evtype) {
  eventtypes.clear();
  AddEvtType(evtype);
}

Bool_t THcTrigger::IsIgnoreType(Int_t evtype) const {
  for (UInt_t i=0; i < eventtypes.size(); i++) 
    if (evtype == eventtypes[i]) return kTRUE;
  return kFALSE; 
}

Bool_t THcTrigger::HaveIgnoreList() const {
  return( (eventtypes.size()>0) ? kTRUE : kFALSE);
}

ClassImp(THcTrigger)
////////////////////////////////////////////////////////////////////////////////
