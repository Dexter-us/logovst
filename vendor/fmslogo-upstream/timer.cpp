/*
 *
 *       Copyright (C) 1995 by the Regents of the University of California
 *       Copyright (C) 1995 by George Mills
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
    #include <wx/timer.h>

    #include "fmslogo.h"
    #include "logoeventqueue.h"
    #include "argumentutils.h"
    #include "lists.h"
    #include "error.h"
    #include "init.h" // Unbound
    #include "eval.h" // stopping_flag

#ifdef FMSLOGO_WXWIDGETS
    #include "mainframe.h" // PostCheckQueueMessage
#endif

    #include "debugheap.h"
//    #include "internationalizedstrings.h"
#endif

// global variables
static wchar_t       * timer_callback[MAX_TIMERS]; // timer cb malloc'd as needed
static class wxTimer * timers[MAX_TIMERS];         // timer objects

class CLogoTimer : public wxTimer
{
public:
    CLogoTimer(int TimerId)
        : m_TimerId(TimerId)
    {
        assert(1 <= m_TimerId && m_TimerId < 32);
    }

private:
    virtual void Notify()
    {
        assert(1 <= m_TimerId && m_TimerId < 32);

        // Queue the corresponding timer callback to be executed (in order).
        callthing * callevent;
        wchar_t * callback = timer_callback[m_TimerId];
        if  (m_TimerId <= 16)
        {
            // not safe to yield
            callevent = callthing::CreateNoYieldFunctionEvent(callback);
        }
        else
        {
            // yieldable
            callevent = callthing::CreateFunctionEvent(callback);
        }

        calllists.insert(callevent);

        // In case FMSLogo is idle, post a message to check the queue.
#ifdef FMSLOGO_WXWIDGETS
        CFmsLogo::GetMainFrame()->PostCheckQueueMessage();
#else
	checkqueue();
#endif
    }

    // private members
    int m_TimerId;
};


static void halt_timer(int id)
{
    if (timers[id] != NULL)
    {
        timers[id]->Stop();
        delete timers[id];
        timers[id] = NULL;
    }
}

NODE *lsettimer(NODE *args)
{
    // get the timer id
    int id = getint(ranged_integer_arg(args, 1, MAX_TIMERS - 1));

    // get delay
    int delay = getint(nonnegative_int_arg(args = cdr(args)));

    // get callback
    wchar_t callback[MAX_BUFFER_SIZE];
    cnv_strnode_string(callback, args = cdr(args));

    if (NOT_THROWING)
    {
        if (timer_callback[id] == NULL)
        {
            timer_callback[id] = static_cast<wchar_t *>(malloc(sizeof(callback)));
        }
        wcscpy(timer_callback[id], callback);

        // delete any timer that may exist
        halt_timer(id);

        // if not set sucessfully error
        timers[id] = new CLogoTimer(id);
        if (!timers[id]->Start(delay))
        {
            err_logo(OUT_OF_MEM, NIL);
            return Unbound;
        }
    }

    return Unbound;
}

NODE *lcleartimer(NODE *args)
{
    // get args
    int id = getint(ranged_integer_arg(args, 1, MAX_TIMERS - 1));
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    // if timer was not set let user know
    if (timers[id] == NULL)
    {
        err_logo(TIMER_NOT_FOUND, NIL);
        return Unbound;
    }

    // halt the timer
    halt_timer(id);

    return Unbound;
}

void init_timers()
{
    for (size_t i = 0; i < MAX_TIMERS; i++)
    {
        timer_callback[i] = NULL;
        timers[i]         = NULL;
    }
}

void halt_all_timers()
{
    for (int i = 1; i < MAX_TIMERS; i++)
    {
        halt_timer(i);
    }
}

void uninitialize_timers()
{
    halt_all_timers();

    for (int id = 0; id < MAX_TIMERS; id++)
    {
        if (timer_callback[id] != NULL)
        {
            free(timer_callback[id]);
            timer_callback[id] = NULL;
        }
    }
}
