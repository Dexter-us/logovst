/*
 * Logo MIDI music primitives.
 *
 * Copyright (C) 2026 Dexter U.S.
 *
 * This file is part of the FMSLogo-based Logo MIDI project and is licensed
 * under the GNU General Public License, version 2 or (at your option) any
 * later version.
 */

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
#include "musiccommands.h"

#include "error.h"
#include "graphics.h"
#include "init.h"
#include "lists.h"
#include "logodata.h"
#include "logocore.h"
#include "logo_midi/music_api.hpp"
#endif

#include "logo_midi/music_api.hpp"

#include <exception>
#include <vector>

namespace {

logo_midi::MusicCommandSink *active_sink()
{
    logo_midi::MusicCommandSink *sink =
        logo_midi::current_music_command_sink();
    if (sink == nullptr)
    {
        err_logo(
            BAD_DATA,
            make_strnode(L"No active MIDI renderer is available."));
    }
    return sink;
}

NODE *music_error()
{
    return err_logo(BAD_DATA, make_strnode(L"Invalid MIDI command input."));
}

} // namespace

NODE *lmusicnote(NODE *args)
{
    NODE *pitch_node = ranged_integer_arg(args, 0, 127);
    FLONUM duration = positive_flonum_arg(cdr(args));
    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->note(static_cast<int>(getint(pitch_node)), duration);
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}

NODE *lmusicchord(NODE *args)
{
    NODE *pitch_list = car(args);
    FLONUM duration = positive_flonum_arg(cdr(args));
    if (!is_list(pitch_list) || pitch_list == NIL)
    {
        return err_logo(BAD_DATA, vref(pitch_list));
    }

    std::vector<int> pitches;
    for (NODE *item = pitch_list; item != NIL && NOT_THROWING; item = cdr(item))
    {
        NODE *pitch_node = cnv_node_to_intnode(vref(car(item)));
        if (pitch_node == Unbound)
        {
            return err_logo(BAD_DATA, vref(car(item)));
        }

        FIXNUM pitch = getint(pitch_node);
        gcref(pitch_node);
        if (pitch < 0 || 127 < pitch)
        {
            return err_logo(BAD_DATA, vref(car(item)));
        }
        pitches.push_back(static_cast<int>(pitch));
    }

    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->chord(pitches.data(), static_cast<int>(pitches.size()),
                            duration);
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}

NODE *lmusicrest(NODE *args)
{
    FLONUM duration = positive_flonum_arg(car(args));
    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->rest(duration);
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}

NODE *lmusiccc(NODE *args)
{
    NODE *controller = ranged_integer_arg(args, 0, 127);
    NODE *value = ranged_integer_arg(cdr(args), 0, 127);
    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->control_change(static_cast<int>(getint(controller)),
                                     static_cast<int>(getint(value)));
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}

NODE *lmusicprogram(NODE *args)
{
    NODE *program = ranged_integer_arg(args, 0, 127);
    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->program_change(static_cast<int>(getint(program)));
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}

NODE *lmusicvelocity(NODE *args)
{
    NODE *velocity = ranged_integer_arg(args, 1, 127);
    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->set_velocity(static_cast<int>(getint(velocity)));
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}

NODE *lmusicchannel(NODE *args)
{
    NODE *channel = ranged_integer_arg(args, 1, 16);
    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->set_channel(static_cast<int>(getint(channel)));
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}

NODE *lmusicgate(NODE *args)
{
    FLONUM gate = positive_flonum_arg(car(args));
    if (NOT_THROWING && 1.0 < gate)
    {
        return err_logo(BAD_DATA, vref(car(args)));
    }
    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->set_gate(gate);
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}

NODE *lmusictempo(NODE *args)
{
    FLONUM tempo = positive_flonum_arg(car(args));
    if (NOT_THROWING)
    {
        try
        {
            logo_midi::MusicCommandSink *sink = active_sink();
            if (sink != nullptr)
            {
                sink->set_tempo(tempo);
            }
        }
        catch (const std::exception &)
        {
            return music_error();
        }
    }
    return Unbound;
}
