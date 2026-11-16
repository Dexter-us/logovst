/*
 *      parse.cpp       logo parser module              dvb
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
    #include <stdio.h>
    #include <setjmp.h>
    #include <algorithm>

    #include "parse.h"
    #include "argumentutils.h"
    #include "fmslogo.h"
    #include "logocore.h"
    #include "dynamicbuffer.h"
    #include "error.h"
    #include "init.h"
    #include "ibmterm.h"
    #include "unix.h"
    #include "logodata.h"
    #include "lists.h"
    #include "eval.h"
    #include "intern.h"
    #include "logomath.h"
    #include "appendablelist.h"
    #include "stringprintednode.h"
    #include "screenwindow.h"
    #include "unicodefile.h"
    #include "internationalizedstrings.h"
    #include "debugheap.h"
#endif

FILE     * loadstream          = stdin;
ENCODING   loadstream_encoding = ENCODING_NotApplicable;

FILE *dribblestream = NULL;
NODE *deepend_proc_name = NIL;
NODE *g_ToLine          = NIL;

INPUTMODE input_mode = INPUTMODE_None;

static CDynamicBuffer g_ReadBuffer;

// A class for allocating and manipulating a buffer that
// is suitable for use with make_strnode_no_copy().
// The intent of this class is to eliminate the need to copy
// the buffer when allocating a string node.
class CStringNodeBuffer
{
    friend class CStringNodeBufferInvariant;

public:
    CStringNodeBuffer();
    ~CStringNodeBuffer();

    void TakeOwnershipOfBuffer();

    wchar_t * GetBufferHeader();
    wchar_t * GetStringPointer() const;
    size_t GetStringLength() const;

    void AppendString(const wchar_t * ToAppend);
    void AppendChar(wchar_t ToAppend);

private:
    void GrowBy(size_t ExtraLength);

    wchar_t * m_Buffer;       // the buffer that holds the header, string, and NUL terminator
    size_t    m_BufferSize;   // the size of the allocated buffer, in bytes
    wchar_t * m_StringLimit;  // a pointer to one beyond the end of the string
    bool      m_IsOwner;      // if this object must free m_Buffer
};

#ifdef NDEBUG
#  define ASSERT_STRINGNODE_INVARIANT
#else
#  define ASSERT_STRINGNODE_INVARIANT CStringNodeBufferInvariant invariant(*this)

class CStringNodeBufferInvariant
{
public:
    CStringNodeBufferInvariant(const CStringNodeBuffer & StringNodeBuffer)
        : m_StringNodeBuffer(StringNodeBuffer)
    {
        AssertInvariant();
    }

    ~CStringNodeBufferInvariant()
    {
        AssertInvariant();
    }

    void AssertInvariant() const
    {
        assert(m_StringNodeBuffer.m_Buffer != NULL);
        assert((m_StringNodeBuffer.m_BufferSize % sizeof(*m_StringNodeBuffer.m_Buffer)) == 0);
        assert(m_StringNodeBuffer.GetStringPointer() <= m_StringNodeBuffer.m_StringLimit);
        assert(m_StringNodeBuffer.m_StringLimit < m_StringNodeBuffer.m_Buffer + (m_StringNodeBuffer.m_BufferSize / sizeof(*m_StringNodeBuffer.m_Buffer)));
    }

private:
    const CStringNodeBuffer & m_StringNodeBuffer;
};

#endif // NDEBUG

CStringNodeBuffer::CStringNodeBuffer()
{
    const size_t DEFAULT_SIZE = 256;

    // allocate enough for the header, the string, and the NUL terminator
    m_BufferSize  = sizeof(wchar_t) * (DEFAULT_SIZE + 2);
    m_Buffer      = static_cast<wchar_t*>(malloc(m_BufferSize));
    m_StringLimit = GetStringPointer(); // just after the header
    m_IsOwner     = true;
}

CStringNodeBuffer::~CStringNodeBuffer()
{
    if (m_IsOwner)
    {
        free(m_Buffer);
    }
}

void CStringNodeBuffer::TakeOwnershipOfBuffer()
{
    ASSERT_STRINGNODE_INVARIANT;

    // Reduce the size of the buffer to only what is needed.
    size_t newBufferSize = (m_StringLimit - m_Buffer + 1) * sizeof(wchar_t);
    void * smallerBuffer = realloc(m_Buffer, newBufferSize);
    if (smallerBuffer != NULL)
    {
        // The reallocation should always succeed, but the C standard
        // does not guarantee this.
        m_Buffer      = static_cast<wchar_t*>(smallerBuffer);
        m_BufferSize  = newBufferSize;
        m_StringLimit = m_Buffer + (newBufferSize / sizeof(wchar_t)) - 1;
    }

    // initialize the reference count to zero.
    *GetBufferHeader() = 0;

    // NUL-terminate the buffer
    *m_StringLimit = '\0';

    // set a flag that we no longer own m_Buffer.
    m_IsOwner = false;
}

// Number of characters in the string, not including the NUL terminator.
size_t CStringNodeBuffer::GetStringLength() const
{
    ASSERT_STRINGNODE_INVARIANT;

    return m_StringLimit - GetStringPointer();
}

wchar_t * CStringNodeBuffer::GetBufferHeader()
{
    return m_Buffer;
}

wchar_t * CStringNodeBuffer::GetStringPointer() const
{
    // Return the pointer just after the header.
    return m_Buffer + 1;
}

// ExtraLength - number of characters to increase the string by.
void CStringNodeBuffer::GrowBy(size_t ExtraLength)
{
    ASSERT_STRINGNODE_INVARIANT;

    // if it won't fit, then make the buffer bigger
    ptrdiff_t charactersInUse = m_StringLimit - m_Buffer;
    size_t requiredSize = (charactersInUse + ExtraLength + 1) * sizeof(wchar_t);
    if (m_BufferSize < requiredSize)
    {
        // Double the size of the buffer, instead of only requesting
        // requiredSize bytes.  If we don't do this, then reading
        // long words takes a *very* long time because we repeatedly
        // reallocate the buffer to be one byte larger.
        size_t newsize = std::max(m_BufferSize * 2, requiredSize);
        m_Buffer      = static_cast<wchar_t*>(realloc(m_Buffer, newsize));
        m_BufferSize  = newsize;
        m_StringLimit = m_Buffer + charactersInUse;
    }
}

// Append a NUL-terminated string to the combo buffer
void CStringNodeBuffer::AppendString(const wchar_t * ToAppend)
{
    ASSERT_STRINGNODE_INVARIANT;

    // grow m_Buffer to be large enough to hold ToAppend
    size_t toAppendLength = wcslen(ToAppend);
    GrowBy(toAppendLength);

    // append ToAppend
    memcpy(m_StringLimit, ToAppend, toAppendLength * sizeof(*ToAppend));
    m_StringLimit += toAppendLength;
}

void CStringNodeBuffer::AppendChar(wchar_t ToAppend)
{
    ASSERT_STRINGNODE_INVARIANT;

    // resize combo_buff to be large enough to hold ToAppend
    GrowBy(1);

    // append ToAppend
    *m_StringLimit = ToAppend;
    m_StringLimit++;
}



// This is a hack to purge the "INPUTMODE_TO" buffer when
// loading from the editor.  If this is not done, any unused
// portion will hang around and be silently used the next time
// TO is run.
void rd_clearbuffer(FILE * Stream)
{
    if (input_mode == INPUTMODE_To && Stream == stdin)
    {
        g_ReadBuffer.Empty();
    }
}

// Returns the next character from Stream, or EOF.
int rd_getc(FILE * Stream, ENCODING CharacterEncoding)
{
    int c;

    if (Stream != stdin)
    {
        wchar_t character;
        if (read_unicode_char(Stream, CharacterEncoding, &character))
        {
            c = character;
        }
        else
        {
            c = EOF;
        }
    }
    else
    {
        if (g_ReadBuffer.IsEmpty())
        {
            // the buffer is empty, so we should read more
            switch (input_mode)
            {
                wchar_t * userInput;

            case INPUTMODE_To:
                {
                    CStringPrintedNode parsedToLine(g_ToLine);
                    ShowProcedureMiniEditor(parsedToLine, g_ReadBuffer);
                    break;
                }

            case INPUTMODE_List:
                userInput = promptuser(g_LOCALIZED_PROMPT_LIST);
                if (userInput == NULL)
                {
                    // Halt when done
                    err_logo(STOP_ERROR, NIL);
                }
                else
                {
                    g_ReadBuffer.AppendString(userInput);
                    free(userInput);
                }
                break;

            case INPUTMODE_Pause:
                userInput = promptuser(g_LOCALIZED_PAUSE_MODE_TITLE);
                if (userInput == NULL)
                {
                    // continue when done
                    g_ReadBuffer.AppendString(L"continue");
                }
                else
                {
                    g_ReadBuffer.AppendString(userInput);
                    free(userInput);
                }
                break;

            case INPUTMODE_None:
                userInput = promptuser(g_LOCALIZED_PROMPT_INPUT);
                if (userInput == NULL)
                {
                    // Halt when done
                    err_logo(STOP_ERROR, NIL);
                }
                else
                {
                    g_ReadBuffer.AppendString(userInput);
                    free(userInput);
                }
                break;
            }

            check_stop(true);
            g_ReadBuffer.AppendChar('\n');
        }

        c = g_ReadBuffer.PopChar();
    }

    return c;
}

static
inline
void rd_print_prompt(const char * /*str*/)
{
    //ndprintf(stdout,"%t",str);
}

// Reads the next complete line from FileStream into a string node.
// This may be more than a physical line of text if the line contains
// an open delimiter, such as "|", "[", or "(",  or if it ends in the
// line continuation character "~".  In this case, the physical lines
// will be concatenated into a single line.
//
// Note that reader() does not parse the line.
//
// FileStream        - The stream to read the line from.
// CharacterEncoding - The character coding of FileStream.
// Prompt            - Mostly ignored by FMSLogo.  If it's "RW", then
//                     the characters "(", ")", "{", "}", "[", "]", and ';'
//                     have no special meaning.
//
NODE *reader(FILE *FileStream, ENCODING CharacterEncoding, const char * Prompt)
{
    int paren   = 0;
    int bracket = 0;
    int brace   = 0;

    bool vbar      = false;
    bool contin    = true;
    bool incomment = false;
    bool raw       = false;

    static const wchar_t ender[] = L"\nEND\n";
    const wchar_t *enderProgress = ender;

    static const wchar_t * const localizedEnder = g_LOCALIZED_READER_ENDER;
    const wchar_t *localizedEnderProgress       = localizedEnder;

    NODETYPE this_type = STRING;

    if (!strcmp(Prompt, "RW"))
    {
        /* called by readword */
        Prompt = "";
        contin = false;
    }
    else if (!strcmp(Prompt, "RAW"))
    {
        /* called by readrawline */
        Prompt = "";
        contin = false;
        raw = true;
    }

    bool dribbling = (dribblestream != NULL && FileStream == stdin);

    if (FileStream == stdin)
    {
        if (*Prompt != '\0')
        {
            rd_print_prompt(Prompt);

            if (dribbling)
            {
                fputs(Prompt, dribblestream);
            }
        }

        clear_is_running_erract_flag();
    }

    CStringNodeBuffer lineBuffer;

    int c = rd_getc(FileStream, CharacterEncoding);
    while (c != EOF && (c != '\n' || vbar || paren || bracket || brace))
    {
        if (dribbling)
        {
            write_unicode_char(dribblestream, ENCODING_Utf8, c);
        }

        // if c is a backslash, then read the next character and escape it
        if (!raw && c == '\\')
        {
            c = rd_getc(FileStream, CharacterEncoding);
            if (c == EOF)
            {
                break;
            }

            if (dribbling)
            {
                write_unicode_char(dribblestream, ENCODING_Utf8, c);
            }
            if (c == 'n')
            {
                c = '\n'; //ggm
            }

            // mark this character as being backslashed
            c = vbar_set(c);

            // the resulting string will be backslashed
            this_type = BACKSLASH_STRING;

            if (c == vbar_set('\n') && FileStream == stdin)
            {
                rd_print_prompt("\\ ");
                if (dribbling)
                {
                    fprintf(dribblestream, "\\ ");
                }
            }
        }

        lineBuffer.AppendChar(c);

        if (raw)
        {
            c = rd_getc(FileStream, CharacterEncoding);
            continue;
        }

        if (input_mode == INPUTMODE_To)
        {
            // We are parsing a TO.  Check if we hit the TO ender.

            // Is this the English ender?
            if (uncapital(c) == uncapital(*enderProgress))
            {
                enderProgress++;
                if (*enderProgress == '\0')
                {
                    // We found the ender while still
                    // inside a [, |, (, or {.
                    err_logo(DEEPEND, deepend_proc_name);
                    break;
                }
            }
            else
            {
                // start over on the ender
                enderProgress = ender;
            }

            // Is this the localized (non-English) ender?
            if (uncapital(c) == uncapital(*localizedEnderProgress))
            {
                localizedEnderProgress++;
                if (*localizedEnderProgress == '\0')
                {
                    // We found the ender while still
                    // inside a [, |, (, or {.
                    err_logo(DEEPEND, deepend_proc_name);
                    break;
                }
            }
            else
            {
                // start over on the localized ender
                localizedEnderProgress = localizedEnder;
            }
        }

        if (!incomment)
        {
            if (c == '|')
            {
                vbar = !vbar;
            }
            else if (contin && !vbar)
            {
                switch (c)
                {
                case '(':
                    paren++;
                    break;

                case ')':
                    if (paren)
                    {
                        paren--;
                    }
                    break;

                case '[':
                    bracket++;
                    break;

                case ']':
                    if (bracket)
                    {
                        bracket--;
                    }
                    break;

                case '{':
                    brace++;
                    break;

                case '}':
                    if (brace)
                    {
                        brace--;
                    }
                    break;

                case ';':
                    incomment = true;
                    break;
                }
            }
        }

        // any string that contains special characters needs to be
        // converted to a VBAR string so that it can be printed
        // out and re-read without a loss of data.  See bug #323.
        if (this_type == STRING && is_special_character(c))
        {
            this_type = VBAR_STRING;
        }

        if (c == '\n')
        {
            // newlines end comment
            incomment = false;
            if (FileStream == stdin)
            {
                rd_print_prompt(vbar ? "| " : "~ ");

                if (dribbling)
                {
                    fprintf(dribblestream, vbar ? "| " : "~ ");
                }
            }
        }

        while (!vbar &&
               c == '~' &&
               (c = rd_getc(FileStream, CharacterEncoding)) != EOF)
        {
            CDynamicBuffer whitespace;

            // Copy linear whitespace to the "whitespace" buffer.
            // We want to ignore if it the goes to the end of the
            // line, but we want to keep it if it does not.
            while (c == ' ' || c == '\t')
            {
                whitespace.AppendChar(c);
                c = rd_getc(FileStream, CharacterEncoding);
            }

            if (c != '\n')
            {
                // append the linear whitespace that we skipped (if any)
                if (whitespace.GetBufferLength() != 0)
                {
                    const wchar_t * whitespaceString = whitespace.GetBuffer();
                    lineBuffer.AppendString(whitespaceString);
                    if (dribbling)
                    {
                        write_unicode_string(
                            dribblestream,
                            ENCODING_Utf8,
                            whitespaceString);
                    }
                }
            }

            if (dribbling)
            {
                write_unicode_char(dribblestream, ENCODING_Utf8, c);
            }

            lineBuffer.AppendChar(c);

            if (c == '\n' && FileStream == stdin)
            {
                incomment = false;

                rd_print_prompt("~ ");

                if (dribbling)
                {
                    fprintf(dribblestream, "~ ");
                }
            }
        }

        c = rd_getc(FileStream, CharacterEncoding);
    }

    if (dribbling)
    {
        putc('\n', dribblestream);
    }

    lineBuffer.TakeOwnershipOfBuffer();

    NODE * line = make_strnode_no_copy(
        lineBuffer.GetStringPointer(),
        lineBuffer.GetBufferHeader(),
        lineBuffer.GetStringLength(),
        this_type);

    return line;
}

static
NODE *list_to_array(NODE *list)
{
    int len = list_length(list);

    NODE * result = make_array(len);
    if (NOT_THROWING)
    {
        setarrorg(result, 1);

        int i = 0;
        for (NODE * np = list; np; np = cdr(np))
        {
            (getarrptr(result))[i++] = vref(car(np));
        }
    }

    return result;
}

#define parens(ch)      (ch == '(' || ch == ')' || ch == ';')
#define infixs(ch)      (ch == '*' || ch == '/' || ch == '+' || ch == '-' || ch == '=' || ch == '<' || ch == '>')
#define white_space(ch) (ch == ' ' || ch == '\t' || ch == '\n')

// Parses from "*input_line" to "inlimit" (exclusive), or until the first "endchar" is found.
// If endchar==-1, then the only limit is inlimit, which is interpreted as the EOF.
// *input_line is updated to be the character on which parsing stopped.
// The parsed text is returned as a NODE* list.
// Nested arrays and lists are parsed into their structured form.
// parser_iterate() sets stopping_flag to THROWING if it encounters a syntax error.
//
// if "ignore_comments" is true then comments should be stripped out of the parsed stream.
//
static
NODE *
parser_iterate(
    const wchar_t ** input_line,
    const wchar_t *  inlimit,
    bool             ignore_comments,
    int              endchar
    )
{
    const wchar_t *word_start;
    int word_length = 0;

    NODETYPE this_type = STRING;

    bool broken = false;
    bool vbar   = false;

    CAppendableList return_list;

    const wchar_t * char_ptr = *input_line; // generally ptr to next char

    while (char_ptr <= inlimit)
    {
        if (!vbar && word_length == 0)
        {
            word_start = char_ptr;
        }

        /* get the current character and increase pointer */
        wchar_t ch = *char_ptr;
        ++char_ptr;

        /* skip through comments and line continuations */
        while (char_ptr <= inlimit &&
               !vbar &&
               ((ignore_comments && ch == ';') || (ch == '~' && *char_ptr == '\n')))
        {
            // If this is a line continuation, keep going
            // until we reach the end of the next line
            while (ch == '~' && *char_ptr == '\n')
            {
                // Advance to the next character.
                if (++char_ptr >= inlimit)
                {
                    break;
                }
                ch = *char_ptr;

                if (word_length == 0)
                {
                    // The line continuation character wasn't inserted in the
                    // middle of a word, so start a new one.
                    word_start = char_ptr;
                }
                else
                {
                    // if we are in the middle of a word and we continued into
                    // the start of an array/list, then set "ch" to insert a word
                    // boundary.
                    if (ch == '[' || ch == ']' || ch == '{' || ch == '}')
                    {
                        ch = ' ';
                        break;
                    }
                    else
                    {
                        // The logical line requires mending across physical lines.
                        broken = true;
                    }
                }

                // Mark the we have processed "ch".
                ++char_ptr;
            }

            // If this is a comment and we're supposed to ignore
            // comments keep going until the end of the line.
            if (ignore_comments && ch == ';')
            {
                // While *char_ptr is not the end of the string and
                // the next char is not the end of the line
                while (char_ptr <= inlimit && *char_ptr != '\n')
                {
                    if (word_length == 0)
                    {
                        // The comment character wasn't inserted in the
                        // middle of a word, so start a new one, just after
                        // the comment.
                        word_start = char_ptr;
                    }
                    else
                    {
                        // The comment character was inserted in the middle of a word,
                        // so flag that this line needs mending.
                        broken = true;
                    }

                    // advance to the next character
                    ch = *char_ptr;
                    char_ptr++;
                }

                if (ch != '~')
                {
                    // We've reached the end of a comment that doesn't have a
                    // line continuation.  From the comment character onward
                    // is equivalent to a newline.
                    ch = '\n';
                }
            }
        }

        // flag that this word will be of BACKSLASH_STRING type
        if (is_vbarred(ch))
        {
            this_type = BACKSLASH_STRING;
        }

        if (ch == '|')
        {
            vbar = !vbar;
            this_type = VBAR_STRING;
            broken = true;   // so we'll copy the chars
        }
        else if (vbar || (!white_space(ch) && ch != ']' &&
                          ch != '{' && ch != '}' &&
                          ch != '[' && char_ptr <= inlimit))
        {
            // This is another character in a simple word, like "abc
            // or a vbar quoted word like "| abc |
            // Either way, extend the word to include this character.
            word_length++;
        }

        NODE *tnode = NIL;

        if (endchar == -1 && inlimit <= char_ptr)
        {
            // We have reached the EOF for the outermost invocation.
            if (ch == '~')
            {
                // We were parsing a line continuation at the time of the EOF.
                // Since there's nothing to continue to, close the
                // string where we are (removing the '~').
                word_length--;
            }
        }

        // Inside a vbar'd string, only a vbar can terminate it.
        if (vbar)
        {
            continue;
        }

        if (ch == endchar && endchar != -1)
        {
            // We have reached the character that we were seeking.
            // Break out of the loop, leaving endchar for the caller
            // to process.
            break;
        }

        if (ch == ']')
        {
            err_logo(UNEXPECTED_BRACKET, NIL);
        }
        else if (ch == '}')
        {
            err_logo(UNEXPECTED_BRACE, NIL);
        }
        else if (ch == '[')
        {
            // this is a '[', parse a new list (until we see a ])
            tnode = cons_list(parser_iterate(&char_ptr, inlimit, ignore_comments, ']'));
        }
        else if (ch == '{')
        {
            // this is a '{', parse a new array (until we see a })
            NODE * array_as_list = parser_iterate(&char_ptr, inlimit, ignore_comments, '}');
            NODE * array         = list_to_array(array_as_list);
            gcref(array_as_list);

            if (NOT_THROWING)
            {
                tnode = cons_list(array);
                if (*char_ptr == '@')
                {
                    // parse the origin as a decimal number
                    wchar_t * unused_portion;
                    long origin = wcstol(char_ptr + 1, &unused_portion, 10);
                    char_ptr = unused_portion;
                    setarrorg(car(tnode), origin);
                }
            }
            else
            {
                tnode = NIL;
            }
        }
        else if (white_space(ch) || char_ptr > inlimit ||
                 *char_ptr == ']' || *char_ptr == '[' ||
                 *char_ptr == '{' || *char_ptr == '}')
        {
            // The next character terminates the word.
            if (word_length > 0)
            {
                // Create any word that we have parsed up until now.
                void (*copyRoutine) (wchar_t *, const wchar_t *, int);

                if (broken)
                {
                    // The logical line is broken across several lines,
                    // so it must be mended.
                    copyRoutine = ignore_comments ? mend_strnzcpy : mend_nosemi;
                }
                else
                {
                    // The logical line is entirely on a single line,
                    // so a normal string copy will suffice.
                    copyRoutine = strnzcpy;
                }

                NODE * string_node = make_strnode(
                    word_start,
                    word_length,
                    this_type,
                    copyRoutine);

                tnode = cons_list(string_node);
                this_type = STRING;
                word_length = 0;
                broken = false;
            }
        }

        // put the word onto the end of the return list
        return_list.AppendList(tnode);
    }

    // Update *input_list to where we stopped parsing.
    *input_line = char_ptr;

    return return_list.GetList();
}

// Parses a node (list or word) into a list.
// if "ignore_comments" is true then comments are not included in the
// returned node.
//
// Returns a list with a reference count of 0, which the caller must free.
// Sets stopping_flag to THROWING if it encounters a syntax error.
NODE *parser(NODE *nd, bool ignore_comments)
{
    NODE * string_node = cnv_node_to_strnode(nd);
    ref(string_node);

    int             slen  = getstrlen(string_node);
    const wchar_t * lnsav = getstrptr(string_node);
    NODE * rtn = parser_iterate(&lnsav, lnsav + slen, ignore_comments, -1);

    gcref(nd);
    deref(string_node);
    return rtn;
}

NODE *lparse(NODE *args)
{
    NODE *val = Unbound;

    NODE * arg = string_arg(args);
    if (NOT_THROWING)
    {
        val = parser(arg, false);
    }
    return val;
}

// Parses token (nd) from a line (list of words) into a runnable form,
// with quoted, colon, operator case nodes.
//
// nd     - the word to parse
// ndsptr - a pointer to set to NIL if a comment is seen (which
//          indicates that the rest of the line should be ignored.
//
// Returns a list, which is the runnable form of nd.
// This may be many nodes.
static
NODE *runparse_node(NODE *nd, NODE **ndsptr)
{
    if (nd == Minus_Tight)
    {
        // Because Minus_Tight is named "--" (two minus signs), runparsing
        // it with the logic below would change it to:
        //   [ Minus_Tight Minus_Sign ]
        // To avoid the changing the semantics, we need a special case that
        // treats a Minus_Tight as it is.
        //
        // REVISIT: if Minus_Tight had the name "-" and wasn't interned,
        // we might be able to remove this special case.
        return cons_list(nd);
    }

    NODE * snd = cnv_node_to_strnode(nd);
    const wchar_t  *wptr  = getstrptr(snd);
    const int       wlen  = getstrlen(snd);
    NODETYPE        wtyp  = nodetype(snd);
    int             wcnt  = 0;

    CAppendableList return_list;

    while (wcnt < wlen)
    {
        if (*wptr == ';')
        {
            // a comment stops parsing
            *ndsptr = NIL;
            break;
        }

        NODE *tnode;

        if (*wptr == '"')
        {
            // a quoted word
            int tcnt = 0;
            const wchar_t * tptr = ++wptr;
            wcnt++;
            while (wcnt < wlen && !parens(*wptr))
            {
                if (wtyp == BACKSLASH_STRING && is_vbarred(*wptr))
                {
                    wtyp = PUNBOUND; // flag for "\( case
                }
                wptr++, wcnt++, tcnt++;
            }

            NODE * strnode;
            if (wtyp == PUNBOUND)
            {
                wtyp = BACKSLASH_STRING;
                strnode = make_strnode(tptr, tcnt, wtyp, clearvbar_strnzcpy);
            }
            else
            {
                strnode = make_strnode(tptr, tcnt, wtyp, strnzcpy);
            }
            tnode = make_quote(intern(strnode));
        }
        else if (*wptr == ':')
        {
            // a variable value, like :x
            int tcnt = 0;
            const wchar_t * tptr = ++wptr;
            wcnt++;
            while (wcnt < wlen && !parens(*wptr) && !infixs(*wptr))
            {
                wptr++, wcnt++, tcnt++;
            }
            tnode = make_colon(intern(make_strnode(tptr, tcnt, wtyp, strnzcpy)));
        }
        else if (parens(*wptr) || infixs(*wptr))
        {
            if (wcnt == 0 && // just after a space (at the begining of the token)
                *wptr == '-' && // it's a minus sign
                wcnt+1 < wlen && // and there's at least one character following
                !white_space(*(wptr + 1))) // and the next character isn't a space
            {
                // A minus sign with space before and no space after is treated
                // as unary (a negative sign) instead  of subtraction.  This ensures
                // that "SUM 1 -X" gets two inputs "1" and "-X", instead of one
                // one "1 - X".  The negation is faked as subtraction (0 - X) with
                // a variant that has high precedence.  The "0" is added now and
                // the "- X" is added below.

                // append "0" now
                return_list.AppendElement(make_intnode(0));

                // append "-" later
                tnode = Minus_Tight;
            }
            else if (wcnt+1 < wlen &&
                     ((wptr[0] == '<' && (wptr[1] == '=' || wptr[1] == '>')) ||
                      (wptr[0] == '>' && wptr[1] == '=')))
            {
                // This is a two character infix operator: "<=" or ">=" or "<>".
                tnode = intern(make_strnode(wptr, 2, STRING, strnzcpy));
                wptr++, wcnt++; // advance the pointers for the extra character
            }
            else
            {
                // This is a one letter paren/operator.
                tnode = intern(make_strnode(wptr, 1, STRING, strnzcpy));
            }
            wptr++, wcnt++;
        }
        else
        {
            int tcnt = 0;
            const wchar_t * tptr = wptr;
            // isnumb
            // 0 means digits so far,
            // 1 means just saw 'e' so minus can be next (exponent)
            // 2 means no longer eligible even if an 'e' comes along
            // 3 means we saw a '?'
            // 4 means nothing yet;
            int isnumb = 4;
            if (*wptr == '?')
            {
                // turn ?5 to (? 5)
                isnumb = 3;
                wptr++, wcnt++, tcnt++;
            }

            bool gotdot = false;
            while (wcnt < wlen    &&
                   !parens(*wptr) &&
                   (!infixs(*wptr) || (isnumb == 1 && (*wptr == '-' || *wptr == '+'))))
            {
                if (isnumb == 4 && IsDigit(*wptr))
                {
                    // could be a number
                    isnumb = 0;
                }

                if (isnumb == 0 && (*wptr == 'e' || *wptr == 'E'))
                {
                    // saw digits followed by an 'e', so expect an exponent
                    // (optional '-' or '+', followed by digits)
                    isnumb = 1;
                }
                else if ((!IsDigit(*wptr) && (*wptr != '.' || gotdot)) || isnumb == 1)
                {
                    // can't be a number, but we need to keep collecting
                    // characters until we reach a paren or operator.
                    isnumb = 2;
                }

                if (*wptr == '.')
                {
                    gotdot = true;
                }
                wptr++, wcnt++, tcnt++;
            }

            if (MAX_NUMBER <= tcnt)
            {
                // too many characters to be a valid number
                isnumb = 2;
            }

            if (isnumb == 3 && tcnt > 1)
            {
                // turn "?5" into (? 5)

                // append "( ? 5" now
                NODE * strnode = make_strnode(tptr + 1, tcnt - 1, wtyp, strnzcpy);
                NODE * qmtnode = cons_list(
                    Left_Paren,
                    Query,
                    cnv_node_to_numnode(strnode));
                gcref(strnode);

                return_list.AppendList(qmtnode);

                // append ")" later
                tnode = Right_Paren;
            }
            else if (isnumb < 2)
            {
                // is a number (with optional exponent)
                NODE * strnode = make_strnode(tptr, tcnt, wtyp, strnzcpy);
                tnode = cnv_node_to_numnode(strnode);
                gcref(strnode);
            }
            else
            {
                // treat it as a case word
                tnode = intern(make_strnode(tptr, tcnt, wtyp, strnzcpy));
            }
        }

        // append tnode to the end of return_list
        return_list.AppendElement(tnode);
    }
    gcref(snd);
    return return_list.GetList();
}

NODE *runparse(NODE *ndlist)
{
    if (nodetype(ndlist) == RUN_PARSE)
    {
        // already run-parsed
        return parsed__runparse(ndlist);
    }

    if (!is_list(ndlist))
    {
        err_logo(BAD_DATA_UNREC, ndlist);
        return NIL;
    }

    if (ndlist != NIL)
    {
        NODE * curnd = car(ndlist);
        if (is_string(curnd)           &&
            getstrlen(curnd) >= 2      &&
            getstrptr(curnd)[0] == '#' &&
            getstrptr(curnd)[1] == '!')
        {
            // shell-script #! treated as comment line
            return NIL;
        }
    }

    CAppendableList return_list;

    while (ndlist != NIL)
    {
        NODE *tnode;

        NODE * curnd = car(ndlist);
        ndlist = cdr(ndlist);
        if (!is_word(curnd))
        {
            tnode = cons_list(curnd);
        }
        else
        {
            if (!numberp(curnd))
            {
                tnode = runparse_node(curnd, &ndlist);
            }
            else
            {
                tnode = cons_list(cnv_node_to_numnode(curnd));
            }
        }

        // append tnode to the end of return_list
        return_list.AppendList(tnode);

        if (check_throwing)
        {
            break;
        }
    }

    return return_list.GetList();
}

NODE *lrunparse(NODE *args)
{
    // Arrays are illegal inputs.
    NODE * arg = car(args);
    while (nodetype(arg) == ARRAY && NOT_THROWING)
    {
        setcar(args, err_logo(BAD_DATA, arg));
        arg = car(args);
    }
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    // If the argument is a word, then parse it into a list.
    // Otherwise, it's already a list and we can use it as-is.
    if (is_word(arg))
    {
        arg = parser(arg, true);
        if (stopping_flag == THROWING)
        {
            // parser() encountered a syntax error.  Free the return value.
            gcref(arg);
            return Unbound;
        }
    }

    // Runparse the parsed argument.
    NODE * rval = runparse(arg);

    // Free the return value from parser().
    // If parser() wasn't called, then arg came from args and would therefore
    // have a non-zero reference count, and so gcref() is a no-op.
    gcref(arg);

    return rval;
}


void uninitialize_parser()
{
    deref(deepend_proc_name);
    deepend_proc_name = NIL;

    g_ReadBuffer.Dispose();
}
