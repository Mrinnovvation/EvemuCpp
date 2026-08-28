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

#ifndef __ESPSERVER_H_INCL__
#define __ESPSERVER_H_INCL__

#include <memory>

#include "eve-common.h"
#include "utils/Singleton.h"
#include "EVEServerConfig.h"
#include "espserver/ESPRequest.h"

class ESPServerListener;

/**
 * \class ESPServer
 *
 * @brief Serves the ESP (EVE Server Pages) GM and game-data web pages.
 *
 * The stock Crucible client builds ESP URLs as "http://<server ip>:50001/<page>" in
 * loginII.GetServerInfo() and opens them with blue.os.ShellExecute(), so this listens
 * on a plain HTTP port and the pages are read in an ordinary browser rather than in
 * the client. That also means there is no EVE session to authorise against, so every
 * request carries HTTP Basic credentials which are checked against the account table.
 *
 * These pages expose GM tooling, so the server is disabled unless the operator sets
 * net/espServerEnabled, and an account needs a role in Acct::Role::VIEW to see
 * anything at all.
 *
 * @see ESPPages for the page set and the URL shapes the client depends on.
 */
class ESPServer : public Singleton<ESPServer>
{
public:
    ESPServer();
    /* Declared here and defined in the .cpp so that the unique_ptr<ESPServerListener>
     * member is destroyed where ESPServerListener is a complete type. */
    ~ESPServer();

    void Run();
    void Stop();

    /// Base URL this server answers on, for logging.
    std::string& url();

    /**
     * @brief Authenticates HTTP Basic credentials for an ESP request.
     *
     * @param[in]  username Account name from the Authorization header.
     * @param[in]  password Password from the Authorization header.
     * @param[out] request  On success, its userName/accountID/accountRole are filled in.
     *
     * @retval true  The credentials are valid and the account may view ESP.
     * @retval false Rejected; the caller should answer 401.
     */
    bool Authenticate(const std::string& username, const std::string& password, ESPRequest& request);

    /// Routes an authenticated request to its page handler.
    void ServeRequest(const ESPRequest& request, ESPResponse& response);

private:
    void RunInternal();

    std::unique_ptr<boost::asio::detail::thread> _ioThread;
    std::unique_ptr<boost::asio::io_context> _io;
    std::unique_ptr<ESPServerListener> _listener;
    std::string _url;
};

#define sESPServer \
    ( ESPServer::get() )

#endif  // __ESPSERVER_H_INCL__
