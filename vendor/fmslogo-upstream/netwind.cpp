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

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
    #include "netwind.h"
    #include "logoeventqueue.h"

    #include "init.h"
    #include "argumentutils.h"
    #include "stringprintednode.h"
    #include "logodata.h"
    #include "logorc.h"
    #include "lists.h"
    #include "eval.h"
    #include "logomath.h"
    #include "parse.h"
    #include "screenwindow.h"
    #include "legacyansi.h"
    #include "error.h"
    #include "print.h"
    #include "debugheap.h"
    #include "internationalizedstrings.h"
#endif


/////////////////////////////////////////////////////////////////////////////////////
// Global Variables

CClientNetworkConnection g_ClientConnection;
CServerNetworkConnection g_ServerConnection;

static int network_dns_sync = 0;

static bool network_is_started = false;


/////////////////////////////////////////////////////////////////////////////////////
// Helper Functions

// converts winsock errorcode to string
// TODO: Use ::FormatMessage so that the error messages come from Windows
// instead of being translated by FMSLogo volunteers.
static
LPCWSTR WSAGetLastErrorString(int error_arg)
{
    int error;

    if (error_arg != 0)
    {
        error = error_arg;
    }
    else
    {
        error = WSAGetLastError();
    }

    switch (error)
    {
    case WSAENAMETOOLONG:
        return g_LOCALIZED_ERROR_NETWORKNAMETOOLONG;

    case WSANOTINITIALISED:
        return g_LOCALIZED_ERROR_NETWORKNOTINITIALIZED;

    case WSASYSNOTREADY:
        return g_LOCALIZED_ERROR_NETWORKSYSTEMNOTREADY;

    case WSAVERNOTSUPPORTED:
        return g_LOCALIZED_ERROR_NETWORKVERSIONNOTSUPPORTED;

    case WSAESHUTDOWN:
        return g_LOCALIZED_ERROR_NETWORKSENDAFTERSHUTDOWN;

    case WSAEINTR:
        return g_LOCALIZED_ERROR_NETWORKINTERRUPTED;

    case WSAHOST_NOT_FOUND:
        return g_LOCALIZED_ERROR_NETWORKHOSTNOTFOUNDAUTH;

    case WSATRY_AGAIN:
        return g_LOCALIZED_ERROR_NETWORKHOSTNOTFOUNDNONAUTH;

    case WSANO_RECOVERY:
        return g_LOCALIZED_ERROR_NETWORKNOTRECOVERABLE;

    case WSANO_DATA:
        return g_LOCALIZED_ERROR_NETWORKNODATA;

    case WSAEBADF:
        return g_LOCALIZED_ERROR_NETWORKBADFILENUMBER;

    case WSAEWOULDBLOCK:
        return g_LOCALIZED_ERROR_NETWORKWOULDBLOCK;

    case WSAEINPROGRESS:
        return g_LOCALIZED_ERROR_NETWORKNOWINPROGRESS;

    case WSAEALREADY:
        return g_LOCALIZED_ERROR_NETWORKALREADYINPROGRESS;

    case WSAEFAULT:
        return g_LOCALIZED_ERROR_NETWORKBADADDRESS;

    case WSAEDESTADDRREQ:
        return g_LOCALIZED_ERROR_NETWORKNEEDDESTADDRESS;

    case WSAEMSGSIZE:
        return g_LOCALIZED_ERROR_NETWORKMESSAGETOOLONG;

    case WSAEPFNOSUPPORT:
        return g_LOCALIZED_ERROR_NETWORKPFNOTSUPPORTED;

    case WSAENOTEMPTY:
        return g_LOCALIZED_ERROR_NETWORKDIRNOTEMPTY;

    case WSAEPROCLIM:
        return g_LOCALIZED_ERROR_NETWORKAPPLIMITREACHED;

    case WSAEDQUOT:
        return g_LOCALIZED_ERROR_NETWORKDISKQUOTA;

    case WSAEINVAL:
        return g_LOCALIZED_ERROR_NETWORKINVALIDARGUMENTS;

    case WSAEMFILE:
        return g_LOCALIZED_ERROR_NETWORKTOOMANYFILES;

    case WSAEACCES:
        return g_LOCALIZED_ERROR_NETWORKACCESSDENIED;

    case WSAENOTSOCK:
        return g_LOCALIZED_ERROR_NETWORKNOTASOCKET;

    case WSAEADDRNOTAVAIL:
        return g_LOCALIZED_ERROR_NETWORKADDRNOTAVAILABLE;

    case WSAEADDRINUSE:
        return g_LOCALIZED_ERROR_NETWORKADDRINUSE;

    case WSAEAFNOSUPPORT:
        return g_LOCALIZED_ERROR_NETWORKAFNOTSUPPORTED;

    case WSAESOCKTNOSUPPORT:
        return g_LOCALIZED_ERROR_NETWORKTYPENOTSUPPORTED;

    case WSAEPROTONOSUPPORT:
        return g_LOCALIZED_ERROR_NETWORKPROTONOTSUPPORTED;

    case WSAENOBUFS:
        return g_LOCALIZED_ERROR_NETWORKNOBUFFER;

    case WSAETIMEDOUT:
        return g_LOCALIZED_ERROR_NETWORKTIMEDOUT;

    case WSAEISCONN:
        return g_LOCALIZED_ERROR_NETWORKALREADYCONNECTED;

    case WSAENOTCONN:
        return g_LOCALIZED_ERROR_NETWORKNOTCONNECTED;

    case WSAENOPROTOOPT:
        return g_LOCALIZED_ERROR_NETWORKBADPROTOOPT;

    case WSAECONNRESET:
        return g_LOCALIZED_ERROR_NETWORKCONNECTIONRESET;

    case WSAECONNABORTED:
        return g_LOCALIZED_ERROR_NETWORKCONNECTIONABORT;

    case WSAENETDOWN:
        return g_LOCALIZED_ERROR_NETWORKISDOWN;

    case WSAENETRESET:
        return g_LOCALIZED_ERROR_NETWORKRESET;

    case WSAECONNREFUSED:
        return g_LOCALIZED_ERROR_NETWORKCONNECTIONREFUSED;

    case WSAEHOSTDOWN:
        return g_LOCALIZED_ERROR_NETWORKHOSTISDOWN;

    case WSAEHOSTUNREACH:
        return g_LOCALIZED_ERROR_NETWORKHOSTISUNREACHABLE;

    case WSAEPROTOTYPE:
        return g_LOCALIZED_ERROR_NETWORKBADPROTOTYPE;

    case WSAEOPNOTSUPP:
        return g_LOCALIZED_ERROR_NETWORKBADOPNOTSUPPORTED;

    case WSAENETUNREACH:
        return g_LOCALIZED_ERROR_NETWORKUNREACHABLE;

    case WSAETOOMANYREFS:
        return g_LOCALIZED_ERROR_NETWORKTOOMANYREFS;

    default:
        {
            static wchar_t buffer[32];
            swprintf(buffer, L"%ls %d", g_LOCALIZED_UNKNOWN, error);
            return buffer;
        }
    }
}

// Formats an error message directly to the commander.
//
// This should only be used for errors happens asynchronously
// from the Logo procedure, such that throwing a throwing an error
// is not appropriate.
//
// LogoFormatString -
//     A Logo format string.  This may have up to one %t format parameter,
//     which will be replaced with the FormatParameter.
//     This should end in a newline.
//
// WsaErrorCode -
//     An error code from Winsock to convert to a string and use to replace
//     the first %t in LogoFormatString.  Pass in 0 to use the last WSA error.
static
void
PrintNetworkError(
    const wchar_t * LogoFormatString,
    int             WsaError
    )
{
    ndprintf(
        stdout,                           // write directly to the commander history.
        ENCODING_NotApplicable,           // not writing to a file.
        MESSAGETYPE_Error,                // print the message as an error
        false,                            // format for display
        LogoFormatString,                 // format string
        WSAGetLastErrorString(WsaError)); // parameter
}

static
void
safe_free(
    char * & buffer
    )
{
    free(buffer);
    buffer = NULL;
}

static
void
safe_free(
    wchar_t * & buffer
    )
{
    free(buffer);
    buffer = NULL;
}

static
void
safe_closesocket(
    SOCKET & socket
    )
{
    if (socket != INVALID_SOCKET)
    {
        closesocket(socket);
        socket = INVALID_SOCKET;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// Helper class CCarryOverBuffer.
//
// This class helps with buffering (carrying over) network data
// gathered from one "recv()" call to another.

class CCarryOverBuffer
{
public:
    CCarryOverBuffer();
    ~CCarryOverBuffer();

    void   EnsureSpace(int DesiredSize);
    void   ShiftLeft(int ShiftAmount);
    char * GetBufferEnd();

    char * m_Buffer;
    int    m_BufferSize;
    int    m_BytesOfData;
};

CCarryOverBuffer::CCarryOverBuffer() :
    m_Buffer(NULL),
    m_BufferSize(0),
    m_BytesOfData(0)
{
}

CCarryOverBuffer::~CCarryOverBuffer()
{
    free(m_Buffer);
}

void
CCarryOverBuffer::EnsureSpace(int DesiredSize)
{
    int totalAdditionalBytesNeeded = DesiredSize - m_BufferSize + m_BytesOfData;
    if (0 < totalAdditionalBytesNeeded)
    {
        // There isn't enough free space in the buffer. Grow it.
        m_BufferSize += totalAdditionalBytesNeeded;
        m_Buffer = (char*) realloc(m_Buffer, m_BufferSize);
    }
}

// A pointer to the location where the buffer ends.
char *
CCarryOverBuffer::GetBufferEnd()
{
    return m_Buffer + m_BytesOfData;
}

void
CCarryOverBuffer::ShiftLeft(
    int ShiftAmount
    )
{
    assert(0 <= ShiftAmount);
    assert(ShiftAmount <= m_BytesOfData);

    // shift the buffer such that it begins at "begin"
    memmove(m_Buffer, m_Buffer + ShiftAmount, m_BytesOfData - ShiftAmount);

    m_BytesOfData -= ShiftAmount;
}

/////////////////////////////////////////////////////////////////////////////////////
// class CNetworkConnection

CNetworkConnection::CNetworkConnection() :
    m_ConnectionSocket(INVALID_SOCKET),
    m_IsConnected(false),
    m_IsBusy(false),
    m_IsEnabled(false),
    m_OnReceiveReady(NULL),
    m_OnSendReady(NULL),
    m_ReceiveValue(NULL)
{
}

void
CNetworkConnection::SetLastPacketReceived(
    char * LastPacket
    )
{
    free(m_ReceiveValue);
    m_ReceiveValue = LastPacket;
}

NODE*
CNetworkConnection::GetLastPacketReceived() const
{
    if (m_ReceiveValue == NULL)
    {
        return NIL;
    }
    else
    {
        // Convert the receive buffer into a NODE.
        wchar_t * receiveValueAsUnicode = ansi_to_unicode(m_ReceiveValue);
        NODE* targ = make_strnode(receiveValueAsUnicode);
        free(receiveValueAsUnicode);

        // Parse the NODE to tokenize it.
        NODE* val = parser(targ, false);
        return val;
    }
}

bool
CNetworkConnection::IsEnabled() const
{
    return m_IsEnabled;
}

void
CNetworkConnection::Disable()
{
    m_IsEnabled    = false;
    m_IsConnected  = false;
    m_IsBusy       = false;

    safe_free(m_ReceiveValue);

    safe_closesocket(m_ConnectionSocket);
}

void
CNetworkConnection::Enable(
    const wchar_t *  OnSendReady,
    const wchar_t *  OnReceiveReady
    )
{
    // copy the receive ready callback
    if (m_OnReceiveReady == NULL)
    {
        m_OnReceiveReady = static_cast<wchar_t *>(malloc(sizeof(wchar_t) * MAX_BUFFER_SIZE));
    }
    wcscpy(m_OnReceiveReady, OnReceiveReady);

    // copy the send ready callback
    if (m_OnSendReady == NULL)
    {
        m_OnSendReady = static_cast<wchar_t *>(malloc(sizeof(wchar_t) * MAX_BUFFER_SIZE));
    }
    wcscpy(m_OnSendReady, OnSendReady);
}

bool
CNetworkConnection::SendValue(
    const char * Data
    )
{
    if (m_IsConnected && !m_IsBusy)
    {
        // send the data
        int rval = send(m_ConnectionSocket, Data, strlen(Data) + 1, 0);
        if (rval == SOCKET_ERROR)
        {
            int lastError = WSAGetLastError();
            if (lastError != WSAEWOULDBLOCK)
            {
                err_logo(
                    NETWORK_GENERAL,
                    make_strnode(WSAGetLastErrorString(lastError)));
                return false;
            }

            // Don't send anymore until we receive confirmation
            // that the remote side received the data.
            m_IsBusy = true;
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

void
CNetworkConnection::AsyncReceive(
    const wchar_t * ErrorFormatString
    )
{
    const int READ_SIZE = 1024;

    CCarryOverBuffer carryOverData;

    for (;;)
    {
        // In case we have some data left from the last time we were called, we
        // always read new data from the socket at the end of the carry-over buffer.

        // Make sure that there's enough space to hold the number of bytes
        // that we want to read.
        carryOverData.EnsureSpace(READ_SIZE);

        // Read the data from socket directly into the carry-over buffer.
        int status = recv(
            m_ConnectionSocket,
            carryOverData.GetBufferEnd(),
            READ_SIZE,
            0);
        if (status == SOCKET_ERROR)
        {
            // if this would block, we just wait until we get called again
            int wsaError = WSAGetLastError();
            if (wsaError != WSAEWOULDBLOCK)
            {
                PrintNetworkError(ErrorFormatString, wsaError);
            }
            break;
        }
        else if (status == 0)
        {
            // The remote host has closed the connect, so there's
            // no more data to read.
            break;
        }
        else
        {
            // We received some data.  Make a note of it.
            carryOverData.m_BytesOfData += status;

            // Now queue up a separate windows message for each framed Logo packet.
            // Logo packets are framed by the NUL character.
            const char * packetBegin = carryOverData.m_Buffer;
            const char * packetLimit = carryOverData.GetBufferEnd();
            for (const char * packetEnd = packetBegin;
                 packetEnd < packetLimit;
                 packetEnd++)
            {
                if (*packetEnd == '\0')
                {
                    // This is the framing byte we've been looking for.
                    // Create the "receive ready" event for this frame.
                    PostNetworkReceiveReadyEvent(
                        packetBegin,
                        packetEnd - packetBegin);

                    // Advance past the NUL to start looking for the end
                    // of the next frame.  Note that packetEnd will
                    // advance by the for loop increment.
                    packetBegin = packetEnd + 1;
                }
            }

            // Everything which remains is a partial frame.
            // Shift the partial frame to the beginning of the buffer for later use.
            carryOverData.ShiftLeft(packetBegin - carryOverData.m_Buffer);
        }
    }

    // In order to give FMSLogo a small chance at interoperating with protocols
    // that don't insert a NUL framing byte (such as HTTP), we make any partial
    // frames available to the FMSLogo programmer.
    //
    // As a result, partial frames which arrive.
    if (carryOverData.m_BytesOfData != 0)
    {
        PostNetworkReceiveReadyEvent(
            carryOverData.m_Buffer,
            carryOverData.m_BytesOfData);
    }
}

// This is called in response to an FD_CLOSE, which happens with the remote
// host initiates a half-close.  In this case, TCP would continue to allow
// us to send, but MSWLogo has always prevented this and the API doesn't
// make it clear what should be possible.
void
CNetworkConnection::AsyncClose()
{
    m_IsConnected = false;
}

void
CNetworkConnection::Shutdown()
{
    Disable();

    safe_free(m_OnReceiveReady);
    safe_free(m_OnSendReady);
    SetLastPacketReceived(NULL);
}

void
CNetworkConnection::PostOnSendReadyEvent()
{
    callthing *callevent = callthing::CreateNoYieldFunctionEvent(m_OnSendReady);

    calllists.insert(callevent);
    PostMessage(GetMainWindow(), WM_CHECKQUEUE, 0, 0);
}

void
CNetworkConnection::PostNetworkReceiveReadyEvent(
    const char * Packet,
    size_t       PacketLength
    )
{
    callthing * callevent = callthing::CreateNetworkReceiveReadyEvent(
        this,
        m_OnReceiveReady,
        Packet,
        PacketLength);

    calllists.insert(callevent);
    PostMessage(GetMainWindow(), WM_CHECKQUEUE, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////////////
// CClientNetworkConnection class

CClientNetworkConnection::CClientNetworkConnection() :
    m_GetHostByNameTaskHandle(NULL)
{
    memset(m_RemoteHostEntry.Buffer, 0x00, sizeof(m_RemoteHostEntry.Buffer));
}

void
CClientNetworkConnection::Enable(
    const wchar_t * OnSendReady,
    const wchar_t * OnReceiveReady,
    unsigned int    RemotePort,
    const char    * RemoteHostName
    )
{
    CNetworkConnection::Enable(OnSendReady, OnReceiveReady);
    if (NOT_THROWING)
    {
        if (network_dns_sync == 1)
        {
            PHOSTENT hostEntry = gethostbyname(RemoteHostName);
            if (hostEntry == NULL)
            {
                err_logo(
                    NETWORK_GENERAL,
                    make_strnode(WSAGetLastErrorString(0)));
                return;
            }

            // This HOSTENTRY structure is owned by WinSock and
            // could be changed.  Therefore, we copy the information
            // that we need before any other WinSock calls can be made.
            // Note that this may result in some dangling pointers, but
            // this is okay, since the pointers will be valid long enough
            // to initialize the SOCKADDR_IN struct, which is all we use
            // it for.
            m_RemoteHostEntry.Entry = *hostEntry;
            m_RemotePort            = RemotePort;
            m_IsEnabled             = true;

            // Set up the rest of the connection by invoking the logic that
            // should be invoked once m_RemoteHostEntry is filled in.
            OnHaveRemoteHostAddress(0);
        }
        else
        {
            // Get address of remote machine asynchronously
            assert(m_GetHostByNameTaskHandle == NULL);
            m_GetHostByNameTaskHandle = WSAAsyncGetHostByName(
                GetMainWindow(),
                WM_NETWORK_ONHAVEREMOTEHOSTADDRESS,
                RemoteHostName,
                m_RemoteHostEntry.Buffer,
                sizeof(m_RemoteHostEntry.Buffer));
            if (m_GetHostByNameTaskHandle == NULL)
            {
                err_logo(
                    NETWORK_GENERAL,
                    make_strnode(WSAGetLastErrorString(0)));
                return;
            }

            m_RemotePort = RemotePort;
            m_IsEnabled  = true;

            // The rest of the connection will be set up when the
            // OnHaveRemoteHostAddress callback is invoked, after
            // m_RemoteHostEntry is filled in by WSAAsyncGetHostByName.
        }
    }
}

void CClientNetworkConnection::Disable()
{
    // Disable the connection socket
    CNetworkConnection::Disable();

    // Additionally cancel any asynchronous DNS lookups.
    if (m_GetHostByNameTaskHandle != NULL)
    {
        WSACancelAsyncRequest(m_GetHostByNameTaskHandle);
        m_GetHostByNameTaskHandle = NULL;
    }
}

// Call this to handle a WM_NETWORK_CLIENTSOCKETEVENT message.
// This handles an asynchronous select on the client connection.
void
CClientNetworkConnection::OnClientSocketEvent(LONG LParam)
{
    if (!IsEnabled())
    {
        // The network client side has been shut down.
        // This message must have been delayed.
        return;
    }

    switch (WSAGETSELECTEVENT(LParam))
    {
    case FD_CONNECT:
        if (WSAGETASYNCERROR(LParam) != 0)
        {
            // There was a connection error.
            // The API doesn't provide a way to communicate this
            // to the programmer, but we can at least print it
            // to the commander.
            PrintNetworkError(
                L"NETCONNECTON connection error: %t\n",
                WSAGETASYNCERROR(LParam));
        }
        else
        {
            // flag it's ok to start firing
            m_IsConnected = true;
            PostOnSendReadyEvent();
        }
        break;

    case FD_READ:
        AsyncReceive(L"recv(sendsock) error: %t\n");
        break;

    case FD_WRITE:
        // allow another frame to go out.
        m_IsBusy = false;
        PostOnSendReadyEvent();
        break;

    case FD_CLOSE:
        if (WSAGETASYNCERROR(LParam) != 0)
        {
            // Notify the programmer that the socket was closed
            // in an unusual way (reset, abort, etc.)
            PrintNetworkError(
                L"NETCONNECTON: socket closed in an unusual way: %t\n",
                WSAGETASYNCERROR(LParam));
        }

        // done
        AsyncClose();
        break;

    default:
        // We received an event that we didn't request.
        assert(0);
        break;
    }
}

// Call this to handle a WM_NETWORK_ONHAVEREMOTEHOSTADDRESS message.
// This handles when an asynchronous DNS query finishes.
void
CClientNetworkConnection::OnHaveRemoteHostAddress(LONG LParam)
{
    // The "GetHostByName" task has completed and so
    // no longer needs to be cancelled.
    m_GetHostByNameTaskHandle = NULL;

    if (WSAGETASYNCERROR(LParam) != 0)
    {
        PrintNetworkError(
            L"WSAAsyncGetHostByName error: %t\n",
            WSAGETASYNCERROR(LParam));
        return;
    }

    if (!IsEnabled())
    {
        // The client-side is not initialized.
        // This must be a delayed event coming in after shutdown.
        PrintNetworkError(g_LOCALIZED_ERROR_NETWORKSHUTDOWN, 0);
        return;
    }

    // initialize the socket
    m_ConnectionSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_ConnectionSocket == INVALID_SOCKET)
    {
        PrintNetworkError(L"socket(sendsock) error: %t\n", 0);
        return;
    }

    // watch for connect
    if (WSAAsyncSelect(
            m_ConnectionSocket,
            GetMainWindow(),
            WM_NETWORK_CLIENTSOCKETEVENT,
            FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE) == SOCKET_ERROR)
    {
        PrintNetworkError(L"WSAAsyncSelect(sendsock) error: %t\n", 0);
        return;
    }

    // connect to the remote host.
    SOCKADDR_IN send_dest_sin = {0};
    send_dest_sin.sin_family = AF_INET;
    memcpy(
        &send_dest_sin.sin_addr,
        m_RemoteHostEntry.Entry.h_addr,
        m_RemoteHostEntry.Entry.h_length);
    send_dest_sin.sin_port = htons(m_RemotePort); // Convert to network ordering

    // Because we called WSASyncSelect with FD_CONNECT, we expect this
    // call to fail with WSAEWOULDBLOCK.
    int rval = connect(
        m_ConnectionSocket,
        (PSOCKADDR) &send_dest_sin,
        sizeof send_dest_sin);
    if (rval == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
            PrintNetworkError(L"connect(sendsock) error: %t\n", 0);
            return;
        }
    }

    // Run the "sendready" instruction list to indicate that FMSLogo
    // has successfully invoked the connection request.
    PostOnSendReadyEvent();
}

/////////////////////////////////////////////////////////////////////////////////////
// CServerNetworkConnection class

CServerNetworkConnection::CServerNetworkConnection() :
    m_ListenSocket(INVALID_SOCKET),
    m_ConnectionIsWaitingToBeAccepted(false)
{
}

void
CServerNetworkConnection::Enable(
    const wchar_t * OnSendReady,
    const wchar_t * OnReceiveReady,
    unsigned int    ServerPort
    )
{
    CNetworkConnection::Enable(OnSendReady, OnReceiveReady);
    if (NOT_THROWING)
    {
        // initialize the listen socket
        m_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_ListenSocket == INVALID_SOCKET)
        {
            err_logo(NETWORK_GENERAL, make_strnode(WSAGetLastErrorString(0)));
            return;
        }

        // Associate an address with the socket. (bind)
        SOCKADDR_IN socket_address = {0};
        socket_address.sin_family      = AF_INET;
        socket_address.sin_addr.s_addr = INADDR_ANY;
        socket_address.sin_port        = htons(ServerPort);

        int rval = bind(
            m_ListenSocket,
            (struct sockaddr *) &socket_address,
            sizeof socket_address);
        if (rval == SOCKET_ERROR)
        {
            err_logo(NETWORK_GENERAL, make_strnode(WSAGetLastErrorString(0)));
            return;
        }

        // listen for connect, setting the backlog to 1 connection so that
        // we never accept concurrent connections (the FMSLogo API cannot
        // handle that).
        const int MAX_PENDING_CONNECTS = 1;
        if (listen(m_ListenSocket, MAX_PENDING_CONNECTS) == SOCKET_ERROR)
        {
            err_logo(NETWORK_GENERAL, make_strnode(WSAGetLastErrorString(0)));
            return;
        }

        // watch for when connect happens
        m_IsEnabled = true;

        rval = WSAAsyncSelect(
            m_ListenSocket,
            GetMainWindow(),
            WM_NETWORK_SERVERSOCKETEVENT,
            FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE);
        if (rval == SOCKET_ERROR)
        {
            err_logo(NETWORK_GENERAL, make_strnode(WSAGetLastErrorString(0)));
            return;
        }

        // Run the "sendready" instruction list to indicate that FMSLogo has
        // successfully invoked listen().
        PostOnSendReadyEvent();
    }
}

void
CServerNetworkConnection::Disable()
{
    // Disable the connection socket
    CNetworkConnection::Disable();

    // Additionally stop listening for new connections.
    safe_closesocket(m_ListenSocket);
    m_ConnectionIsWaitingToBeAccepted = false;
}

void
CServerNetworkConnection::AcceptNewConnection()
{
    assert(!m_IsConnected);
    assert(m_ConnectionSocket == INVALID_SOCKET); // we leaked a socket

    struct sockaddr acc_sin;
    int             acc_sin_len = sizeof acc_sin;

    m_ConnectionSocket = accept(m_ListenSocket, &acc_sin, &acc_sin_len);
    if (m_ConnectionSocket != INVALID_SOCKET)
    {
        // We have successfully accepted the new connection.
        m_IsConnected = true;

        // Invoke the "sendready" instructions list for the second time
        // to notify the programmer that a new connection has been established.
        PostOnSendReadyEvent();
    }
}

// Call this to handle a WM_NETWORK_SERVERSOCKETEVENT message.
// This handles an asynchronous select on the server connection.
void
CServerNetworkConnection::OnServerSocketEvent(
    LONG        LParam
    )
{
    if (!IsEnabled())
    {
        // The server-side is not initialized.
        // This must be a delayed event coming in after shutdown.
        return;
    }


    switch (WSAGETSELECTEVENT(LParam))
    {
    case FD_ACCEPT:
        // A new client has connected.
        if (m_IsConnected)
        {
            // We are already servicing a connection.
            // The NETACCEPT API only supports one connection at a time,
            // so set a flag to come back to this one once that connection
            // is done.
            m_ConnectionIsWaitingToBeAccepted = true;
        }
        else
        {
            // Accept the connection.
            AcceptNewConnection();
            if (!m_IsConnected)
            {
                PrintNetworkError(L"accept error: %t\n", 0);
            }
        }
        break;

    case FD_READ:
        // We have received some data from the client.
        AsyncReceive(L"NETACCEPTON recv error: %t\n");
        break;

    case FD_WRITE:
        // The socket is ready for writing.
        // Allow another frame to go out and run the "sendready" instruction
        // list to let the programmer know they can start sending.
        m_IsBusy = false;
        PostOnSendReadyEvent();
        break;

    case FD_CLOSE:
        if (WSAGETASYNCERROR(LParam) != 0)
        {
            // Notify the programmer that the socket was closed
            // in an unusual way (reset, abort, etc.)
            PrintNetworkError(
                L"NETACCEPTON: socket closed in an unusual way: %t\n",
                WSAGETASYNCERROR(LParam));
        }

        // The remote host has closed the socket.
        AsyncClose();

        // In order to make it possible for a single server to service multiple
        // clients, initiate a half-close in response.
        safe_closesocket(m_ConnectionSocket);

        if (m_ConnectionIsWaitingToBeAccepted)
        {
            // We have a connection that is waiting to be accepted.
            // We don't log errors because it may be that the client gave
            // up before we could get to it.  More important is that winsock
            // won't call FD_ACCEPT until we call accept() at least once,
            // so merely trying to accept the connection puts winsock back
            // into a good state.
            AcceptNewConnection();

            m_ConnectionIsWaitingToBeAccepted = false;
        }
        break;

    default:
        // We received a notification that we didn't request.
        assert(0);
        break;
    }
}


// startup network
NODE *lnetstartup(NODE *args)
{
    // check if already started
    if (network_is_started)
    {
        err_logo(NETWORK_ALREADY_STARTED, NIL);
        return Unbound;
    }

    if (args != NIL)
    {
        network_dns_sync = int_arg(args);
    }

    // tell winsock to wakeup
    WSADATA WSAData;
    int statusCode = WSAStartup(MAKEWORD(2,2), &WSAData);
    if (statusCode != 0)
    {
        err_logo(
            NETWORK_GENERAL,
            make_strnode(WSAGetLastErrorString(statusCode)));
        return Unbound;
    }

    network_is_started = true;
    return Unbound;
}

// put everything back to original state
NODE *lnetshutdown(NODE *)
{
    // cleanup receive
    g_ServerConnection.Shutdown();

    // cleanup send
    g_ClientConnection.Shutdown();

    // cleanup library
    if (network_is_started)
    {
        WSACleanup();
        network_is_started = false;
    }

    network_dns_sync = 0;
    return Unbound;
}

// enable receiving
NODE *lnetaccepton(NODE *args)
{
    // check sanity
    if (!network_is_started)
    {
        err_logo(NETWORK_NOT_STARTED, NIL);
        return Unbound;
    }

    if (g_ServerConnection.IsEnabled())
    {
        err_logo(NETWORK_SERVER_ALREADY_ON, NIL);
        return Unbound;
    }

    // get args (socket and callback)
    int port = getint(nonnegative_int_arg(args));

    wchar_t networksend[MAX_BUFFER_SIZE];
    cnv_strnode_string(networksend, cdr(args));

    wchar_t networkreceive[MAX_BUFFER_SIZE];
    cnv_strnode_string(networkreceive, cdr(cdr(args)));

    if (NOT_THROWING)
    {
        g_ServerConnection.Enable(networksend, networkreceive, port);
    }

    if (NOT_THROWING)
    {
        return Truex.GetNode();
    }

    return Unbound;
}

NODE *lnetacceptoff(NODE *)
{
    // tell handler not to do anything with messages for network receive
    g_ServerConnection.Disable();
    return Unbound;
}

NODE *lnetacceptreceivevalue(NODE *)
{
    // return current network value
    if (g_ServerConnection.IsEnabled())
    {
        return g_ServerConnection.GetLastPacketReceived();
    }

    return Unbound;
}

NODE *lnetconnectreceivevalue(NODE *)
{
    // return current network value
    if (g_ClientConnection.IsEnabled())
    {
        return g_ClientConnection.GetLastPacketReceived();
    }

    return Unbound;
}

NODE *lnetconnecton(NODE *args)
{
    // sanity check first
    if (!network_is_started)
    {
        err_logo(NETWORK_NOT_STARTED, NIL);
        return Unbound;
    }

    if (g_ClientConnection.IsEnabled())
    {
        err_logo(NETWORK_CLIENT_ALREADY_ON, NIL);
        return Unbound;
    }

    // get args (remotemachinename, socket, callback)
    CStringPrintedNode remotehostname(car(args));

    int remote_port = getint(nonnegative_int_arg(cdr(args)));

    wchar_t networksend[MAX_BUFFER_SIZE];
    cnv_strnode_string(networksend, cdr(cdr(args)));

    wchar_t networkreceive[MAX_BUFFER_SIZE];
    cnv_strnode_string(networkreceive, cdr(cdr(cdr(args))));

    if (NOT_THROWING)
    {
        char * ansiRemoteHostName = unicode_to_ansi(remotehostname);
        g_ClientConnection.Enable(
            networksend,
            networkreceive,
            remote_port,
            ansiRemoteHostName);
        free(ansiRemoteHostName);
    }

    if (NOT_THROWING)
    {
        return Truex.GetNode();
    }

    return Unbound;
}

NODE *lnetconnectoff(NODE *)
{
    // tell handler not to do anything with messages for network send
    g_ClientConnection.Disable();
    return Unbound;
}

static
NODE* sendvalue_helper(
    NODE               * Args,
    CNetworkConnection & NetworkConnection
    )
{
    // get args (data)
    CStringPrintedNode data(car(Args));

    if (NOT_THROWING)
    {
        // try to send data
        char * ansiData = unicode_to_ansi(data);
        bool isOk = NetworkConnection.SendValue(ansiData);
        free(ansiData);
        return true_or_false(isOk);
    }

    return Unbound;
}

NODE *lnetconnectsendvalue(NODE *args)
{
    return sendvalue_helper(args, g_ClientConnection);
}

NODE *lnetacceptsendvalue(NODE *args)
{
    return sendvalue_helper(args, g_ServerConnection);
}
