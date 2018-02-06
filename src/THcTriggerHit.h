#ifndef ROOT_THcTriggerHit
#define ROOT_THcTriggerHit

#include "THcRawHodoHit.h"

class THcTriggerHit : public THcRawHodoHit {

 public:
  friend class THcTrigger;

 protected:

 private:

  ClassDef(THcTriggerHit,0); // Raw Trigger hit
};

#endif
