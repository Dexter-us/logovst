/*
 *      lists.cpp       logo list functions module              dvb
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
   #include <math.h>
   #include <stdio.h>
   #include <wchar.h> // swprintf

   #include "logodata.h"
   #include "init.h"
   #include "eval.h"
   #include "error.h"
   #include "ibmterm.h"
   #include "logomath.h"
   #include "lists.h"
   #include "appendablelist.h"
   #include "graphics.h" // numeric_node_to_flonum
   #include "debugheap.h"
#endif

static
bool is_valid_butfirst_arg(NODE * argument)
{
    return
        argument != NIL       &&     // not the empty list
        argument != Unbound   &&     // not an error
        argument != Null_Word &&     // not the empty string
        nodetype(argument) != ARRAY; // not an array
}

static
bool is_valid_first_arg(NODE *argument)
{
    return
        argument != NIL       &&  // not the empty list
        argument != Unbound   &&  // not an error
        argument != Null_Word;    // not the empty string
}

// Returns the argument if it satisfies is_valid_argument.
// Returns Unbound and sets stopping_flag to THROWING, otherwise.
// If the argument doesn't satisfy is_valid_argument,
// this function attempts to get a replacement from :erract.
NODE *
validate_argument(
    NODE              * argument,
    IS_VALID_ARGUMENT   is_valid_argument
    )
{
    // The argument should be owned by an argument list,
    // otherwise the gcref() below will unexpectedly free it.
    assert(argument == NIL || 0 < argument->ref_count);

    // If the argument isn't valid, try to get
    // one from :erract until the user gives up.
    while (stopping_flag != THROWING && !is_valid_argument(argument))
    {
        // Try to get a replacement argument from :erract.
        // Note that err_logo will gcref(argument), which frees if
        // we get multiple arguments from err_logo.
        argument = err_logo(BAD_DATA, argument);
    }

    return argument;
}

static
bool is_valid_list_arg(NODE *argument)
{
    return is_list(argument);
}

// Returns the argument if it's a list, including if it's NIL.
// Returns Unbound and sets stopping_flag to THROWING, otherwise.
// If the argument isn't a list, this function attempts to get a
// replacement from :erract.
NODE * validate_list_argument(NODE *argument)
{
    return validate_argument(argument, is_valid_list_arg);
}

static
NODE * butfirst(NODE *arg)
{
    assert(is_valid_butfirst_arg(arg)); // argument should already validated

    NODE * val;
    if (is_list(arg))
    {
        val = cdr(arg);
    }
    else
    {
        // arg is a word, which includes numbers.
        // Return a string of everything but the first character/digit.
        NODE * full_string = cnv_node_to_strnode(arg);

        val = make_strnode(
            getstrptr(full_string) + 1,
            getstrlen(full_string) - 1,
            nodetype(full_string),
            strnzcpy);

        gcref(full_string);
    }

    return val;
}

NODE *lbutfirst(NODE *args)
{
    NODE * arg = validate_argument(car(args), is_valid_butfirst_arg);
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    NODE * val = butfirst(arg);

    // Because val may the cdr(arg) and because arg may have been newly allocated
    // by :erract, we cannot free arg without possibly freeing val.  Therefore,
    // we re-insert it into the argument list.  The caller will take ownership
    // of a reference of val before freeing args.
    setcar(args, arg);

    return val;
}

NODE *lbutlast(NODE *args)
{
    NODE *val = Unbound, *lastnode, *tnode;

    NODE * arg = validate_argument(car(args), is_valid_butfirst_arg);
    if (NOT_THROWING)
    {
        if (is_list(arg))
        {
            // Copy all elements (except the last one) from arg into a new list.
            CAppendableList butlast;
            for (NODE * node = arg; cdr(node) != NIL; node = cdr(node))
            {
                butlast.AppendElement(car(node));
                if (check_throwing)
                {
                    break;
                }
            }
            val = butlast.GetList();
        }
        else
        {
            // arg is a word, which includes numbers.
            // Return a string of everything but the last character/digit.
            NODE * full_string = cnv_node_to_strnode(arg);
            val = make_strnode(
                getstrptr(full_string),
                getstrlen(full_string) - 1,
                nodetype(full_string),
                strnzcpy);
            gcref(full_string);
        }
        gcref(arg);
    }
    return val;
}

static
NODE * first(NODE * arg)
{
    assert(is_valid_first_arg(arg)); // argument should already validated

    NODE * val;

    if (nodetype(arg) == ARRAY)
    {
        // arg is an array -- output the origin
        val = make_intnode((FIXNUM) getarrorg(arg));
    }
    else if (is_list(arg))
    {
        // arg is a list -- output the first item
        val = car(arg);
    }
    else
    {
        // arg is a word, which includes numbers.
        // Return a string of the first character (or digit).
        NODE * full_string = cnv_node_to_strnode(arg);
        val = make_strnode(
            getstrptr(full_string),
            1,
            nodetype(full_string),
            strnzcpy);
        gcref(full_string);
    }

    return val;
}

NODE *lfirst(NODE *args)
{
    NODE * arg = validate_argument(car(args), is_valid_first_arg);
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    NODE * val = first(arg);

    // Because val may the car(arg) and because arg may have been newly allocated
    // by :erract, we cannot free arg without possibly freeing val.  Therefore,
    // we re-insert it into the argument list.  The caller will take ownership
    // of a reference of val before freeing args.
    setcar(args, arg);

    return val;
}

NODE *lfirsts(NODE *args)
{
    NODE * val = Unbound;

    NODE * arg = car(args);
    do
    {
        if (is_list(arg))
        {
            CAppendableList firsts;

            bool is_ok = true;
            for (NODE * argp = arg; argp != NIL; argp = cdr(argp))
            {
                // Check if this element is valid for FIRST.
                NODE * first_arg = car(argp);
                if (!is_valid_first_arg(first_arg))
                {
                    is_ok = false;
                    break;
                }
                if (check_throwing)
                {
                    break;
                }

                // This element is valid.
                // Append what FIRST would output for it to the output list.
                firsts.AppendElement(first(first_arg));
            }

            val = firsts.GetList();
            if (!is_ok)
            {
                gcref(val);
                val = Unbound;
            }
        }

        // If there is a problem with the input, try to get
        // a replacement from :erract.
        if (val == Unbound)
        {
            arg = err_logo(BAD_DATA, arg);
        }
    } while (stopping_flag != THROWING && val == Unbound);

    gcref(arg);
    return val;
}

NODE *lbutfirsts(NODE *args)
{
    NODE * val = Unbound;

    NODE * arg = car(args);
    do
    {
        if (is_list(arg))
        {
            CAppendableList butfirsts;

            bool is_ok = true;
            for (NODE * argp = arg; argp != NIL; argp = cdr(argp))
            {
                // Check if this element is valid for BUTFIRST.
                NODE * butfirst_arg = car(argp);
                if (!is_valid_butfirst_arg(butfirst_arg))
                {
                    is_ok = false;
                    break;
                }
                if (check_throwing)
                {
                    break;
                }

                // This element is valid.
                // Append what BUTFIRST would output for to the output list.
                butfirsts.AppendElement(butfirst(butfirst_arg));
            }

            val = butfirsts.GetList();
            if (!is_ok)
            {
                gcref(val);
                val = Unbound;
            }
        }

        // If there is a problem with the input, try to get
        // a replacement from :erract.
        if (val == Unbound)
        {
            arg = err_logo(BAD_DATA, arg);
        }
    } while (stopping_flag != THROWING && val == Unbound);

    gcref(arg);
    return val;
}

NODE *llast(NODE *args)
{
    NODE *val = Unbound;

    NODE * arg = validate_argument(car(args), is_valid_butfirst_arg);
    if (NOT_THROWING)
    {
        if (is_list(arg))
        {
            // Find the last node in the list.
            NODE * last_node = arg;
            while (cdr(last_node) != NIL)
            {
                last_node = cdr(last_node);
                if (check_throwing)
                {
                    break;
                }
            }

            // Return the element of the last node.
            val = car(last_node);
        }
        else
        {
            // arg is a word, which includes numbers.
            // Return a string of the last character/digit.
            NODE * full_string = cnv_node_to_strnode(arg);

            val = make_strnode(
                getstrptr(full_string) + getstrlen(full_string) - 1,
                1,
                nodetype(full_string),
                strnzcpy);

            gcref(full_string);
        }

        // If arg is a list and was newly allocated by :erract, we can't free
        // arg without also freeing val.  Therefore, we re-insert avl into the
        // argument list.  The caller will take ownership of a reference of val
        // before freeing args.
        setcar(args, arg);
    }
    return val;
}

NODE *llist(NODE *args)
{
    return args;
}

NODE *lemptyp(NODE *arg)
{
    return true_or_false(car(arg) == NIL || car(arg) == Null_Word);
}

// Validate that the argument is convertable to a word with one character.
// If so, returns that character.
// If not, sets stopping_flag = THROWING and returns -1.
static
wchar_t validate_char_argument(NODE * arg)
{
    // Read the input, which must be a word with one character.
    NODE * val = cnv_node_to_strnode(arg);
    while ((val == Unbound || getstrlen(val) != 1) && NOT_THROWING)
    {
        // Try to get a new value from :erract
        arg = err_logo(BAD_DATA, arg);

        gcref(val);
        val = cnv_node_to_strnode(arg);
    }

    if (val == Unbound)
    {
        // The input is not a valid
        return -1;
    }

    // Return the character.
    wchar_t ch = *getstrptr(val);
    gcref(val);
    return ch;
}

NODE *lascii(NODE *args)
{
    wchar_t ch = validate_char_argument(car(args));
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    FIXNUM i = (FIXNUM) (vbar_clear(ch) & 0xFFFF);
    return make_intnode(i);
}

NODE *lrawascii(NODE *args)
{
    wchar_t ch = validate_char_argument(car(args));
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    // cast to unsigned so that this won't return a negative number.
    unsigned int i = static_cast<unsigned int>(ch);
    return make_intnode(i);
}

NODE *lbackslashedp(NODE *args)
{
    wchar_t ch = validate_char_argument(car(args));
    if (stopping_flag == THROWING)
    {
        return Unbound;
    }

    return true_or_false(is_vbarred(ch));
}

NODE *lchar(NODE *args)
{
    NODE *val = Unbound;

    NODE * arg = nonnegative_int_arg(args);
    if (NOT_THROWING)
    {
        wchar_t c = getint(arg) & 0xFFFF;
        val = make_strnode(
            &c,
            1,
            is_vbarred(c) ? STRING : BACKSLASH_STRING,
            strnzcpy);
    }
    return val;
}

NODE *lcount(NODE *args)
{
    int cnt;

    NODE* arg = car(args);
    if (is_list(arg))
    {
        cnt = 0;
        for (; arg != NIL; arg = cdr(arg))
        {
            cnt++;
        }
    }
    else if (nodetype(arg) == ARRAY)
    {
        cnt = getarrdim(arg);
    }
    else
    {
        setcar(args, cnv_node_to_strnode(arg));
        cnt = getstrlen(car(args));
    }

    return make_intnode((FIXNUM) cnt);
}

NODE *lfput(NODE *args)
{
    NODE * arg = car(args);
    NODE * lst = validate_list_argument(cadr(args));
    if (NOT_THROWING)
    {
        return cons(arg, lst);
    }
    else
    {
        return Unbound;
    }
}

NODE *llput(NODE *args)
{
    NODE * newtail = car(args);
    NODE * oldlist = validate_list_argument(cadr(args));
    if (stopping_flag == THROWING)
    {
        // something is wrong with the inputs
        return Unbound;
    }

    // copy the list
    CAppendableList newlist;

    for (NODE * node = oldlist; node != NIL; node = cdr(node))
    {
        newlist.AppendElement(car(node));
        if (check_throwing)
        {
            break;
        }
    }

    // Append the new tail element.
    newlist.AppendElement(newtail);

    gcref(oldlist);
    return newlist.GetList();
}

NODE *string_arg(NODE *args)
{
    NODE *arg = car(args);

    NODE * val = cnv_node_to_strnode(arg);
    while (val == Unbound && NOT_THROWING)
    {
        gcref(val);
        setcar(args, err_logo(BAD_DATA, arg));
        arg = car(args);
        val = cnv_node_to_strnode(arg);
    }
    setcar(args, val);
    return val;
}

NODE *lword(NODE *args)
{
    int length = 0;
    NODETYPE str_type = STRING;
    for (NODE * arg = args; arg != NIL; arg = cdr(arg))
    {
        NODE * val = string_arg(arg);
        if (stopping_flag == THROWING)
        {
            return Unbound;
        }

        if (backslashed(val))
        {
            // REVISIT: should all strings that are merely
            // backslashed result in a VBAR_STRING?
            str_type = VBAR_STRING;
        }
        length += getstrlen(val);
    }

    return make_strnode_from_wordlist(args, length, str_type);
}

NODE *lsentence(NODE *args)
{
    CAppendableList sentence;

    while (args != NIL && NOT_THROWING)
    {
        NODE * arg = car(args);

        while (nodetype(arg) == ARRAY && NOT_THROWING)
        {
            setcar(args, err_logo(BAD_DATA, arg));
            arg = car(args);
        }

        args = cdr(args);
        if (stopping_flag == THROWING)
        {
            break;
        }

        if (is_list(arg))
        {
            // arg is a list.
            // Append each element in the list, instead of appending
            // the list so that we don't change the input.
            while (arg != NIL && NOT_THROWING)
            {
                sentence.AppendElement(car(arg));

                arg = cdr(arg);
            }
        }
        else
        {
            // arg must be a word
            sentence.AppendElement(arg);
        }
    }

    if (stopping_flag == THROWING)
    {
        gcref(sentence.GetList());
        return Unbound;
    }

    return sentence.GetList();
}

NODE *lwordp(NODE *arg)
{
    arg = car(arg);
    return true_or_false(arg != Unbound && !is_aggregate(arg));
}

NODE *llistp(NODE *arg)
{
    arg = car(arg);
    return true_or_false(is_list(arg));
}

NODE *lnumberp(NODE *arg)
{
    return true_or_false(0 != numberp(car(arg)));
}

NODE *larrayp(NODE *arg)
{
    return true_or_false(nodetype(car(arg)) == ARRAY);
}

static
NODE *memberp_helper(NODE *args, bool notp, bool substr)
{
    bool caseig = isCaseIgnored();

    NODE * obj1 = car(args);
    NODE * obj2 = cadr(args);
    if (is_list(obj2))
    {
        if (substr)
        {
            return Falsex.GetNode();
        }

        while (obj2 != NIL && NOT_THROWING)
        {
            if (equalp_help(obj1, car(obj2), caseig))
            {
                return notp ? obj2 : Truex.GetNode();
            }
            obj2 = cdr(obj2);
            if (check_throwing)
            {
                break;
            }
        }
        return notp ? NIL : Falsex.GetNode();
    }
    else if (nodetype(obj2) == ARRAY)
    {
        int len = getarrdim(obj2);
        NODE **data = getarrptr(obj2);

        if (notp)
        {
            err_logo(BAD_DATA_UNREC, obj2);
        }

        if (substr)
        {
            return Falsex.GetNode();
        }

        while (--len >= 0 && NOT_THROWING)
        {
            if (equalp_help(obj1, *data++, caseig))
            {
                return Truex.GetNode();
            }
        }
        return Falsex.GetNode();
    }
    else
    {
        if (is_aggregate(obj1))
        {
            return (notp ? Null_Word : Falsex.GetNode());
        }

        setcar(cdr(args), cnv_node_to_strnode(obj2));
        obj2 = cadr(args);

        setcar(args, cnv_node_to_strnode(obj1));
        obj1 = car(args);

        if (obj1 != Unbound &&
            obj2 != Unbound &&
            getstrlen(obj1) <= getstrlen(obj2) &&
            (substr || (getstrlen(obj1) == 1)))
        {
            // how many different places are there to compare obj1 in obj2
            int leng = getstrlen(obj2) - getstrlen(obj1);

            // Create a copy of obj2 with obj1's length so that we can
            // quickly extract all potential matches of obj1 within obj2.
            // Note that tmp contains the full string of obj2, but its
            // string pointer and length are manipulated so that it's treated
            // as if it's only as long as obj1.
            // REVISIT: why modify args?  why not just keep tmp separate?
            setcar(
                cdr(args),
                make_strnode(getstrptr(obj2), getstrlen(obj2), nodetype(obj2), strnzcpy));
            NODE * tmp = cadr(args);
            setstrlen(tmp, getstrlen(obj1));

            for (int i = 0; i <= leng; i++)
            {
                if (equalp_help(obj1, tmp, caseig))
                {
                    if (notp)
                    {
                        setstrlen(tmp, leng + getstrlen(obj1) - i);
                        return tmp;
                    }
                    else
                    {
                        return Truex.GetNode();
                    }
                }
                // Slide the potential string match down one byte.
                // Note that the string length remains the same.
                setstrptr(tmp, getstrptr(tmp) + 1);
            }
        }
        return notp ? Null_Word : Falsex.GetNode();
    }
}

NODE *lmemberp(NODE *args)
{
    return memberp_helper(args, false, false);
}

NODE *lsubstringp(NODE *args)
{
    return memberp_helper(args, false, true);
}

NODE *lmember(NODE *args)
{
    return memberp_helper(args, true, false);
}

// Validate that the argument is convertable to an integer.
// If so, returns an INTEGER node.
// If not, sets stopping_flag = THROWING and returns Unbound.
//
// The caller must hold a reference on the argument and must
// free the return value with gcref().
NODE * validate_integer_argument(NODE * argument)
{
    // The argument should be owned by an argument list,
    // otherwise the gcref() below will unexpectedly free it.
    assert(argument == NIL || 0 < argument->ref_count);

    NODE * val = cnv_node_to_intnode(argument);
    while (val == Unbound && NOT_THROWING)
    {
        argument = err_logo(BAD_DATA, argument);
        val = cnv_node_to_intnode(argument);
    }

    gcref(argument);
    return val;
}

// If argument can be interpreted as an integer within the specified range,
// then it is converted into an int and returned.
// Otherwise the programmer is given a chance to correct it with :erract.
// stopping_flag is set to THROWING on error.
static
FIXNUM ranged_fixnum_arg(NODE *argument, int MinValue, int MaxValue)
{
    // The argument should be owned by an argument list,
    // otherwise the gcref() below will unexpectedly free it.
    assert(argument == NIL || 0 < argument->ref_count);

    while (NOT_THROWING)
    {
        NODE * node = cnv_node_to_intnode(argument);
        if (nodetype(node) == INTEGER)
        {
            int value = getint(node);
            gcref(node);
            if (MinValue <= value && value <= MaxValue)
            {               
                // This is an integer within the requested range.
                return value;
            }
        }

        // grab a new value from ERRACT and try again
        argument = err_logo(BAD_DATA, argument);
    }

    gcref(argument);
    return 0;
}

// If argument can be interpreted as a non-negative integer, then it is
// converted into an int and returned.
// Otherwise the programmer is given a chance to correct it with :erract.
// stopping_flag is set to THROWING on error.
static
FIXNUM nonnegative_fixnum_arg(NODE *argument)
{
    return ranged_fixnum_arg(argument, 0, FIXNUM_MAX);
}


// If the first element in args can be interpreted as an integer
// within the specified range, then it is changed into an integer
// node and returned.
// Otherwise the programmer is given a chance to correct it with ERRACT.
NODE *ranged_integer_arg(NODE *args, int MinValue, int MaxValue)
{
    while (NOT_THROWING)
    {
        NODE * arg = car(args);
        NODE * val = cnv_node_to_intnode(arg);

        if (nodetype(val) == INTEGER &&
            MinValue <= getint(val) && getint(val) <= MaxValue)
        {
            // This is an integer within the requested range.
            // Update the arguments with the new node and return it.
            setcar(args, val);
            return val;
        }

        // grab a new value from ERRACT and try again
        gcref(val);
        setcar(args, err_logo(BAD_DATA, arg));
    }

    return Unbound;
}

// Returns an integer node if args can be interpreted as a non-negative integer.
// otherwise sets an error in args and returns Unbound.
NODE *nonnegative_int_arg(NODE *args)
{
    return ranged_integer_arg(args, 0, FIXNUM_MAX);
}

// Returns an integer node if args can be interpreted as a positive integer.
// otherwise sets an error in args and returns Unbound.
NODE *positive_int_arg(NODE *args)
{
    return ranged_integer_arg(args, 1, FIXNUM_MAX);
}

FIXNUM int_arg(NODE *args)
{
    // This could be made faster by calling a function that
    // doesn't create an INTEGER node when the argument can
    // be converted into one.
    NODE *arg = validate_integer_argument(car(args));
    if (NOT_THROWING)
    {
        int integer = getint(arg);
        gcref(arg);
        return integer;
    }
    return 0;
}

// Returns the argument as a FLONUM if it can be be interpreted as one.
// Sets stopping_flag to THROWING, otherwise.
// If the argument can't be interpreted as a float, this function attempts
// to get a replacement from :erract.
FLONUM flonum_arg(NODE *argument)
{
    // The argument should be owned by an argument list,
    // otherwise the gcref() below will unexpectedly free it.
    assert(argument == NIL || 0 < argument->ref_count);

    while (stopping_flag != THROWING)
    {
        NODE * numnode = cnv_node_to_numnode(argument);
        if (is_number(numnode))
        {
            // We got a number, just like we wanted.
            FLONUM value = numeric_node_to_flonum(numnode);
            gcref(numnode);
            gcref(argument);
            return value;
        }

        // try to get a new value from ERRACT
        gcref(numnode);
        argument = err_logo(BAD_DATA, argument);
    }

    gcref(argument);
    return 0.0;
}

NODE *litem(NODE *args)
{
    NODE * obj = cadr(args);
    while ((obj == NIL || obj == Null_Word) && NOT_THROWING)
    {
        setcar(cdr(args), err_logo(BAD_DATA, obj));
        obj = cadr(args);
    }

    while (NOT_THROWING)
    {
        int i = int_arg(args);
        if (is_list(obj))
        {
            if (i <= 0)
            {
                // all list indices must be positive
                setcar(args, err_logo(BAD_DATA, car(args)));
                continue;
            }

            // advance item to the ith element in obj
            NODE * item = obj;
            while (--i > 0)
            {
                item = cdr(item);
                if (item == NIL)
                {
                    // reached the end of the list
                    break;
                }
            }

            if (item == NIL)
            {
                // the index goes beyond the length of the list
                setcar(args, err_logo(BAD_DATA, car(args)));
                continue;
            }

            return car(item);
        }
        else if (nodetype(obj) == ARRAY)
        {
            i -= getarrorg(obj);
            if (i < 0 || i >= getarrdim(obj))
            {
                // i is out of range
                setcar(args, err_logo(BAD_DATA, car(args)));
                continue;
            }
            return getarrptr(obj)[i];
        }
        else
        {
            // we will treat obj as a word
            if (i <= 0)
            {
                // letters of a word are 1-indexed
                setcar(args, err_logo(BAD_DATA, car(args)));
                continue;
            }
            setcar(cdr(args), cnv_node_to_strnode(obj));
            obj = cadr(args);
            if (i > getstrlen(obj))
            {
                // letters of a word are 1-indexed
                setcar(args, err_logo(BAD_DATA, car(args)));
                continue;
            }
            return make_strnode(
                getstrptr(obj) + i - 1,
                1,
                nodetype(obj),
                strnzcpy);
        }
    }
    return Unbound;
}

static
bool circular(NODE *arr, NODE *newx)
{
    if (newx == NIL)
    {
        return false;
    }
    else if (nodetype(newx) == ARRAY)
    {
        int i = getarrdim(newx);
        NODE **p = getarrptr(newx);

        if (newx == arr)
        {
            return true;
        }
        while (--i >= 0)
        {
            if (circular(arr, *p++))
            {
                return true;
            }
        }
        return false;
    }
    else if (is_list(newx))
    {
        while (newx != NIL)
        {
            if (circular(arr, car(newx)))
            {
                return true;
            }
            newx = cdr(newx);
        }
        return false;
    }
    else
    {
        return false;
    }
}

static
NODE *setitem_helper(NODE *args, bool safe)
{
    // Get the index argument.
    FIXNUM i = int_arg(args);

    // Get the array argument.
    NODE * obj = cadr(args);
    while (nodetype(obj) != ARRAY && NOT_THROWING)
    {
        obj = err_logo(BAD_DATA, obj);
    }

    // Get the new item argument.
    NODE * cont = car(cddr(args));
    if (NOT_THROWING)
    {
        if (safe)
        {
            while (circular(obj, cont) && NOT_THROWING)
            {
                cont = err_logo(BAD_DATA, cont);
            }
        }
        if (NOT_THROWING)
        {
            i -= getarrorg(obj);
            if (i < 0 || i >= getarrdim(obj))
            {
                err_logo(BAD_DATA_UNREC, car(args));
            }
            else
            {
                // Replace the item in the array.
                assign(getarrptr(obj)[i], cont);
            }
        }
    }

    gcref(cont);
    gcref(obj);
    return Unbound;
}

NODE *lsetitem(NODE *args)
{
    return setitem_helper(args, true);
}

NODE *l_setitem(NODE *args)
{
    return setitem_helper(args, false);
}

NODE *larray(NODE *args)
{
    FIXNUM dimension = nonnegative_fixnum_arg(car(args));
    FIXNUM origin;
    if (cdr(args) != NIL)
    {
        origin = int_arg(cdr(args));
    }
    else
    {
        origin = 1;
    }

    if (NOT_THROWING)
    {
        NODE * array = make_array(dimension);

        if (NOT_THROWING)
        {
            setarrorg(array, origin);
            return array;
        }
    }
    return Unbound;
}

NODE *lform(NODE *args)
{
    FLONUM number = flonum_arg(car(args));

    NODE * width_arg = cdr(args);
    int    width = (int) int_arg(width_arg);
    if (width < 0)
    {
        NODE * error_node = err_logo(BAD_DATA, car(width_arg));
        setcar(args, error_node);
        return Unbound;
    }

    int precision = (int) int_arg(cddr(args));

    if (NOT_THROWING)
    {
        wchar_t result[100];

        // Take care not to overflow the buffer
        if (width >= sizeof(result))
        {
            width = sizeof(result) - 1;
        }
        if (precision > sizeof(result))
        {
            precision = sizeof(result) - 1;
        }

        swprintf(result, ARRAYSIZE(result), L"%*.*f", width, precision, number);

        return make_strnode(result);
    }

    return Unbound;
}

NODE *l_setfirst(NODE *args)
{
    NODE * list = car(args);
    NODE * newval = cadr(args);
    while (NOT_THROWING && (list == NIL || !is_list(list)))
    {
        setcar(args, err_logo(BAD_DATA, list));
        list = car(args);
    }

    if (NOT_THROWING)
    {
        setcar(list, newval);
    }

    return Unbound;
}

NODE *l_setbf(NODE *args)
{
    NODE * list = car(args);
    NODE * newval = cadr(args);
    while (NOT_THROWING && (list == NIL || !is_list(list)))
    {
        setcar(args, err_logo(BAD_DATA, list));
        list = car(args);
    }

    if (NOT_THROWING)
    {
        setcdr(list, newval);
    }

    return Unbound;
}
