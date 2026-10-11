# Reviewing the active forks

Four forks of this project were carrying work not in upstream. This note
records what was taken from each, what was rejected and why, so the rejections
are not re-litigated from scratch.

| fork | state |
|---|---|
| [`rumpelstompskin/evemu_Crucible_Claude`](https://github.com/rumpelstompskin/evemu_Crucible_Claude) | merged |
| [`brandonbelisle/evemu_Crucible`](https://github.com/brandonbelisle/evemu_Crucible) | merged |
| [`pixelcatburglar/evemu_Crucible`](https://github.com/pixelcatburglar/evemu_Crucible) | merged |
| [`Black007Angel/evemu_Crucible-physics`](https://github.com/Black007Angel/evemu_Crucible-physics) | taken module by module; see below |

The first three merged with three hand-resolved conflicts. One of them did not
survive the compiler: `Follow()` was resolved to pixelcat's surface-to-surface
distance while a later hunk still referenced rumpel's `rawDist`, which git
merged happily and `g++` caught.

## The physics fork

This one is a 3,658-line rewrite of `DestinyManager.cpp` plus unrelated work,
all in large bundled commits, with comments in Russian. It was taken a module
at a time rather than merged.

### Taken

**Vector math** (`GaMath.cpp`, `Vector3D.cpp`, `gpoint.h`). Three real defects:

- `GaFloat` is a `double`, but its epsilon, min and max constants were
  initialised from `FLT_EPSILON`, `FLT_MIN` and `FLT_MAX`.
- `Vector3D::reflection()` computed `norm * (-2 * dotProduct(norm))` and
  subtracted it, yielding `d + 2(d·n)n` — the reflection formula with the sign
  inverted. Only reachable through bumping, which is disabled.
- `normalize()` compared the magnitude against exact zero before dividing, so
  a degenerate vector produced inf/nan instead of `(0,0,0)`. Now guarded at
  1e-12.

Plus explicit `normalize()`, `length()` and `isZero()` on `GVector`.

**The idea behind its `StackAll()` rewrite**, but not the rewrite. See below.

**Two defects it exposed in our own `MarketBotMgr`**, which are upstream's, not
the fork's: `ExpireOldOrders()` called twice per cycle, and four log strings
carrying a stray cp1252 byte.

### Rejected

**`WARP_Struct::speed`, `int32` → `double`.** Possibly correct — the author
claims it fixes warp speed display, and the neighbouring `followRange` and
`followID` are `int64`s holding what look like double bit patterns, which
suggests upstream guessed these types. But the struct sits inside the
`#pragma pack(1)` region of `DestinyStructs.h` and is memcpy'd onto the wire
(`DestinyBinDump.cpp` advances by `sizeof(WARP_Struct)`), so the change grows
the packet from **49 to 53 bytes**. That is client-visible and belongs in its
own commit, testable with a single warp.

**The `StackAll()` rewrite** (`Inventory.cpp`), offered as *"items disappearing
from station and database"*. It is O(n²) where ours is O(n log n), carries
commented-out dead code, and its only behavioural difference from ours is
reachable solely when `ownerID` is 0 — which no caller passes. The symptom
could not be reproduced. The sound idea in it was keying the merge map on
owner as well as type, which was taken separately: with `ownerID` 0, a
type-only key merges two owners' stacks in a shared station inventory and
`Delete()`s the loser.

**Its `MarketBotMgr` changes.** Local gameplay tuning, not fixes: the bot is
narrowed from ~70 item groups to 8 minerals, and `BOT_OWNER_ID` is repointed
from CONCORD (1000125) to 90000002, a character on that author's own server.
Every market commit in its history (#303, #291, #290, #278, #250) is already
in our tree via upstream; the remaining difference is our own asks-query cache
from the rumpel merge, which their tree predates. Taking their files would
revert it.

**`AttrWarpSpeedMultiplier`** (`InventoryItem.cpp`). Drops the `/3` applied
when populating item info, having moved it to database load time. Coupled to
the warp-speed rework and the `WARP_Struct` change; taking it alone would make
the fitting window disagree with the server.

**`/bay` and `/sell`** (`SystemCommands.cpp`). New commands that create market
orders from raw SQL, at `Acct::Role::PLAYER` — any player could conjure orders.
Also inverted: `/bay` creates a sell order and `/sell` creates a buy order.

### Still undecided

**Drones** (`Drone*`, `DroneAI*`, 996 insertions). A genuine either/or with
loss in both directions. Their side adds an attack-order state machine
(`m_hasAttackOrder`, `StopAttack()`, `IsTargetValid()`) and derives attack
range from optimal + falloff. Our side has rumpel's `ReturnBay()` and
`m_returnToBay`, which their tree has no equivalent of — a wholesale swap
would regress *"Fix drone Return/ReturnBay never reaching ship"*. Cherry-picking
the attack path onto our return path is the right shape, and it is real work.

**`DestinyManager` itself** (4,204 lines). Not a merge; a replacement. Two
findings worth recording before anyone attempts it:

- It fixes the same spurious warp-align error we fixed, and arguably better:
  it gates on a *measured* `speedRatio = |v| / m_maxShipSpeed` instead of the
  modelled `1 - exp(-t/agility)`, which sidesteps the stale-read and the
  `m_moveTime`-versus-`m_stateStamp` epoch mismatch entirely rather than
  correcting for them. It does not, however, honour `m_alignTime`, so a ship
  with a long turn onto the warp vector would still trip the error.
- It does **not** fix the position integrator: it still advances
  `m_position + m_velocity`, i.e. a Euler step over a logarithmic speed curve,
  which overstates the first tic of an accel from rest by ~93%.

So a wholesale replacement would trade one of our fixes away to gain a cleaner
version of the other.
