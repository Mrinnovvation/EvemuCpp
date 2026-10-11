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

#include "EVE_Roles.h"
#include "espserver/ESPDB.h"
#include "espserver/ESPPages.h"
#include "espserver/ESPServer.h"
#include "espserver/ESPServerListener.h"

ESPServer::ESPServer()
{
    std::stringstream urlBuilder;
    urlBuilder << "http://" << sConfig.net.espServer << ":" << sConfig.net.espServerPort << "/";
    _url = urlBuilder.str();
}

ESPServer::~ESPServer()
{
}

std::string& ESPServer::url()
{
    return _url;
}

void ESPServer::Run()
{
    _ioThread = std::unique_ptr<boost::asio::detail::thread>(
        new boost::asio::detail::thread(std::bind(&ESPServer::RunInternal, this)));
}

void ESPServer::Stop()
{
    // RunInternal() creates _io on the worker thread, and Run() may never have been
    // called at all when the ESP server is disabled, so both can legitimately be null.
    if (_io)
        _io->stop();
    if (_ioThread)
        _ioThread->join();
}

void ESPServer::RunInternal()
{
    _io = std::unique_ptr<boost::asio::io_context>(new boost::asio::io_context());
    _listener = std::unique_ptr<ESPServerListener>(new ESPServerListener(*_io));
    _io->run();
}

bool ESPServer::Authenticate(const std::string& username, const std::string& password, ESPRequest& request)
{
    uint32 accountID = 0;
    int64 role = 0;

    if (!ESPDB::AuthenticateAccount(username, password, &accountID, &role)) {
        sLog.Warning("ESP", "Rejected ESP login for account '%s'", username.c_str());
        return false;
    }

    /* Acct::Role::VIEW is ADMIN|CONTENT|GML|GMH|QA, which mirrors the ROLEMASK_VIEW the
     * client's own spwriters.py uses to decide whether to show the ESP menus at all.
     * A plain player account authenticates fine but has no business on these pages.
     */
    if ((role & Acct::Role::VIEW) == 0) {
        sLog.Warning("ESP", "Account '%s' authenticated but holds no Acct::Role::VIEW role", username.c_str());
        return false;
    }

    request.userName = username;
    request.accountID = accountID;
    request.accountRole = role;
    return true;
}

void ESPServer::ServeRequest(const ESPRequest& request, ESPResponse& response)
{
    /* Page names are fixed by the client; see ESPPages.h. Trailing ".py" is kept
     * because the client writes it into the URLs it opens.
     */
    if ((request.path == "/") or (request.path == "/index.py"))
        ESPPages::Index(request, response);
    else if (request.path == "/gm/character.py")
        ESPPages::GmCharacter(request, response);
    else if (request.path == "/gm/users.py")
        ESPPages::GmUsers(request, response);
    else if (request.path == "/gd/type.py")
        ESPPages::GdType(request, response);
    else if (request.path == "/gd/npc.py")
        ESPPages::GdNpc(request, response);
    else
        ESPPages::NotFound(request, response);
}
