# EVEmu Crucible — arm64

An **arm64-native** fork of [EVEmu](https://evemu.dev), the open-source server
emulator for EVE Online. It builds and runs on Apple Silicon and ARM Linux,
where upstream does not, and carries a small number of bug fixes found by
actually playing against it with a real Crucible client.

Everything here is a strict addition to upstream — no features removed, no
protocol changes. If you are on x86 you lose nothing by using this tree.

| | |
|---|---|
| **Upstream** | [EvEmu-Project/evemu_Crucible](https://github.com/EvEmu-Project/evemu_Crucible) |
| **Client** | EVE Online *Crucible*, build **360229** (machoNet 320, EVE 7.31) |
| **Verified on** | Apple Silicon (Docker, `debian:12` + `mariadb:11.8`), Windows x64 client |

---

## Why "arm64"

Upstream's `FindMySQL.cmake` looked only for `mysqlclient` under a `mysql`
path suffix. On Debian the client library is `libmariadb` under a `mariadb`
suffix, so the find silently failed and CMake fell through to
`BuildMySQL.cmake`, which downloads a **hardcoded x86_64 MariaDB tarball** —
there is no aarch64 branch. The link then died with `file in wrong format`.

Adding the mariadb names and path suffix finds the library the OS already has:

```
-- Found MySQL: /usr/lib/aarch64-linux-gnu/libmariadb.so
```

x86 builds only ever worked because the downloaded tarball happened to match
the host. This affects every Apple Silicon and ARM Linux build.

## What else is fixed

### Spurious warp align error, on every warp

Every single warp logged `warp align/speed is incorrect, but time >
shipTimeToWarp` while the ship sat at **0.00 degrees** off target. The gate
that should have caught the aligned case first was unreachable for two
independent reasons:

- `m_timeFraction` is assigned only in `MoveObject()`, which `ProcessState()`
  calls *after* the gate — so the gate read a value one tic (~1 s) stale.
- the deadline was measured as `GetStamp() - m_stateStamp`, an integer 1 Hz
  tic count, while `m_timeFraction` derives from `m_moveTime`, a continuous
  millisecond clock reset independently. The two epochs differ by about a tic.

Measured on a rookie ship: the fraction reached 0.7220 of the required 0.749
at the moment the 4.63 s deadline expired. It actually crosses 0.749 at
t = 4.32 s — but that only became visible to the gate at t = 5.32 s. The warp
then succeeded anyway through the forced `InitWarp()` in the catchall, so the
error was pure noise. The fix also honours `m_alignTime`, which `Turn()`
computed but nothing ever read; without it any warp more than ~34 degrees off
heading could not meet its own budget. **0 occurrences** after the fix.

### Position integrated as a Euler step over a logarithmic curve

Position advanced as `m_position + m_velocity` — velocity sampled at the *end*
of the tic, applied across the whole tic, over `v(t) = Vmax·(1 − e^(−t/agility))`.
That overstates distance by **~93 % on the first tic** of an accel from rest
and ~130 m cumulatively over a rookie ship's first ten seconds, which is a
straight server/client desync. Averaging the tic's two endpoints integrates
the same curve to second order and is exact at constant speed.

### Marshal opcodes, named from CCP's own source

CCP/Fenris Creations published the Carbon engine in 2026. Its `blue`
component carries the real `enum PYTYPES`, which settles **every** opcode this
tree had marked unknown or guessed — `TY_DBROW`, `TY_REDUCE`, `TY_NEWOBJ`,
`TY_PICKLER`, `TY_LONG` and the rest. The numbering is otherwise identical,
which is the headline: EVEmu's marshal layer was already right.

One real divergence came out of it: the packed-row length. CCP computes
`(bits + 7) / 8`; ours rounded up even when the bool and null bits already
divided by 8, giving 22 bytes where the client computes 21. Benign on the wire
in both directions — `RLEUnPack` stops at its own `outlen` and zero-fills —
but it was not the length the client calculates. Now exact.

Write-up: [`doc/code_and_design_notes/marshal_vs_carbon.md`](doc/code_and_design_notes/marshal_vs_carbon.md).

## What's new

### Fitting from a DNA or EFT string

`/fit` was **disabled upstream** — its body was commented out behind
*"This command is currently disabled."* — so there was no way to fit a module
from a command at all.

```
/fit 3170                     # by typeID       -- Light Ion Blaster II
/fit "Damage Control II"      # by name         -- quote names with spaces
/fit 12612 401                # with a quantity -- 401 x Void S

/fitdna 16240:10190;3:438;1:3568;1:3170;8:12612;401:31532;1:31358;1:2456;1:12614;400::
```

That DNA is a full blaster Catalyst: 3 Magnetic Field Stabilizer II, a 1MN
Afterburner II, a Small Capacitor Booster II, 8 Light Ion Blaster II loaded
with Void S, a Burst Aerator and a Current Router, and Hobgoblin II drones.

`/fitdna` takes the same ship-DNA format `GetShipDNA()` emits and the client's
own `<url=fitting:...>` links carry. Both walk the identical path a client-side
autofit uses — `FindAvailableModuleSlot` → `VerifyHoldType` →
`TryModuleLimitChecks` → `AddItemByFlag` — so slot, skill, powergrid, CPU,
calibration and group rules stay enforced and the client renders its usual
error dialogs. Speculatively created items are deleted on refusal rather than
orphaned, and `/fitdna` continues past a rejected module so the pilot keeps as
much of the fit as the hull can take. It will not swap hulls.

`utils/dev/eft_to_dna.py` converts **EFT** — the clipboard format every fitting
site offers — into DNA, resolving names against the server's own `invTypes` so
modules from later expansions are reported rather than silently dropped:

```bash
python3 utils/dev/eft_to_dna.py fit.txt
```

> ⚠️ The fitting commands build clean and deploy, but are **not yet verified
> in-client**. The enforcement checks are believed intact and unproven.

### Logging in without the game client

A Crucible client is hard to obtain, which makes the server hard to test.
`utils/dev/evemu_login.py` speaks enough python-marshal and machoNet to
complete the full handshake — version exchange, VK command, placebo crypto,
credential auth, session handshake — and decodes the `TY_DBROW` rows in
`CryptoHandshakeAck`. Stdlib only, Python 3.9+:

```bash
python3 utils/dev/evemu_login.py --user test --password test --source .
```

This is also how the packed-row layout above was verified: the decoded
`liveupdates` rows match `select * from liveupdates` byte for byte.

---

## Quickstart

```bash
git clone https://github.com/Mrinnovvation/EvemuCpp.git
cd EvemuCpp
docker compose up -d
```

Ports: **26000** game, **26001** image server. On first start the container
copies `utils/config/*` into `./config/`, which is bind-mounted at `/app/etc` —
so config edits need only a restart, never a rebuild.

```bash
docker compose stop          # shut down, keep the database
docker compose down -v       # wipe the database volume too
docker compose up -d --build # rebuild after code changes
```

First boot takes a while — the database migrates from scratch. Follow it with
`docker logs -f server`. Market seeding is off by default; set
`SEED_MARKET=TRUE` in `docker-compose.yml` to populate orders on first boot,
and expect it to add substantially to that first start.

## Connecting a real client

Three things will cost you an evening if you don't know them:

**1. The image server must advertise a reachable address.** `config/eve-server.xml`
ships `<imageServer>127.0.0.1</imageServer>` (line 285), which tells the client
to fetch portraits from *its own* loopback. Set it to the LAN IP of the machine
running the server:

```xml
<imageServer>192.168.1.10</imageServer>   <!-- your server's LAN IP -->
```

**2. Create accounts by logging in, never by editing the database.** The server
always replies `passwordVersion: 2`, so the real client sends a **hash** with
an empty plaintext field. `Client::_VerifyLogin` branches on which one is
present: a row you inserted by hand has a plaintext password the client never
sends, and the hashed comparison fails *silently* — that warning is commented
out upstream. Auto-creation (`<autoAccountRole>` greater than zero) stores both
forms exactly as the client sent them, so it just works.

**3. Roles are read into the session at login.** Set `<autoAccountRole>`
(line 22) before first login to have new accounts created with it, or change it
in the database afterwards and **relog** — nothing re-reads it in place. The
shipped default is already `ROLE_DEV`, which is enough for most GM commands;
`ROLE_BOSS` adds `ADMIN`.

```
ROLE_DEV    7131450020691447808    (CREATOR | QA | PROGRAMMER)   <- shipped default
ROLE_BOSS   7203507614729375744    (DEV | ADMIN)
```

Accounts are created automatically on first login if the username is free.

## Status

Working end to end, confirmed with a live client: login, character creation,
portrait upload, undock, warp, gate travel, market, docking.

Fixes from other active forks are being pulled in one reviewed module at a
time rather than as a bulk merge. This work lives on integration branches and
is **not yet on this branch**:

| fork | what it brings | state |
|---|---|---|
| [`rumpelstompskin`](https://github.com/rumpelstompskin/evemu_Crucible_Claude) | drones, wormholes, anomalies, market seeding, ESP/XML API, titans | merged, builds |
| [`brandonbelisle`](https://github.com/brandonbelisle/evemu_Crucible) | autopilot and login fixes | merged, builds |
| [`pixelcatburglar`](https://github.com/pixelcatburglar/evemu_Crucible) | follow distance measured surface to surface | merged, builds |
| [`Black007Angel`](https://github.com/Black007Angel/evemu_Crucible-physics) | vector-math fixes | taken |
| ″ | Destiny rewrite, drones, market bot, admin commands | under review |

Known gaps: capital *features* (jump drives, fighters, doomsday, triage and
siege) look absent rather than broken.

## Documentation

- [ChangeLog](doc/ChangeLog.md)
- [Marshal vs. CCP's Carbon](doc/code_and_design_notes/marshal_vs_carbon.md)
- GM commands: `/help` in-game, or the `COMMAND(...)` table in [`src/eve-server/admin/GMCommands.h`](src/eve-server/admin/GMCommands.h)
- Upstream [wiki](https://wiki.evemu.dev), [Discord](https://discord.gg/fTfAREYxbz), [forums](https://forums.evemu.dev)

## Disclaimer

***EVEmu is an educational project.*** Its primary interest is learning and
teaching C++ project development at scale. This software is not intended for
running public servers, and that is not supported. We are not responsible for
what others do with source code downloaded from this project.

EVE Online and all associated trademarks and game content are the property of
CCP hf. Nothing in this project's licence grants any rights to them.

## Legal

    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev/
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, see https://www.gnu.org/licenses/.
    ------------------------------------------------------------------------------------
