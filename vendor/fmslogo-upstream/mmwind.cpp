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
    #include <assert.h>
    #ifdef __WXMSW__
        #include <windows.h>
    #endif
    #include <wx/sound.h>
    #include <wx/time.h> // for wxGetUTCTimeMillis()

    #include "mmwind.h"
    #include "argumentutils.h"
    #include "stringprintednode.h"
    #include "logocore.h"
    #include "logodata.h"
    #include "lists.h"
    #include "error.h"
    #include "init.h"
    #include "eval.h"
    #include "parse.h"
    #include "ibmterm.h"
    #include "screenwindow.h" // for GetMainWindow()
    #include "graphics.h"     // for numeric_node_to_fixnum
    #include "debugheap.h"

    #include "devwind.h"

    #include "internationalizedstrings.h"
#endif

#if !defined WX_PURE

// global variables
wchar_t  mci_callback[MAX_BUFFER_SIZE]; // MCI callback code

static HMIDIOUT hMidiOut = 0;

#endif // WX_PURE


struct WAVEFILE {
    char    Riff[4];              // always 'RIFF'
    int32_t WaveSize;             // size of buffer from Wave onward
    char    Wave[4];              // always 'WAVE'
    char    FormatChunkHeader[4]; // always 'fmt '
    int32_t FormatChunkSize;      // always 16 for PCM
    int16_t FormatTag;            // 1 == PCM
    int16_t TotalChannels;
    int32_t SamplesPerSecond;     // sample rate
    int32_t BytesPerSecond;
    int16_t BlockAlign;
    int16_t BitsPerSample;
    char    DataChunkHeader[4];   // always 'data'
    int32_t DataChunkSize;
    int16_t Samples[0];           // everything that follows the header is data
};

static void tone(FIXNUM frequency, FIXNUM duration)
{
    assert(0 <= frequency);
    assert(0 <= duration);
    
    // Generate a sine wave tone
    const int32_t sampleRate    = 44100; // Sample rate in Hz
    const int32_t sampleSize    = 2;     // 16-bit audio
    const int32_t totalChannels = 1;     // mono

#ifdef __WXMSW__
    // If the duration is long, then it's more memory efficient to
    // loop it.
    const bool loop = 100 <= duration;
#else
    // wxSOUND_LOOP caused stability problems on Fedora 39,
    // so we create a wave file that's as long as the duration
    // and play it once.
    const bool loop = false;
#endif

    const int samplesPerCycle = frequency == 0 ?
        1 : // avoid division by zero
        static_cast<int>(sampleRate / frequency);

    int totalSamples;
    if (loop)
    {
        // To play a tone in a loop, we only need one cycle in the wave file.
        totalSamples = samplesPerCycle;
    }
    else
    {
        // Duration is given in milliseconds, so we have to divide by 1000
        // to get seconds.

        // totalSamples = sampleRate * duration / 1000
        int sampleRateTimesDuration;
        bool isOk = safe_multiply(
            sampleRate,
            duration,
            &sampleRateTimesDuration);
        if (!isOk)
        {
            // This value would be used to allocate a buffer.  If the product
            // can't be held in an int, the buffer shouldn't be allocated.
            err_logo(OUT_OF_MEM, NIL);
            return;
        }

        totalSamples = sampleRateTimesDuration / 1000;
    }

    // totalSamples <= INT_MAX / 1000 and sampleSize==2,
    // so this math can't overflow.
    const int pcmDataBufferSize = totalSamples * sampleSize;
    const size_t bufferSize = sizeof(WAVEFILE) + pcmDataBufferSize;
    void * buffer = malloc(bufferSize);
    if (buffer == NULL)
    {
        // Not enough memory
        err_logo(OUT_OF_MEM, NIL);
        return;
    }

    WAVEFILE * header = static_cast<WAVEFILE*>(buffer);
    header->Riff[0]  = 'R';
    header->Riff[1]  = 'I';
    header->Riff[2]  = 'F';
    header->Riff[3]  = 'F';
    header->WaveSize = bufferSize - 8;
    header->Wave[0]  = 'W';
    header->Wave[1]  = 'A';
    header->Wave[2]  = 'V';
    header->Wave[3]  = 'E';
    header->FormatChunkHeader[0] = 'f';
    header->FormatChunkHeader[1] = 'm';
    header->FormatChunkHeader[2] = 't';
    header->FormatChunkHeader[3] = ' ';
    header->FormatChunkSize      = 16;
    header->FormatTag            = 1; // WAVE_FORMAT_PCM
    header->TotalChannels        = totalChannels;
    header->SamplesPerSecond     = sampleRate;
    header->BytesPerSecond       = sampleRate * sampleSize * totalChannels;
    header->BlockAlign           = sampleSize * totalChannels;
    header->BitsPerSample        = sampleSize * CHAR_BIT;
    header->DataChunkHeader[0]   = 'd';
    header->DataChunkHeader[1]   = 'a';
    header->DataChunkHeader[2]   = 't';
    header->DataChunkHeader[3]   = 'a';
    header->DataChunkSize        = pcmDataBufferSize;

    // Fill the first part of the buffer with the wave form for a single cycle.
    int16_t * const samples      = header->Samples;
    int16_t * const samplesLimit = samples + totalSamples;
    int16_t * const cycleLimit   = samplesPerCycle < totalSamples ?
        samples + samplesPerCycle :
        samplesLimit;
    const double twoPiFrequency   = 2.0 * M_PI * frequency;
    const double secondsPerSample = 1.0 / sampleRate;
    double t = 0.0;
    for (int16_t * sample = samples; sample < cycleLimit; sample++)
    {
        // Each sample is computed as
        //    16384.0 * sin(sample_index * 2*PI * frequency / sample_rate)
        // 16384.0 is the amplitude of the wave (0x4000)
        // It can go to a max of 0x7FFF with 2 bytes per sample, but that is too loud.
        t += secondsPerSample;
        *sample = static_cast<int16_t>(16384.0 * std::sin(twoPiFrequency * t));
    }

    // Repeat the wave form for whatever remains in the buffer.
    // This is faster than recomputing it.
    for (int16_t * nextSampleBlock = cycleLimit;
         nextSampleBlock < samplesLimit;
         nextSampleBlock += samplesPerCycle)
    {
        int totalSamplesToCopy = std::min(
            samplesPerCycle,   // a full block
            static_cast<int>(samplesLimit - nextSampleBlock));  // remaining space
        memcpy(
            nextSampleBlock,
            samples,
            totalSamplesToCopy * sampleSize);
    }

    //
    // Play the generated sound wave
    //
    wxSound sound(bufferSize, buffer);

    if (duration < 100)
    {
        // Tones with short durations should be played synchronously because:
        // 1) They will halt quickly on their own.
        // 2) If played asynchronously, it's possible that they won't even
        //    start playing by the time they're stopped.
        sound.Play(wxSOUND_SYNC);
    }
    else
    {
        // The duration is long enough to want TONE to be haltable.
        wxLongLong endTime = wxGetUTCTimeMillis() + duration;
        sound.Play(wxSOUND_ASYNC | (loop ? wxSOUND_LOOP : 0));

        // Do a busy sleep so that a long TONE can be interrupted by a HALT.
        while (wxGetUTCTimeMillis() < endTime && !check_stop(true)
#if defined(__WXGTK__)
               && sound.IsPlaying() // wxSound::IsPlaying isn't defined on MSW
#endif
            )
        {
            if (wxGetUTCTimeMillis() + 10 < endTime)
            {
                // We're more than 10 ms away from the target time,
                // so we don't risk missing our target time by yielding.
                wxMilliSleep(1);  // yield
            }
        }

        sound.Stop();
    }

    free(buffer);
}


// Returns true iff arg is a valid input to SOUND.
// The input to SOUND must be a non-empty list with
// an even number of non-negative integer members.
static bool is_sound_arg(const NODE * arg)
{
    // input must be a non-empty list.
    if (arg == NIL || !is_list(arg))
    {
        return false;
    }

    // Check that the input is a list of integers.
    int total_arguments = 0;
    for (; arg != NIL; arg = cdr(arg))
    {
        ++total_arguments;

        NODE * integer = cnv_node_to_intnode(car(arg));
        if (integer == Unbound)
        {
            // arg is not purely a list of integers
            return false;
        }
        if (getint(integer) < 0)
        {
            // frequencies can't be negative
            return false;
        }
        gcref(integer);
    }

    if (odd_p(total_arguments))
    {
        // arg list does not have even number of members.
        return false;
    }

    return true;
}

// Repeatedly reads car(args) until is_good_arg() returns true.
// If is_good_arg() returns false, throws a recoverable BAD_DATA
// to let the user try to provide good input.
//
// Returns car(args), always.
// If the argument is bad, also sets the throwing flag.
static NODE * get_arg(NODE* args, bool (*is_good_arg) (const NODE *))
{
    NODE *arg = car(args);
    while (NOT_THROWING && !is_good_arg(arg))
    {
        setcar(args, err_logo(BAD_DATA, arg));
        arg = car(args);
    }
    return arg;
}

NODE *lsound(NODE *args)
{
    // Check that the input is a non-empty list of paired integers.
    NODE *arglist = get_arg(args, is_sound_arg);

    if (NOT_THROWING)
    {
        /* play each freq/duration pair */
        for (; arglist != NIL; arglist = cdr(cdr(arglist)))
        {
            int hertz    = int_arg(arglist);
            int duration = int_arg(cdr(arglist));

            /* play it */
            tone(hertz, duration);
        }
    }

    return Unbound;
}


NODE *ltone(NODE *args)
{
    NODE * pitchnode    = nonnegative_int_arg(args);
    NODE * durationnode = nonnegative_int_arg(cdr(args));

    if (NOT_THROWING)
    {
        FIXNUM pitch    = numeric_node_to_fixnum(pitchnode);
        FIXNUM duration = numeric_node_to_fixnum(durationnode);
        tone(pitch, duration);
    }

    return Unbound;
}


#if !defined WX_PURE

static
void
ThrowGeneralMidiError(
    UINT MidiError
    )
{
    // convert the error into a string
    wchar_t midiErrorBuffer[MAX_BUFFER_SIZE];
    midiOutGetErrorText(MidiError, midiErrorBuffer, ARRAYSIZE(midiErrorBuffer));

    // report the error
    err_logo(MIDI_GENERAL, make_strnode(midiErrorBuffer));
}


NODE *lmidiopen(NODE *args)
{
    if (hMidiOut != NULL)
    {
        // The device is already open.
        err_logo(MIDI_DEVICE_ALREADY_OPEN, NIL);
        return Unbound;
    }

    // The device is not already open, so open it.
    UINT id = MIDIMAPPER;

    if (args != NIL)
    {
        id = int_arg(args);
        if (stopping_flag == THROWING)
        {
            // non-integer input
            return Unbound;
        }
        if (id > midiOutGetNumDevs())
        {
            err_logo(MIDI_INVALID_DEVICE, NIL);
            return Unbound;
        }
    }

    MIDIOUTCAPS moc;
    UINT MidiError = midiOutGetDevCaps(id, &moc, sizeof(moc));
    if (!MidiError)
    {
        MidiError = midiOutOpen(&hMidiOut, id, NULL, 0L, 0L);
    }

    if (MidiError)
    {
        // report the midi error
        ThrowGeneralMidiError(MidiError);
        return Unbound;
    }

    NODE * targ = make_strnode(moc.szPname);
    NODE * val = parser(targ, false);
    return val;
}

NODE *lmidiclose(NODE *  /*args*/)
{
    if (hMidiOut == NULL)
    {
        // the MIDI device isn't open
        err_logo(MIDI_NOT_OPEN, NIL);
        return Unbound;
    }

    // Close the device
    UINT MidiError = midiOutClose(hMidiOut);
    hMidiOut = 0;

    if (MidiError)
    {
        ThrowGeneralMidiError(MidiError);
        return Unbound;
    }

    return Unbound;
}

NODE *lmidimessage(NODE *arg)
{
    union
    {
        long mylong;
        char mybyte[4];
    }
    bytetolong;

    if (hMidiOut == NULL)
    {
        // the MIDI device isn't open
        err_logo(MIDI_NOT_OPEN, NIL);
        return Unbound;
    }

    NODE * args = car(arg);

    if (!is_list(args) || (args == NIL))
    {
        // The input must be a list with something in it.
        err_logo(BAD_DATA_UNREC, args);
        return Unbound;
    }

    UINT MidiError;

    // if not system exclusive then use shortmsg else use longmsg
    if (int_arg(args) != 0xF0)
    {

        // pack 3 bytes at a time and send them as short messages
        arg = args;

        while (arg != NIL)
        {
            bytetolong.mylong = 0L;
            bytetolong.mybyte[0] = int_arg(arg);
            if (cdr(arg) != NIL) bytetolong.mybyte[1] = int_arg(arg = cdr(arg));
            if (cdr(arg) != NIL) bytetolong.mybyte[2] = int_arg(arg = cdr(arg));
            MidiError = midiOutShortMsg(hMidiOut, bytetolong.mylong);
            if (MidiError) break;
            if (arg != NIL) arg = cdr(arg);
        }

    }
    else
    {
        // count elements in list so we can allocate buffer
        int i = list_length(args);

        /* allocate structure buffer */
        // REVISIT: why not use malloc()?
        HANDLE    HdrHandle = (HANDLE) GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, sizeof(MIDIHDR));
        MIDIHDR * MidiOutHdr = (MIDIHDR *) GlobalLock((HGLOBAL) HdrHandle);

        HANDLE DataHandle = (HANDLE) GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, i);
        LPSTR  MidiOutData = (CHAR *) GlobalLock((HGLOBAL) DataHandle);

        /* pack the buffer array and set size */
        arg = args;

        for (int j = 0; j < i; j++)
        {
            MidiOutData[j] = int_arg(arg);
            arg = cdr(arg);
        }

        MidiOutHdr->dwBufferLength = i;

        MidiOutHdr->lpData = MidiOutData;

        /* prepare it, send it out, and unprepare it */

        MidiError = midiOutPrepareHeader(hMidiOut, MidiOutHdr, sizeof(MIDIHDR));
        if (!MidiError) MidiError = midiOutLongMsg(hMidiOut, MidiOutHdr, sizeof(MIDIHDR));
        if (!MidiError) MidiError = midiOutUnprepareHeader(hMidiOut, MidiOutHdr, sizeof(MIDIHDR));

        /* free buffer and struct */

        GlobalUnlock(DataHandle);
        GlobalFree(DataHandle);

        GlobalUnlock(HdrHandle);
        GlobalFree(HdrHandle);
    }

    if (MidiError)
    {
        // Let the user know that a midi error occured.
        ThrowGeneralMidiError(MidiError);
        return Unbound;
    }

    return Unbound;
}

NODE *lmci(NODE *args)
{
    // get mci command string
    CStringPrintedNode command(car(args));

    // check for optional callback routine
    wchar_t callback[MAX_BUFFER_SIZE];
    if (cdr(args) != NIL)
    {
        cnv_strnode_string(callback, cdr(args));
        wcscpy(mci_callback, callback);
    }

    wchar_t mciReturnBuffer[MAX_BUFFER_SIZE];
    mciReturnBuffer[0] = '\0';

    // send out command
    DWORD mciError = mciSendString(
        command,
        mciReturnBuffer,
        ARRAYSIZE(mciReturnBuffer),
        GetMainWindow());
    if (mciError != ERROR_SUCCESS)
    {
        // let user know about the error
        // The documentation for mciGetErrorString states that the maximum
        // length for any return string is 128 characters.
        wchar_t mciErrorBuffer[128];
        mciGetErrorString(mciError, mciErrorBuffer, ARRAYSIZE(mciErrorBuffer));
        err_logo(MCI_ERROR, make_strnode(mciErrorBuffer));
    }
    else
    {
        // if something was returned then return it to user
        if (mciReturnBuffer[0] != '\0')
        {
            NODE * targ = make_strnode(mciReturnBuffer);
            NODE * val = parser(targ, false);
            return val;
        }
    }

    return Unbound;
}

#endif // WX_PURE

NODE *lplaywave(NODE *args)
{
    CStringPrintedNode fileName(car(args));

    int flag = getint(nonnegative_int_arg(args = cdr(args)));

#ifdef __WXMSW__
    // For compatibility, to support the rich set of undocumented flags on win32
    // that MSWLogo has supported, PLAYWAVE continues to be implemented with
    // sndPlaySound on Windows.
    if (fileName.GetString()[0] == '\0')
    {
        sndPlaySound(NULL, flag);
    }
    else
    {
        sndPlaySound(fileName, flag);
    }
#else
    // from mmsystem.h
    const unsigned SND_SYNC  = 0x0000;
    const unsigned SND_ASYNC = 0x0001;
    const unsigned SND_LOOP  = 0x0008;

    // map the win32 flags to wxSOUND flags as best as possible.
    unsigned wxFlags = wxSOUND_SYNC;
    if (flag & SND_ASYNC)
    {
        wxFlags |= wxSOUND_ASYNC;

        if (flag & SND_LOOP)
        {
            wxFlags |= wxSOUND_LOOP;
        }
    }

    // Play the requested wave file.
    wxSound::Play(fileName.GetString(), wxFlags);
#endif

    return Unbound;
}
