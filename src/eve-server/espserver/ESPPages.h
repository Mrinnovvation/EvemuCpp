/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:        EVEmu contributors
*/

#ifndef __ESPPAGES_H_INCL__
#define __ESPPAGES_H_INCL__

#include "espserver/ESPRequest.h"

/**
 * @brief The ESP page handlers.
 *
 * The page names and their "action" parameters are fixed by the client: menusvc.py
 * builds these URLs directly and hands them to blue.os.ShellExecute(), so renaming
 * one breaks the corresponding GM right-click menu entry.
 *
 *   /gm/character.py?action=MyCharacters
 *   /gm/character.py?action=Character&characterID=N
 *   /gm/users.py?action=BanUserByCharacterID&characterID=N
 *   /gd/type.py?action=Type&typeID=N
 *   /gd/type.py?action=TypeDogma&typeID=N
 *   /gd/npc.py?action=GetNPCInfo&shipID=N&solarSystemID=N
 */
namespace ESPPages
{
    /// Landing page: server summary plus links into the other pages.
    void Index(const ESPRequest& request, ESPResponse& response);

    /// /gm/character.py -- character search, detail, and the caller's own characters.
    void GmCharacter(const ESPRequest& request, ESPResponse& response);

    /// /gm/users.py -- account list, and the ban/unban action.
    void GmUsers(const ESPRequest& request, ESPResponse& response);

    /// /gd/type.py -- inventory type overview and dogma attribute list.
    void GdType(const ESPRequest& request, ESPResponse& response);

    /// /gd/npc.py -- in-space item detail, reached from the NPC right-click menu.
    void GdNpc(const ESPRequest& request, ESPResponse& response);

    /// Renders a styled 404 for an unrouted path.
    void NotFound(const ESPRequest& request, ESPResponse& response);
}

#endif  // __ESPPAGES_H_INCL__
