# Testing Checklist

Things to verify after recent changes (0.8.7). Check off each item as you confirm it works.

---

## Drones

- [ ] **Launch drones** — Undock, open drone bay, launch drones. They should immediately begin orbiting your ship.
- [ ] **Engage target** — Lock a target, right-click drones → Engage. Drones should fly to and attack the target.
- [ ] **Return to drone bay** — With drones engaged, click Return to Drone Bay. Drones should fly all the way back and disappear into the bay (not stop at orbit range).
- [ ] **Return and orbit** — Right-click → Return and Orbit. Drones should fly back and resume orbiting the ship (not enter the bay).
- [ ] **Abandon drone** — Right-click a drone → Abandon. It should go neutral in space.
- [ ] **Reconnect to drones** — Log out with drones in space, log back in, use Reconnect to Drones. They should re-enter your control.

---

## Warp / Movement

- [ ] **Warp to object** — Warp to a planet, station, or gate. Ship should smoothly decelerate and stop without any visual glitch or snap.
- [ ] **Immediate re-warp** — As soon as you drop out of warp, immediately warp somewhere else. Should work cleanly without freezing or desyncing.
- [ ] **Orbit entry** — Lock a target and set orbit. The ship should smoothly enter orbit without a one-tick direction jerk at the start.
- [ ] **Orbit at range** — Start orbit from far away (ship approaches first) and from close up (ship backs off). Both should transition correctly.

---

## Wormholes

- [ ] **Fit probe launcher** — Fit a Scan Probe Launcher I to a ship and load probes. The module should show as fittable and loadable.
- [ ] **Launch probes** — Activate the launcher. Probes should appear in space. Open the probe scanner window — probes should be visible and moveable.
- [ ] **Scan down a wormhole** — Position probes around a wormhole signature and scan. Result should reach 100% and appear in the probe results list.
- [ ] **Warp to wormhole** — Right-click the scan result → Warp To. Ship should warp to the wormhole object in space.
- [ ] **Jump through wormhole** — Right-click the wormhole → Jump. Ship should appear in the destination system near the K162 exit.
- [ ] **Oversized ship blocked** — Attempt to jump a ship heavier than the wormhole's max jump mass. Should receive an error message and not jump.
- [ ] **Collapsed wormhole blocked** — After enough jumps deplete mass, attempt another jump. Should receive "This wormhole has collapsed" and not jump.
- [ ] **Mass depletion visual** — After several heavy ship jumps, the wormhole should change visually: Full → Reduced → Disrupted size, Adolescent → Decaying → Closing age.
- [ ] **Wormhole collapse** — Jump enough mass through (or wait for the timer). Both entrance and K162 should disappear from space.

---

## Anomalies

- [ ] **Anomalies appear on scanner** — Open the probe scanner without launching probes. Cosmic Anomalies (combat sites) should appear automatically.
- [ ] **Signatures appear on scanner** — Probe scan a system. Gravimetric, Magnetometric, Radar, and Ladar signatures should show up and be scannable to 100%.
- [ ] **Warp to anomaly** — Right-click a cosmic anomaly → Warp To. Ship warps to the site and NPCs/objects are present in space.
- [ ] **Site respawn after expiry** — After ~2 hours an anomaly site should disappear from the scanner and a new site of the same type should appear shortly after. *(To test faster, temporarily reduce the expiry in `DungeonMgr::MakeDungeon()` from `2 * EvE::Time::Hour` to a smaller value and rebuild.)*
- [ ] **Multiple site types per system** — Systems should populate with a variety of site types (Grav, Mag, Radar, Ladar, Anomaly) up to the system's max, not just one type repeating.

---

## Market

- [ ] **Market browser speed** — Open the market browser and navigate categories. Should be fast with no multi-second lag between clicks.
- [ ] **Seeded regions** — Characters in Caldari (The Forge/Citadel), Amarr (Domain), Gallente (Essence), and Minmatar (Heimatar) space should all find buy/sell orders in the market.

---

## GM Commands

- [ ] **`/giveallskills me`** — Run the command, open the character sheet Skills tab. All skills should appear at level 5.
- [ ] **Skills persist after restart** — After running `/giveallskills me`, restart the server and log back in. All skills should still be level 5 (not reset).

---

## Docking / Logout

- [ ] **Dock at station** — Warp to a station and request dock. Ship should approach and complete the dock — not oscillate or wiggle in place.
- [ ] **Position saved on logout** — Warp to a location in space, log out, log back in. Ship should appear at or near where you logged out, not the previous system/location.

---

## NPC Combat

- [ ] **NPC repair visual** — Attack an NPC rat until its health drops. If it has repair modules (common on cruisers/battlecruisers), it should show a shield glow or armor repair beam animation on the NPC model.
- [ ] **NPC health bar updates on repair** — With an NPC targeted, its health bar should visibly rise when it activates a repair module, not stay frozen at a low value.

---

## Stability

- [ ] **No crash on combat** — Engage NPCs with modules active and drones out. Killing NPCs or having modules deactivate mid-combat should not crash the server.
- [ ] **Multiple players** — If possible, have two clients in the same system performing actions simultaneously. No crashes from concurrent module/target activity.
