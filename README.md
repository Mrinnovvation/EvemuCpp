# EVEmu Crucible — Claude Fork

This is a personal fork of [EVEmu Crucible](https://github.com/EvEmu-Project/evemu_Crucible), an open-source server emulator for the space MMO EVE Online. This fork is actively developed with AI-assisted bug fixes and feature implementations. See the [ChangeLog](doc/ChangeLog.md) for what has been added on top of the upstream project.

**Upstream project:** https://github.com/EvEmu-Project/evemu_Crucible
**This fork:** https://github.com/rumpelstompskin/evemu_Crucible_Claude

---

## Introduction
EVEmu is a work-in-progress server emulator for the space MMO EVE Online. This is an educational project. Please see the disclaimer below for details.

## ChangeLog
[ChangeLog](doc/ChangeLog.md)

## `docker compose` Quickstart

Clone **this fork** and run with Docker Compose:
```
git clone https://github.com/rumpelstompskin/evemu_Crucible_Claude.git
cd evemu_Crucible_Claude
docker compose up -d
```

> **Note:** If your Docker installation is older (pre-Compose V2), use `docker-compose` (with a hyphen) instead of `docker compose`.

**NOTE:** Add `--build` to the command to force a rebuild of the source after pulling new changes:
```
docker compose up -d --build
```

Configuration files are stored in `./config/`. These can be modified and will persist across restarts.

To shut down:
```
docker compose stop
```

To wipe and start fresh (removes database volume — market will re-seed on next start):
```
docker compose down -v
docker compose up -d
```

## Market Seeding

On first start, the market is automatically seeded. The default configuration seeds the following regions:

- The Forge (Caldari)
- The Citadel (Caldari secondary)
- Domain (Amarr)
- Essence (Gallente)
- Heimatar (Minmatar)

These can be changed via the `SEED_REGIONS` environment variable in `docker-compose.yml`.

## Building with Docker
EVEmu can be built with Docker to ensure a consistent dependency base:
```
docker compose build
```

## Accounts
Accounts are created automatically when logging in with the EVE client if the username is not already taken.

## Communication / Contact
For the upstream project: [EVEmu Project website](https://evemu.dev), [Discord](https://discord.gg/fTfAREYxbz), and [Forums](https://forums.evemu.dev).

## Disclaimer
***EVEmu is an educational project.***
This means our primary interest is to learn and teach ourselves and our users more about C++ project development at scale. This software is not intended for running public servers, and we do not support that. We are not responsible for what others do with the source code downloaded from this project.

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
