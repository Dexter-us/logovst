/*
 *  Copyright (C) 1995 by the Regents of the University of California
 *  Copyright (C) 1995 by George Mills
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include <stdlib.h>
   #include <string.h> // strdup
   #include <cwchar>

   #include "logoeventqueue.h"

   #include "logodata.h"
   #include "eval.h"
   #include "wrksp.h"
   #include "init.h"
   #include "netwind.h"
   #include "devwind.h"
   #include "debugheap.h"
#endif

calllist calllists;

// inserts an element at the front of the list.
void qlist::insert(void * a)
{
    assert(a != NULL && "invalid parameter");

    // class "event list queue" member to insert event
    if (last)
    {
        // list has at least one element.
        qlink * previousHead = last->next;
        qlink * head = new qlink(a, NULL, NULL);
        last->next = head;
        head->prev = last;
        head->next = previousHead;
        previousHead->prev = head;

        last = last->next;
    }
    else
    {
        // list was empty, so this is the new last node
        last = new qlink(a, NULL, NULL);
        last->next = last;
        last->prev = last;
    }
}

// returns the first element in the list.
// returns NULL if the list is empty.
void * qlist::get()
{
    if (last == NULL)
    {
        return NULL;
    }

    return last->next->e;
}

// removes the first item in the list
void qlist::zap()
{
    if (last == NULL)
    {
        return;
    }

    qlink * p = last->next;

    if (last == p)
    {
        // this was the only element in the list
        last = NULL;
    }
    else
    {
        p->prev->next = p->next;
        p->next->prev = p->prev;
    }

    delete p;
}

// deletes the list structure, but does not modify any of the list elements.
void qlist::clear()
{
    qlink *l = last;

    if (l == NULL)
    {
        return;
    }

    do
    {
        qlink *ll = l;
        l = l->next;
        delete ll;
    } while (l != last);
}

callthing * callthing::CreateKeyboardEvent(wchar_t * function, int key)
{
    callthing * callevent = new callthing;
    callevent->kind = EVENTTYPE_Keyboard;
    callevent->func = function;
    callevent->arg1 = key;

    return callevent;
}

callthing * callthing::CreateMouseEvent(wchar_t * function, int x, int y)
{
    callthing * callevent = new callthing;
    callevent->kind = EVENTTYPE_Mouse;
    callevent->func = function;
    callevent->arg1 = x;
    callevent->arg2 = y;

    return callevent;
}

callthing * callthing::CreateFunctionEvent(wchar_t * function)
{
    callthing * callevent = new callthing;
    callevent->kind = EVENTTYPE_YieldFunction;
    callevent->func = function;

    return callevent;
}

callthing * callthing::CreateNoYieldFunctionEvent(wchar_t * function)
{
    callthing * callevent = new callthing;
    callevent->kind = EVENTTYPE_NoYieldFunction;
    callevent->func = function;

    return callevent;
}

callthing * callthing::CreateNetworkReceiveReadyEvent(
    CNetworkConnection * NetworkConnection,
    const wchar_t      * OnReadyReceiveCallback,
    const char         * Packet,
    size_t               PacketLength
    )
{
    callthing * callevent = new callthing;
    callevent->kind = EVENTTYPE_NetworkReceiveReady;

    callevent->networkconnection = NetworkConnection;

    // Copy OnReceiveReadyCallback now because it might be freed
    // by the time the event is processed.
    callevent->func = wcsdup(OnReadyReceiveCallback);

    // Copy the network packet into the event (instead of into NetworkConnection)
    // so that it can be processed in the order in which is was received.
    callevent->networkpacket = static_cast<char*>(malloc(PacketLength + 1));
    memcpy(callevent->networkpacket, Packet, PacketLength);
    callevent->networkpacket[PacketLength] = '\0';

    return callevent;
}

callthing::~callthing()
{
    switch (kind)
    {
    case EVENTTYPE_NetworkReceiveReady:
        free(func);
        free(networkpacket);
        break;

    case EVENTTYPE_Unknown:
    case EVENTTYPE_Mouse:
    case EVENTTYPE_Keyboard:
    case EVENTTYPE_YieldFunction:
    case EVENTTYPE_NoYieldFunction:
        // These event types have no special cleanup
        break;
    }
}

NODE *leventcheck(NODE *)
{
#ifndef WX_PURE
    // checkqueue();
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif

    return Unbound;
}

// function that processes our own queued events
void checkqueue()
{
    while (callthing * thing = calllists.get())
    {
        bool    save_yield_flag   = yield_flag;
        FIXNUM  save_value_status = g_ValueStatus;

        calllists.zap();
        switch (thing->kind)
        {
        case EVENTTYPE_Mouse:
            // mouse event must not yield while processing
            yield_flag = false;
            mouse_posx = thing->arg1;
            mouse_posy = thing->arg2;
            do_execution(thing->func);
            break;

        case EVENTTYPE_Keyboard:
            // keyboard event must not yield while processing
            yield_flag = false;
            keyboard_value = thing->arg1;
            do_execution(thing->func);
            break;

        case EVENTTYPE_YieldFunction:
            // Button, timer or other event ok to yield while processing
            do_execution(thing->func);
            break;

        case EVENTTYPE_NoYieldFunction:
            // Scrollbar, MCI, Net, timer or other event must not yield while processing
            yield_flag = false;
            do_execution(thing->func);
            break;

#ifndef WX_PURE
        case EVENTTYPE_NetworkReceiveReady:
            // Network events must not yield while processing
            yield_flag = false;

            // use the new value
            thing->networkconnection->SetLastPacketReceived(thing->networkpacket);
            thing->networkpacket = NULL;

            do_execution(thing->func);
#endif
            break;

        default:
            assert(0 && "bad callthing type");
            break;
        }

        delete thing;

        yield_flag    = save_yield_flag;
        g_ValueStatus = save_value_status;
    }
}

/* function to dump the queue */
void emptyqueue()
{
    while (callthing * thing = calllists.get())
    {
        calllists.zap();

        delete thing;
    }
}
