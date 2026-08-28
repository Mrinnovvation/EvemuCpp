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
    Author:        Aknor Jaden
*/

#include "eve-server.h"

#include "apiserver/APIDBUtil.h"

#include "apiserver/APIAccountDB.h"

APIAccountDB::APIAccountDB()
{
}

bool APIAccountDB::GetCharactersList(uint32 accountID, std::vector<std::string> & charIDList, std::vector<std::string> & charNameList,
    std::vector<std::string> & charCorpIDList, std::vector<std::string> & charCorpNameList)
{
    DBQueryResult res;

    // Get list of characters and their corporation info from the accountID:
    if ( !sDatabase.RunQuery(res,
        " SELECT "
        "   chrCharacters.characterID, "
        "   chrCharacters.corporationID, "
        "   crpCorporation.corporationName, "
        "   chrCharacters.characterName AS name "
        " FROM chrCharacters "
        "   LEFT JOIN crpCorporation ON crpCorporation.corporationID = chrCharacters.corporationID "
        " WHERE `accountID` = %u ", accountID ))
    {
        sLog.Error( "APIAccountDB::GetCharactersList()", "Cannot find accountID %u", accountID );
        return false;
    }

    DBResultRow row;
    std::map<std::string, std::string> charInfo;
    while( res.GetRow( row ) )
    {
        charIDList.push_back( APIText(row, 0) );
        charCorpIDList.push_back( APIText(row, 1) );
        charCorpNameList.push_back( APIText(row, 2) );
        charNameList.push_back( APIText(row, 3) );
    }

    return true;
}


bool APIAccountDB::GetAccountInfo(uint32 accountID, std::vector<std::string> & accountInfoList)
{
    DBQueryResult res;

    // Get account table info using the accountID:
    if ( !sDatabase.RunQuery(res,
        " SELECT "
        "   online, "
        "   banned, "
        "   logonCount, "
        "   lastLogin "
        " FROM account "
        " WHERE `accountID` = %u ", accountID ))
    {
        sLog.Error( "APIAccountDB::GetAccountInfo()", "Cannot find accountID %u", accountID );
        return false;
    }

    DBResultRow row;
    if ( !res.GetRow(row) )
    {
        sLog.Error( "APIServiceDB::GetAccountInfo()", "res.GetRow(row) failed for unknown reason." );
        return false;
    }

    accountInfoList.push_back( APIText(row, 0) );
    accountInfoList.push_back( APIText(row, 1) );
    accountInfoList.push_back( APIText(row, 2) );
    accountInfoList.push_back( APIText(row, 3) );

    return true;
}
