/*
 *      logodata.cpp    logo data management module             dvb
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
   #ifndef WX_PURE
      #include <windows.h>
   #endif

   #include <limits.h>
   #include <ctype.h>
   #include <stdlib.h>
   #include <math.h> // fmod
   #include <cwchar>
   #include <cstring> // memcpy

   #include "logodata.h"
   #include "version.h"
   #include "init.h"
   #include "error.h"
   #include "mem.h"
   #include "parse.h"
   #include "logomath.h"
   #include "lists.h"
   #include "eval.h"
   #include "intern.h"
   #include "print.h"
   #include "files.h"
   #include "appendablelist.h"
   #include "avltree.h"
   #include "debugheap.h"

   #include "internationalizedstrings.h"
#endif

#ifdef WX_PURE
// TODO: The original, non-internationalized code is used when not on Windows.
// This should be rewritten to use the Unicode-aware C routines.
#define upper_p(c) (c >= 'A' && c <= 'Z')
#endif

// FMSLogo has some special characters that must be escaped with a backslash
// for them to appear within word.  These characters are
//
//   ' ', '\t', '\n', '(', '?', '+', '~', ')', '[', ']', '-', '*', '/'
//   '=', '\"', '\\', '<', '>', ':', ';', '|', '{', '}'
//
// Since FMSLogo procedures must printable and re-parsable, the escaping
// is stored within the word NODE*, so that the backslash can be
// re-constructed when needed.
//
// Early versions of FMSLogo (called UCBLogo) only supported ASCII, and
// stored the escaped character by setting the high bit of each byte, which
// would otherwise have been 0 for every character.
//
// To add support for non-English language that use ANSI code points over
// over 127, a different strategy was adopted.  A mapping was maintained
// between each special character and a low character code that are
// usually called "control characters".  The escaped characters were
// stored using these control characters, which meant that the control
// characters could not be used within strings.
//
// Now that FMSLogo stores characters as wchar_t, it uses a similar
// strategy, but instead of low ASCII characters, which are needed to
// support binary data, it uses Unicode code points that are not in use.
// All of the code points are chosen from the Private Use Area: E000-F8FF.

// REVISIT: why do "?" and the infix operators need to be backslashed?
static wchar_t const g_SpecialCharacters[] = {
    ' ', '\t', '\n', '(', '?', '+', '~', ')', '[', ']', '-', '*', '/', '=',
    '<', '>', '"', '\\', ':', ';', '|', '{', '}'
};
const wchar_t MIN_BACKSLASHED_CHAR = 0xE000;

bool
is_special_character(wchar_t ch)
{
    return NULL != wmemchr(g_SpecialCharacters, ch, ARRAYSIZE(g_SpecialCharacters));
}

// Return the vbar'd form of "ch".
wchar_t vbar_set(wchar_t ch)
{
    const wchar_t * specialCharacter = wmemchr(
        g_SpecialCharacters,
        ch,
        ARRAYSIZE(g_SpecialCharacters));
    if (specialCharacter == NULL)
    {
        return ch; // not vbar'd
    }

    return specialCharacter - g_SpecialCharacters + MIN_BACKSLASHED_CHAR;
}

// Return the normal (non-vbar'd) form of "ch".
wchar_t vbar_clear(wchar_t ch)
{
    if (ch < MIN_BACKSLASHED_CHAR)
    {
        return ch; // ch is not vbar'd
    }

    size_t specialCharacterIndex = ch - MIN_BACKSLASHED_CHAR;
    if (ARRAYSIZE(g_SpecialCharacters) <= specialCharacterIndex)
    {
        return ch; // ch is not vbar'd
    }

    // ch is vbar'd
    return g_SpecialCharacters[specialCharacterIndex];
}

// return true if "ch" is vbar'd.
// return false, otherwise.
bool is_vbarred(wchar_t ch)
{
    return MIN_BACKSLASHED_CHAR <= ch &&
        ch < MIN_BACKSLASHED_CHAR + ARRAYSIZE(g_SpecialCharacters);
}

void strnzcpy(wchar_t *dst, const wchar_t *src, int len)
{
    memcpy(dst, src, len * sizeof(*src));
    dst[len] = '\0';
}

void word_strnzcpy(wchar_t *dst, NODE *wordlist, int len)
{
    while (wordlist != NIL)
    {
        NODE * word = car(wordlist);
        memcpy(dst, getstrptr(word), getstrlen(word) * sizeof(*dst));
        dst += getstrlen(word);
        wordlist = cdr(wordlist);
    }
    *dst = '\0';
}

void clearvbar_strnzcpy(wchar_t *dst, const wchar_t * src, int len)
{
    for (int i = 0; i < len; i++)
    {
        dst[i] = vbar_clear(src[i]);
    }
    dst[len] = '\0';
}

void mend_strnzcpy(wchar_t *dst, const wchar_t * src, int len)
{
    bool vbar = false;

    for (int i = 0; i < len;)
    {
        while (*src == '|')
        {
            vbar = !vbar;
            src++;
        }
        if (vbar)
        {
            dst[i++] = vbar_set(*src++);
        }
        else
        {
            // Advance the "src" pointer to the next non-comment
            // or next non-continuation character.
            while (*src == ';' || (*src == '~' && *(src + 1) == '\n'))
            {
                // Advance beyond any simple line continuation sequences.
                while (*src == '~' && *(src + 1) == '\n')
                {
                    src += 2;
                }

                // Advance past the comment until the EOL (the normal
                // comment ender) or until we reach a line continuation sequence
                if (*src == ';')
                {
                    do
                    {
                        src++;
                    } while (*src != '\0' && *src != '~' && *(src + 1) != '\n');
                }
            }

            // If we're not on a vbar, copy the character to the output string.
            // Otherwise, continue the loop and enter the special vbar handling
            // above.
            if (*src != '|')
            {
                dst[i++] = *src++;
            }
        }
    }
    dst[len] = '\0';
}

void mend_nosemi(wchar_t *dst, const wchar_t * src, int len)
{
    bool vbar = false;

    for (int i = 0; i < len;)
    {
        while (*src == '|')
        {
            vbar = !vbar;
            src++;
        }
        if (vbar)
        {
            dst[i++] = vbar_set(*src++);
        }
        else
        {
            while ((*src == '~' && *(src + 1) == '\n'))
            {
                while (*src == '~' && *(src + 1) == '\n')
                {
                    src += 2;
                }
            }
            if (*src != '|')
            {
                dst[i++] = *src++;
            }
        }
    }
    dst[len] = '\0';
}

void quote_strnzcpy(wchar_t *dst, const wchar_t *src, int len)
{
    dst[0] = '"';
    memcpy(dst + 1, src, (len - 1) * sizeof(*src));
    dst[len] = '\0';
}

void colon_strnzcpy(wchar_t *dst, const wchar_t * src, int len)
{
    dst[0] = ':';
    memcpy(dst + 1, src, (len - 1) * sizeof(*src));
    dst[len] = '\0';
}

wchar_t
uncapital(
    wchar_t Capital
    )
{
    wchar_t lowercase;

#ifdef WX_PURE
    lowercase = tolower(Capital);
#else
    LCMapString(
        MAKELCID(LANG_USER_DEFAULT, SORT_DEFAULT),
        LCMAP_LOWERCASE,
        &Capital,
        sizeof(Capital),
        &lowercase,
        sizeof(lowercase));
#endif

    return lowercase;
}

static
bool
islowercase(
    wchar_t Character
    )
{
    return uncapital(Character) == Character;
}

void low_strnzcpy(wchar_t *dst, const wchar_t * src, int len)
{
#ifdef WX_PURE
    for (int i = 0; i < len; i++)
    {
        dst[i] = uncapital(src[i]);
    }
#else
    LCMapString(
        MAKELCID(LANG_USER_DEFAULT, SORT_DEFAULT),
        LCMAP_LOWERCASE,
        src,
        len,
        dst,
        len);
#endif

    dst[len] = '\0';
}

wchar_t
capital(
    wchar_t LowerCase
    )
{
    wchar_t capital;

#ifdef WX_PURE
    capital = toupper(LowerCase);
#else
    LCMapString(
        MAKELCID(LANG_USER_DEFAULT, SORT_DEFAULT),
        LCMAP_UPPERCASE,
        &LowerCase,
        sizeof(LowerCase),
        &capital,
        sizeof(capital));
#endif

    return capital;
}

void cap_strnzcpy(wchar_t *dst, const wchar_t * src, int len)
{
#ifdef WX_PURE
    for (size_t i = 0; i < len; i++)
    {
        dst[i] = capital(src[i]);
    }
#else
    LCMapString(
        MAKELCID(LANG_USER_DEFAULT, SORT_DEFAULT),
        LCMAP_UPPERCASE,
        src,
        len,
        dst,
        len);
#endif

    dst[len] = '\0';
}

void NormalizeCaseForDisplay(wchar_t *dst, const wchar_t * src, int len)
{
    return cap_strnzcpy(dst, src, len);
}

void clearvbar_low_strnzcpy(wchar_t *dst, const wchar_t *src, int len)
{
    for (int i = 0; i < len; i++)
    {
        wchar_t c = vbar_clear(*src++);
        *dst++ = uncapital(c);
    }
    *dst = '\0';
}

int low_strncmp(const wchar_t *string1, const wchar_t * string2, int length)
{
#ifdef WX_PURE
    for (size_t i = 0; i < length; i++)
    {
        if (*string1 != *string2)
        {
            if (upper_p(*string2))
            {
                if (upper_p(*string1))
                {
                    if (uncapital(*string1) != uncapital(*string2))
                        return (uncapital(*string1) - uncapital(*string2));
                }
                else
                {
                    if (*string1 != uncapital(*string2))
                    {
                        return (*string1 - uncapital(*string2));
                    }
                }
            }
            else if (upper_p(*string1))
            {
                if (uncapital(*string1) != *string2)
                {
                    return (uncapital(*string1) - *string2);
                }
            }
            else
            {
                return (*string1 - *string2);
            }
        }
        string1++;
        string2++;
    }

    return 0;

#else
    int rval = CompareString(
        MAKELCID(LANG_USER_DEFAULT, SORT_DEFAULT),
        NORM_IGNORECASE,
        string1,
        length,
        string2,
        length);

    // convert from CompareString() to C return value
    rval -= 2;

    return rval;
#endif
}

int clearvbar_strncmp(const wchar_t * s1, const wchar_t * s2, int len)
{
    for (int i = 0; i < len; i++)
    {
        wchar_t c1 = vbar_clear(*s1);
        wchar_t c2 = vbar_clear(*s2);
        if (c1 != c2)
        {
            return c1 - c2;
        }
        s1++, s2++;
    }
    return 0;
}

int clearvbar_low_strncmp(const wchar_t * s1, const wchar_t * s2, int len)
{
    for (int i = 0; i < len; i++)
    {
        wchar_t c1 = vbar_clear(*s1);
        wchar_t c2 = vbar_clear(*s2);
        if (c1 != c2)
        {
            int rval = low_strncmp(&c1, &c2, 1);
            if (rval != 0)
            {
                return rval;
            }
        }
        s1++;
        s2++;
    }

    return 0;
}



// Makes a string node by copying a NUL-terminated string
// into a NODE structure.
//
// string       - the string to copy into a NODE.
// len          - number of characters in string.
// typ          - the type of string node.
// copy_routine - the way in which string should be copied
NODE *
make_strnode(
    const wchar_t * string,
    int             len,
    NODETYPE        typ,
    void          (*copy_routine) (wchar_t *, const wchar_t *, int)
    )
{
    assert(typ & NT_STRING);

    if (len == 0 && Null_Word != NIL)
    {
        return Null_Word;
    }

    // allocate enough to hold the header character, the string, and NUL.
    void * buffer = malloc((len + 2) * sizeof(wchar_t));
    if (buffer == NULL)
    {
        err_logo(OUT_OF_MEM, NIL);
        return Unbound;
    }

    // set the reference count to 1.
    wchar_t * header = static_cast<wchar_t *>(buffer);
    setstrrefcnt(header, 1);

    // set the "string pointer" to just after the header
    wchar_t * strptr = header + 1;
    copy_routine(strptr, string, len);

    NODE * strnode = newnode(typ);
    setstrlen(strnode, len);
    setstrptr(strnode, strptr);
    setstrhead(strnode, header);
    return strnode;
}

// Makes a STRING node by copying a NUL-terminated string
// into a NODE structure.
// It uses strnzcpy as its copy routine.
NODE *
make_strnode(
    const wchar_t *string
    )
{
    return make_strnode(string, wcslen(string), STRING, strnzcpy);
}


// Makes a string node by copying from a list of word nodes.
NODE *
make_strnode_from_wordlist(
    NODE *      wordlist,
    int         len,
    NODETYPE    typ
    )
{
    if (len == 0 && Null_Word != NIL)
    {
        return Null_Word;
    }

    // allocate enough to hold the header, the string, and NUL.
    void * buffer = malloc((len + 2) * sizeof(wchar_t));
    if (buffer == NULL)
    {
        err_logo(OUT_OF_MEM, NIL);
        return Unbound;
    }

    // set the reference count to 1.
    wchar_t * header = static_cast<wchar_t *>(buffer);
    setstrrefcnt(header, 1);

    // set the "string pointer" to just after the header
    wchar_t * strptr = header + 1;
    word_strnzcpy(strptr, wordlist, len);

    NODE * strnode = newnode(typ);
    setstrlen(strnode, len);
    setstrptr(strnode, strptr);
    setstrhead(strnode, header);
    return strnode;
}


// Makes a string node by incrementing the reference count on a given
// "string buffer".
//
// strptr - pointer to a NUL-terminated string to use.
// header - pointer to the string buffer's header.
//          This must have been allocated with malloc.
// len    - number of characters in strptr, not including the terminal NUL.
// typ    - the type of NODE to create, must contain NT_STRING
NODE *
make_strnode_no_copy(
    const wchar_t  * strptr,
    wchar_t        * header,
    int              len,
    NODETYPE         typ
    )
{
    // increment the reference count
    assert(header != NULL);       // string is in static memory
    assert(*header != USHRT_MAX); // ref count would overflow
    assert(Null_Word != NIL);     // Null_Word should be created some other way.

    if (len == 0)
    {
        // The caller wants the empty string NODE, so we can give
        // them Null_Word instead of allocating a new node for them.
        // Since we will not take a reference on the buffer, we must free it.
        if (getstrrefcnt(header) == 0)
        {
            free(header);
        }

        return Null_Word;
    }

    incstrrefcnt(header);

    NODE * strnode = newnode(typ);
    setstrlen(strnode, len);
    setstrptr(strnode, strptr);
    setstrhead(strnode, header);
    return strnode;
}

NODE *
make_lowercase_strnode_from_strnode(
    NODE * OriginalNode
    )
{
    assert(is_string(OriginalNode));

    // Check to see if this is already a lower-case node.
    // There's no need to allocate a new node if the current
    // one will do.

    const wchar_t * originalString    = getstrptr(OriginalNode);
    size_t       originalStringLength = getstrlen(OriginalNode);
    for (size_t i = 0; i < originalStringLength; i++)
    {
        if (!islowercase(originalString[i]))
        {
            // OriginalNode is not already lowercase.
            return make_strnode(
                originalString,
                originalStringLength,
                nodetype(OriginalNode),
                low_strnzcpy);
        }
    }

    // OriginalNode is already a lower-case string
    return OriginalNode;
}

void make_runparse(NODE *ndi)
{
    NODE * rp_list = runparse(ndi);
    setobject(ndi, rp_list);
    settype(ndi, RUN_PARSE);
}

NODE *make_quote(NODE *qnd)
{
    NODE * nd = cons_list(qnd);

    settype(nd, QUOTE);
    return nd;
}

NODE *maybe_quote(NODE *nd)
{
    if (nd == Unbound || is_aggregate(nd) || numberp(nd))
    {
        return nd;
    }
    return make_quote(nd);
}

NODE *make_caseobj(NODE *cstrnd, NODE *obj)
{
    NODE *nd = cons(cstrnd, obj);

    settype(nd, CASEOBJ);
    return nd;
}

NODE *make_colon(NODE *cnd)
{
    NODE *nd = cons_list(cnd);

    settype(nd, COLON);
    return nd;
}

NODE *make_intnode(FIXNUM i)
{
    NODE *nd = newnode(INTEGER);

    nd->nunion.nint = i;
    return nd;
}

NODE *make_floatnode(FLONUM f)
{
    NODE *nd = newnode(FLOATINGPOINT);

    nd->nunion.nfloat = f;
    return nd;
}

// Returns an INTEGER node that has the value in ndi if it can be interpreted
// as an integer without any information loss.
// Caller must call gcref() on the returned value.
// If the node cannot be converted to an integer, then Unbound is returned.
NODE *cnv_node_to_intnode(NODE *ndi)
{
    NODE * val = cnv_node_to_numnode(ndi);
    if (nodetype(val) == INTEGER)
    {
        return val;
    }

    if (nodetype(val) == FLOATINGPOINT)
    {
        FLONUM f = getfloat(val);
        gcref(val);
        if (-(FLONUM) FIXNUM_MAX <= f && f <= (FLONUM) FIXNUM_MAX &&
            fmod(f, 1.0) == 0.0)
        {
            // The floating point node can be converted to an integer
            // without information loss.
            FIXNUM i = f;
            return make_intnode(i);
        }
    }

    return Unbound;
}

// Returns a numeric node (INTEGER or FLOAT) that has the value in ndi if it can
// be interpreted as a number.
// Caller must call gcref() on the returned value
NODE *cnv_node_to_numnode(NODE *ndi)
{
    if (is_number(ndi))
    {
        return ndi;
    }

    NODE * strnode = cnv_node_to_strnode(ndi);
    if (strnode == Unbound)
    {
        return Unbound;
    }

    int dr;
    if (getstrlen(strnode) < MAX_NUMBER && (dr = numberp(strnode)))
    {
        wchar_t buffer[MAX_NUMBER];

        // Copy the contents of the string node into buffer
        if (backslashed(strnode))
        {
            clearvbar_strnzcpy(buffer, getstrptr(strnode), getstrlen(strnode));
        }
        else
        {
            strnzcpy(buffer, getstrptr(strnode), getstrlen(strnode));
        }

        // If the string ends in a decimal point, as in "123.", then
        // remove the decimal point.
        if (buffer[getstrlen(strnode) - 1] == '.')
        {
            buffer[getstrlen(strnode) - 1] = '\0';
        }

        // Convert the string to a number node.
        NODE * val;
        if (dr - 1 || getstrlen(strnode) > 9)
        {
            val = make_floatnode(wcstod(buffer, NULL));
        }
        else
        {
            val = make_intnode(wcstol(buffer, NULL, 10));
        }
        if (strnode != ndi)
        {
            gcref(strnode);
        }
        return val;
    }
    else
    {
        if (strnode != ndi)
        {
            gcref(strnode);
        }
        return Unbound;
    }
}

// Returns a string node that has the stringized form of "nd".
//
// If nd is already a string node, then it is returned.
//
// If nd is not already a string, but can be converted to one, then new node is
// created with a reference count of 0.
//
// Returns Unbound if nd cannot be converted to a word node.
//
// If "nd" can be guaranteed by the caller to have a non-zero reference count,
// such as if it's an argument in an argument list, then the returned node
// can safely freed with gcref().  This handles all cases listed above.
NODE *cnv_node_to_strnode(NODE *nd)
{
    if (nd == Unbound || is_aggregate(nd))
    {
        return Unbound;
    }

    wchar_t s[MAX_NUMBER];
    switch (nodetype(nd))
    {
    case STRING:
    case BACKSLASH_STRING:
    case VBAR_STRING:
        // the node is already a string
        return nd;

    case CASEOBJ:
        return strnode__caseobj(nd);

    case QUOTE:
        nd = vref(cnv_node_to_strnode(node__quote(nd)));
        assign(
            nd,
            make_strnode(getstrptr(nd), getstrlen(nd) + 1, nodetype(nd), quote_strnzcpy));
        unref(nd);
        return nd;

    case COLON:
        nd = vref(cnv_node_to_strnode(node__colon(nd)));
        assign(
            nd,
            make_strnode(getstrptr(nd), getstrlen(nd) + 1, nodetype(nd), colon_strnzcpy));
        unref(nd);
        return nd;

    case INTEGER:
        swprintf(s, ARRAYSIZE(s), L"%ld", getint(nd));
        return make_strnode(s);

    case FLOATINGPOINT:
        swprintf(s, ARRAYSIZE(s), L"%0.15g", getfloat(nd));
        return make_strnode(s);

    default: // stop compiler warnings
        assert(0);
    }

    /*NOTREACHED*/
    assert(0);
    return NIL;
}

// Returns a string node with refcount=0 from the given string.
// The string is referenced in-place and so must not be freed until
// after the node has been garbage collected.  This is easiest to
// guarantee for strings in static memory.
NODE *make_static_strnode(const wchar_t *strptr)
{
    NODE *strnode = newnode(STRING);

    setstrptr(strnode, strptr);
    setstrhead(strnode, NULL);
    setstrlen(strnode, (int) wcslen(strptr));
    return strnode;
}

// Returns a NODE with refcount=0.
// Caller must free.
NODE *cons(NODE *x, NODE *y)
{
    NODE *val = newnode(CONS);

    val->nunion.ncons.ncar = vref(x);
    val->nunion.ncons.ncdr = vref(y);
    val->nunion.ncons.nobj = NIL;
    return val;
}

NODE *cons_list()
{
    return NIL;
}

NODE *cons_list(NODE * node1)
{
    return cons(node1, NIL);
}

NODE *cons_list(NODE * node1, NODE * node2)
{
    return cons(
        node1,
        cons_list(node2));
}

NODE *cons_list(NODE * node1, NODE * node2, NODE * node3)
{
    return cons(
        node1,
        cons_list(node2, node3));
}

NODE *cons_list(NODE * node1, NODE * node2, NODE * node3, NODE * node4)
{
    return cons(
        node1,
        cons_list(node2, node3, node4));
}

NODE *cons_list(NODE * node1, NODE * node2, NODE * node3, NODE * node4, NODE * node5)
{
    return cons(
        node1,
        cons_list(node2, node3, node4, node5));
}

int list_length(const NODE * List)
{
    int length = 0;

    for (const NODE * current_node = List;
         current_node != NIL;
         current_node = cdr(current_node))
    {
        length++;
    }

    return length;
}

// Constructs an array node with "len" elements, all of which are NIL.
// Returns Unbound when out of memory.
NODE *make_array(int len)
{
    assert(0 <= len);  // can't have a negative length array

    NODE ** data;
    if (len != 0)
    {
        // calloc() handles both integer overflow and
        // initializing each of the members to NIL.
        data = (NODE **) calloc(sizeof *data, len);
        if (data == NULL)
        {
            // The array is too large to allocate.
            err_logo(OUT_OF_MEM, NIL);
            return Unbound;
        }
    }
    else
    {
        data = NULL;
    }

    NODE* node = newnode(ARRAY);
    setarrptr(node, data);
    setarrorg(node, 1);
    setarrdim(node, len);
    return node;
}

NODE *llowercase(NODE *args)
{
    NODE * arg = string_arg(args);
    if (NOT_THROWING)
    {
        return make_strnode(getstrptr(arg), getstrlen(arg), nodetype(arg), low_strnzcpy);
    }
    return Unbound;
}

NODE *luppercase(NODE *args)
{
    NODE * arg = string_arg(args);
    if (NOT_THROWING)
    {
        return make_strnode(getstrptr(arg), getstrlen(arg), nodetype(arg), cap_strnzcpy);
    }
    return Unbound;
}

// *****************************************************
// Property list stuff
// *****************************************************

// A well-ordered comparison function for arbitrary nodes
//
// Return Value:
// <0, if NodeA < NodeB
// 0,  if NodeA == NodeB
// >0, if NodeA > NodeB
int ComparePropertyListKeys(NODE * NodeA, NODE * NodeB)
{
    const int A_EQUALS_B       = 0;
    const int A_LESS_THAN_B    = -1;
    const int A_GREATER_THAN_B = 1;

    if (NodeA == NIL)
    {
        if (NodeB == NIL)
        {
            // Both keys are NIL.
            return A_EQUALS_B;
        }
        else
        {
            // NIL is smaller than everything
            return A_LESS_THAN_B;
        }
    }
    else
    {
        if (NodeB == NIL)
        {
            // NIL is smaller than everything
            return A_GREATER_THAN_B;
        }
        else
        {
            if (is_word(NodeA))
            {
                if (is_word(NodeB))
                {
                    // compare two words
                    bool ignoreCase = isCaseIgnored();
                    return compare_node(NodeA, NodeB, ignoreCase);
                }
                else
                {
                    // words are the next smaller entites
                    return A_LESS_THAN_B;
                }
            }
            else
            {
                if (is_word(NodeB))
                {
                    // words are the next smaller entites
                    return A_GREATER_THAN_B;
                }
                else
                {
                    // NodeA and NodeB are neither NIL nor words.
                    if (is_list(NodeA))
                    {
                        if (is_list(NodeB))
                        {
                            // Both nodes are lists.  Compare them member-by-member.
                            NODE * cursorA = NodeA;
                            NODE * cursorB = NodeB;
                            while (cursorA != NIL && cursorB != NIL)
                            {
                                NODE * elementA = car(cursorA);
                                NODE * elementB = car(cursorB);
                                int rval = ComparePropertyListKeys(elementA, elementB);
                                if (rval != A_EQUALS_B)
                                {
                                    // we found a way to distinguish the lists.
                                    return rval;
                                }

                                // Advance to the next element
                                cursorA = cdr(cursorA);
                                cursorB = cdr(cursorB);
                            }

                            // We made it to the end of at least one of the lists
                            // without finding any differences.
                            if (cursorA == NULL && cursorB == NULL)
                            {
                                // Both lists are the same size and equal.
                                return A_EQUALS_B;
                            }
                            else if (cursorA == NULL)
                            {
                                // List B is longer than List A.
                                // Return that List A is smaller.
                                return A_LESS_THAN_B;
                            }
                            else
                            {
                                // List A is longer than List B.
                                // Return that List A is larger.
                                return A_GREATER_THAN_B;
                            }
                        }
                        else
                        {
                            // lists are smaller than whatever types remain.
                            return A_LESS_THAN_B;
                        }
                    }
                    else
                    {
                        if (is_list(NodeB))
                        {
                            // lists are smaller than whatever types remain.
                            return A_GREATER_THAN_B;
                        }
                        else
                        {
                            assert(is_aggregate(NodeA));
                            assert(is_aggregate(NodeB));
                            // Arrays are always compared by pointer
                            if (NodeA == NodeB)
                            {
                                return A_EQUALS_B;
                            }
                            else if (NodeA < NodeB)
                            {
                                return A_LESS_THAN_B;
                            }
                            else
                            {
                                return A_GREATER_THAN_B;
                            }
                        }
                    }
                }
            }
        }
    }
}

NODE *lgprop(NODE *args)
{
    NODE * plname = string_arg(args);
    NODE * pname  = cadr(args);
    if (NOT_THROWING)
    {
        plname = intern(plname);

        NODE * plist = plist__caseobj(plname);
        return AvlTreeSearch(plist, ComparePropertyListKeys, pname);
    }
    return NIL;
}

NODE *lpprop(NODE *args)
{
    NODE * plname = string_arg(args);
    NODE * pname  = cadr(args);
    NODE * newval = car(cddr(args));
    if (NOT_THROWING)
    {
        // Create/find the propety list with this name.
        plname = intern(plname);
        if (flag__caseobj(plname, PLIST_TRACED))
        {
            // This property list is being traced.
            // Trace the pprop to the write stream.
            NODE * propertylist  = vref(maybe_quote(plname));
            NODE * propertyname  = vref(maybe_quote(pname));
            NODE * propertyvalue = vref(maybe_quote(newval));

            ndprintf(
                g_Writer.GetStream(),
                g_Writer.GetEncoding(),
                MESSAGETYPE_Trace,
                false, // format for display
                g_LOCALIZED_TRACING_PPROP,
                propertylist,
                propertyname,
                propertyvalue);

            deref(propertylist);
            deref(propertyname);
            deref(propertyvalue);

            // trace the name of the function where the pprop was run.
            if (ufun != NIL)
            {
                ndprintf(
                    g_Writer.GetStream(),
                    g_Writer.GetEncoding(),
                    MESSAGETYPE_Trace,
                    false, // format for display
                    g_LOCALIZED_TRACING_LOCATION,
                    ufun,
                    this_line);
            }
            print_newline(
                g_Writer.GetStream(),
                g_Writer.GetEncoding(),
                MESSAGETYPE_Trace);
        }

        // Insert the property into the tree.
        NODE ** plistptr = plistptr__caseobj(plname);
        AvlTreeInsert(plistptr, ComparePropertyListKeys, pname, newval);
    }
    return Unbound;
}

NODE *lremprop(NODE *args)
{
    NODE * plname = string_arg(args);
    NODE * pname = cadr(args);
    if (NOT_THROWING)
    {
        plname = intern(plname);

        // Remove the property from the list
        NODE ** plistptr = plistptr__caseobj(plname);
        AvlTreeDelete(plistptr, ComparePropertyListKeys, pname);
    }

    return Unbound;
}

NODE *lplist(NODE *args)
{
    NODE *val = NIL;

    NODE * plname = string_arg(args);
    if (NOT_THROWING)
    {
        plname = intern(plname);
        NODE * plist = plist__caseobj(plname);
        val = AvlTreeFlatten(plist);
    }
    return val;
}


NODE * llogoversion(NODE * args)
{
    NODE * application_name    = make_static_strnode(g_LOCALIZED_GENERAL_PRODUCTNAME);
    NODE * application_version = make_static_strnode(FMSLOGO_VERSION);
    NODE * os_name             = make_static_strnode(L"Windows");
    NODE * locale              = make_intnode(g_LOCALE);

    return cons_list(
        application_name,
        application_version,
        os_name,
        locale);
}

bool variableIsTrue(NODE *variable)
{
    return Truex.Equals(valnode__caseobj(variable));
}

// Returns the value of :CASEIGNOREDP, according to the following rules
// If CASEIGNOREDP is defined, then its value is used.
// Otherwise, if this is a non-English build and the non-English version
// is defined, then its value is used.
// Otherwise, it returns true.
bool isCaseIgnored()
{
    // Get the value of the English version or the localized version,
    // depending on which is defined.
    NODE * value = Caseignoredp.GetValue();
    if (value != Unbound)
    {
        // CASEIGNOREDP is defined.  Return if it's true.
        return Truex.Equals(value);
    }

    // Neither the English nor the alternate is defined,
    // so we use the default value of true.
    return true;
}

// Returns the value of :REDEFP, according to the following rules:
// If REDEFP is defined to be "true, then its value is used.
// Otherwise, if this is a non-English build and the non-English version
// is defined, then its value is used.
// Otherwise, it returns false.
bool canRedefinePrimitives()
{
    // Get the value of the English version or the localized version,
    // depending on which is defined.
    NODE * value = Redefp.GetValue();
    if (value != Unbound)
    {
        // REDEFP is defined.  Return if it's true.
        return Truex.Equals(value);
    }

    // Neither the English nor the alternate is defined,
    // so we use the default value of false.
    return false;
}
