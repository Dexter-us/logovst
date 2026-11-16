/*
 *      logocore.h          logo header file                dvb
 *
 *	Copyright (C) 1993 by the Regents of the University of California
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

#ifndef __LOGOCORE_H_
#define __LOGOCORE_H_

#include <stddef.h>
#include <assert.h> // assert
#include <cstdint>  // intptr_t
#include <cfloat>   // DBL_MAX

#ifndef ARRAYSIZE
    #define ARRAYSIZE(ARRAY)        (sizeof(ARRAY) / sizeof(*(ARRAY)))
#endif

#ifdef DEBUG

extern struct NODE * tracked_node;

void set_tracked_node(struct NODE * node);
void trace_node_change(struct NODE * Node);

#define TRACE_NODE_CHANGE(NODE) trace_node_change(NODE)
#define SET_TRACKED_NODE(NODE)  set_tracked_node(NODE)

#else

#define TRACE_NODE_CHANGE(NODE)
#define SET_TRACKED_NODE(NODE)

#endif // DEBUG

const wchar_t BYTE_ORDER_MARK = 0xFEFF;
const unsigned char UTF8_BOM[3] = {0xEF, 0xBB, 0xBF}; // BYTE ORDER MARK encoded in UTF8.

#define check_throwing (check_stop(false) || stopping_flag == THROWING)

enum mode_type
{
    wrapmode,
    fencemode,
    windowmode,
    perspectivemode,
};

// A logofunc is the function signature for a primitive.
//
// args -
//    A list of arguments to the function.
//    The caller guarantees that the length of this list is between the
//    function's declared mininum and maximum number of inputs.
//    The list is freed by the caller after the return value has been
//    referenced.
//    The function may modify the contents of args.
//
// If there's an error when processing the function, the stopping_flag
// must be set to THROWING.
//
// When the function is called, stopping_flag == RUN.
// The function may control what happens after the fucntion returns by setting
// the stopping_flag:
//   RUN -
//     (no change) evaluate the next expression.
//   STOP -
//     The evaluator ignores the return value (aside from release the reference)
//     and returns from the user-defined proceedure with no return value.
//   OUTPUT -
//     The evaluator references the return value, exits the currently running
//     user-defined proceedure, and provides it as input the the user-defined
//     caller of the user-defined procedure.
//   THROWING -
//     The evaluator derefences, and otherwise ignores, the return value and
//     continues throwing until a matching CATCH block is reached.
//
// The function's return value is referenced and used as input to another
// procedure.  If the function is a command (doesn't return anything), then
// it should return Unbound.
//
typedef struct NODE * (*logofunc) (struct NODE * args);

const int WORDSIZE        = 32;             // bits per int

#define NIL             ((NODE *) 0)
#define UNDEFINED       Unbound

const int HASH_LEN        = 1021;           // a prime number
const int MAX_NUMBER      = 50;             // max number of digits in a float
const int MAX_BUFFER_SIZE = 256;

// SEG_SIZE - how many cons cells are allocated in contigious memory when
// memory is allocated for a cons cell.  Using a large number reduces the
// number of times malloc() and free() are called.
// This is ignored when MEM_DEBUG is defined.
const int SEG_SIZE = 1000;

const int STOP_PRIORITY   = 0;
const int OUTPUT_PRIORITY = 1;
const int MAYBE_PRIORITY  = 2;
const int TAIL_PRIORITY   = 2;	/* largest tailcall priority */
const int MACRO_PRIORITY  = 3;
const int PREFIX_PRIORITY = 4;

const int TOTAL_SPECIAL_TURTLES = 3; // number of special turtles
const int SPECIAL_TURTLE_EYE_LOCATION   = 0;
const int SPECIAL_TURTLE_EYE_FIXATION   = 1;
const int SPECIAL_TURTLE_LIGHT_LOCATION = 2;

// All NODETYPEs share the same basic NODE* data structure.
// The primitive types INTEGER, STRING, FLOATINGPOINT, ARRAY, and CONS are
// straight-foward, as each uses the values on the union that you'd expect.
// The other types add additional semantics to the primitives and are worth
// describing, as their structure and purpose are not self-evident.
//
// "bodyline" is a parsed text.  The text is tokenized and has all comments
//        stripped out.
//
// TREE - Trees are parentheized lines code code which know which functions take
//        which arguments.
//
//        If a function definition changes after a TREE is created, then the
//        associated line must be re-treeified, as the default number of
//        arguments that procedures take may have changed.
//
//        There are two types of tree nodes.
//        The first is a parenthesized line, which has the following
//        structure:
//        CAR/CDR - a list of STRING tokens (not treeified or runparsed)
//        OBJ - tree pair
//            CAR - generation (time when treeified)
//            CDR - parse tree (list of lists)
//                  This is never a LINE.
//                  The strings, including names of procedures,
//                  are CASEOBJ nodes and the variables are COLON nodes.
//            OBJ - NIL
//
//        The second type of tree node is a list of parenthesized body lines.
//        CAR/CDR - a list tokenized body lines.
//        OBJ - tree pair
//            CAR - generation (time when treeified)
//            CDR - a list of bodyline LIST/CONS nodes.
//                  The CDR of each node is either a CONS which is a
//                  parse tree of the next expression in the current line,
//                  or the LINE of the next body line.
//            OBJ - NIL
//
// LINE - This is the CDR of a tree pair object in a TREE.
//        CAR/CDR - A tree (list of lists) of expressions.
//        OBJ     - A TREE of the original tokenized line.
//                  This is used for error reporting.
//
// RUN_PARSE - A body line that knows about case objects, ?, :, "
//
// "procnode" is not a node type, but a building block.
// A procnode may be a PRIM, in which case the union holds
// all the information needed.  If a procnode is a list, then
// it's a user-defined procedure with the following structure
//   [ text bodywords minarg defaultarg maxargs ]
//      |     |
//      V     +- [ unparsed text ]
//   [ formals line1 line2 ... lineN ]
//
// "object" is also not a type, but another building block.
// However, this is an important concept within Logo.
// An "object" is a word that is associated with a procedure, variable, and/or
// property list.  It has the following structure:
//   [ canonical procnode value plist obflags caseobj1 caseobj2 ... ]
//
//    canonical is the canonical name for the object.
//
//    procnode is the procedure value of the object.
//
//    value is the variable value of the object.
//
//    plist is the property list value of the object.
//
//    objflags is an integer that holds some flags for the object.
//    For example, one flag holds if the object's value is buried.
//
//    The "caselist" constitutes the rest of the object's list, which is of
//    variable size.  Each entry in the caselist is a different case object
//    which represents the object (see CASEOBJ below).
//
// CASEOBJ - A case object is a word (with whatever casing the programmer used)
//    that is bound to an object.  The object, itself, has a case list that
//    points back to the CASEOBJ.  A case object is used to associate text that
//    a programmer typed with the interal object, so that the original text can
//    be used in error messages, if necessary.  For example, we might have
//    different case objects for "forward" and "FORWARD".
//
//    CAR - token (string name)
//    CDR - object
//    OBJ
//
// CONT - A node that represents the location in evaluator() where control
//        should continue to.  The CAR is not really a NODE, but an integer
//        that represents a label in evaluator().
//

/* Note that some of these values are used twice; they must be
 * distinguishable by the other bits used with them. */

enum NODETYPE
{
    NT_TREE     = 0x8000,
    NT_EMPTY    = 0x4000,
    NT_AGGR     = 0x2000,
    NT_LIST     = 0x1000,
    NT_RUNP     = 0x0800,
    NT_ARRAY    = 0x0400,
    NT_WORD     = 0x0200,
    NT_NUMBER   = 0x0100,
    NT_FLOAT    = 0x0080,
    NT_CONT     = 0x0080,
    NT_PRIM     = 0x0040,
    NT_INFIX    = 0x0020,
    NT_LINE     = 0x0020,
    NT_VBAR     = 0x0020,
    NT_STRING   = 0x0010,
    NT_BACKSL   = 0x0008,
    NT_PUNCT    = 0x0004,
    NT_TAILFORM = 0x0004,
    NT_COLON    = 0x0002,
    NT_MACRO    = 0x0002,
    NT_CASEOBJ  = 0x0001,

    PNIL             = NT_EMPTY|NT_AGGR|NT_LIST,            // 0x7000
    PUNBOUND         = 0,                                   // 0x0000
    CONS             = NT_AGGR|NT_LIST,                     // 0x3000
    STRING           = NT_WORD|NT_STRING,                   // 0x0210
    INTEGER          = NT_WORD|NT_NUMBER,                   // 0x0300
    FLOATINGPOINT    = NT_WORD|NT_NUMBER|NT_FLOAT,          // 0x0308
    PRIM             = NT_PRIM,                             // 0x0040
    MACRO            = NT_PRIM|NT_MACRO,                    // 0x0042
    TAILFORM         = NT_PRIM|NT_TAILFORM,                 // 0x0044
    CASEOBJ          = NT_WORD|NT_CASEOBJ,                  // 0x0201
    INFIX            = NT_PRIM|NT_INFIX,                    // 0x0060
    TREE             = NT_AGGR|NT_LIST|NT_TREE,             // 0xB000
    RUN_PARSE        = NT_AGGR|NT_LIST|NT_RUNP,             // 0x3800
    QUOTE            = NT_WORD|NT_PUNCT,                    // 0x0204
    COLON            = NT_WORD|NT_PUNCT|NT_COLON,           // 0x0206
    BACKSLASH_STRING = NT_WORD|NT_STRING|NT_BACKSL,         // 0x0218
    VBAR_STRING      = NT_WORD|NT_STRING|NT_BACKSL|NT_VBAR, // 0x0238
    ARRAY            = NT_AGGR|NT_ARRAY,                    // 0x2400
    LINE             = NT_LINE|NT_LIST|NT_AGGR,             // 0x3020
    CONT             = NT_CONT|NT_LIST,                     // 0x1080
    NT_FREE          = 0xFFFF,
};

typedef long    FIXNUM;
// typedef __int64 FIXNUM
const FIXNUM FIXNUM_MAX = 0x7fffffffL;

const FIXNUM SAFEINT    = 0x00003fff;   // safe to multiply w/o overflow

typedef double FLONUM;
const FLONUM FLONUM_MAX     = DBL_MAX;
const FLONUM FLONUM_EPSILON = 1.0e-12; // don't use DLB_EPSILON (see bug #269)

struct NODE
{
#ifdef MEM_DEBUG
    int magic; // set to 'NODE'
#endif

    NODETYPE type;

    long ref_count;
    union
    {
        struct
        {
            struct NODE * ncar;
            struct NODE * ncdr;
            struct NODE * nobj;    // used only for oblist
        }
            ncons;
        struct
        {
            const wchar_t * ptr;    // string contents
            wchar_t       * header; // reference count header
            int             length; // characters in ptr (not including NUL)
        }
            nstring;
        struct
        {
            logofunc fun;
            short    npriority;
            short    nmin_args;
            short    ndef_args;
            short    nmax_args;
        }
            nprim;
        FIXNUM nint;
        FLONUM nfloat;
        struct
        {
            int            dim;    // number of elements in data.
            int            origin; // logical offset
            struct NODE ** data;   // elements of array
        }
            narray;
    }
        nunion;
};


inline
bool
is_freed(const NODE * node)
{
    return node != NIL && (node->type == 0xCCCCCCCC || node->type == NT_FREE);
}

inline
NODETYPE
nodetype(const NODE *nd)
{
    assert(!is_freed(nd));
    return nd == NIL ? PNIL : nd->type;
}

inline
int
is_aggregate(const NODE * node)
{
    return nodetype(node) & NT_AGGR;
}

inline
int
is_cont(const NODE * node)
{
    return nodetype(node) == CONT;
}

inline
int
is_list(const NODE * node)
{
    return nodetype(node) & NT_LIST;
}

inline
int
is_tree(const NODE * node)
{
    return nodetype(node) & NT_TREE;
}

inline
int
is_string(const NODE * node)
{
    return nodetype(node) & NT_STRING;
}

inline
int
is_number(const NODE * node)
{
    return nodetype(node) & NT_NUMBER;
}

inline
int
is_prim(const NODE * node)
{
    return nodetype(node) & NT_PRIM;
}

inline
int
is_word(const NODE * node)
{
    return nodetype(node) & NT_WORD;
}

inline
int
is_caseobject(const NODE * node)
{
    return nodetype(node) == CASEOBJ;
}

inline
int
runparsed(const NODE * node)
{
    return nodetype(node) & NT_RUNP;
}

inline
int
backslashed(const NODE * node)
{
    return nodetype(node) & NT_BACKSL; // word with specical chars, may be vbarred
}

inline
int
is_tailform(const NODE * node)
{
    return nodetype(node) == TAILFORM;
}

// IMPORTANT: Logo programs depend on the value of these
// enumerated types, so any new values must be added to the
// end of the list.
enum ERR_TYPES
{
    SUCCESS              = 0,

    OUT_OF_MEM           = 1,
    STACK_OVERFLOW       = 2,
    TURTLE_OUT_OF_BOUNDS = 3,
    BAD_DATA_UNREC       = 4,
    DIDNT_OUTPUT         = 5,
    NOT_ENOUGH           = 6,
    BAD_DATA             = 7,
    TOO_MUCH             = 8,
    DK_WHAT              = 9,
    PAREN_MISMATCH       = 10,
    NO_VALUE             = 11,
    UNEXPECTED_PAREN     = 12,
    DK_HOW               = 13,
    NO_CATCH_TAG         = 14,
    ALREADY_DEFINED      = 15,
    STOP_ERROR           = 16,
    ALREADY_DRIBBLING    = 17,
    FILE_ERROR           = 18,
    IF_WARNING           = 19,
    SHADOW_WARN          = 20,
    USER_ERR             = 21,
    IS_PRIM              = 22,
    NOT_INSIDE           = 23,
    DK_HOW_UNREC         = 24,
    NO_TEST              = 25,
    UNEXPECTED_BRACKET   = 26,
    UNEXPECTED_BRACE     = 27,
    BAD_GRAPH_INIT       = 28,
    ERR_MACRO            = 29,
    DK_WHAT_UP           = 30,
    AT_TOPLEVEL          = 31,
    APPLY_BAD_DATA       = 32,
    DEEPEND              = 33,
    OUT_OF_MEM_UNREC     = 34,

    // FMSLogo-specific error codes start at 1000
    WINDOW_GENERAL                = 1000,
    WINDOW_ALREADY_EXISTS         = 1001,
    WINDOW_DOES_NOT_EXIST         = 1002,

    DLL_GENERAL                   = 1100,
    DLL_LOAD_FAILED               = 1101,
    DLL_NOT_LOADED                = 1102,
    DLL_TYPE_DATA_NOT_PAIRED      = 1103,
    DLL_INVALID_DATA_TYPE         = 1104,
    DLL_INVALID_OUTPUT_TYPE       = 1105,
    DLL_FUNCTION_NOT_FOUND        = 1106,

    IMAGE_GENERAL                 = 1200,
    IMAGE_GIF_LOAD_FAILED         = 1201,
    IMAGE_GIF_SAVE_FAILED         = 1202,
    IMAGE_BMP_OPEN_FAILED         = 1211,
    IMAGE_BMP_CREATE_FAILED       = 1212,
    IMAGE_BMP_WRITE_FAILED        = 1213,
    IMAGE_BMP_INVALID             = 1214,

    MIDI_GENERAL                  = 1300,
    MIDI_DEVICE_ALREADY_OPEN      = 1301,
    MIDI_INVALID_DEVICE           = 1302,
    MIDI_NOT_OPEN                 = 1303,

    TIMER_GENERAL                 = 1400,
    TIMER_NOT_FOUND               = 1401,

    INVALID_STATE_FOR_INSTRUCTION = 1500,

    MCI_ERROR                     = 1600,

    NETWORK_GENERAL               = 1700,
    NETWORK_ALREADY_STARTED       = 1701,
    NETWORK_NOT_STARTED           = 1702,
    NETWORK_CLIENT_ALREADY_ON     = 1703,
    NETWORK_SERVER_ALREADY_ON     = 1704,

    COMPORT_GENERAL               = 1800,
    COMPORT_OPEN_FAILED           = 1801,
    COMPORT_FLUSH_FAILED          = 1802,
    COMPORT_BUILD_DCB_FAILED      = 1803,
    COMPORT_SET_STATE_FAILED      = 1804,
    COMPORT_ALREADY_OPEN          = 1805,
    COMPORT_NOT_OPEN              = 1806,

    BITMAP_GENERAL                = 1900,
    BITMAP_CUT_FAILED             = 1901,
    BITMAP_NOTHING_TO_PASTE       = 1902,
    BITMAP_INDEX_OUT_OF_RANGE     = 1903,
    BITMAP_NO_BITMAP_AT_INDEX     = 1904,

    SPECIAL_FORM_PROC_UNREC       = 2001,

    FATAL                         = 0xFFFF,
};

#define even_p(x)  ((x % 2) == 0)
#define odd_p(x)   ((x % 2) != 0)

void     gc(NODE* node);
NODETYPE nodetype(const NODE *nd);

// Do not use isdigit() or isspace() because Borland's version does
// not work properly for non-ASCII characters.
// See bug #342 for details.
inline
bool
IsDigit(wchar_t ch)
{
    return '0' <= ch && ch <= '9';
}

// Do not use isspace() because Borland's version does not work properly
// for non-ASCII characters.  See bug #342 for details.
inline
bool
IsSpace(wchar_t ch)
{
    switch (ch)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
    case '\v':
        return true;

    default:
        return false;
    }
}

inline
void
settype(NODE * Node, NODETYPE Type)
{
    Node->type = Type;
}

inline
long
getrefcnt(const NODE * Node)
{
    return Node->ref_count;
}

inline
void increfcnt(NODE * Node)
{
    TRACE_NODE_CHANGE(Node);
    Node->ref_count++;
}

inline
long
decrefcnt(NODE * Node)
{
    TRACE_NODE_CHANGE(Node);
    return --Node->ref_count;
}


// Increments the reference count on a NODE.
inline
void
ref(NODE * object)
{
    if (object != NIL)
    {
        assert(!is_freed(object));
        increfcnt(object);
    }
}

// Increments the reference count on a node and returns it.
inline
NODE*
vref(NODE * object)
{
    if (object != NIL)
    {
        assert(!is_freed(object));
        increfcnt(object);
    }
    return object;
}


// Decrements the reference count on a node and deletes it if
// the reference count reaches 0.
// This should be called on referenced objects.
inline
void
deref(NODE * object)
{
    if (object != NIL)
    {
        assert(!is_freed(object));
        assert(getrefcnt(object) != 0); // memleak

        if (decrefcnt(object) == 0)
        {
            gc(object);
        }
    }
}

// Decrements the reference count and returns the object, but will not free it.
inline
NODE *
unref(NODE *ret_var)
{
    if (ret_var != NIL)
    {
        decrefcnt(ret_var);
    }
    return ret_var;
}


// Deletes an object if its reference count is 0.
// Does nothing, otherwise.
inline
void
gcref(NODE * object)
{
    if (object == NIL)
    {
        return;
    }

    assert(!is_freed(object));

    if (getrefcnt(object) == 0)
    {
        TRACE_NODE_CHANGE(object);
        gc(object);
    }
}

inline
void
assign(
    NODE * & To,
    NODE *   From
    )
{
    // Reference the new value.
    // Do this before dereferencing the old value because
    // the old value may hold the only reference on the new
    // value.  For example: assign(nd, car(nd))
    ref(From);

    // Dereference the old value.
    deref(To);

    // Make the assignment.
    To = From;
}

inline
void
setnode(NODE ** oldptr, NODE *newnode)
{
    assert(oldptr != NULL);

    ref(newnode);
    deref(*oldptr);

    *oldptr = newnode;
}

inline
NODE*
car(const NODE * node)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(node->type != STRING);
    assert(node->type != INTEGER);
    assert(node->type != FLOATINGPOINT);

    return node->nunion.ncons.ncar;
}


inline
void
setcar(NODE *node, NODE *newcar)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(node->type != STRING);
    assert(node->type != INTEGER);
    assert(node->type != FLOATINGPOINT);

    setnode(&node->nunion.ncons.ncar, newcar);
}

inline
NODE*
cdr(const NODE * node)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(node->type != STRING);
    assert(node->type != INTEGER);
    assert(node->type != FLOATINGPOINT);

    return node->nunion.ncons.ncdr;
}


inline
void
setcdr(NODE *node, NODE *newcdr)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(node->type != STRING);
    assert(node->type != INTEGER);
    assert(node->type != FLOATINGPOINT);

    setnode(&node->nunion.ncons.ncdr, newcdr);
}

inline
NODE*
getobject(const NODE * node)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(node->type != STRING);
    assert(node->type != INTEGER);
    assert(node->type != FLOATINGPOINT);

    return node->nunion.ncons.nobj;
}

inline
void
setobject(NODE *node, NODE *newobj)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(node->type != STRING);
    assert(node->type != INTEGER);
    assert(node->type != FLOATINGPOINT);

    setnode(&node->nunion.ncons.nobj, newobj);
}

inline
NODE*
caar(const NODE * node)
{
    return car(car(node));
}

inline
NODE*
cadr(const NODE * node)
{
    return car(cdr(node));
}

inline
NODE *
cdar(const NODE * node)
{
    return cdr(car(node));
}

inline
NODE *
cddr(const NODE * node)
{
    return cdr(cdr(node));
}

inline
const wchar_t *
getstrptr(const NODE * node)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(is_word(node));
    return (node)->nunion.nstring.ptr;
}

inline
int
getstrlen(const NODE * node)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(is_word(node));
    return node->nunion.nstring.length;
}


inline
wchar_t *
getstrhead(const NODE * node)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(is_word(node));
    return node->nunion.nstring.header;
}

inline
void
setstrptr(NODE * node, const wchar_t * str)
{
    assert(node != NULL);
    assert(!is_freed(node));
    assert(is_word(node));
    node->nunion.nstring.ptr = str;
}

inline
void
setstrlen(NODE * node, int length)
{
    node->nunion.nstring.length = length;
}

inline
void
setstrhead(NODE * node, wchar_t * ptr)
{
    node->nunion.nstring.header = ptr;
}

inline
unsigned short
getstrrefcnt(const wchar_t * refcnt)
{
    return *reinterpret_cast<const unsigned short*>(refcnt);
}

inline
void
setstrrefcnt(wchar_t * refcnt, unsigned short value)
{
    *reinterpret_cast<unsigned short*>(refcnt) = value;
}

inline
unsigned short
incstrrefcnt(wchar_t * refcnt)
{
    return (*refcnt)++;
}

inline
unsigned short
decstrrefcnt(wchar_t * refcnt)
{
    return --(*refcnt);
}

inline FIXNUM
getint(const NODE * Node)
{
    assert(Node != NULL);
    //assert(Node->type == INTEGER);
    return Node->nunion.nint;
}

inline FLONUM
getfloat(const NODE * Node)
{
    assert(Node != NULL);
    //assert(Node->type == FLOATINGPOINT);
    return Node->nunion.nfloat;
}

inline
logofunc
getprimfun(const NODE * Node)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    return Node->nunion.nprim.fun;
}

inline
void
setprimfun(NODE * Node, logofunc Func)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    Node->nunion.nprim.fun = Func;
}

// Returns the minimum number of inputs for procnode that
// represents a primitive.
//
// -1, if this is a "special form" (TO or .MACRO)
// OK_NO_ARG, if the command can appear on a line without any
//   inputs even if the default is not zero (HELP or CONTINUE).
inline
short
getprimmin(const NODE * Node)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    return Node->nunion.nprim.nmin_args;
}

inline
void
setprimmin(NODE * Node, short Min)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    Node->nunion.nprim.nmin_args = Min;
}


inline
short
getprimmax(const NODE * Node)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    return Node->nunion.nprim.nmax_args;
}

inline
void
setprimmax(NODE * Node, short Max)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    Node->nunion.nprim.nmax_args = Max;
}

inline
short
getprimdflt(const NODE * Node)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    return Node->nunion.nprim.ndef_args;
}

inline
void
setprimdflt(NODE * Node, short Default)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    Node->nunion.nprim.ndef_args = Default;
}

inline
short
getprimpri(const NODE * Node)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    return Node->nunion.nprim.npriority;
}

inline
void
setprimpri(NODE * Node, short Priority)
{
    assert(Node != NULL);
    assert(is_prim(Node));
    Node->nunion.nprim.npriority = Priority;
}

// Special value for pmin, means that it's
// OK if primitive name on line by itself even though defltargs=1 (ED, CO)
const int OK_NO_ARG = 0x200;

// Returns the number of elements within the array.
inline
int
getarrdim(const NODE * Node)
{
    assert(Node != NULL);
    assert(Node->type == ARRAY);
    return Node->nunion.narray.dim;
}

inline
int
getarrorg(const NODE * Node)
{
    assert(Node != NULL);
    assert(Node->type == ARRAY);
    return Node->nunion.narray.origin;
}

inline
NODE **
getarrptr(const NODE * Node)
{
    assert(Node != NULL);
    assert(Node->type == ARRAY);
    return Node->nunion.narray.data;
}

inline
void
setarrdim(NODE * Node, int Length)
{
    assert(Node != NULL);
    assert(Node->type == ARRAY);
    Node->nunion.narray.dim = Length;
}

inline
void
setarrorg(NODE * Node, int Origin)
{
    assert(Node != NULL);
    assert(Node->type == ARRAY);
    Node->nunion.narray.origin = Origin;
}

inline
void
setarrptr(NODE * Node, NODE ** Ptr)
{
    assert(Node != NULL);
    assert(Node->type == ARRAY);
    Node->nunion.narray.data = Ptr;
}

enum CTRLTYPE
{
    RUN,
    STOP,
    OUTPUT,
    THROWING,
    MACRO_RETURN,
};

#define NOT_THROWING            (stopping_flag != THROWING)
#define RUNNING                 (stopping_flag == RUN)
#define STOPPING                (stopping_flag == STOP)

inline
NODE *
canonical__object(const NODE * object)
{
    return car(object);
};

inline
NODE *
procnode__object(const NODE * object)
{
    return cadr(object);
}

inline
void
setprocnode__object(NODE * object, NODE * proc)
{
    setcar(cdr(object), proc);
}

inline
NODE *
valnode__object(const NODE * object)
{
    return cadr(cdr(object));
}

inline
void
setvalnode__object(NODE * object, NODE * value)
{
    setcar(cddr(object), value);
}

inline
NODE **
plistptr__object(const NODE * object)
{
    return &(cdr(cddr(object))->nunion.ncons.ncar);
}

inline
NODE *
plist__object(const NODE * object)
{
    return *plistptr__object(object);
}

inline
void
setplist__object(NODE * object, NODE * value)
{
    setnode(plistptr__object(object), value);
}

inline
NODE *
obflags__object(const NODE * object)
{
    return car(cddr(cddr(object)));
}

inline
NODE *
caselistptr__object(const NODE * object)
{
    return cddr(cddr(object));
}

inline
NODE *
caselist__object(const NODE * object)
{
    return cdr(cddr(cddr(object)));
}

inline
NODE *
strnode__caseobj(const NODE * caseobj)
{
    assert(is_caseobject(caseobj));
    return car(caseobj);
}

inline
NODE *
object__caseobj(const NODE * caseobj)
{
    assert(is_caseobject(caseobj));
    return cdr(caseobj);
}

inline
NODE *
procnode__caseobj(const NODE * caseobj)
{
    assert(is_caseobject(caseobj));
    return procnode__object(object__caseobj(caseobj));
}

inline
void
setprocnode__caseobj(const NODE * caseobj, NODE * procnode)
{
    assert(is_caseobject(caseobj));
    setprocnode__object(object__caseobj(caseobj), procnode);
}

inline
NODE *
valnode__caseobj(const NODE * caseobj)
{
    assert(is_caseobject(caseobj));
    return valnode__object(object__caseobj(caseobj));
}

inline
void
setvalnode__caseobj(const NODE * caseobj, NODE * value)
{
    setvalnode__object(object__caseobj(caseobj), value);
}

inline
NODE **
plistptr__caseobj(const NODE * caseobj)
{
    return plistptr__object(object__caseobj(caseobj));
}

inline
NODE *
plist__caseobj(const NODE * caseobj)
{
    return *plistptr__caseobj(caseobj);
}

inline
void
setplist__caseobj(const NODE * caseobj, NODE * plist)
{
    setnode(plistptr__caseobj(caseobj), plist);
}

inline
NODE *
obflags__caseobj(const NODE * caseobj)
{
    return obflags__object(object__caseobj(caseobj));
}

inline
NODE *
text__procnode(const NODE * procnode)
{
    return car(procnode);
}

inline
NODE *
formals__procnode(const NODE * procnode)
{
    return caar(procnode);
}

inline
NODE *
bodylist__procnode(const NODE * procnode)
{
    return cdar(procnode);
}

inline
NODE *
dfltargs__procnode(const NODE * procnode)
{
    return cadr(cddr(procnode));
}

inline
NODE *
minargs__procnode(const NODE * procnode)
{
    return car(cddr(procnode));
}

inline
NODE *
maxargs__procnode(const NODE * procnode)
{
    return car(cddr(cddr(procnode)));
}

inline
NODE *
bodywords__procnode(const NODE * procnode)
{
    return cadr(procnode);
}

inline
void
setbodywords__procnode(NODE * procnode, NODE * bodywords)
{
    setcar(cdr(procnode), bodywords);
}

inline
NODE*
parsed__runparse(
    const NODE * runparsed_node
    )
{
    assert(runparsed_node->type == RUN_PARSE);
    return getobject(runparsed_node);
}

inline
NODE *
node__quote(NODE * quotednode)
{
    return car(quotednode);
}

inline
NODE *
node__colon(NODE * colonnode)
{
    return car(colonnode);
}

inline
NODE *
valnode__colon(NODE * colonnode)
{
    return valnode__caseobj(node__colon(colonnode));
}

inline
NODE *
treepair__tree(const NODE * tree)
{
    assert(is_tree(tree));
    return getobject(tree);
}

inline
void
settreepair__tree(NODE * tree, NODE * treepair)
{
    assert(is_tree(tree));
    setobject(tree, treepair);
}

inline
NODE *
generation__tree(const NODE * tree)
{
    assert(is_tree(tree));
    return car(treepair__tree(tree));
}

inline
void
setgeneration__tree(NODE * tree, NODE * generation)
{
    return setcar(treepair__tree(tree), generation);
}

inline
NODE *
tree__tree(const NODE * tree)
{
    return cdr(treepair__tree(tree));
}

inline
NODE *
unparsed__line(NODE * line)
{
    assert(nodetype(line) == LINE);
    return getobject(line);
}

inline
NODE *
generation__line(NODE * line)
{
    assert(nodetype(line) == LINE);
    return generation__tree(unparsed__line(line));
}

inline
intptr_t
cont__cont(const NODE * cont)
{
    return reinterpret_cast<intptr_t>(car(cont));
}

inline
NODE *
val__cont(const NODE * cont)
{
    return cdr(cont);
}

/* Object flags.  Ones settable by users via bury_helper must come in threes
* for proc, val, plist even if meaningless for some of those. */
const int PROC_BURIED   =    01;
const int VAL_BURIED    =    02;
const int PLIST_BURIED  =    04;
const int PROC_TRACED   =   010;
const int VAL_TRACED    =   020;
const int PLIST_TRACED  =   040;
const int PROC_STEPPED  =  0100;
const int VAL_STEPPED   =  0200;
const int PLIST_STEPPED =  0400;
const int PROC_MACRO    = 01000;

inline
void
setflag__caseobj(NODE * caseobj, int flag)
{
    obflags__caseobj(caseobj)->nunion.nint |= flag;
}

inline
void
clearflag__caseobj(NODE * caseobj, int flag)
{
    obflags__caseobj(caseobj)->nunion.nint &= ~flag;
}

inline
int
flag__caseobj(const NODE * caseobj, int flag)
{
    return (int)(obflags__caseobj(caseobj)->nunion.nint & flag);
}

inline
int
flag__object(const NODE * object, int flag)
{
    return (int)(obflags__object(object)->nunion.nint & flag);
}

inline
int
is_macro(const NODE * casenode)
{
    return flag__caseobj(casenode, PROC_MACRO);
}

#define push(obj, stack)    (stack) = spush(obj, stack)
#define pop(stack)          (stack) = spop(stack)

/* evaluator labels, needed by macros in other files */

#define do_list(x) \
   x(all_done) \
   x(begin_line) \
   x(end_line) \
   x(begin_seq) \
   x(begin_apply) \
   x(eval_sequence_continue) \
   x(accumulate_arg) \
   x(compound_apply_continue) \
   x(set_args_continue) \
   x(macro_return) \
   x(after_lambda) \
   x(runresult_continuation) \
   x(runresult_followup) \
   x(repeat_continuation) \
   x(repeat_followup) \
   x(catch_continuation) \
   x(catch_followup) \
   x(goto_continuation) \


#define do_enum(x) x,

enum labels
{
    do_list(do_enum)
    NUM_TOKENS
};

const double ScreenSz = 0.70;      // Default proportion of screen size
const int    MAX_TIMERS = 32;

// Safe math abstraction
#ifdef __GNUC__
  // return true if the operation is safe (no overflow)
  #define safe_add(A, B, REF)      !__builtin_add_overflow(A, B, REF)
  #define safe_multiply(A, B, REF) !__builtin_mul_overflow(A, B, REF)
#else
  #error safe math abstraction not defined for this compiler
#endif

#endif // __LOGOCORE_H_
