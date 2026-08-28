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

#include <Base64.h>

#include "espserver/ESPServer.h"
#include "espserver/ESPServerConnection.h"

ESPServerConnection::ESPServerConnection(boost::asio::io_context& io)
: _socket(io),
  _contentLength(0)
{
}

boost::asio::ip::tcp::socket& ESPServerConnection::socket()
{
    return _socket;
}

void ESPServerConnection::Process()
{
    boost::asio::async_read_until(_socket, _buffer, "\r\n\r\n",
        std::bind(&ESPServerConnection::ProcessHeaders, shared_from_this(), std::placeholders::_1));
}

std::string ESPServerConnection::UrlDecode(const std::string& text)
{
    std::string out;
    out.reserve(text.length());

    for (size_t i = 0; i < text.length(); ++i) {
        if (text[i] == '+') {
            out += ' ';
        } else if ((text[i] == '%') and ((i + 2) < text.length())) {
            const std::string hex = text.substr(i + 1, 2);
            char* end = nullptr;
            const long value = strtol(hex.c_str(), &end, 16);
            if ((end != nullptr) and (*end == '\0')) {
                out += static_cast<char>(value);
                i += 2;
            } else {
                out += text[i];     // malformed escape; keep the '%' verbatim
            }
        } else {
            out += text[i];
        }
    }

    return out;
}

void ESPServerConnection::ParseFormEncoded(const std::string& text, std::map<std::string, std::string>& into)
{
    size_t start = 0;

    while (start <= text.length()) {
        size_t amp = text.find('&', start);
        if (amp == std::string::npos)
            amp = text.length();

        const std::string pair = text.substr(start, amp - start);
        if (!pair.empty()) {
            const size_t equals = pair.find('=');
            std::string name = ((equals == std::string::npos) ? pair : pair.substr(0, equals));
            std::string value = ((equals == std::string::npos) ? std::string() : pair.substr(equals + 1));

            name = UrlDecode(name);
            // The client writes "characterID"; look-ups here are all lowercase.
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            into[name] = UrlDecode(value);
        }

        if (amp == text.length())
            break;
        start = amp + 1;
    }
}

void ESPServerConnection::ProcessHeaders(const boost::system::error_code& error)
{
    if (error) {
        // Client hung up mid-request; nothing to answer.
        return;
    }

    std::istream stream(&_buffer);
    std::string line;

    // Request line: "GET /gm/character.py?action=MyCharacters HTTP/1.1"
    if (!std::getline(stream, line)) {
        SendResponse(ESPResponse());
        return;
    }
    if (!line.empty() and (line[line.length() - 1] == '\r'))
        line.erase(line.length() - 1);

    const size_t firstSpace = line.find(' ');
    const size_t secondSpace = ((firstSpace == std::string::npos) ? std::string::npos
                                                                 : line.find(' ', firstSpace + 1));
    if ((firstSpace == std::string::npos) or (secondSpace == std::string::npos)) {
        ESPResponse bad;
        bad.status = 400;
        bad.body = "<html><body>400 - Bad Request</body></html>";
        SendResponse(bad);
        return;
    }

    _request.method = line.substr(0, firstSpace);
    std::string target = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);

    if ((_request.method != "GET") and (_request.method != "POST") and (_request.method != "HEAD")) {
        ESPResponse notAllowed;
        notAllowed.status = 405;
        notAllowed.body = "<html><body>405 - Method Not Allowed</body></html>";
        SendResponse(notAllowed);
        return;
    }

    const size_t question = target.find('?');
    if (question == std::string::npos) {
        _request.path = UrlDecode(target);
    } else {
        _request.path = UrlDecode(target.substr(0, question));
        ParseFormEncoded(target.substr(question + 1), _request.params);
    }

    // Remaining headers, up to the blank line.
    while (std::getline(stream, line)) {
        if (!line.empty() and (line[line.length() - 1] == '\r'))
            line.erase(line.length() - 1);
        if (line.empty())
            break;

        const size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string name = line.substr(0, colon);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        std::string value = line.substr(colon + 1);
        const size_t firstNonSpace = value.find_first_not_of(" \t");
        value = ((firstNonSpace == std::string::npos) ? std::string() : value.substr(firstNonSpace));

        if (name == "content-length") {
            const long announced = strtol(value.c_str(), nullptr, 10);
            _contentLength = ((announced > 0) ? static_cast<size_t>(announced) : 0);
        } else if (name == "host") {
            _request.host = value;
        } else if (name == "origin") {
            _request.origin = value;
        } else if (name == "referer") {
            _request.referer = value;
        } else if (name == "authorization") {
            // "Basic <base64 of user:pass>"
            if (value.compare(0, 6, "Basic ") == 0) {
                std::string decoded;
                Base64::decode(value.substr(6), decoded);
                _credentials = decoded;
            }
        }
    }

    if (_contentLength > MAX_BODY_BYTES) {
        ESPResponse tooLarge;
        tooLarge.status = 413;
        tooLarge.body = "<html><body>413 - Request body too large</body></html>";
        SendResponse(tooLarge);
        return;
    }

    // async_read_until stops at the blank line but may have buffered part of the body.
    if (_buffer.size() > 0) {
        std::ostringstream rest;
        rest << &_buffer;
        _bodyPrefix = rest.str();
    }

    if ((_request.method == "POST") and (_bodyPrefix.length() < _contentLength)) {
        const size_t remaining = _contentLength - _bodyPrefix.length();
        boost::asio::async_read(_socket, _buffer, boost::asio::transfer_exactly(remaining),
            std::bind(&ESPServerConnection::ProcessBody, shared_from_this(), std::placeholders::_1));
        return;
    }

    ProcessBody(boost::system::error_code());
}

void ESPServerConnection::ProcessBody(const boost::system::error_code& error)
{
    if (error and (error != boost::asio::error::eof)) {
        return;
    }

    if (_request.method == "POST") {
        std::string body = _bodyPrefix;
        if (_buffer.size() > 0) {
            std::ostringstream rest;
            rest << &_buffer;
            body += rest.str();
        }

        if (body.length() > _contentLength)
            body.resize(_contentLength);

        // Body parameters win over same-named query parameters.
        ParseFormEncoded(body, _request.params);
    }

    Dispatch();
}

void ESPServerConnection::Dispatch()
{
    const size_t colon = _credentials.find(':');
    if (colon == std::string::npos) {
        SendUnauthorized();
        return;
    }

    const std::string username = _credentials.substr(0, colon);
    const std::string password = _credentials.substr(colon + 1);

    if (!sESPServer.Authenticate(username, password, _request)) {
        SendUnauthorized();
        return;
    }

    /* A POST changes server state (currently the ban action), so require that it came
     * from one of our own pages. Without this a page on another site could auto-submit
     * a form here and the browser would attach the GM's cached Basic credentials.
     */
    if ((_request.method == "POST") and !IsSameOrigin()) {
        sLog.Warning("ESP", "Rejected cross-origin POST to %s from origin '%s' referer '%s' (account '%s')",
                     _request.path.c_str(), _request.origin.c_str(), _request.referer.c_str(), username.c_str());

        ESPResponse forbidden;
        forbidden.status = 403;
        forbidden.body = "<html><body>403 - Cross-origin form submission rejected.</body></html>";
        SendResponse(forbidden);
        return;
    }

    sLog.Debug("ESP", "%s %s (%s)", _request.method.c_str(), _request.path.c_str(), username.c_str());

    ESPResponse response;
    sESPServer.ServeRequest(_request, response);
    SendResponse(response);
}

bool ESPServerConnection::IsSameOrigin() const
{
    if (_request.host.empty())
        return false;

    // Origin is the reliable signal when present; it is "scheme://host[:port]".
    if (!_request.origin.empty())
        return (HostOf(_request.origin) == _request.host);

    // Older browsers only send Referer on same-origin form posts.
    if (!_request.referer.empty())
        return (HostOf(_request.referer) == _request.host);

    // Neither header: cannot establish that this came from our own page.
    return false;
}

std::string ESPServerConnection::HostOf(const std::string& url)
{
    // "http://host:port/path" -> "host:port"
    const size_t schemeEnd = url.find("://");
    const size_t hostStart = ((schemeEnd == std::string::npos) ? 0 : (schemeEnd + 3));
    const size_t hostEnd = url.find('/', hostStart);

    return ((hostEnd == std::string::npos) ? url.substr(hostStart)
                                           : url.substr(hostStart, hostEnd - hostStart));
}

void ESPServerConnection::SendUnauthorized()
{
    ESPResponse response;
    response.status = 401;
    response.body =
        "<html><head><title>401 - Authentication required</title></head>"
        "<body style=\"background:#1b1d21;color:#d5d8dc;font-family:Verdana,sans-serif\">"
        "<h1>EVEmu Server Pages</h1>"
        "<p>Sign in with an EVEmu account that holds a GM role.</p>"
        "</body></html>";

    // The realm makes the browser prompt rather than just showing the body.
    std::ostringstream out;
    out << "HTTP/1.1 401 Unauthorized\r\n";
    out << "WWW-Authenticate: Basic realm=\"EVEmu Server Pages\"\r\n";
    out << "Content-Type: text/html; charset=utf-8\r\n";
    out << "Content-Length: " << response.body.length() << "\r\n";
    out << "Connection: close\r\n";
    out << "\r\n";
    out << response.body;

    _responseData = out.str();
    boost::asio::async_write(_socket, boost::asio::buffer(_responseData), boost::asio::transfer_all(),
        std::bind(&ESPServerConnection::OnWriteComplete, shared_from_this()));
}

void ESPServerConnection::SendResponse(const ESPResponse& response)
{
    const char* reason = "OK";
    switch (response.status) {
        case 302: reason = "Found";                 break;
        case 400: reason = "Bad Request";           break;
        case 403: reason = "Forbidden";             break;
        case 404: reason = "Not Found";             break;
        case 405: reason = "Method Not Allowed";    break;
        case 413: reason = "Payload Too Large";     break;
        case 500: reason = "Internal Server Error"; break;
        default:  reason = "OK";                    break;
    }

    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << " " << reason << "\r\n";
    if (!response.redirectTo.empty())
        out << "Location: " << response.redirectTo << "\r\n";
    out << "Content-Type: " << response.contentType << "\r\n";
    out << "Content-Length: " << response.body.length() << "\r\n";
    // These pages are GM tooling; keep them out of caches and out of frames.
    out << "Cache-Control: no-store\r\n";
    out << "X-Frame-Options: DENY\r\n";
    out << "X-Content-Type-Options: nosniff\r\n";
    out << "Connection: close\r\n";
    out << "\r\n";
    if (_request.method != "HEAD")
        out << response.body;

    _responseData = out.str();
    boost::asio::async_write(_socket, boost::asio::buffer(_responseData), boost::asio::transfer_all(),
        std::bind(&ESPServerConnection::OnWriteComplete, shared_from_this()));
}

void ESPServerConnection::OnWriteComplete()
{
    boost::system::error_code ignored;
    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    _socket.close(ignored);
}
