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

#ifndef __ESPSERVERCONNECTION_H_INCL__
#define __ESPSERVERCONNECTION_H_INCL__

#include <memory>
#include <string>

#include "eve-common.h"
#include "espserver/ESPRequest.h"

/**
 * \class ESPServerConnection
 *
 * @brief One HTTP connection to the ESP server.
 *
 * Reads a whole request (headers, plus a form body for POST), authenticates it from
 * the Authorization header, hands it to ESPServer::ServeRequest() and writes the reply.
 * Connections are not kept alive: the reply always closes the socket, which keeps the
 * state machine small and matches what the image and API servers already do.
 */
class ESPServerConnection
: public std::enable_shared_from_this<ESPServerConnection>
{
public:
    static std::shared_ptr<ESPServerConnection> create(boost::asio::io_context& io)
    {
        return std::shared_ptr<ESPServerConnection>(new ESPServerConnection(io));
    }

    boost::asio::ip::tcp::socket& socket();

    /// Begins reading the request.
    void Process();

private:
    ESPServerConnection(boost::asio::io_context& io);

    void ProcessHeaders(const boost::system::error_code& error);
    void ProcessBody(const boost::system::error_code& error);
    void Dispatch();
    void SendResponse(const ESPResponse& response);
    void SendUnauthorized();
    void OnWriteComplete();

    /// True when a state-changing request demonstrably came from one of our own pages.
    bool IsSameOrigin() const;
    /// Extracts "host:port" from an absolute URL.
    static std::string HostOf(const std::string& url);

    /// Percent-decodes one query-string or form value ('+' means space).
    static std::string UrlDecode(const std::string& text);
    /// Splits "a=1&b=2" into @a into, lowercasing the names.
    static void ParseFormEncoded(const std::string& text, std::map<std::string, std::string>& into);

    boost::asio::ip::tcp::socket _socket;
    boost::asio::streambuf _buffer;
    std::string _responseData;

    ESPRequest _request;
    /// Raw "user:pass" from the Authorization header, before it is split.
    std::string _credentials;
    size_t _contentLength;
    /// Bytes of the body that arrived alongside the headers in the same read.
    std::string _bodyPrefix;

    /// Guards against a client announcing an absurd Content-Length.
    static const size_t MAX_BODY_BYTES = 64 * 1024;
};

#endif  // __ESPSERVERCONNECTION_H_INCL__
