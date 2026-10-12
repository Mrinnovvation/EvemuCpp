 /**
  * @name SuperWeapon.cpp
  *   titan doomsday device module class
  * @Author:         AknorJaden
  * @Updates:    Reimplemented against the current module API to make titan doomsday devices work.
  * @date:   10 June 2015
  */


#include "eve-server.h"

#include "StatisticMgr.h"
#include "ship/modules/SuperWeapon.h"
#include "system/Damage.h"

/** Doomsday devices (EVEDB::invGroups::Super_Weapon, 588) are unlike every other weapon in the game:
 *   they carry their damage on the module itself and take NO charge (no AttrChargeGroup1, no
 *   chargeFitted/useMissiles effect), so m_chargeRef is always null and must never be read here.
 *
 *      Judgement       (24550) - Avatar    (11567) - AttrEmDamage         2,000,000
 *      Oblivion        (24552) - Leviathan  (3764) - AttrKineticDamage    2,000,000
 *      Aurora Ominae   (24554) - Erebus      (671) - AttrThermalDamage    2,000,000
 *      Gjallarhorn     (23674) - Ragnarok  (23773) - AttrExplosiveDamage  2,000,000
 *
 *   Each burns AttrConsumptionQuantity (50,000) units of AttrConsumptionType (its racial isotopes)
 *   out of the cargo hold per shot - that deduction is done once, generically, by
 *   ActiveModule::DoCycle(), so this class must NOT burn fuel a second time.
 *
 *   dgmEffects rows 4489-4492 have rangeAttributeID = NULL, so there is no range attribute to test;
 *   the ship's targeting range bounds the shot, which ActiveModule::CanActivate() now honours.
 *   AttrDuration is 600,000ms - that is the reactivation lockout.  the client picks its own beam
 *   length for these four guids (Repository.py gives them scaleTime=False), so the server does not
 *   need to shorten the duration it advertises.
 */

SuperWeapon::SuperWeapon(ModuleItemRef mRef, ShipItemRef sRef)
: ActiveModule(mRef, sRef)
{
}

void SuperWeapon::Activate(uint16 effectID, uint32 targetID/*0*/, int16 repeat/*0*/)
{
    /* a doomsday is single-shot (AttrDisallowRepeatingActivation).  its cycle time IS the
     * reactivation delay, so never auto-repeat no matter what repeat count the client sends. */
    ActiveModule::Activate(effectID, targetID, 1);
}

bool SuperWeapon::CanActivate()
{
    if (m_targetSE == nullptr) {
        m_shipRef->GetPilot()->SendNotifyMsg("Your %s requires a target.", m_modRef->name());
        return false;
    }

    // test the VALUE - presence alone is meaningless, the attribute defaults to 0
    if (m_targetSE->GetSelf()->GetAttribute(AttrAI_ImmuneToSuperWeapon).get_bool()) {
        m_shipRef->GetPilot()->SendNotifyMsg("The %s is immune to your %s.", \
                m_targetSE->GetName(), m_modRef->name());
        return false;
    }

    /* ccp restricted these to capital-sized targets ("This weapon can only fire on ships that are
     * capital-sized or larger"), which the data encodes as AttrIsCapitalSize on the target hull.
     * off by default: this emulator has always let doomsdays shoot anything, and enforcing it would
     * make them useless against every npc.  set doomsdayCapitalOnly to get ccp's rule back. */
    if (sConfig.world.doomsdayCapitalOnly)
        if (!m_targetSE->GetSelf()->GetAttribute(AttrIsCapitalSize).get_bool()) {
            m_shipRef->GetPilot()->SendNotifyMsg("Your %s can only be fired at capital-sized ships.", m_modRef->name());
            return false;
        }

    /* doomsdays are flagged AttrDisallowInEmpireSpace.  made configurable so a test server can
     * actually fire one without hauling a titan out to nullsec first. */
    if (!sConfig.world.highSecDoomsday)
        if (m_modRef->GetAttribute(AttrDisallowInEmpireSpace).get_bool())
            if (m_shipRef->GetPilot()->SystemMgr()->GetSecValue() > 0.0f) {
                m_shipRef->GetPilot()->SendNotifyMsg("Your %s cannot be activated in empire space.", m_modRef->name());
                return false;
            }

    // all specific checks pass.  run generic checks (fuel, capacitor, range) in base class
    return ActiveModule::CanActivate();
}

uint32 SuperWeapon::DoCycle()
{
    /* the base class runs the capacitor check, charges the cap, and burns the isotopes.
     *  do NOT deduct fuel again here. */
    uint32 cycleTime = ActiveModule::DoCycle();
    if (cycleTime == 0)
        return 0;   // base aborted the cycle (no fuel, no cap, no destiny)

    ApplyDamage();

    return cycleTime;
}

void SuperWeapon::ApplyDamage()
{
    if (m_targetSE == nullptr)
        return;

    // add data to StatisticMgr
    sStatMgr.Increment(Stat::pcShots);

    /* damage lives on the module, not on a charge.  a doomsday only carries the one damage type for
     * its race, and GetAttribute() returns EvilZero for the three it does not have. */
    Damage d(m_shipRef->GetPilot()->GetShipSE(),
             m_modRef,
             GetAttribute(AttrKineticDamage).get_float(),
             GetAttribute(AttrThermalDamage).get_float(),
             GetAttribute(AttrEmDamage).get_float(),
             GetAttribute(AttrExplosiveDamage).get_float(),
             1.0f,           // doomsdays cannot miss
             m_effectID
    );

    d *= sConfig.rates.turretDamage;

    m_targetSE->ApplyDamage(d);
}
