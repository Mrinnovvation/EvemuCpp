 /**
  * @name SuperWeapon.h
  *   titan doomsday device module class
  * @Author:         AknorJaden
  * @Updates:    Reimplemented against the current module API to make titan doomsday devices work.
  * @date:   10 June 2015
  */


#ifndef _EVE_SHIP_MODULES_SUPER_WEAPON_H_
#define _EVE_SHIP_MODULES_SUPER_WEAPON_H_

#include "ship/modules/ActiveModule.h"

class SuperWeapon : public ActiveModule
{
public:
    SuperWeapon(ModuleItemRef mRef, ShipItemRef sRef);
    virtual ~SuperWeapon()                              { /* do nothing here */ }

    /* class type pointer querys, public for anyone to access. */
    virtual SuperWeapon*        GetSuperWeapon()        { return this; }
    /* class type helpers.  public for anyone to access. */
    virtual bool                IsSuperWeapon() const   { return true; }

    /* ActiveModule overrides */
    virtual void        Activate(uint16 effectID, uint32 targetID=0, int16 repeat=0);
    virtual uint32      DoCycle();
    virtual bool        CanActivate();
    virtual void        ApplyDamage();
};


#endif  // _EVE_SHIP_MODULES_SUPER_WEAPON_H_
