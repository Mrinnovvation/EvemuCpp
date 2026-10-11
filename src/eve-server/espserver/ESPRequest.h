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

#ifndef __ESPREQUEST_H_INCL__
#define __ESPREQUEST_H_INCL__

#include <map>
#include <string>

#include "eve-common.h"

/**
 * \struct ESPRequest
 *
 * @brief One decoded ESP HTTP request, after authentication.
 *
 * Parameter names are lowercased when parsed, matching the way the client writes
 * them ("characterID" in the URL is looked up here as "characterid"). Query-string
 * and form-body parameters share one map; the body wins on a collision.
 */
struct ESPRequest
{
    ESPRequest() : accountID(0), accountRole(0) {}

    std::string method;         ///< "GET" or "POST".
    std::string path;           ///< Path with no query string, e.g. "/gm/character.py".
    std::map<std::string, std::string> params;

    /* Kept for the same-origin check on state-changing requests: the pages sit behind
     * HTTP Basic auth, and browsers replay those credentials on cross-site form posts.
     */
    std::string host;           ///< Host header.
    std::string origin;         ///< Origin header, when the browser sent one.
    std::string referer;        ///< Referer header, when the browser sent one.

    std::string userName;       ///< Authenticated account name.
    uint32 accountID;           ///< Authenticated account.
    int64 accountRole;          ///< Account role mask, for per-page role checks.

    bool Has(const char* name) const
    {
        return (params.find(name) != params.end());
    }

    std::string Str(const char* name, const char* fallback = "") const
    {
        std::map<std::string, std::string>::const_iterator itr = params.find(name);
        return ((itr == params.end()) ? std::string(fallback) : itr->second);
    }

    int64 Int(const char* name, int64 fallback = 0) const
    {
        std::map<std::string, std::string>::const_iterator itr = params.find(name);
        if (itr == params.end())
            return fallback;
        return strtoll(itr->second.c_str(), nullptr, 10);
    }

    /// True when the account holds any of the bits in @a mask.
    bool HasRole(int64 mask) const
    {
        return ((accountRole & mask) != 0);
    }
};

/**
 * \struct ESPResponse
 *
 * @brief The reply to an ESPRequest, before it is framed as HTTP.
 */
struct ESPResponse
{
    ESPResponse() : status(200), contentType("text/html; charset=utf-8") {}

    int status;                 ///< HTTP status code.
    std::string contentType;
    std::string body;
};

#endif  // __ESPREQUEST_H_INCL__
