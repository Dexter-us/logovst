// -*- c++ -*-
// Copyright (C) 1995 by the Regents of the University of California
// Copyright (C) 1995 by George Mills
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef __NETWND_H_
#define __NETWND_H_

#ifndef WX_PURE

#include <windows.h>
#include <winsock.h>

struct NODE;

// Abstract base class for network connections
class CNetworkConnection
{
public:
    bool IsEnabled() const;
    virtual void Disable();

    NODE * GetLastPacketReceived() const;
    void SetLastPacketReceived(char * LastPacket);
    void Shutdown();
    bool SendValue(const char * Data);

protected:
    CNetworkConnection(); // enforce abstract class

    void
    Enable(
        const wchar_t *    OnSendReady,
        const wchar_t *    OnReceiveReady
        );

    // private helper functions
    void AsyncReceive(const wchar_t * ErrorMessage);
    void AsyncClose();
    void PostOnSendReadyEvent();
    void PostNetworkReceiveReadyEvent(const char * Packet, size_t PacketLength);

    // private member variables
    SOCKET m_ConnectionSocket; // socket for the connection
    bool   m_IsConnected;      // socket is connected
    bool   m_IsBusy;           // socket is too busy to send
    bool   m_IsEnabled;        // if message processing is enabled for this socket

    wchar_t * m_OnReceiveReady;  // Buffer for receive callback
    wchar_t * m_OnSendReady;     // Buffer for send    callback
    char    * m_ReceiveValue;    // pointer to the last packet received
};

// Class for client network connections
class CClientNetworkConnection : public CNetworkConnection
{
private:
    HANDLE m_GetHostByNameTaskHandle;

    union _union_host_entry_buffer
    {
        HOSTENT  Entry;                    // The host entry
        char     Buffer[MAXGETHOSTSTRUCT]; // enough space for host entry
    } m_RemoteHostEntry;

    unsigned int m_RemotePort;

public:
    CClientNetworkConnection();

    void
    Enable(
        const wchar_t * OnSendReady,
        const wchar_t * OnReceiveReady,
        unsigned int    RemotePort,
        const char    * RemoteHostName
        );

    void Disable();

    void OnHaveRemoteHostAddress(LONG LParam);
    void OnClientSocketEvent(LONG LParam);
};


// Class for server network connections
class CServerNetworkConnection : public CNetworkConnection
{
public:
    CServerNetworkConnection();

    void
    Enable(
        const wchar_t * OnSendReady,
        const wchar_t * OnReceiveReady,
        unsigned int    ServerPort
        );

    void Disable();
    void OnServerSocketEvent(LONG LParam);

private:
    void AcceptNewConnection();

    SOCKET m_ListenSocket; // socket for the listen()
    bool   m_ConnectionIsWaitingToBeAccepted;
};

// function declarations
NODE *lnetaccepton(NODE *args);
NODE *lnetacceptoff(NODE *arg);
NODE *lnetacceptsendvalue(NODE *args);
NODE *lnetacceptreceivevalue(NODE *arg);
NODE *lnetconnecton(NODE *args);
NODE *lnetconnectoff(NODE *arg);
NODE *lnetconnectsendvalue(NODE *args);
NODE *lnetconnectreceivevalue(NODE *arg);
NODE *lnetstartup(NODE *args);
NODE *lnetshutdown(NODE *arg);

// global variables
extern CClientNetworkConnection g_ClientConnection;
extern CServerNetworkConnection g_ServerConnection;

#endif // WX_PURE

#endif // __NETWND_H_
