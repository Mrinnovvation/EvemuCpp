# Changelog

Work on top of [rennr/evemu_Revised](https://github.com/rennr/evemu_Revised) `revised`.
Everything here was found by playing the server with a real Crucible client and
verified in play unless a note says otherwise.

Fixes are grouped by what they protect.  Each one names the symptom first,
because the symptom is what another fork will recognise.

## 2026-10-15

### Database layer -- affects every fork

**`DBResultRow::GetBool` understood only one of the two boolean encodings.**
The schema holds 103 `tinyint(1)` booleans and 70 `bit(1)` ones, and MySQL's
text protocol hands them over differently:  a `tinyint` as the characters `"0"`
or `"1"`, a `bit` as a single raw byte, `0x00` or `0x01`.

* Testing the first byte against 0 (the original) read **every `tinyint` as
  true**.  `dunSpawnGroups.warpIn` is one, and it put every dungeon wave
  100-150km off its beacon, in a bubble with no player in it, where it went
  dormant.  Anomalies looked empty.
* Parsing the value as text instead reads **every true `bit` as false**, because
  `strtol` finds no digits in `"\x01"`.  `dgmTypeEffects.isDefault` is one, and
  it leaves modules with no default effect, so the server tells the client
  `effectID 0` and no hardpoint can show a cycle.

Both halves have to be handled.  Getting either one wrong breaks a different
part of the game, and the second failure was introduced here while fixing the
first -- it cost an evening, hence the warning.  73 call sites read this.

### Dogma and modules

* **No hardpoint ever showed an active state.**  Four separate bugs, none
  visible while any other remained:  the `bit(1)` read above; the launcher
  reporting its *charge's* effect (`missileLaunching`) as its own, which the
  client cannot match to the module (the substitution belongs to the missile
  graphic alone);  `duration` sent as the cycle over 1000 with `startTime` that
  many *seconds* in the past, giving a 9ms cycle that ended 9 seconds ago;  and
  a weapon bank's slaves, which are never `Activate()`d and so sent `effectID 0`
  with a zero duration, leaving three guns of four unable to draw anything.
* **The fitting window's `Fit` button had no handler.**  `FitFitting` was
  unimplemented, so the client got `None` and died on `'NoneType' object is not
  iterable` after fitting a single rig.  It arrives on the *inventory* bind, not
  on `dogmaIM` -- the client's traceback names a `DogmaLocation` and is
  misleading.  Its arguments are the client's own resolution of the fit and
  include no part of the saved fitting:  `(shipID, itemsToFit as a
  collections.defaultdict(set), stationID, modulesByFlag, dronesByType)`.
* **A missing source attribute was read as 0 and a multiply became an erase.**
  The Imperial Navy Capacitor Power Relay carries no
  `shieldRechargeRateMultiplier`, so a ship's shield recharge went to zero,
  `CalcRechargeRate` clamped it to 1ms, and the result was ~6,100,000 HP/s:  the
  ship was unkillable and nothing said so.  A missing attribute now takes
  dogma's default.
* **Weapon groups did not survive a login.**  One unusable row made
  `LoadWeaponGroups` call `UnlinkAllWeapons(true)`, deleting every bank on the
  ship.  A bad row now costs its own bank, and each rejection says which row and
  why.

### Electronic warfare and targeting

* **A jam the client was never told had ended.**  `EwarState::Clear()` -- the
  teardown that ends effects *with* their notifications -- was never called from
  anywhere.  A jam live when the ship's entity went (docking, jumping, logging
  off) vanished server side in silence and stayed latched in the client, which
  then would not even ask to lock anything.  Only a relog cleared it.
* **Locks lost to a jam could not be re-acquired.**  `LoseAllTargets` sent a
  per-target "lost" and never the protocol's "clear", which only
  `ClearAllTargets` sent.  A lock still being acquired lives in a list of its own
  in the client, and a "lost" for something it was never told was "added" is
  discarded:  rats already engaged became unlockable while a fresh wave locked
  fine.
* **Stacked ECM left no window to lock anything.**  Every new jam entry costs the
  target all of its locks and each rat re-rolls on its own cycle, so a site full
  of them wiped locks every few seconds against a seven second lock time.  A rat
  now drops its own jam before rolling and skips the roll if another source
  already holds the target.  One jammer at a time;  a jam that ends is a window.

### NPCs and dungeons

* **A dungeon NPC is bound by its site, not by its spawn point.**  No home leash
  inside an anomaly, incursion or mission bubble, and an idle npc returns to the
  site by its own orbit range rather than by a leash that no type carries
  (`entityMaxWanderRange` has no rows at all).
* **`/killallnpcs` killed the scenery.**  Large Collidable Structures load as
  npcs;  the sweep now skips anything that cannot move, and reports its counts.

### Scanning

* **Scanning during warp broke scanning for the session.**  `ConeScan` owes the
  client a list of results and refused with a `None`, so the client's scan
  service threw and stayed broken until a relog.  `RequestScans` returns `None`
  on success, so a refusal there was indistinguishable from results still to
  come.  Both now throw.  *(Reasoned from the call contract;  the client does not
  report this one to the server.)*

### Content and tooling

* **`/givefit`** stocks a station hangar with everything a saved fit needs,
  counting what is already there and creating only the shortfall.  Takes a saved
  fit's name, its id, or ship DNA, and lists the pilot's fits when given no
  argument.  **`/fitdna`** fits a saved fit or DNA to the ship being flown.
* **`utils/dev/eft_to_dna.py`** converts any fitting site's EFT text to ship DNA
  against this build's own `invTypes`, so a module from a later expansion is
  named as missing rather than silently dropped.
* **`utils/checker/check_data.py`** -- 13 static data invariants over npc combat
  data, the missing-multiplier family, dungeon content and referential
  integrity.  Most bugs here have been missing data read as zero, and each was a
  query away.
* **Pirate anomaly decoration** (830 objects over 150 rooms) and anomaly waves
  that spawn inside 10km of the beacon instead of up to 155km.

## Notes for other forks

The database fix and the dogma and ewar fixes are not specific to this base and
should apply to any EVEmu.  `GetBool` is the one to take first:  it is three
lines, it affects every boolean column in the schema, and a half-fix is worse
than none.
