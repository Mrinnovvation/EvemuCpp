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

#ifndef __ESPSERVERLISTENER_H_INCL__
#define __ESPSERVERLISTENER_H_INCL__

#include "espserver/ESPServerConnection.h"

/**
 * \class ESPServerListener
 *
 * @brief Accepts connections for the ESP server.
 */
class ESPServerListener
{
    typedef boost::asio::ip::tcp proto;
public:
    ESPServerListener(boost::asio::io_context& io);
    ~ESPServerListener();

private:
    void StartAccept();
    void HandleAccept(std::shared_ptr<ESPServerConnection> connection);

#if BOOST_VERSION >= 107400
    boost::asio::basic_socket_acceptor<proto>* _acceptor;
#else
    proto::acceptor* _acceptor;
#endif
};

#endif  // __ESPSERVERLISTENER_H_INCL__
