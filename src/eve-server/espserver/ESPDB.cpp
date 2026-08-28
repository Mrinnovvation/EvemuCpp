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

#include "eve-server.h"

#include "auth/PasswordModule.h"
#include "espserver/ESPDB.h"

/* Local helpers ---------------------------------------------------------------- */

/// Reads a column as text, mapping SQL NULL onto an empty string.
static std::string TextOf(const DBResultRow& row, uint32 index)
{
    return (row.IsNull(index) ? std::string() : std::string(row.GetText(index)));
}

/// Formats a number for display in a table cell.
static std::string NumberOf(int64 value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRId64, value);
    return std::string(buf);
}

static std::string DoubleOf(double value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", value);
    return std::string(buf);
}

/**
 * Escapes a user-supplied substring for use inside a LIKE pattern.
 *
 * DoEscapeString() handles quoting, but LIKE also treats '%' and '_' as wildcards,
 * so a search for "a_b" would otherwise match far more than the operator asked for.
 */
static std::string EscapeForLike(const std::string& text)
{
    std::string wildcardSafe;
    wildcardSafe.reserve(text.length());

    for (std::string::const_iterator itr = text.begin(); itr != text.end(); ++itr) {
        if ((*itr == '%') or (*itr == '_'))
            wildcardSafe += '!';        // paired with ESCAPE '!' in the query
        wildcardSafe += *itr;
    }

    std::string escaped;
    sDatabase.DoEscapeString(escaped, wildcardSafe);
    return escaped;
}

/// Clamps a caller-supplied row limit into a sane range.
static uint32 ClampLimit(uint32 limit)
{
    if (limit == 0)
        return 1;
    if (limit > 500)
        return 500;
    return limit;
}

/* Authentication --------------------------------------------------------------- */

bool ESPDB::AuthenticateAccount(const std::string& username, const std::string& password,
                                uint32* accountID, int64* role)
{
    /* Deliberately does not use ServiceDB::GetAccountInformation(): that creates an
     * account when autoAccountRole is set, which must never happen from a web request.
     */
    DBQueryResult res;
    std::string eUsername;
    sDatabase.DoEscapeString(eUsername, username);

    if (!sDatabase.RunQuery(res,
        "SELECT accountID, password, hash, role, banned"
        " FROM account"
        " WHERE accountName = '%s'", eUsername.c_str()))
    {
        sLog.Error("ESPDB::AuthenticateAccount()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DBResultRow row;
    if (!res.GetRow(row))
        return false;               // no such account

    if (row.GetInt(4) != 0)
        return false;               // banned

    const std::string dbPassword = TextOf(row, 1);
    const std::string dbHash     = TextOf(row, 2);

    bool authenticated = false;
    if (!dbPassword.empty())
        authenticated = (dbPassword == password);

    if (!authenticated and !dbHash.empty()) {
        std::string passHash;
        if (PasswordModule::GeneratePassHash(username, password, passHash))
            authenticated = (dbHash == passHash);
    }

    if (!authenticated)
        return false;

    *accountID = row.GetUInt(0);
    *role = row.GetInt64(3);
    return true;
}

/* Characters ------------------------------------------------------------------- */

/// Shared projection for the character list pages, so both callers decode the same columns.
static const char* const CHARACTER_LIST_COLUMNS =
    " chrCharacters.characterID,"
    " chrCharacters.characterName,"
    " chrCharacters.accountID,"
    " account.accountName,"
    " chrCharacters.corporationID,"
    " crpCorporation.corporationName,"
    " chrCharacters.online";

static void DecodeCharacterRows(DBQueryResult& res, std::vector<ESPCharacterRow>& into)
{
    DBResultRow row;
    while (res.GetRow(row)) {
        ESPCharacterRow entry;
        entry.characterID     = row.GetUInt(0);
        entry.characterName   = TextOf(row, 1);
        entry.accountID       = (row.IsNull(2) ? 0 : row.GetUInt(2));
        entry.accountName     = TextOf(row, 3);
        entry.corporationID   = row.GetUInt(4);
        entry.corporationName = TextOf(row, 5);
        entry.online          = (row.GetInt(6) != 0);
        into.push_back(entry);
    }
}

bool ESPDB::SearchCharacters(const std::string& nameFilter, uint32 limit,
                             std::vector<ESPCharacterRow>& into)
{
    DBQueryResult res;
    const uint32 rowLimit = ClampLimit(limit);
    bool ok = false;

    if (nameFilter.empty()) {
        ok = sDatabase.RunQuery(res,
            "SELECT %s"
            " FROM chrCharacters"
            "  LEFT JOIN account ON account.accountID = chrCharacters.accountID"
            "  LEFT JOIN crpCorporation ON crpCorporation.corporationID = chrCharacters.corporationID"
            " ORDER BY chrCharacters.characterName"
            " LIMIT %u", CHARACTER_LIST_COLUMNS, rowLimit);
    } else {
        const std::string pattern = EscapeForLike(nameFilter);
        ok = sDatabase.RunQuery(res,
            "SELECT %s"
            " FROM chrCharacters"
            "  LEFT JOIN account ON account.accountID = chrCharacters.accountID"
            "  LEFT JOIN crpCorporation ON crpCorporation.corporationID = chrCharacters.corporationID"
            " WHERE chrCharacters.characterName LIKE '%%%s%%' ESCAPE '!'"
            " ORDER BY chrCharacters.characterName"
            " LIMIT %u", CHARACTER_LIST_COLUMNS, pattern.c_str(), rowLimit);
    }

    if (!ok) {
        sLog.Error("ESPDB::SearchCharacters()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DecodeCharacterRows(res, into);
    return true;
}

bool ESPDB::GetCharactersForAccount(uint32 accountID, std::vector<ESPCharacterRow>& into)
{
    DBQueryResult res;

    if (!sDatabase.RunQuery(res,
        "SELECT %s"
        " FROM chrCharacters"
        "  LEFT JOIN account ON account.accountID = chrCharacters.accountID"
        "  LEFT JOIN crpCorporation ON crpCorporation.corporationID = chrCharacters.corporationID"
        " WHERE chrCharacters.accountID = %u"
        " ORDER BY chrCharacters.characterName", CHARACTER_LIST_COLUMNS, accountID))
    {
        sLog.Error("ESPDB::GetCharactersForAccount()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DecodeCharacterRows(res, into);
    return true;
}

bool ESPDB::GetCharacterDetail(uint32 characterID, std::vector<std::string>& into,
                               std::string& characterName, uint32* accountID)
{
    DBQueryResult res;

    if (!sDatabase.RunQuery(res,
        "SELECT"
        "  chrCharacters.characterID,"
        "  chrCharacters.characterName,"
        "  chrCharacters.accountID,"
        "  account.accountName,"
        "  account.banned,"
        "  chrCharacters.corporationID,"
        "  crpCorporation.corporationName,"
        "  chrCharacters.title,"
        "  chrCharacters.balance,"
        "  chrCharacters.skillPoints,"
        "  chrCharacters.securityRating,"
        "  chrCharacters.bounty,"
        "  chrCharacters.online,"
        "  chrCharacters.logonMinutes,"
        "  chrCharacters.stationID,"
        "  chrCharacters.solarSystemID,"
        "  mapSolarSystems.solarSystemName,"
        "  chrCharacters.shipID,"
        "  chrCharacters.description"
        " FROM chrCharacters"
        "  LEFT JOIN account ON account.accountID = chrCharacters.accountID"
        "  LEFT JOIN crpCorporation ON crpCorporation.corporationID = chrCharacters.corporationID"
        "  LEFT JOIN mapSolarSystems ON mapSolarSystems.solarSystemID = chrCharacters.solarSystemID"
        " WHERE chrCharacters.characterID = %u", characterID))
    {
        sLog.Error("ESPDB::GetCharacterDetail()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DBResultRow row;
    if (!res.GetRow(row))
        return false;               // no such character

    characterName = TextOf(row, 1);
    *accountID = (row.IsNull(2) ? 0 : row.GetUInt(2));

    into.push_back("characterID");     into.push_back(NumberOf(row.GetUInt(0)));
    into.push_back("Name");            into.push_back(characterName);
    into.push_back("accountID");       into.push_back(row.IsNull(2) ? "(none - NPC)" : NumberOf(row.GetUInt(2)));
    into.push_back("Account");         into.push_back(TextOf(row, 3));
    into.push_back("Account banned");  into.push_back((!row.IsNull(4) and (row.GetInt(4) != 0)) ? "YES" : "no");
    into.push_back("corporationID");   into.push_back(NumberOf(row.GetUInt(5)));
    into.push_back("Corporation");     into.push_back(TextOf(row, 6));
    into.push_back("Title");           into.push_back(TextOf(row, 7));
    into.push_back("Balance");         into.push_back(DoubleOf(row.GetDouble(8)) + " ISK");
    into.push_back("Skill points");    into.push_back(NumberOf(row.GetInt64(9)));
    into.push_back("Security rating"); into.push_back(DoubleOf(row.GetFloat(10)));
    into.push_back("Bounty");          into.push_back(DoubleOf(row.GetDouble(11)));
    into.push_back("Online");          into.push_back((row.GetInt(12) != 0) ? "YES" : "no");
    into.push_back("Logon minutes");   into.push_back(NumberOf(row.GetUInt(13)));
    into.push_back("stationID");       into.push_back(NumberOf(row.GetUInt(14)));
    into.push_back("solarSystemID");   into.push_back(NumberOf(row.GetUInt(15)));
    into.push_back("Solar system");    into.push_back(TextOf(row, 16));
    into.push_back("shipID");          into.push_back(NumberOf(row.GetUInt(17)));
    into.push_back("Description");     into.push_back(TextOf(row, 18));

    return true;
}

/* Accounts --------------------------------------------------------------------- */

bool ESPDB::SearchAccounts(const std::string& nameFilter, uint32 limit,
                           std::vector<ESPAccountRow>& into)
{
    DBQueryResult res;
    const uint32 rowLimit = ClampLimit(limit);
    bool ok = false;

    if (nameFilter.empty()) {
        ok = sDatabase.RunQuery(res,
            "SELECT accountID, accountName, role, online, banned, logonCount, lastLogin"
            " FROM account"
            " ORDER BY accountName"
            " LIMIT %u", rowLimit);
    } else {
        const std::string pattern = EscapeForLike(nameFilter);
        ok = sDatabase.RunQuery(res,
            "SELECT accountID, accountName, role, online, banned, logonCount, lastLogin"
            " FROM account"
            " WHERE accountName LIKE '%%%s%%' ESCAPE '!'"
            " ORDER BY accountName"
            " LIMIT %u", pattern.c_str(), rowLimit);
    }

    if (!ok) {
        sLog.Error("ESPDB::SearchAccounts()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DBResultRow row;
    while (res.GetRow(row)) {
        ESPAccountRow entry;
        entry.accountID   = row.GetUInt(0);
        entry.accountName = TextOf(row, 1);
        entry.role        = row.GetInt64(2);
        entry.online      = (row.GetInt(3) != 0);
        entry.banned      = (row.GetInt(4) != 0);
        entry.logonCount  = row.GetUInt(5);
        entry.lastLogin   = TextOf(row, 6);
        into.push_back(entry);
    }

    return true;
}

bool ESPDB::SetAccountBanned(uint32 accountID, bool banned)
{
    DBerror err;

    if (!sDatabase.RunQuery(err,
        "UPDATE account SET banned = %u WHERE accountID = %u",
        (banned ? 1 : 0), accountID))
    {
        sLog.Error("ESPDB::SetAccountBanned()", "Error in query: %s.", err.c_str());
        return false;
    }

    return true;
}

/* Inventory types -------------------------------------------------------------- */

bool ESPDB::GetTypeDetail(uint16 typeID, std::vector<std::string>& into, std::string& typeName)
{
    DBQueryResult res;

    if (!sDatabase.RunQuery(res,
        "SELECT"
        "  invTypes.typeID,"
        "  invTypes.typeName,"
        "  invTypes.groupID,"
        "  invGroups.groupName,"
        "  invGroups.categoryID,"
        "  invCategories.categoryName,"
        "  invTypes.mass,"
        "  invTypes.volume,"
        "  invTypes.capacity,"
        "  invTypes.radius,"
        "  invTypes.portionSize,"
        "  invTypes.basePrice,"
        "  invTypes.raceID,"
        "  invTypes.published,"
        "  invTypes.marketGroupID,"
        "  invTypes.description"
        " FROM invTypes"
        "  LEFT JOIN invGroups ON invGroups.groupID = invTypes.groupID"
        "  LEFT JOIN invCategories ON invCategories.categoryID = invGroups.categoryID"
        " WHERE invTypes.typeID = %u", typeID))
    {
        sLog.Error("ESPDB::GetTypeDetail()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DBResultRow row;
    if (!res.GetRow(row))
        return false;               // no such type

    typeName = TextOf(row, 1);

    into.push_back("typeID");        into.push_back(NumberOf(row.GetUInt(0)));
    into.push_back("Name");          into.push_back(typeName);
    into.push_back("groupID");       into.push_back(NumberOf(row.GetUInt(2)));
    into.push_back("Group");         into.push_back(TextOf(row, 3));
    into.push_back("categoryID");    into.push_back(row.IsNull(4) ? "" : NumberOf(row.GetUInt(4)));
    into.push_back("Category");      into.push_back(TextOf(row, 5));
    into.push_back("Mass");          into.push_back(DoubleOf(row.GetFloat(6)));
    into.push_back("Volume");        into.push_back(DoubleOf(row.GetFloat(7)));
    into.push_back("Capacity");      into.push_back(DoubleOf(row.GetFloat(8)));
    into.push_back("Radius");        into.push_back(DoubleOf(row.GetFloat(9)));
    into.push_back("Portion size");  into.push_back(row.IsNull(10) ? "" : NumberOf(row.GetInt(10)));
    into.push_back("Base price");    into.push_back(DoubleOf(row.GetFloat(11)));
    into.push_back("raceID");        into.push_back(NumberOf(row.GetUInt(12)));
    into.push_back("Published");     into.push_back((row.GetInt(13) != 0) ? "yes" : "no");
    into.push_back("marketGroupID"); into.push_back(row.IsNull(14) ? "" : NumberOf(row.GetInt(14)));
    into.push_back("Description");   into.push_back(TextOf(row, 15));

    return true;
}

bool ESPDB::GetTypeAttributes(uint16 typeID, std::vector<ESPTypeAttributeRow>& into)
{
    DBQueryResult res;

    if (!sDatabase.RunQuery(res,
        "SELECT"
        "  dgmTypeAttributes.attributeID,"
        "  dgmAttributeTypes.attributeName,"
        "  dgmAttributeTypes.displayName,"
        "  dgmTypeAttributes.valueInt,"
        "  dgmTypeAttributes.valueFloat"
        " FROM dgmTypeAttributes"
        "  LEFT JOIN dgmAttributeTypes"
        "    ON dgmAttributeTypes.attributeID = dgmTypeAttributes.attributeID"
        " WHERE dgmTypeAttributes.typeID = %u"
        " ORDER BY dgmAttributeTypes.attributeName", typeID))
    {
        sLog.Error("ESPDB::GetTypeAttributes()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DBResultRow row;
    while (res.GetRow(row)) {
        ESPTypeAttributeRow entry;
        entry.attributeID   = row.GetUInt(0);
        entry.attributeName = TextOf(row, 1);
        entry.displayName   = TextOf(row, 2);

        // Crucible stores a dogma value in exactly one of the two columns.
        if (!row.IsNull(3))
            entry.value = NumberOf(row.GetInt(3));
        else if (!row.IsNull(4))
            entry.value = DoubleOf(row.GetDouble(4));
        else
            entry.value = "(null)";

        into.push_back(entry);
    }

    return true;
}

bool ESPDB::SearchTypes(const std::string& nameFilter, uint32 limit,
                        std::vector<std::pair<uint16, std::string> >& into)
{
    DBQueryResult res;
    const std::string pattern = EscapeForLike(nameFilter);

    if (!sDatabase.RunQuery(res,
        "SELECT typeID, typeName"
        " FROM invTypes"
        " WHERE typeName LIKE '%%%s%%' ESCAPE '!'"
        " ORDER BY typeName"
        " LIMIT %u", pattern.c_str(), ClampLimit(limit)))
    {
        sLog.Error("ESPDB::SearchTypes()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DBResultRow row;
    while (res.GetRow(row))
        into.push_back(std::make_pair(static_cast<uint16>(row.GetUInt(0)), TextOf(row, 1)));

    return true;
}

/* In-space items --------------------------------------------------------------- */

bool ESPDB::GetItemDetail(uint32 itemID, std::vector<std::string>& into,
                          std::string& itemName, uint16* typeID)
{
    DBQueryResult res;

    if (!sDatabase.RunQuery(res,
        "SELECT"
        "  entity.itemID,"
        "  entity.itemName,"
        "  entity.typeID,"
        "  invTypes.typeName,"
        "  invGroups.groupName,"
        "  entity.ownerID,"
        "  entity.locationID,"
        "  entity.flag,"
        "  entity.quantity,"
        "  entity.x,"
        "  entity.y,"
        "  entity.z"
        " FROM entity"
        "  LEFT JOIN invTypes ON invTypes.typeID = entity.typeID"
        "  LEFT JOIN invGroups ON invGroups.groupID = invTypes.groupID"
        " WHERE entity.itemID = %u", itemID))
    {
        sLog.Error("ESPDB::GetItemDetail()", "Error in query: %s.", res.error.c_str());
        return false;
    }

    DBResultRow row;
    if (!res.GetRow(row))
        return false;               // no such item

    itemName = TextOf(row, 1);
    *typeID = static_cast<uint16>(row.GetUInt(2));

    into.push_back("itemID");     into.push_back(NumberOf(row.GetUInt(0)));
    into.push_back("Name");       into.push_back(itemName);
    into.push_back("typeID");     into.push_back(NumberOf(row.GetUInt(2)));
    into.push_back("Type");       into.push_back(TextOf(row, 3));
    into.push_back("Group");      into.push_back(TextOf(row, 4));
    into.push_back("ownerID");    into.push_back(NumberOf(row.GetUInt(5)));
    into.push_back("locationID"); into.push_back(NumberOf(row.GetUInt(6)));
    into.push_back("Flag");       into.push_back(NumberOf(row.GetUInt(7)));
    into.push_back("Quantity");   into.push_back(NumberOf(row.GetInt(8)));
    into.push_back("Position");   into.push_back(DoubleOf(row.GetDouble(9)) + ", "
                                               + DoubleOf(row.GetDouble(10)) + ", "
                                               + DoubleOf(row.GetDouble(11)));

    return true;
}
