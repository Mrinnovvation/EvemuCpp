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

#ifndef __APIDBUTIL_H_INCL__
#define __APIDBUTIL_H_INCL__

#include <string>

#include "eve-common.h"

/**
 * @brief Reads a result column as text, mapping SQL NULL onto an empty string.
 *
 * DBResultRow::GetText() hands back the raw MYSQL_ROW pointer, which is NULL for a
 * NULL column. Constructing a std::string from that pointer is undefined behaviour and
 * aborts on current libstdc++, which is easy to hit here because most of the API
 * queries LEFT JOIN onto reference tables that may not have a matching row.
 */
inline std::string APIText(const DBResultRow& row, uint32 index)
{
    return (row.IsNull(index) ? std::string() : std::string(row.GetText(index)));
}

#endif  // __APIDBUTIL_H_INCL__
