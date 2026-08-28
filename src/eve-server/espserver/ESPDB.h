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

#ifndef __ESPDB_H_INCL__
#define __ESPDB_H_INCL__

#include <map>
#include <string>
#include <vector>

#include "eve-common.h"

/// One row of the character list pages.
struct ESPCharacterRow {
    uint32 characterID;
    std::string characterName;
    uint32 accountID;
    std::string accountName;
    uint32 corporationID;
    std::string corporationName;
    bool online;
};

/// One row of the user (account) list page.
struct ESPAccountRow {
    uint32 accountID;
    std::string accountName;
    int64 role;
    bool online;
    bool banned;
    uint32 logonCount;
    std::string lastLogin;
};

/// One dogma attribute of a type.
struct ESPTypeAttributeRow {
    uint16 attributeID;
    std::string attributeName;
    std::string displayName;
    std::string value;
};

/**
 * \class ESPDB
 *
 * @brief Database queries backing the ESP pages.
 *
 * All lookups are read-only apart from SetAccountBanned(). Every caller-supplied
 * string is escaped here rather than at the call site, and every numeric filter is
 * passed as an integer, so page handlers can hand raw query-string values straight in.
 */
class ESPDB
{
public:
    /// Authenticates an account name / password pair for HTTP Basic auth.
    /// Read-only: never creates an account, and refuses banned accounts.
    static bool AuthenticateAccount(const std::string& username, const std::string& password,
                                    uint32* accountID, int64* role);

    /// Lists characters whose name contains @a nameFilter (all characters when empty).
    static bool SearchCharacters(const std::string& nameFilter, uint32 limit,
                                 std::vector<ESPCharacterRow>& into);
    /// Lists the characters belonging to one account.
    static bool GetCharactersForAccount(uint32 accountID, std::vector<ESPCharacterRow>& into);
    /// Fetches one character's detail as ordered name/value pairs for display.
    /// @param[out] characterName Receives the character's name, for the page title.
    /// @param[out] accountID     Receives the owning account, for cross-links.
    static bool GetCharacterDetail(uint32 characterID, std::vector<std::string>& into,
                                   std::string& characterName, uint32* accountID);

    /// Lists accounts whose name contains @a nameFilter (all accounts when empty).
    static bool SearchAccounts(const std::string& nameFilter, uint32 limit,
                               std::vector<ESPAccountRow>& into);
    /// Sets or clears the ban flag on an account.
    static bool SetAccountBanned(uint32 accountID, bool banned);

    /// Fetches one inventory type's detail as ordered name/value pairs for display.
    /// @param[out] typeName Receives the type's name, for the page title.
    static bool GetTypeDetail(uint16 typeID, std::vector<std::string>& into, std::string& typeName);
    /// Lists an inventory type's dogma attributes.
    static bool GetTypeAttributes(uint16 typeID, std::vector<ESPTypeAttributeRow>& into);
    /// Lists types whose name contains @a nameFilter.
    static bool SearchTypes(const std::string& nameFilter, uint32 limit,
                            std::vector<std::pair<uint16, std::string> >& into);

    /// Fetches an in-space item's detail (used by the NPC info page).
    /// @param[out] typeID Receives the item's typeID, so the caller can link to its type page.
    static bool GetItemDetail(uint32 itemID, std::vector<std::string>& into,
                              std::string& itemName, uint16* typeID);
};

#endif  // __ESPDB_H_INCL__
