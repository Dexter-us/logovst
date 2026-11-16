// Copyright (C) 2019 by David Costanzo
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
    #include <assert.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include "logocore.h" // for ARRAYSIZE
    #include "legacyansi.h"
    #include "unicodefile.h"
    #include "debugheap.h"
#endif

static
bool read_char_binary(FILE* fp, wchar_t * nextChar)
{
    int nextByte = fgetc(fp);
    if (nextByte == EOF)
    {
        return false;
    }

    // Return the byte, without conversion
    *nextChar = nextByte;
    return true;
}

static
bool read_char_system_code_page(FILE* fp, wchar_t * nextChar)
{
    int nextByte = fgetc(fp);
    if (nextByte == EOF)
    {
        return false;
    }

    // Convert CRLF into LF.
    if (nextByte == '\r')
    {
        long int restorePosition = ftell(fp);
        int byte2 = fgetc(fp);
        if (byte2 == '\n')
        {
            // This was a CRLF sequence.
            // Replace the CRLF with an LF.
            *nextChar = '\n';
            return true;
        }
        else
        {
            // This is not a CRLF sequence.
            // Put back the byte that we read.
            fseek(fp, restorePosition, SEEK_SET);
        }
    }

    // Wrap the byte in an ANSI string.
    char ansiString[2];
    ansiString[0] = nextByte;
    ansiString[1] = '\0';

    // Convert the ANSI string into a Unicode string using the
    // current system page (the way it would have been interpreted
    // before Unicode).
    wchar_t * unicodeString = ansi_to_unicode(ansiString);
    *nextChar = unicodeString[0];
    free(unicodeString);
    return true;
}


static
bool read_char_utf16le(FILE* fp, wchar_t * nextChar)
{
    int byte1 = fgetc(fp);
    if (byte1 == EOF)
    {
        return false;
    }

    int byte2 = fgetc(fp);
    if (byte2 == EOF)
    {
        // malformed file (ended in middle of character)
        return false;
    }

    // Decode the two bytes into a unicode character.
    wchar_t ch = ((0xFF & byte2) << 8) | (byte1 & 0xFF);

    // Convert CRLF into LF.
    if (ch == '\r')
    {
        long int restorePosition = ftell(fp);
        int byte1 = fgetc(fp);
        if (byte1 == '\n')
        {
            int byte2 = fgetc(fp);
            if (byte2 == '\0')
            {
                // This was a CRLF sequence.
                // Replace the CRLF with an LF.
                *nextChar = '\n';
                return true;
            }
        }

        // Put back any bytes that we read when looking for the LF.
        fseek(fp, restorePosition, SEEK_SET);
    }

    *nextChar = ch;
    return true;
}

static
bool read_char_utf8(FILE* fp, wchar_t * nextChar)
{
    int octetsRemainingInCharacterCode = 0;
    wchar_t characterCode = 0;

    int nextByte;
    while ((nextByte = fgetc(fp)) != EOF)
    {
        if ((nextByte & 0x80) == 0x00)
        {
            // This character is within ASCII-7.

            // Convert CRLF into LF.
            if (nextByte == '\r')
            {
                long int restorePosition = ftell(fp);
                int byte2 = fgetc(fp);
                if (byte2 == '\n')
                {
                    // This was a CRLF sequence.
                    // Replace the CRLF with an LF.
                    *nextChar = '\n';
                    return true;
                }
                else
                {
                    // This is not a CRLF sequence.
                    // Put back the byte that we read.
                    fseek(fp, restorePosition, SEEK_SET);
                }
            }

            *nextChar = nextByte;
            return true;
        }
        else if ((nextByte & 0xC0) == 0x80)
        {
            // 10xxxxxx

            // This is a continuation of a multi-octet sequence
            characterCode = (characterCode << 6) | (nextByte & 0x3F);

            octetsRemainingInCharacterCode--;
            if (octetsRemainingInCharacterCode == 0)
            {
                // This is the final octet in the mutli-octet code.
                *nextChar = characterCode;
                return true;
            }
        }
        else if ((nextByte & 0xE0) == 0xC0)
        {
            // 110xxxxx 10xxxxxx
            characterCode = nextByte & 0x1F;
            octetsRemainingInCharacterCode = 1;
        }
        else if ((nextByte & 0xF0) == 0xE0)
        {
            // 1110xxxx 10xxxxxx 10xxxxxx
            characterCode = nextByte & 0x0F;
            octetsRemainingInCharacterCode = 2;
        }
        else if ((nextByte & 0xF8) == 0xF0)
        {
            // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            characterCode = nextByte & 0x07;
            octetsRemainingInCharacterCode = 3;
        }
        else
        {
            // Illegal UTF-8 code.  Drop it.
        }
    }

     // The UTF-8 was cut short or we reached EOF.
     return false;
}

bool read_unicode_char(FILE* fp, ENCODING encoding, wchar_t * nextChar)
{
    switch (encoding)
    {
    case ENCODING_Binary:
        return read_char_binary(fp, nextChar);

    case ENCODING_SystemCodePage:
        return read_char_system_code_page(fp, nextChar);

    case ENCODING_Utf16Le:
        return read_char_utf16le(fp, nextChar);

    case ENCODING_Utf8:
        return read_char_utf8(fp, nextChar);

    default:
        assert(false);
        return false;
    }
}

// Reads the BOM from a FILE* (if any) to determine its character encoding.
// As a side-effect, if a BOM exists, the file pointer is set just after it.
//
// Stream - The FILE* from which to read the encoding.
//          The file posistion should be at the beginning of the
//          file, where the BOM is located.
//          This FILE* must support seeking (fseek)
//
// Returns
//   ENCODING_SystemCodePage if no BOM was found.
//   ENCODING_Utf8           if a UTF-8 BOM was found.
//   ENCODING_Utf16Le        if a UTF-16LE BOM was found.
ENCODING read_character_encoding(FILE * Stream)
{
    assert(ftell(Stream) == 0); // nothing should have been read

    ENCODING characterEncoding;

    // Read enough data from Stream to hold the longest byte
    // encoding of a BOM, which is 3 bytes (UTF-8).
    unsigned char buffer[3];
    size_t bytesRead = fread(buffer, sizeof(*buffer), ARRAYSIZE(buffer), Stream);
    if (bytesRead == 3)
    {
        // The file contained at least 3 bytes.
        if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
        {
            characterEncoding = ENCODING_Utf8;
        }
        else if (buffer[0] == 0xFF && buffer[1] == 0xFE)
        {
            // Put back the byte that was read that wasn't part of the BOM.
            fseek(Stream, 2, SEEK_SET);
            characterEncoding = ENCODING_Utf16Le;
        }
        else
        {
            // Put back three bytes that were read.
            fseek(Stream, 0, SEEK_SET);
            characterEncoding = ENCODING_SystemCodePage;
        }
    }
    else if (bytesRead == 2)
    {
        if (buffer[0] == 0xFF && buffer[1] == 0xFE)
        {
            // This a file that contains nothing but a BOM.
            characterEncoding = ENCODING_Utf16Le;
        }
        else
        {
            // Put back three bytes that were read.
            fseek(Stream, 0, SEEK_SET);
            characterEncoding = ENCODING_SystemCodePage;
        }
    }
    else
    {
        // The file was too small to have any BOM.
        // For example, this could be an empty file.
        fseek(Stream, 0, SEEK_SET);
        characterEncoding = ENCODING_SystemCodePage;
    }

    return characterEncoding;
}

static
bool write_char_binary(FILE* fp, wchar_t ch)
{
    char byte = 0xFF & ch;

    // Write the byte to the file stream.
    int bytesWritten = fwrite(&byte, sizeof byte, 1, fp);

    // Return true if successful.
    return bytesWritten == 1;
}


static
bool write_char_system_code_page(FILE* fp, wchar_t ch)
{
    bool isOk;
    if (ch == '\n')
    {
        // Convert LF into CRLF.
        int bytesWritten = fwrite("\r\n", sizeof(char), 2, fp);

        // Get the status of the write (true for success)
        isOk = bytesWritten == 2;
    }
    else
    {
        // Convert the Unicode character to an ANSI character using the
        // current system page (the way it would have been interpreted
        // before Unicode.

        // Wrap the Unicode character in a Unicode string.
        wchar_t unicodeString[2];
        unicodeString[0] = ch;
        unicodeString[1] = '\0';

        // Get the ANSI equivalent to the string.
        char * ansiString = unicode_to_ansi(unicodeString);
        if (ansiString == NULL)
        {
            return false;
        }

        // Write the ANSI string to the file stream.
        int bytesWritten = fwrite(ansiString, sizeof(char), 1, fp);
        isOk = bytesWritten == 1;

        // Cleanup
        free(ansiString);
    }

    // Return true if successful.
    return isOk;
}


static
bool write_char_utf16le(FILE* fp, wchar_t ch)
{
    bool isOk;
    if (ch == '\n')
    {
        // Convert LF into CRLF.
        int bytesWritten = fwrite("\r\0\n\0", sizeof(char), 4, fp);

        // Get the status of the write (true for success)
        isOk = bytesWritten == 4;
    }
    else
    {
        // Encode the character as UTF-16.
        char buffer[2];
        buffer[0] = static_cast<char>((ch >> 0) & 0xFF);
        buffer[1] = ((ch >> 8) & 0xFF);

        // Write the string to the file stream.
        int bytesWritten = fwrite(buffer, sizeof(char), 2, fp);

        // Get the status of the write (true for success)
        isOk = bytesWritten == 2;
    }

    // Return the status of the write (true for success)
    return isOk;
}


static
bool write_char_utf8(FILE* fp, wchar_t ch)
{
    if (ch == '\n')
    {
        // Convert LF to CRLF.
        fwrite("\r\n", sizeof(char), 2, fp);
    }
    else if (ch < 0x80)
    {
        // Single byte ASCII encoding: 0xxxxxxx
        char buffer[1];
        buffer[0] = ch;
        fwrite(buffer, sizeof(*buffer), ARRAYSIZE(buffer), fp);
   }
    else if (ch < 0x0800)
    {
        // two-byte encoding: 110xxxxx 10xxxxxx
        char buffer[2];
        buffer[0] = 0xC0 | (ch >> 6);
        buffer[1] = 0x80 | (ch & 0x3F);
        fwrite(buffer, sizeof(*buffer), ARRAYSIZE(buffer), fp);
    }
    else if (ch < 0x10000)
    {
        // three-byte encoding: 1110xxxx 10xxxxxx 10xxxxxx
        char buffer[3];
        buffer[0] = 0xE0 |  (ch >> 12);
        buffer[1] = 0x80 | ((ch >>  6) & 0x3F);
        buffer[2] = 0x80 | ((ch >>  0) & 0x3F);
        fwrite(buffer, sizeof(*buffer), ARRAYSIZE(buffer), fp);
    }
    else if (ch < 0x110000)
    {
        // four-byte encoding: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        char buffer[4];
        buffer[0] = 0xF0 |  (ch >> 18);
        buffer[1] = 0x80 | ((ch >> 12) & 0x3F);
        buffer[2] = 0x80 | ((ch >>  6) & 0x3F);
        buffer[3] = 0x80 | ((ch >>  0) & 0x3F);
        fwrite(buffer, sizeof(*buffer), ARRAYSIZE(buffer), fp);
    }
    else
    {
        assert(0); // character too large to encode
    }

     return true;
}

bool write_unicode_char(FILE* fp, ENCODING encoding, wchar_t ch)
{
    switch (encoding)
    {
    case ENCODING_Binary:
        return write_char_binary(fp, ch);

    case ENCODING_SystemCodePage:
        return write_char_system_code_page(fp, ch);

    case ENCODING_Utf16Le:
        return write_char_utf16le(fp, ch);

    case ENCODING_Utf8:
        return write_char_utf8(fp, ch);

    default:
        assert(false);
        return false;
    }
}

bool write_unicode_string(FILE* fp, ENCODING encoding, const wchar_t * string)
{
    for (const wchar_t * ptr = string; *ptr != '\0'; ptr++)
    {
        if (!write_unicode_char(fp, encoding, *ptr))
        {
            // There was a problem writing to the stream.
            return false;
        }
    }

    return true;
}
