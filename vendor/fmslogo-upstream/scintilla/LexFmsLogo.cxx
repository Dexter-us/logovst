// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "ILexer.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

static inline bool IsAnOperatorChar(const int ch) 
{
    switch (ch)
    {
    case '+':
    case '-':
    case '/':
    case '*':
    case '>':
    case '<':
    case '=':
        return true;

    default:
        return false;
    }
}

// true iff the character should extend an identifier.
static inline bool IsAWordChar(const int ch) 
{
    switch (ch)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\r':

    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':

    case '+':
    case '-':
    case '/':
    case '*':
    case '>':
    case '<':
    case '=':

    case ';':
        return false;

    default:
        return true;
    }
}

static
void
AdvanceWithinVbar(
    StyleContext & Context
    )
{
    while (Context.ch != '|' && Context.More())
    {
        // A backslash wihtin vbars can escape a backslash or a vbar.
        if (Context.ch == '\\')
        {
            Context.Forward();
        }

        Context.Forward();
    }
}

static
void
ColorizeFmsLogoDoc(
    unsigned int   startPos,
    int            length,
    int            initStyle,
    WordList    *  keywordlists[],
    Accessor    &  styler
    ) 
{
    StyleContext sc(startPos, length, initStyle, styler);
    for (; sc.More(); sc.Forward()) 
    {
        // Handle line continuation generically.
        if (sc.ch == '~') 
        {
            if (sc.chNext == '\n' || sc.chNext == '\r')
            {
                // This was a line continuation, which breaks a comment
                switch (sc.state)
                {
                case SCE_FMS_COMMENT:
                    sc.SetState(SCE_FMS_DEFAULT);
                    break;

                case SCE_FMS_COMMENT_IN_STRING:
                    // Next line continues as a word literal.
                    sc.SetState(SCE_FMS_STRING);
                    break;
                    
                case SCE_FMS_COMMENT_IN_VARIABLE:
                    // Next line continues as a variable name.
                    sc.SetState(SCE_FMS_VARIABLE);
                    break;
                }

                sc.Forward();
                if (sc.ch == '\r' && sc.chNext == '\n') 
                {
                    sc.Forward();
                }
                continue;
            }
        }

        // Determine if the current state should terminate.
        switch (sc.state)
        {
        case SCE_FMS_OPERATOR:
            sc.SetState(SCE_FMS_DEFAULT);
            break;

        case SCE_FMS_NUMBER:
            if (!IsAWordChar(sc.ch)) 
            {
                sc.SetState(SCE_FMS_DEFAULT);
            }
            break;

        case SCE_FMS_STRING:
            if (sc.ch == '|')
            {
                sc.SetState(SCE_FMS_STRING_VBAR);
            }
            else if (sc.ch == '\\') 
            {
                sc.Forward();
            }
            else if (sc.atLineEnd  ||
                     sc.ch == ' '  ||
                     sc.ch == '\t' ||
                     sc.ch == '('  ||
                     sc.ch == ')'  ||
                     sc.ch == '['  ||
                     sc.ch == ']'  ||
                     sc.ch == '{'  ||
                     sc.ch == '}') 
            {
                sc.SetState(SCE_FMS_DEFAULT);
            }
            else if (sc.ch == ';')
            {
                sc.SetState(SCE_FMS_COMMENT_IN_STRING);
            }
            break;

        case SCE_FMS_STRING_VBAR:
            AdvanceWithinVbar(sc);
            sc.SetState(SCE_FMS_STRING);
            break;

        case SCE_FMS_VBAR:
            AdvanceWithinVbar(sc);
            sc.SetState(SCE_FMS_DEFAULT);
            sc.Forward();
            break;

        case SCE_FMS_COMMENT:
        case SCE_FMS_COMMENT_IN_STRING:
        case SCE_FMS_COMMENT_IN_VARIABLE:
            // comments end at a newline, but can be continued
            // onto the next line if the newline is preceeded by
            // a backslash.
            if (sc.atLineEnd)
            {
                sc.SetState(SCE_FMS_DEFAULT);
            }
            else if (sc.ch == '\\')
            {
                // This is a backslash followed by a newline,
                // so the comment continues onto the next line.
                if (sc.chNext == '\n' || sc.chNext == '\r')
                {
                    sc.Forward();
                    if (sc.ch == '\r' && sc.chNext == '\n') 
                    {
                        sc.Forward();
                    }
                    sc.SetState(SCE_FMS_DEFAULT);
                }
            }
            break;

        case SCE_FMS_VARIABLE:
            if (sc.ch == '|')
            {
                sc.SetState(SCE_FMS_VARIABLE_VBAR);
            }
            if (sc.ch == ';')
            {
                sc.SetState(SCE_FMS_COMMENT_IN_VARIABLE);
            }
            else if (sc.ch == '\\')
            {
                sc.Forward();
            }
            else if (!IsAWordChar(sc.ch))
            {
                sc.SetState(SCE_FMS_DEFAULT);
            }
            break;

        case SCE_FMS_VARIABLE_VBAR:
            AdvanceWithinVbar(sc);
            sc.SetState(SCE_FMS_VARIABLE);
            break;

        case SCE_FMS_DEFAULT_BACKSLASH:
            sc.Forward();
            sc.SetState(SCE_FMS_DEFAULT);
            break;

        case SCE_FMS_IDENTIFIER:
            if (!IsAWordChar(sc.ch)) 
            {
                sc.SetState(SCE_FMS_DEFAULT);
            }
            break;

        default:
            break;
        }

        // Determine if a new state should be entered.
        if (sc.state == SCE_FMS_DEFAULT) 
        {
            if (IsADigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext))) 
            {
                sc.SetState(SCE_FMS_NUMBER);
            }
            else if (sc.ch == ':')
            {
                sc.SetState(SCE_FMS_VARIABLE);
            }
            else if (sc.ch == '|')
            {
                sc.SetState(SCE_FMS_VBAR);
            }
            else if (sc.ch == ';') 
            {
                sc.SetState(SCE_FMS_COMMENT);
            }
            else if (sc.ch == '\"') 
            {
                sc.SetState(SCE_FMS_STRING);
            }
            else if (IsAnOperatorChar(sc.ch))
            {
                sc.SetState(SCE_FMS_OPERATOR);
            }
            else if (sc.ch == '\\')
            {
                sc.SetState(SCE_FMS_DEFAULT_BACKSLASH);
            }
            else if (IsAWordChar(sc.ch))
            {
                sc.SetState(SCE_FMS_IDENTIFIER);
            }
        }
    }
    sc.Complete();
}


static void FoldFmsLogoDoc(unsigned int, int, int, WordList *[], Accessor &)
{
}

static const char * const g_FmsLogoWordLists[] = 
{
    "Structural keywords",
    "Primitives",
    "Library procedures",
    "Abbreviations",
    "Special Variables",
    0,
};

LexerModule lmFmsLogo(SCLEX_FMSLOGO, ColorizeFmsLogoDoc, "FMSLogo", FoldFmsLogoDoc, g_FmsLogoWordLists);
