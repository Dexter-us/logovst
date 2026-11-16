/*
*      print.cpp         logo printing module                    dvb
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
*/

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
    #include <stdarg.h>
    #include <stdio.h>
    #include <string.h>
    #include <algorithm>
    #include <cwchar>

    #include "unix.h"
    #include "parse.h"
    #include "print.h"
    #include "logocore.h"
    #include "logodata.h"
    #include "ibmterm.h"
    #include "init.h"
    #include "eval.h"
    #include "files.h"
    #include "unicodefile.h"
    #include "debugheap.h"

    #include "internationalizedstrings.h"
#endif

// structures
struct STRING_PRINT_INFORMATION
{
    size_t     TotalCharactersNeeded;
    size_t     BufferLength;
    wchar_t  * Buffer;
};

// global variable
static STRING_PRINT_INFORMATION g_StringPrintInformation;

void
print_char(
    FILE        * strm,
    ENCODING      encoding,
    MESSAGETYPE   type,
    wchar_t       ch
    )
{
    if (strm != NULL)
    {
        // printing to a file stream
        if (strm == stdout)
        {
            // FMSLogo isn't a command-line application, so printing
            // to "stdout" really goes to the commander history.
            assert(encoding == ENCODING_NotApplicable);
            putcombochar(type, ch);
            if (dribblestream != NULL)
            {
                write_unicode_char(dribblestream, ENCODING_Utf8, ch);
            }
        }
        else
        {
            write_unicode_char(strm, encoding, ch);
        }
    }
    else
    {
        // printing to string
        assert(encoding == ENCODING_NotApplicable);
        if (g_StringPrintInformation.TotalCharactersNeeded + 1 < g_StringPrintInformation.BufferLength)
        {
            g_StringPrintInformation.Buffer[g_StringPrintInformation.TotalCharactersNeeded] = ch;
        }
        g_StringPrintInformation.TotalCharactersNeeded++;
    }
}

void print_space(FILE * strm, ENCODING encoding, MESSAGETYPE type)
{
    print_char(strm, encoding, type, ' ');
}

void print_newline(FILE * strm, ENCODING encoding, MESSAGETYPE type)
{
    print_char(strm, encoding, type, '\n');
}

void print_string(FILE *strm, ENCODING encoding, MESSAGETYPE type, const wchar_t * string)
{
    for (const wchar_t *cp = string; *cp != '\0'; cp++)
    {
        print_char(strm, encoding, type, *cp);
    }
}

void
ndprintf(
    FILE          * strm,
    ENCODING        encoding,
    MESSAGETYPE     type,
    bool            print_backslashes,
    const wchar_t * fmt,
    ...
    )
{
    va_list ap;
    va_start(ap, fmt);

    wchar_t ch;
    while ((ch = *fmt++) != '\0')
    {
        if (ch == '%')
        {
            ch = *fmt++;
            if (ch == 's') // show
            {
                NODE * nd = va_arg(ap, NODE *);
                print_node(strm, encoding, type, print_backslashes, nd);
            }
            else if (ch == 'p')  // print
            {
                /* print */
                NODE * nd = va_arg(ap, NODE *);
                if (is_list(nd))
                {
                    print_helper(strm, encoding, type, print_backslashes, nd);
                }
                else
                {
                    print_node(strm, encoding, type, print_backslashes, nd);
                }
            }
            else if (ch == 't') // text
            {
                wchar_t *cp = va_arg(ap, wchar_t *);
                print_string(strm, encoding, type, cp);
            }
            else
            {
                print_char(strm, encoding, type, '%');
                print_char(strm, encoding, type, ch);
            }
        }
        else
        {
            print_char(strm, encoding, type, ch);
        }
    }

    va_end(ap);
}

static
void
real_print_node(
    FILE       * strm,
    ENCODING     encoding,
    MESSAGETYPE  type,
    bool         print_backslahes,
    const NODE * nd,
    int          depth,
    int          width
    );

static
void
real_print_helper(
    FILE       * strm,
    ENCODING     encoding,
    MESSAGETYPE  type,
    bool         print_backslashes,
    const NODE * ndlist,
    int          depth,
    int          width
    )
{
    assert(is_list(ndlist));

    int wid = width;

    while (ndlist != NIL)
    {
        NODE * arg = car(ndlist);

        // Print this element of the list.
        real_print_node(
            strm,
            encoding,
            type,
            print_backslashes,
            arg,
            depth,
            width);

        ndlist = cdr(ndlist);
        if (ndlist != NIL)
        {
            // There are elements which follow this one.
            // Print a space before printing the next element.
            print_space(strm, encoding, type);
            if (--wid == 0)
            {
                // We've printed out as many elements as we're supposed to.
                // Print the elipsese and exit.
                print_string(strm, encoding, type, L"...");
                break;
            }
        }
    }
}


// Prints a node to a file stream in a way that is consistent
// with how FMSLogo parses nodes.
//
//   strm              - the file stream.
//   encoding          - the character encoding of strm.
//   type              - the type of message that is being printed
//   print_backslashes - true, if array origins and backslashes before
//                       special chracters in words should be printed.
//                       Use false when printing for display and true
//                       when printing so that it can be later LOADed.
//   nd                - the node to print
//   depth             - how "deep" to go (how many elements of a list)
//   width             - how many characters to print at most ???
static
void
real_print_node(
    FILE       * strm,
    ENCODING     encoding,
    MESSAGETYPE  type,
    bool         print_backslashes,
    const NODE * nd,
    int          depth,
    int          width
    )
{
    NODETYPE ndty;

    if (depth == 0)
    {
        print_string(strm, encoding, type, L"...");
    }
    else if (nd == NIL)
    {
        print_string(strm, encoding, type, L"[]");
    }
    else if (nd == Unbound)
    {
        print_string(strm, encoding, type, g_LOCALIZED_UNBOUND);
    }
    else if ((ndty = nodetype(nd)) & NT_PRIM)
    {
        print_string(strm, encoding, type, L"PRIM");
    }
    else if (ndty & NT_LIST)
    {
        print_char(strm, encoding, type, '[');
        real_print_helper(
            strm,
            encoding,
            type,
            print_backslashes,
            nd,
            depth - 1, // -1 because this list is one level of depth
            width);
        print_char(strm, encoding, type, ']');
    }
    else if (ndty == ARRAY)
    {
        int dim = getarrdim(nd);

        // figure out how many items to print
        int wid;
        if (width < 0)
        {
            wid = dim;
        }
        else
        {
            wid = (dim > width ? width : dim);
        }

        print_char(strm, encoding, type, '{');

        // print each item in the array
        NODE **pp = getarrptr(nd);
        int i = 0;
        while (i < wid)
        {
            real_print_node(
                strm,
                encoding,
                type,
                print_backslashes,
                *pp++,
                depth - 1, // -1 because this array is one level of depth
                width);
            if (++i < dim)
            {
                print_space(strm, encoding, type);
            }
        }
        if (wid < dim)
        {
            print_string(strm, encoding, type, L"...");
        }
        print_char(strm, encoding, type, '}');

        // print the origin when printing a workspace.
        if (print_backslashes && (getarrorg(nd) != 1))
        {
            wchar_t origin[32];
            swprintf(origin, ARRAYSIZE(origin), L"@%d", getarrorg(nd));
            print_string(strm, encoding, type, origin);
        }
    }
    else if (ndty == QUOTE)
    {
        print_char(strm, encoding, type, '\"');
        real_print_node(
            strm,
            encoding,
            type,
            print_backslashes,
            car(nd),
            width - 1, // -1 to account for the quote printed above
            depth);
    }
    else if (ndty == COLON)
    {
        print_char(strm, encoding, type, ':');
        real_print_node(
            strm,
            encoding,
            type,
            print_backslashes,
            car(nd),
            width - 1, // -1 to account for the colon printed above
            depth);
    }
    else if (ndty == FLOATINGPOINT)
    {
        wchar_t buffer[MAX_NUMBER];
        swprintf(buffer, ARRAYSIZE(buffer), L"%0.15g", getfloat(nd));

        // REVISIT: is it okay to ignore the width parameter?
        print_string(strm, encoding, type, buffer);
    }
    else if (ndty == INTEGER)
    {
        wchar_t buffer[MAX_NUMBER];
        swprintf(buffer, ARRAYSIZE(buffer), L"%ld", getint(nd));

        // REVISIT: is it okay to ignore the width parameter?
        print_string(strm, encoding, type, buffer);
    }
    else if (ndty == CASEOBJ)
    {
        real_print_node(
            strm,
            encoding,
            type,
            print_backslashes,
            strnode__caseobj(nd),
            depth,
            width);
    }
    else
    {
        assert(is_string(nd));

        // Figure out how many charaters to print and
        // whether we should print some elipses at the end.
        const wchar_t *cp = getstrptr(nd);
        bool dots;
        const wchar_t * cpLimit;
        if (width < 0)
        {
            // There is no width limit.  Print the entire string.
            cpLimit = cp + getstrlen(nd);
            dots = false;
        }
        else
        {
            // Print at least 10 characters.
            int totalCharsToPrint = std::max(width, 10);

            if (totalCharsToPrint < getstrlen(nd))
            {
                // We are printing less than the entire string
                // so we need to print some elipses at the end.
                cpLimit = cp + totalCharsToPrint;
                dots = true;
            }
            else
            {
                // We are printing the entire string.
                cpLimit = cp + getstrlen(nd);
                dots = false;
            }
        }

        // print the characters from cp to cpLimit from nd's string.
        if (!backslashed(nd))
        {
            while (cp < cpLimit)
            {
                print_char(strm, encoding, type, *cp++);
            }
        }
        else if (!print_backslashes)
        {
            while (cp < cpLimit)
            {
                print_char(strm, encoding, type, vbar_clear(*cp++));
            }
        }
        else
        {
            // determine if the word was in vbars
            // Note: it should be possible with ndty==VBAR_STRING,
            // but the NT_VBAR flag is not consistently set correctly.
            const wchar_t * firstVbarredCharPtr;
            for (firstVbarredCharPtr = cp;
                 firstVbarredCharPtr < cpLimit;
                 firstVbarredCharPtr++)
            {
                if (is_vbarred(*firstVbarredCharPtr))
                {
                    break;
                }
            }

            if (firstVbarredCharPtr != cpLimit)
            {
                // word was in vbars
                print_char(strm, encoding, type, '|');
                while (cp < cpLimit)
                {
                    wchar_t ch = vbar_clear(*cp++);

                    // Even within vbars, '|' and '\' must be escaped.
                    // We also prefer to escape newlines as '\n' to avoid
                    // the vbar'd string from breaking across lines.
                    if (ch == '|' || ch == '\\' || ch == '\n')
                    {
                        print_char(strm, encoding, type, '\\');
                    }

                    // Use the "\n" escape sequence for newlines.
                    if (ch == '\n')
                    {
                        ch = 'n';
                    }

                    print_char(strm, encoding, type, ch);
                }
                print_char(strm, encoding, type, '|');
            }
            else
            {
                // word was not in vbars
                while (cp < cpLimit)
                {
                    wchar_t ch = *cp++;
                    if (is_special_character(ch))
                    {
                        // ch must be escaped with a backslash
                        print_char(strm, encoding, type, '\\');
                    }

                    // Use the "\n" escape sequence for newlines.
                    // Otherwise we get a weird-looking backlash followed
                    // by a literal newline character.
                    if (ch == '\n')
                    {
                        ch = 'n';
                    }

                    print_char(strm, encoding, type, ch);
                }
            }
        }

        if (dots)
        {
            print_string(strm, encoding, type, L"...");
        }
    }
}

static
bool is_ascii(const wchar_t * string, int length)
{
    for (const wchar_t * ptr = string + length - 1; string <= ptr; ptr--)
    {
        // If this character is outside the ASCII range and it's not
        // a special Logo character that is mapped outside ASCII range,
        // such as '(' or ';', then the string contains a non-ASCII character.
        if (0x7F < *ptr && !is_vbarred(*ptr))
        {
            return false; // non-ASCII character found
        }
    }

    return true; // Non non-ASCII characters found
}

// Determines if real_print_node would print only ASCII characters
// for a given node and an infinite depth.
bool is_ascii(const NODE * nd)
{
    if (nd == NIL)
    {
        return true; // NIL is the empty list, represented as []
    }

    if (nd == Unbound)
    {
        return is_ascii(g_LOCALIZED_UNBOUND, wcslen(g_LOCALIZED_UNBOUND));
    }

    NODETYPE node_type = nodetype(nd);
    if (node_type & NT_LIST)
    {
        // walk the list
        while (nd != NIL)
        {
            if (!is_ascii(car(nd)))
            {
                return false;
            }
            nd = cdr(nd);
        }
    }
    else if (node_type == ARRAY)
    {
        // Check each NODE* in the array.
        NODE **pp      = getarrptr(nd);
        NODE **pplimit = pp + getarrdim(nd);;
        while (pp < pplimit)
        {
            if (!is_ascii(*pp))
            {
                return false;
            }
            pp++;
        }
    }
    else if (node_type == QUOTE || node_type == COLON)
    {
        // Either "name or :name.
        return is_ascii(car(nd));
    }
    else if (node_type == CASEOBJ)
    {
        return is_ascii(strnode__caseobj(nd));
    }
    else if (is_string(nd))
    {
        return is_ascii(getstrptr(nd), getstrlen(nd));
    }

    return true;
}

int find_limit(const CLocalizedNode & Node)
{
    NODE * nd = cnv_node_to_numnode(Node.GetValue());

    int val = -1;
    if (nodetype(nd) == INTEGER)
    {
        val = getint(nd);
    }
    gcref(nd);
    return val;
}

void
print_helper(
    FILE        * strm,
    ENCODING      encoding,
    MESSAGETYPE   type,
    bool          print_backslashes,
    const NODE  * ndlist
    )
{
    real_print_helper(
        strm,
        encoding,
        type,
        print_backslashes,
        ndlist,
        find_limit(Printdepthlimit),
        find_limit(Printwidthlimit));
}

void
print_node(
    FILE        * strm,
    ENCODING      encoding,
    MESSAGETYPE   type,
    bool          print_backslashes,
    const NODE  * nd
    )
{
    real_print_node(
        strm,
        encoding,
        type,
        print_backslashes,
        nd,
        find_limit(Printdepthlimit),
        find_limit(Printwidthlimit));
}

NODE *lshow(NODE *args)
{
    print_helper(
        g_Writer.GetStream(),
        g_Writer.GetEncoding(),
        MESSAGETYPE_Normal,
        false, // format for display
        args); // args wraps the inputs in a list
    print_newline(
        g_Writer.GetStream(),
        g_Writer.GetEncoding(),
        MESSAGETYPE_Normal);
    return Unbound;
}

static
void type_helper(NODE * args, bool print_space_between_arguments)
{
    // Loop over all arguments.
    while (args != NIL)
    {
        NODE * arg = car(args);

        // Print each argument.
        if (is_list(arg))
        {
            print_helper(
                g_Writer.GetStream(),
                g_Writer.GetEncoding(),
                MESSAGETYPE_Normal,
                false,
                arg);
        }
        else
        {
            print_node(
                g_Writer.GetStream(),
                g_Writer.GetEncoding(),
                MESSAGETYPE_Normal,
                false,
                arg);
        }

        args = cdr(args);

        // If requested, print a space between this argument and the next one.
        if (print_space_between_arguments && args != NIL)
        {
            print_space(
                g_Writer.GetStream(),
                g_Writer.GetEncoding(),
                MESSAGETYPE_Normal);
        }
    }
}

NODE *ltype(NODE *args)
{
    type_helper(args, false);
    return Unbound;
}

NODE *lprint(NODE *args)
{
    type_helper(args, true);
    print_newline(
        g_Writer.GetStream(),
        g_Writer.GetEncoding(),
        MESSAGETYPE_Normal);
    return Unbound;
}

// Initializes the printing engine to print to Buffer.
void
InitializeStringPrintInformation(
    wchar_t * Buffer,
    size_t    BufferLength
    )
{
    g_StringPrintInformation.TotalCharactersNeeded = 0;
    g_StringPrintInformation.Buffer                = Buffer;
    g_StringPrintInformation.BufferLength          = BufferLength;
}

// NUL-terminate the string and return how many characters would be
// required to hold the entire print job.
size_t FinalizeStringPrintInformation()
{
    // NUL-terminate the string
    if (g_StringPrintInformation.TotalCharactersNeeded + 1 < g_StringPrintInformation.BufferLength)
    {
        // The string fit within the buffer.
        // NUL-terminate it at the end of what we wrote.
        g_StringPrintInformation.Buffer[g_StringPrintInformation.TotalCharactersNeeded] = '\0';
    }
    else if (g_StringPrintInformation.BufferLength != 0)
    {
        // The string didn't fit within the buffer.
        // NUL-terminate it at the end of the buffer.
        g_StringPrintInformation.Buffer[g_StringPrintInformation.BufferLength - 1] = '\0';
    }
    else
    {
        // No buffer was given, so we can't NUL-terminate anything.
    }
    g_StringPrintInformation.TotalCharactersNeeded++;

    // Return the total number of characters needed.
    return g_StringPrintInformation.TotalCharactersNeeded;
}


// Prints a node to a string, as would happen with PRINT, using
// the given limits on width and depth.  No backslashing is applied
// when printing.
//
// Node         - the node to print to Buffer
// Buffer       - the location to which to print the NODE.
// BufferLength - the number of characters in Buffer
//
// Returns the number of characters needed to print the entire string for Node,
// including the NUL-terminator.
size_t
PrintNodeToString(
    const NODE * Node,
    wchar_t *    Buffer,
    size_t       BufferLength,
    int          PrintDepthLimit,
    int          PrintWidthLimit
    )
{
    // Initialize the printing engine to print to Buffer
    InitializeStringPrintInformation(Buffer, BufferLength);

    // Print the node
    if (is_list(Node))
    {
        real_print_helper(
            NULL,
            ENCODING_NotApplicable,
            MESSAGETYPE_Normal,
            false, // do not include backslashes
            Node,
            PrintDepthLimit,
            PrintWidthLimit);
    }
    else
    {
        real_print_node(
            NULL,
            ENCODING_NotApplicable,
            MESSAGETYPE_Normal,
            false, // do not include backslashes
            Node,
            PrintDepthLimit,
            PrintWidthLimit);
    }

    // NUL-terminate the string and return the total number of characters needed.
    return FinalizeStringPrintInformation();
}


// Prints a node to a string, as would happen with PRINT if no limits
// were placed on its width or depth.
// Returns the number of characters needed to print the entire string for Node,
// including the NUL-terminator.
size_t
PrintNodeToString(
    const NODE * Node,
    wchar_t *    Buffer,
    size_t       BufferLength
    )
{
    return PrintNodeToString(Node, Buffer, BufferLength, -1, -1);
}
