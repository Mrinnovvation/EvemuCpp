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

#include "EntityList.h"
#include "EVE_Roles.h"
#include "espserver/ESPDB.h"
#include "espserver/ESPHtmlWriter.h"
#include "espserver/ESPPages.h"

/* Local helpers ---------------------------------------------------------------- */

/// How many rows a list page shows before the operator has to narrow the search.
static const uint32 ESP_LIST_LIMIT = 200;

static std::string IdToString(int64 value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRId64, value);
    return std::string(buf);
}

/// Link to a character's detail page.
static std::string CharacterLink(uint32 characterID, const std::string& name)
{
    return ESPHtmlWriter::Link("/gm/character.py?action=Character&characterID=" + IdToString(characterID),
                               name.empty() ? IdToString(characterID) : name);
}

/// Link to a type's overview page.
static std::string TypeLink(uint16 typeID, const std::string& name)
{
    return ESPHtmlWriter::Link("/gd/type.py?action=Type&typeID=" + IdToString(typeID),
                               name.empty() ? IdToString(typeID) : name);
}

/// Emits a single-field GET search form.
static void SearchForm(ESPHtmlWriter& page, const std::string& action,
                       const std::string& hiddenAction, const std::string& current)
{
    std::string form = "<form method=\"get\" action=\"" + ESPHtmlWriter::Escape(action) + "\">";
    if (!hiddenAction.empty())
        form += "<input type=\"hidden\" name=\"action\" value=\"" + ESPHtmlWriter::Escape(hiddenAction) + "\">";
    form += "<input type=\"text\" name=\"search\" size=\"30\" value=\""
          + ESPHtmlWriter::Escape(current) + "\"> ";
    form += "<button type=\"submit\">Search</button></form>";

    // SearchForm builds its own markup, so push it through a raw single-cell row.
    page.BeginTable(std::vector<std::string>(1, "Search"));
    page.RawRow(std::vector<std::string>(1, form));
    page.EndTable();
}

/// Renders a table of characters.
static void CharacterTable(ESPHtmlWriter& page, const std::vector<ESPCharacterRow>& rows)
{
    std::vector<std::string> columns;
    columns.push_back("characterID");
    columns.push_back("Name");
    columns.push_back("Account");
    columns.push_back("Corporation");
    columns.push_back("Online");
    page.BeginTable(columns);

    for (size_t i = 0; i < rows.size(); ++i) {
        const ESPCharacterRow& row = rows.at(i);

        std::vector<std::string> cells;
        cells.push_back(ESPHtmlWriter::Escape(IdToString(row.characterID)));
        cells.push_back(CharacterLink(row.characterID, row.characterName));
        cells.push_back(ESPHtmlWriter::Escape(row.accountName));
        cells.push_back(ESPHtmlWriter::Escape(row.corporationName));
        cells.push_back(row.online ? "<b>YES</b>" : "no");
        page.RawRow(cells);
    }

    page.EndTable();
}

/* Pages ------------------------------------------------------------------------ */

void ESPPages::Index(const ESPRequest& request, ESPResponse& response)
{
    ESPHtmlWriter page("EVEmu Server Pages", request.userName);

    page.Heading("Server");

    std::vector<std::string> summary;
    summary.push_back("Clients online");
    summary.push_back(IdToString(sEntityList.GetClientCount()));
    summary.push_back("Signed in as");
    summary.push_back(request.userName);
    summary.push_back("Role mask");
    summary.push_back(IdToString(request.accountRole));
    page.DefinitionList(summary);

    page.Heading("Pages");

    std::vector<std::string> columns;
    columns.push_back("Page");
    columns.push_back("What it shows");
    page.BeginTable(columns);

    std::vector<std::string> cells;
    cells.push_back(ESPHtmlWriter::Link("/gm/character.py?action=MyCharacters", "My characters"));
    cells.push_back("Characters on your own account.");
    page.RawRow(cells);

    cells.clear();
    cells.push_back(ESPHtmlWriter::Link("/gm/character.py", "Characters"));
    cells.push_back("Search every character on the server.");
    page.RawRow(cells);

    cells.clear();
    cells.push_back(ESPHtmlWriter::Link("/gm/users.py", "Users"));
    cells.push_back("Accounts, with ban and unban.");
    page.RawRow(cells);

    cells.clear();
    cells.push_back(ESPHtmlWriter::Link("/gd/type.py", "Types"));
    cells.push_back("Inventory types and their dogma attributes.");
    page.RawRow(cells);

    page.EndTable();

    response.body = page.Finish();
}

void ESPPages::GmCharacter(const ESPRequest& request, ESPResponse& response)
{
    const std::string action = request.Str("action");

    /* action=Character: one character's detail. This is the target of the client's
     * "Show in ESP" right-click entry.
     */
    if (action == "Character") {
        const uint32 characterID = static_cast<uint32>(request.Int("characterid"));

        std::vector<std::string> detail;
        std::string characterName;
        uint32 accountID = 0;

        if ((characterID == 0) or !ESPDB::GetCharacterDetail(characterID, detail, characterName, &accountID)) {
            ESPHtmlWriter page("Character", request.userName);
            page.Error("No character with ID " + IdToString(characterID) + ".");
            response.status = 404;
            response.body = page.Finish();
            return;
        }

        ESPHtmlWriter page("Character - " + characterName, request.userName);
        page.DefinitionList(detail);

        if (accountID != 0) {
            page.Heading("Other characters on this account");

            std::vector<ESPCharacterRow> siblings;
            ESPDB::GetCharactersForAccount(accountID, siblings);
            CharacterTable(page, siblings);

            if (request.HasRole(Acct::Role::BANNING | Acct::Role::GMH | Acct::Role::ADMIN)) {
                page.Heading("Actions");
                std::vector<std::string> hidden;
                hidden.push_back("action");
                hidden.push_back("BanUserByCharacterID");
                hidden.push_back("characterID");
                hidden.push_back(IdToString(characterID));
                page.ConfirmForm("/gm/users.py", "Ban this account", hidden);
            }
        }

        response.body = page.Finish();
        return;
    }

    // action=MyCharacters: the characters on the caller's own account.
    if (action == "MyCharacters") {
        ESPHtmlWriter page("My Characters", request.userName);

        std::vector<ESPCharacterRow> rows;
        if (!ESPDB::GetCharactersForAccount(request.accountID, rows)) {
            page.Error("Character lookup failed; see the server log.");
        } else if (rows.empty()) {
            page.Paragraph("This account has no characters.");
        } else {
            CharacterTable(page, rows);
        }

        response.body = page.Finish();
        return;
    }

    // Default: the character search page.
    ESPHtmlWriter page("Characters", request.userName);
    const std::string search = request.Str("search");

    SearchForm(page, "/gm/character.py", "", search);

    std::vector<ESPCharacterRow> rows;
    if (!ESPDB::SearchCharacters(search, ESP_LIST_LIMIT, rows)) {
        page.Error("Character lookup failed; see the server log.");
    } else {
        if (rows.size() >= ESP_LIST_LIMIT)
            page.Paragraph("Showing the first " + IdToString(ESP_LIST_LIMIT)
                         + " matches. Narrow the search to see the rest.");
        CharacterTable(page, rows);
    }

    response.body = page.Finish();
}

void ESPPages::GmUsers(const ESPRequest& request, ESPResponse& response)
{
    const std::string action = request.Str("action");
    const bool mayBan = request.HasRole(Acct::Role::BANNING | Acct::Role::GMH | Acct::Role::ADMIN);

    /* action=BanUserByCharacterID: the client's "Ban User (ESP)" menu entry links here
     * with a GET, so the GET renders a confirmation and only the POST actually bans.
     */
    if ((action == "BanUserByCharacterID") or (action == "UnbanUser")) {
        const bool banning = (action == "BanUserByCharacterID");

        if (!mayBan) {
            ESPHtmlWriter page("Users", request.userName);
            page.Error("Your account does not hold the BANNING, GMH or ADMIN role.");
            response.status = 403;
            response.body = page.Finish();
            return;
        }

        // Reached either by characterID (from the client menu) or accountID (from this page).
        uint32 accountID = static_cast<uint32>(request.Int("accountid"));
        std::string subject;

        if (accountID == 0) {
            const uint32 characterID = static_cast<uint32>(request.Int("characterid"));
            std::vector<std::string> detail;
            std::string characterName;

            if ((characterID == 0) or !ESPDB::GetCharacterDetail(characterID, detail, characterName, &accountID)
                or (accountID == 0))
            {
                ESPHtmlWriter page("Users", request.userName);
                page.Error("Could not resolve an account from characterID "
                         + IdToString(request.Int("characterid")) + ".");
                response.status = 404;
                response.body = page.Finish();
                return;
            }

            subject = characterName;
        }

        if (request.method == "POST") {
            ESPHtmlWriter page("Users", request.userName);

            if (!ESPDB::SetAccountBanned(accountID, banning)) {
                page.Error("The update failed; see the server log.");
                response.status = 500;
            } else {
                sLog.Warning("ESP", "%s %s account %u via ESP", request.userName.c_str(),
                             (banning ? "BANNED" : "unbanned"), accountID);
                page.Notice(std::string("Account ") + IdToString(accountID)
                          + (banning ? " is now banned." : " is no longer banned."));
                page.Paragraph("A ban takes effect at the account's next login attempt; "
                               "an already-connected client is not disconnected by this.");
            }

            page.Paragraph("");
            response.body = page.Finish();
            return;
        }

        // GET: confirm first. Nothing has been changed at this point.
        ESPHtmlWriter page(banning ? "Confirm ban" : "Confirm unban", request.userName);
        page.Paragraph(std::string("About to ") + (banning ? "ban" : "unban") + " accountID "
                     + IdToString(accountID)
                     + (subject.empty() ? std::string() : " (character " + subject + ")") + ".");

        std::vector<std::string> hidden;
        hidden.push_back("action");
        hidden.push_back(banning ? "BanUserByCharacterID" : "UnbanUser");
        hidden.push_back("accountID");
        hidden.push_back(IdToString(accountID));
        page.ConfirmForm("/gm/users.py", banning ? "Confirm ban" : "Confirm unban", hidden);

        response.body = page.Finish();
        return;
    }

    // Default: the account list.
    ESPHtmlWriter page("Users", request.userName);
    const std::string search = request.Str("search");

    SearchForm(page, "/gm/users.py", "", search);

    std::vector<ESPAccountRow> rows;
    if (!ESPDB::SearchAccounts(search, ESP_LIST_LIMIT, rows)) {
        page.Error("Account lookup failed; see the server log.");
        response.body = page.Finish();
        return;
    }

    if (rows.size() >= ESP_LIST_LIMIT)
        page.Paragraph("Showing the first " + IdToString(ESP_LIST_LIMIT)
                     + " matches. Narrow the search to see the rest.");

    std::vector<std::string> columns;
    columns.push_back("accountID");
    columns.push_back("Account");
    columns.push_back("Role");
    columns.push_back("Online");
    columns.push_back("Banned");
    columns.push_back("Logons");
    columns.push_back("Last login");
    if (mayBan)
        columns.push_back("Action");
    page.BeginTable(columns);

    for (size_t i = 0; i < rows.size(); ++i) {
        const ESPAccountRow& row = rows.at(i);

        std::vector<std::string> cells;
        cells.push_back(ESPHtmlWriter::Escape(IdToString(row.accountID)));
        cells.push_back(ESPHtmlWriter::Escape(row.accountName));
        cells.push_back(ESPHtmlWriter::Escape(IdToString(row.role)));
        cells.push_back(row.online ? "<b>YES</b>" : "no");
        cells.push_back(row.banned ? "<b class=\"err\">YES</b>" : "no");
        cells.push_back(ESPHtmlWriter::Escape(IdToString(row.logonCount)));
        cells.push_back(ESPHtmlWriter::Escape(row.lastLogin));

        if (mayBan) {
            const std::string href = std::string("/gm/users.py?action=")
                                   + (row.banned ? "UnbanUser" : "BanUserByCharacterID")
                                   + "&accountID=" + IdToString(row.accountID);
            cells.push_back(ESPHtmlWriter::Link(href, row.banned ? "unban" : "ban"));
        }

        page.RawRow(cells);
    }

    page.EndTable();
    response.body = page.Finish();
}

void ESPPages::GdType(const ESPRequest& request, ESPResponse& response)
{
    const std::string action = request.Str("action");
    const uint16 typeID = static_cast<uint16>(request.Int("typeid"));

    // action=TypeDogma: the dogma attribute list for one type.
    if (action == "TypeDogma") {
        std::vector<std::string> detail;
        std::string typeName;

        if ((typeID == 0) or !ESPDB::GetTypeDetail(typeID, detail, typeName)) {
            ESPHtmlWriter page("Type dogma", request.userName);
            page.Error("No type with ID " + IdToString(request.Int("typeid")) + ".");
            response.status = 404;
            response.body = page.Finish();
            return;
        }

        ESPHtmlWriter page("Dogma - " + typeName, request.userName);

        std::vector<std::string> overviewCell;
        overviewCell.push_back(TypeLink(typeID, "Back to the type overview"));
        page.BeginTable(std::vector<std::string>(1, "Type"));
        page.RawRow(overviewCell);
        page.EndTable();

        std::vector<ESPTypeAttributeRow> attributes;
        if (!ESPDB::GetTypeAttributes(typeID, attributes)) {
            page.Error("Attribute lookup failed; see the server log.");
        } else if (attributes.empty()) {
            page.Paragraph("This type has no dogma attributes.");
        } else {
            std::vector<std::string> columns;
            columns.push_back("attributeID");
            columns.push_back("Name");
            columns.push_back("Display name");
            columns.push_back("Value");
            page.BeginTable(columns);

            for (size_t i = 0; i < attributes.size(); ++i) {
                const ESPTypeAttributeRow& attribute = attributes.at(i);

                std::vector<std::string> cells;
                cells.push_back(IdToString(attribute.attributeID));
                cells.push_back(attribute.attributeName);
                cells.push_back(attribute.displayName);
                cells.push_back(attribute.value);
                page.Row(cells);
            }

            page.EndTable();
        }

        response.body = page.Finish();
        return;
    }

    // action=Type: the type overview.
    if ((action == "Type") or (typeID != 0)) {
        std::vector<std::string> detail;
        std::string typeName;

        if ((typeID == 0) or !ESPDB::GetTypeDetail(typeID, detail, typeName)) {
            ESPHtmlWriter page("Type", request.userName);
            page.Error("No type with ID " + IdToString(request.Int("typeid")) + ".");
            response.status = 404;
            response.body = page.Finish();
            return;
        }

        ESPHtmlWriter page("Type - " + typeName, request.userName);
        page.DefinitionList(detail);

        std::vector<std::string> dogmaCell;
        dogmaCell.push_back(ESPHtmlWriter::Link(
            "/gd/type.py?action=TypeDogma&typeID=" + IdToString(typeID), "Dogma attributes"));
        page.BeginTable(std::vector<std::string>(1, "More"));
        page.RawRow(dogmaCell);
        page.EndTable();

        response.body = page.Finish();
        return;
    }

    // Default: the type search page.
    ESPHtmlWriter page("Types", request.userName);
    const std::string search = request.Str("search");

    SearchForm(page, "/gd/type.py", "", search);

    if (search.empty()) {
        page.Paragraph("Enter part of a type name to search.");
        response.body = page.Finish();
        return;
    }

    std::vector<std::pair<uint16, std::string> > rows;
    if (!ESPDB::SearchTypes(search, ESP_LIST_LIMIT, rows)) {
        page.Error("Type lookup failed; see the server log.");
        response.body = page.Finish();
        return;
    }

    if (rows.size() >= ESP_LIST_LIMIT)
        page.Paragraph("Showing the first " + IdToString(ESP_LIST_LIMIT)
                     + " matches. Narrow the search to see the rest.");

    std::vector<std::string> columns;
    columns.push_back("typeID");
    columns.push_back("Name");
    columns.push_back("Dogma");
    page.BeginTable(columns);

    for (size_t i = 0; i < rows.size(); ++i) {
        std::vector<std::string> cells;
        cells.push_back(ESPHtmlWriter::Escape(IdToString(rows.at(i).first)));
        cells.push_back(TypeLink(rows.at(i).first, rows.at(i).second));
        cells.push_back(ESPHtmlWriter::Link(
            "/gd/type.py?action=TypeDogma&typeID=" + IdToString(rows.at(i).first), "attributes"));
        page.RawRow(cells);
    }

    page.EndTable();
    response.body = page.Finish();
}

void ESPPages::GdNpc(const ESPRequest& request, ESPResponse& response)
{
    /* The client sends shipID (the in-space itemID) plus the solarSystemID it was
     * looked at from; see menusvc.NPCInfoMenu.
     */
    const uint32 itemID = static_cast<uint32>(request.Int("shipid", request.Int("itemid")));

    std::vector<std::string> detail;
    std::string itemName;
    uint16 typeID = 0;

    if ((itemID == 0) or !ESPDB::GetItemDetail(itemID, detail, itemName, &typeID)) {
        ESPHtmlWriter page("NPC info", request.userName);
        page.Error("No item with ID " + IdToString(request.Int("shipid")) + ".");
        page.Paragraph("Only items that exist in the 'entity' table can be shown here; "
                       "an NPC spawned into space for this session may not have a database row.");
        response.status = 404;
        response.body = page.Finish();
        return;
    }

    ESPHtmlWriter page("NPC - " + itemName, request.userName);

    if (request.Has("solarsystemid")) {
        std::vector<std::string> context;
        context.push_back("Viewed from solarSystemID");
        context.push_back(IdToString(request.Int("solarsystemid")));
        page.DefinitionList(context);
    }

    page.DefinitionList(detail);

    std::vector<std::string> typeCell;
    typeCell.push_back(TypeLink(typeID, "Type overview"));
    typeCell.push_back(ESPHtmlWriter::Link(
        "/gd/type.py?action=TypeDogma&typeID=" + IdToString(typeID), "Dogma attributes"));
    std::vector<std::string> columns;
    columns.push_back("Type");
    columns.push_back("Dogma");
    page.BeginTable(columns);
    page.RawRow(typeCell);
    page.EndTable();

    response.body = page.Finish();
}

void ESPPages::NotFound(const ESPRequest& request, ESPResponse& response)
{
    ESPHtmlWriter page("Not found", request.userName);
    page.Error("No ESP page is served at " + request.path + ".");
    response.status = 404;
    response.body = page.Finish();
}
