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
  // Constructor
}

THcTrigger::THcTrigger() : THaNonTrackingDetector()
{
  // Constructor
}

THcTrigger::~THcTrigger()
{
  // Destructor
}

void THcTrigger::Setup(const char* name, const char* description) {
  // Prefix for parameters in `param` file.
  string kwPrefix = string(GetApparatus()->GetName()) + "_" + name;
  transform(kwPrefix.begin(), kwPrefix.end(), kwPrefix.begin(), ::tolower);
  fKwPrefix = kwPrefix;
  // Obtain values in database
  DBRequest numChanList[] = {
    {"_numAdc", &fNumAdc, kInt}, // Number of ADC channels.
    {"_numTdc", &fNumTdc, kInt}, // Number of TDC channels.
    {0}
  };
  // Load parameter values
  gHcParms->LoadParmValues((DBRequest*)&numChanList, kwPrefix.c_str());
  cout << "The trigger detector has fNumAdc = " << fNumAdc << " & fNumTdc = " << fNumTdc << " defined " << endl;
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

  // Initialize vector<TClonesArray*> objects
  // ADC objects
  for (int i = 0; i < fNumAdc; i++) {
    // Raw ADC objects
    fAdcPulsePedRaw.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    fAdcPulseIntRaw.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    fAdcPulseAmpRaw.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    fAdcPulseTimeRaw.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    // ADC objects
    fAdcPulsePed.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    fAdcPulseInt.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    fAdcPulseAmp.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    fAdcPulseTime.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    fAdcErrorFlag.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
    fAdcMultiplicity.push_back(new TClonesArray("THcSignalHit", MaxNumAdcPulse));
  } // fNumAdc loop
  // TDC objects
  for (int i = 0; i < fNumTdc; i++) {
    fTdcTimeRaw.push_back(new TClonesArray("THcSignalHit", MaxNumTdcHits));
    fTdcTime.push_back(new TClonesArray("THcSignalHit", MaxNumTdcHits));
    fTdcMultiplicity.push_back(new TClonesArray("THcSignalHit", MaxNumTdcHits));
  }
  
  fStatus = kOK;
  return fStatus;

}

Int_t THcTrigger::ReadDatabase(const TDatime& date) 
{
  string adcNames, tdcNames;

  DBRequest list[] = {
    {"_adcNames",       &adcNames,      kString},        // Names of ADC channels.
    {"_tdcNames",       &tdcNames,      kString},        // Names of TDC channels.
    {"_tdcoffset",      &fTdcOffset,    kDouble, 0, 1},  // Offset of tdc channels
    {"_adc_tdc_offset", &fAdcTdcOffset, kDouble, 0, 1},  // Offset of Adc-Tdc time (ns)
    {"_tdcchanperns",   &fTdcChanPerNs, kDouble, 0, 1},  // Convert channesl to ns
    {0}
  };
  // Hard code DB values in case not derfined in parameter file
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
  vector<TString> adcPulsePedRawTitle(fNumAdc),  adcPulsePedRawVar(fNumAdc);
  vector<TString> adcPulseIntRawTitle(fNumAdc),  adcPulseIntRawVar(fNumAdc);
  vector<TString> adcPulseAmpRawTitle(fNumAdc),  adcPulseAmpRawVar(fNumAdc);
  vector<TString> adcPulseTimeRawTitle(fNumAdc), adcPulseTimeRawVar(fNumAdc);
  vector<TString> adcPulseTimeTitle(fNumAdc),    adcPulseTimeVar(fNumAdc);
  vector<TString> adcPulsePedTitle(fNumAdc),     adcPulsePedVar(fNumAdc);
  vector<TString> adcPulseIntTitle(fNumAdc),     adcPulseIntVar(fNumAdc);
  vector<TString> adcPulseAmpTitle(fNumAdc),     adcPulseAmpVar(fNumAdc);
  vector<TString> adcErrorFlagTitle(fNumAdc),    adcErrorFlagVar(fNumAdc);
  vector<TString> adcMultiplicityTitle(fNumAdc), adcMultiplicityVar(fNumAdc);

  // RVarDef vars[] = {
  //   {"pAER_adcPulsePedRaw", "test", "fAdcPedRaw[0].THcSignalHit.GetData()"},        // Cherenkov occupancy
  //   { 0 }
  // };
  // DefineVarsFromList( vars, mode);

  // Loop through the adcNames defined in the trigger parameter file, 
  // asign them a leaf name, and acquire the associated data object
  for (int i = 0; i < fNumAdc; i++) {
    
    cout << "DefineVariables Pad i = " << i << endl;
    cout << "fNumAdc = " << fNumAdc << endl;

    adcPulsePedRawTitle.at(i) = fAdcNames.at(i) + "_adcPulsePedRaw";
    adcPulsePedRawVar.at(i) = Form("fAdcPulsePedRaw[%d].THcSignalHit.GetData()", i);
    RVarDef entry1 {
      adcPulsePedRawTitle.at(i).Data(), 
      adcPulsePedRawTitle.at(i).Data(),   
      adcPulsePedRawVar.at(i).Data()
    };
    vars.push_back(entry1);

    adcPulseIntRawTitle.at(i) = fAdcNames.at(i) + "_adcPulseIntRaw";
    adcPulseIntRawVar.at(i) = Form("fAdcPulseIntRaw[%d].THcSignalHit.GetData()", i);
    RVarDef entry2 {
      adcPulseIntRawTitle.at(i).Data(),
      adcPulseIntRawTitle.at(i).Data(),
      adcPulseIntRawVar.at(i).Data()
    };
    vars.push_back(entry2);

    adcPulseAmpRawTitle.at(i) = fAdcNames.at(i) + "_adcPulseAmpRaw";
    adcPulseAmpRawVar.at(i) = Form("fAdcPulseAmpRaw[%d].THcSignalHit.GetData()", i);
    RVarDef entry3 {
      adcPulseAmpRawTitle.at(i).Data(),
      adcPulseAmpRawTitle.at(i).Data(),
      adcPulseAmpRawVar.at(i).Data()
    };
    vars.push_back(entry3);

    adcPulseTimeRawTitle.at(i) = fAdcNames.at(i) + "_adcPulseTimeRaw";
    adcPulseTimeRawVar.at(i) = Form("fAdcPulseTimeRaw[%d].THcSignalHit.GetData()", i);
    RVarDef entry4 {
      adcPulseTimeRawTitle.at(i).Data(),
      adcPulseTimeRawTitle.at(i).Data(),
      adcPulseTimeRawVar.at(i).Data()
    };
    vars.push_back(entry4);

    adcPulsePedTitle.at(i) = fAdcNames.at(i) + "_adcPulsePed";
    adcPulsePedVar.at(i) = Form("fAdcPulsePed[%d].THcSignalHit.GetData()", i);
    RVarDef entry5 {
      adcPulsePedTitle.at(i).Data(),
      adcPulsePedTitle.at(i).Data(),
      adcPulsePedVar.at(i).Data()
    };
    vars.push_back(entry5);

    adcPulseIntTitle.at(i) = fAdcNames.at(i) + "_adcPulseInt";
    adcPulseIntVar.at(i) = Form("fAdcPulseInt[%d].THcSignalHit.GetData()", i);
    RVarDef entry6 {
      adcPulseIntTitle.at(i).Data(),
      adcPulseIntTitle.at(i).Data(),
      adcPulseIntVar.at(i).Data()
    };
    vars.push_back(entry6);

    adcPulseAmpTitle.at(i) = fAdcNames.at(i) + "_adcPulseAmp";
    adcPulseAmpVar.at(i) = Form("fAdcPulseAmp[%d].THcSignalHit.GetData()", i);
    RVarDef entry7 {
      adcPulseAmpTitle.at(i).Data(),
      adcPulseAmpTitle.at(i).Data(),
      adcPulseAmpVar.at(i).Data()
    };
    vars.push_back(entry7);
 
    adcPulseTimeTitle.at(i) = fAdcNames.at(i) + "_adcPulseTime";
    adcPulseTimeVar.at(i) = Form("fAdcPulseTime[%d].THcSignalHit.GetData()", i);
    RVarDef entry8 {
      adcPulseTimeTitle.at(i).Data(),
      adcPulseTimeTitle.at(i).Data(),
      adcPulseTimeVar.at(i).Data()
    };
    vars.push_back(entry8);

    adcErrorFlagTitle.at(i) = fAdcNames.at(i) + "_adcErrorFlag";
    adcErrorFlagVar.at(i) = Form("fAdcErrorFlag[%d].THcSignalHit.GetData()", i);
    RVarDef entry9 {
      adcErrorFlagTitle.at(i).Data(),
      adcErrorFlagTitle.at(i).Data(),
      adcErrorFlagVar.at(i).Data()
    };
    vars.push_back(entry9);

    adcMultiplicityTitle.at(i) = fAdcNames.at(i) + "_adcMultiplicity";
    adcMultiplicityVar.at(i) = Form("fAdcMultiplicity[%d].THcSignalHit.GetData()", i);
    RVarDef entry10 {
      adcMultiplicityTitle.at(i).Data(),
      adcMultiplicityTitle.at(i).Data(),
      adcMultiplicityVar.at(i).Data()
    };
    vars.push_back(entry10);
  } // fNumAdc loop

  // Loop through the adcNames defined in the trigger parameter file, 
  // asign them a leaf name, and acquire the associated data object
  vector<TString> tdcTimeRawTitle(fNumTdc),      tdcTimeRawVar(fNumTdc);
  vector<TString> tdcTimeTitle(fNumTdc),         tdcTimeVar(fNumTdc);
  vector<TString> tdcMultiplicityTitle(fNumTdc), tdcMultiplicityVar(fNumTdc);

  for (int i = 0; i < fNumTdc; i++) {
    tdcTimeRawTitle.at(i) = fTdcNames.at(i) + "_tdcTimeRaw";
    tdcTimeRawVar.at(i) = Form("fTdcTimeRaw[%d].THcSignalHit.GetData()", i);
    RVarDef entry1 {
      tdcTimeRawTitle.at(i).Data(),
      tdcTimeRawTitle.at(i).Data(),
      tdcTimeRawVar.at(i).Data()
    };
    vars.push_back(entry1);

    tdcTimeTitle.at(i) = fTdcNames.at(i) + "_tdcTime";
    tdcTimeVar.at(i) = Form("fTdcTime[%d].THcSignalHit.GetData()", i);
    RVarDef entry2 {
      tdcTimeTitle.at(i).Data(),
      tdcTimeTitle.at(i).Data(),
      tdcTimeVar.at(i).Data()
    };
    vars.push_back(entry2);

    tdcMultiplicityTitle.at(i) = fTdcNames.at(i) + "_tdcMultiplicity";
    tdcMultiplicityVar.at(i) = Form("fTdcMultiplicity[%d].THcSignalHit.GetData()", i);
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

  //return DefineVarsFromList(vars, mode);
}

inline
void THcTrigger::Clear(Option_t* opt)
{
  
  // Clear ADC the hit lists
  fAdcPulsePedRaw.clear();
  fAdcPulseIntRaw.clear();
  fAdcPulseAmpRaw.clear();
  fAdcPulseTimeRaw.clear();
  fAdcPulsePed.clear();
  fAdcPulseInt.clear();
  fAdcPulseAmp.clear();
  fAdcPulseTime.clear();
  fAdcErrorFlag.clear();
  fAdcMultiplicity.clear();
  // Clear the TDC hit lists
  fTdcTimeRaw.clear();
  fTdcTime.clear();
  fTdcMultiplicity.clear();

  // for (UInt_t ielem = 0; ielem < fNumAdcHits.size(); ielem++)
  //   fNumAdcHits.at(ielem) = 0;
  // for (UInt_t ielem = 0; ielem < fNumGoodAdcHits.size(); ielem++)
  //   fNumGoodAdcHits.at(ielem) = 0;
  for (UInt_t ielem = 0; ielem < fGoodAdcPed.size(); ielem++) {
    fGoodAdcPed.at(ielem)         = 0.0;
    fGoodAdcPulseInt.at(ielem)    = 0.0;
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
  Int_t nTdcHits = 0;

  cout << "\n\n\n=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!\n\n\n" << endl;
  cout << "iHit = " << iHit << " numHits = " << numHits << endl;

  while (iHit < numHits) {

    THcTriggerHit* hitObj  = dynamic_cast <THcTriggerHit*> (fRawHitList->At(iHit));
    Int_t          pmtNum  = hitObj->fCounter;

    cout << "hitObj = " << hitObj << " hitObj->fPlane = " << hitObj->fPlane << " pmtNum = " << pmtNum << endl;
    
    if (hitObj->fPlane == 1) {
      
      THcRawAdcHit& rawAdcHit = hitObj->GetRawAdcHitPos();
      
      cout << "rawAdcHit.GetNPulses() = " << rawAdcHit.GetNPulses() << endl;

      for (UInt_t thit = 0; thit < rawAdcHit.GetNPulses(); thit++) {

	cout << "thit = " << thit << endl;

	for (UInt_t iAdcChan = 0; iAdcChan < fNumAdc; iAdcChan++) {
	  // Acquire raw ADC objects
	  ((THcSignalHit*) fAdcPulsePedRaw[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetPedRaw());
	  ((THcSignalHit*) fAdcPulseIntRaw[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetPulseIntRaw());
	  ((THcSignalHit*) fAdcPulseAmpRaw[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetPulseAmpRaw());
	  ((THcSignalHit*) fAdcPulseTimeRaw[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetPulseTimeRaw());
	  // Acquire ADC objects
	  ((THcSignalHit*) fAdcPulsePed[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetPed()); 
	  ((THcSignalHit*) fAdcPulseInt[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetPulseInt()); 
	  ((THcSignalHit*) fAdcPulseAmp[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetPulseAmp()); 
	  ((THcSignalHit*) fAdcPulseTime[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetPulseTime()+fAdcTdcOffset); 
	  // ((THcSignalHit*) fAdcMultiplicity[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, rawAdcHit.GetNPulses()); 
	  if (rawAdcHit.GetPulseAmpRaw(thit) > 0)  ((THcSignalHit*) fAdcErrorFlag[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, 0); 
	  if (rawAdcHit.GetPulseAmpRaw(thit) <= 0) ((THcSignalHit*) fAdcErrorFlag[iAdcChan]->ConstructedAt(nAdcHits))->Set(pmtNum, 1); 
	}
	nAdcHits++;
      }  // Adc pulses

    }  // plane == 1 conditional

    if (hitObj->fPlane == 2) {
      
      THcRawTdcHit& rawTdcHit = hitObj->GetRawTdcHitPos();

      cout << "&rawTdcHit = " << &rawTdcHit << endl;
      cout << "rawTdcHit.GetNHits() = " << rawTdcHit.GetNHits() << endl;
      cout << "pmtNum = " << pmtNum << endl;

      for (UInt_t iTdcChan = 0; iTdcChan < fNumTdc; iTdcChan++) {
	((THcSignalHit*) fTdcTimeRaw[iTdcChan]->ConstructedAt(nTdcHits))->Set(pmtNum, rawTdcHit.GetTimeRaw());
	((THcSignalHit*) fTdcTime[iTdcChan]->ConstructedAt(nTdcHits))->Set(pmtNum, rawTdcHit.GetTime()*fTdcChanPerNs+fTdcOffset);
	// ((THcSignalHit*) fTdcMultiplicity[iTdcChan]->ConstructedAt(nTdcHits))->Set(pmtNum, rawTdcHit.GetNHits());
	((THcSignalHit*) fTdcMultiplicity[iTdcChan]->ConstructedAt(nTdcHits))->Set(12, rawTdcHit.GetNHits());
      }
      nTdcHits++;
    } // plane == 2 conditional
    // else 
    //   throw std::out_of_range("`THcTrigDet::Decode`: only planes `1` and `2` available!");
    iHit++;
  } // while hits

  cout << "\n\n\n=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!\n\n\n" << endl;

  return iHit;
}

Int_t THcTrigger::CoarseProcess( TClonesArray&  )
{
  return 0;
}

Int_t THcTrigger::FineProcess( TClonesArray&  )
{
  return 0;
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
