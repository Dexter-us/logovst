/*
 *      paren.cpp            logo parenthesizing module          dko
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
    #include "paren.h"
    #include "logodata.h"
    #include "eval.h"
    #include "init.h"
    #include "error.h"
    #include "lists.h"
    #include "logomath.h"
    #include "appendablelist.h"
    #include "logocore.h"
    #include "debugheap.h"
#endif

NODE *the_generation;
bool tree_dk_how;

static NODE *gather_args(NODE *proc, NODE **args, bool inparen, NODE **ifnode);
static NODE *paren_infix(NODE *left_arg, NODE **rest, int old_pri, bool inparen);
static NODE * compute_treepair(NODE * line);

// Set the line pointer for a tree.
static
void make_line(NODE *tree, NODE *line)
{
    setobject(tree, line);
    settype(tree, LINE);
}

void untreeify_body(NODE *body)
{
    settreepair__tree(body, NIL);
    settype(body, CONS);
}

// Treeify a procedure's bodylines by appending the trees of the lines.
//
// is_tree(body) will return zero on error and if all body lines were empty.
// is_tree(body) will return non-zero if the body was treeified and there was
// at least one non-empty line within the body.
void treeify_body(NODE *body)
{
    if (body == NIL ||
        (is_tree(body) && generation__tree(body) == the_generation))
    {
        return;
    }

    if (is_tree(body))
    {
        // The definition of something has changed.
        // We must re-treeify this body in case it calls the
        // procedure which changed.
        assert(generation__tree(body) != the_generation);
        untreeify_body(body);
    }

    NODE * treelist_start = NIL;
    NODE * treelist_end = NIL;
    for (NODE * body_ptr = body; body_ptr != NIL; body_ptr = cdr(body_ptr))
    {
        NODE * line = car(body_ptr);
        if (line == NIL)
        {
            // skip blank lines or else we'd error out at is_tree()
            continue;
        }

        // store the current line (for error reporting)
        assign(this_line, line);

        // tree-ify this line
        NODE * treepair = compute_treepair(line);
        if (treepair != NIL)
        {
            // Add a link from the first expression in the parse tree
            // to the line from which it was created.  This is needed
            // for error reporting when evaluating.
            NODE * parsetree = cdr(treepair);
            make_line(parsetree, line);

            // append the treeified line to the end of body's tree list
            if (treelist_end == NIL)
            {
                // body can't be considered "treeified" until there is at
                // least one line to treeify so that treepair__tree() isn't NIL.
                // To a caller that only checks the status of this function by
                // calling is_tree(body), this looks the same as an error.
                // This is okay because the evaulator considers a treeification
                // error the same as a body consisting of only empty lines
                // (there's nothing to evaulate in either case).
                //
                // REVISIT: Fix the evaluator to handle when
                //     is_tree(body) && treepair__tree(body)==NIL
                // so that we can more clearly distinguish between the error
                // case and the "empty tree" case.
                settype(body, TREE);
                settreepair__tree(body, treepair);

                // Take a reference on the treeified list of expressions.
                // This is needed because treeifying a subsequent line could
                // results in calling load_procedure_if_necessary(), which can
                // redefine this procedure, which would free the body's reference
                // on the treepair.
                treelist_start = vref(treepair);
            }
            else
            {
                setcdr(treelist_end, parsetree);

                // If the current line needs to be re-treeified later, then flag
                // the body list (effectively all lines) as needing to be
                // re-treeified.
                NODE * generation = car(treepair);
                if (generation == Unbound)
                {
                    setgeneration__tree(body, Unbound);
                }
            }

            // advance treelist_end to the end of body's tree list
            while (cdr(parsetree) != NIL)
            {
                parsetree = cdr(parsetree);
            }
            treelist_end = parsetree;

            gcref(treepair);
        }
        else
        {
            // error while treeifying
            if (is_tree(body))
            {
                untreeify_body(body);
            }
            break;
        }
    }

    deref(treelist_start);
}


// Parenthesize an expression (list of tokens) and return it as a tree.
// Set expr to the node after the first full expression.
// Set stopping_flag to THROWING on a parse error (unbalanced parens).
// Set tree_dk_how to true if a procedure was called that isn't
// defined, which implies the number of default arguments isn't known.
static
NODE *paren_expr(NODE **expr, bool inparen)
{
    if (*expr == NIL)
    {
        // This is the expected base case.
        return NIL;
    }

    NODE *retval;

    // REVISIT: do we really need to ref and deref first?
    NODE * first = vref(car(*expr));
    pop(*expr);
    if (is_caseobject(first) && !numberp(first))
    {
        if (first == Left_Paren)
        {
            deref(first);

            // Parse this sub-expression.
            NODE * tree = paren_expr(expr, true);
            tree = paren_infix(tree, expr, -1, true);

            if (*expr == NIL)
            {
                // There's an open left paren with no close right paren.
                // Create a delayed error that will be thrown when evaluated.
                gcref(tree);
                tree = cons_list(first, g_ParseError, make_intnode(PAREN_MISMATCH));
            }
            else if (car(*expr) != Right_Paren)
            {
                // There was a problem parenthesizing expr.
                NODE * proc;
                if (is_list(tree))
                {
                    // expr is something like "(BINARYPROC 1)".
                    // tree is something like [BINARYPROC 1].
                    // In this case, "proc" is BINARYPROC.
                    proc = car(tree);
                }
                else
                {
                    // expr is something like "(1 2)".
                    // tree is something like 1 (because paren_infix failed)
                    // In this case "proc" is "1".
                    proc = tree;
                }

                if (nodetype(proc) != CASEOBJ ||
                    procnode__caseobj(proc) == Unbound)
                {
                    // The procedure is not defined, so we don't know
                    // how many parameters it takes.
                    // Set a flag to indicate that this line must be re-treeified.
                    // before it can be run.
                    tree_dk_how = true;
                    NODE * newtree = cons_list(proc);

                    // tree has no references and it's possible that newtree
                    // only references the car(tree).  Therefore, we must free
                    // the unreferenced parts of tree.
                    gcref(tree);
                    tree = newtree;
                }
                else
                {
                    // There were too many inputs.
                    // For example, if the *expr was [ ( FORWARD 100 50 ) ]
                    // then *expr now points to the 100 ----------^.
                    // Advance to the closing ")" to recover.
                    while (*expr != NIL && *expr != Right_Paren)
                    {
                        pop(*expr);
                    }

                    // Add a node that delays throwing a TOO_MUCH error
                    // until it's evaluated.  This lets instructions prior
                    // to this error run.  It's also possible that the code
                    // path that contains this error won't ever be run.
                    NODE * newtree = cons_list(
                        proc,
                        g_ParseError,
                        make_intnode(TOO_MUCH));

                    // tree has no references and it's possible that newtree
                    // only references the car(tree).  Therefore, we must free
                    // the unreferenced parts of tree.
                    gcref(tree);
                    tree = newtree;
                }
            }
            else
            {
                pop(*expr);
            }
            retval = tree;
        }
        else if (first == Right_Paren)
        {
            // Either a ")" with no matching "(" or a empty parens "()".
            deref(first);
            if (inparen)
            {
                // There are a pair of parens nothing in-between: "()".
                // Put the right paren back onto expr.  why?
                push(first, *expr);
            }

            // Add a node that delays throwing the UNEXPECTED_PAREN
            // error until it's evaluated.  This lets instructions
            // prior to this error run.  It's also possible that
            // the code path that contains this error won't ever be run.
            retval = cons_list(
                first,
                g_ParseError,
                make_intnode(UNEXPECTED_PAREN));
        }
        else
        {
            // it must be a procedure
            NODE * proc = load_procedure_if_necessary(first);
            if (proc == UNDEFINED)
            {
                // Even after attempting a silent load of
                // the procedure, it couldn't be defined.
                // We act as if it takes no inputs.
                retval = cons_list(first);

                // Set a flag to indicate that this line must be re-treeified.
                // before it can be run.
                tree_dk_how = true;
            }
            else if (nodetype(proc) == INFIX && NOT_THROWING)
            {
                if (first == Minus_Sign)
                {
                    // The procedure is Minus_Sign (-).  Since this is infix operator
                    // the "-" is more likely to be intended as negation than subtraction.
                    retval = gather_args(procnode__caseobj(Minus_Tight), expr, false, NULL);
                    if (retval != Unbound)
                    {
                        retval = cons(first, retval);
                    }
                }
                else
                {
                    // If proc is an infix operator, then an operand should
                    // have come before it but didn't.
                    // This makes sure that 5**6 says "not enough inputs to *"
                    retval = cons_list(first, g_ParseError, make_intnode(NOT_ENOUGH));
                }
            }
            else
            {
                NODE **ifnode = NULL;

                if (compare_node(first, If, true) == 0)
                {
                    // Kludge: turn IF to IFELSE sometimes.
                    ifnode = &first;
                }

                // update "fun" in case this is a special form (TO or .MACRO)
                // and can encounter an error within gather_args().
                // This allows proper error reporting if fun receives bad input.
                assign(fun, first);

                retval = gather_args(proc, expr, inparen, ifnode);
                if (retval != Unbound)
                {
                    retval = cons(first, retval);
                }
            }
            deref(first);
        }
    }
    else if (is_list(first))
    {
        /* quoted list */
        retval = make_quote(first);
        deref(first);
    }
    else
    {
        unref(first);
        return first;
    }
    return retval;
}

// Fully parenthesize a complete line.
// i.e. transform it from a flat list to a tree.
static
NODE *paren_line(NODE *line)
{
    CAppendableList parenthesizedLine;

    while (line != NIL)
    {
        NODE * addition = paren_expr(&line, false);
        if (stopping_flag == THROWING || addition == Unbound)
        {
            // Something went wrong.
            // For example, a syntax error was detected.
            // Clean up whatever hasn't been consumed.
            deref(line);
            gcref(addition);
            gcref(parenthesizedLine.GetList());
            return Unbound;
        }

        addition = paren_infix(addition, &line, -1, false);
        parenthesizedLine.AppendElement(addition);
    }

    return parenthesizedLine.GetList();
}


static
bool
argument_is_available(
    const NODE * RemainingArguments,
    bool         ForceSubtractionBinary
    )
{
    if (RemainingArguments == NIL)
    {
        // no more arguments in the list
        return false;
    }
    NODE * nextArgument = car(RemainingArguments);
    if (nextArgument == Right_Paren)
    {
        // we hit a right-paren (which ends this)
        return false;
    }
    if (is_caseobject(nextArgument) &&
        nodetype(procnode__caseobj(nextArgument)) == INFIX)
    {
        // We reached an infix procedure, which typicallly indicates the end of
        // the inputs.  Therefore, we return false except in the case where "-"
        // acts as a prefix argument (a negation).
        // For example, "SHOW - 1" has one argument, not zero.
        if (ForceSubtractionBinary ||
            (Minus_Sign != nextArgument && nextArgument != Minus_Tight))
        {
            return false;
        }
    }

    return true;
}

// Return a list of the next n expressions, where n is between min and max.
// Set args to immediately after the last expression.
static
NODE *gather_some_args(
    int     min,
    int     max,
    NODE ** args,
    int     priority,
    int     force_subtraction_as_binary,
    NODE ** ifnode
    )
{
    CAppendableList inputs;

    while (argument_is_available(*args, force_subtraction_as_binary))
    {
        // At least one more argument is available.
        if (max == 0)
        {
            // We have already gathered the maximum number of arguments for the
            // procedure.
            if (ifnode != nullptr && is_list(car(*args)))
            {
                // The procedure is IF and the excess argument is a list.
                // The programmer might have meant to use IFELSE, so we
                // convert IF -> IFELSE and continue parsing one last
                // argument.
                err_logo(IF_WARNING, NIL);
                assign(*ifnode, Ifelse);
            }
            else
            {
                // we have gathered the maximum number of arguments
                break;
            }
        }
        else
        {
            min--;
            max--;
        }

        // Parse an expression tree of the next argument.
        NODE *nextinput = paren_expr(args, false);
        nextinput = paren_infix(nextinput, args, priority, false);

        inputs.AppendElement(nextinput);
    }

    if (min > 0)
    {
        // We weren't able to find enough inputs.
        gcref(inputs.GetList());

        // Set a special marker at this point in the parse
        // tree so that evaluator() knows to report the error.
        return cons_list(g_ParseError, make_intnode(NOT_ENOUGH));
    }

    return inputs.GetList();
}

// Gather the correct number of inputs to proc starting at *args.
// Upon exit, this sets *args to immediately after the last arg.
// If inparen is true, then *args is set to Right_Paren.
//
// ifnode is a hack that allows for IF->IFELSE conversion.
// If proc is IF, then *ifnode is a pointer to the proc.
// If it should be an IFELSE, then *ifnode is changed to IfElse.
static
NODE *gather_args(NODE *proc, NODE **args, bool inparen, NODE **ifnode)
{
    int min, max, priority, force_subtraction_as_binary;

    if (nodetype(proc) == CONS)
    {
        // user-defined procedure
        if (inparen)
        {
            min = getint(minargs__procnode(proc));
            max = getint(maxargs__procnode(proc));
        }
        else
        {
            // outside of parens, a procedure is given
            // exactly its default number of inputs.
            min = max = getint(dfltargs__procnode(proc));
        }
        priority = PREFIX_PRIORITY;
        force_subtraction_as_binary = false;
    }
    else
    {
        // primitive
        min = (inparen ? getprimmin(proc) : getprimdflt(proc));
        if (min < 0)
        {
            // Special form (either TO or .MACRO)
            // Invoking the primitive gathers its arguments.
            return ((logofunc) *getprimfun(proc)) (*args);
        }

        if (getprimmin(proc) == OK_NO_ARG)
        {
            // Kludge: allow EDIT and CO without input without paren
            min = 0;
        }

        max = (inparen ? getprimmax(proc) : getprimdflt(proc));
        priority = getprimpri(proc);

        // Normally, a "-" without an expression before it is converted
        // to a unary negation, as in "SHOW -1".  However, in the case of
        // a question mark, this can give counter-intuitive evaluation of
        // "2 * (? - 1)".  In that case, a negation would be treated as the
        // "which parameter" input to "?", instead of subtracting on the
        // output of "?".  To preserve backward compatibility, the behavior
        // of changing subtraction to negation is disabled when gathering
        // arguments for "?".
        force_subtraction_as_binary = getprimfun(proc) == lqm;
    }

    return gather_some_args(
        min,
        max,
        args,
        priority,
        force_subtraction_as_binary,
        ifnode);
}

// Treeify a list of tokens (runparsed or not) and builds a parse
// tree according to how many inputs each procedure currently expects.
//
// On success returns a treepair with no references.
// On error, returns NIL.
// On some errors, stopping_node is set to THROWING.
//
// Note that NIL cannot be tree'ified, but it can be run.
static
NODE * compute_treepair(NODE * line)
{
    // Make sure the line is runparsed.
    if (!runparsed(line))
    {
        make_runparse(line);
    }

    // must reference parsed_node because paren_line() derefs it
    ref(parsed__runparse(line));

    tree_dk_how = false;
    NODE * parsetree = paren_line(parsed__runparse(line));
    if (parsetree == NIL || parsetree == Unbound)
    {
        // Unable to build a parse tree.
        return NIL;
    }

    NODE * generation;
    if (tree_dk_how || stopping_flag == THROWING)
    {
        // make sure this gets re-treeified later
        generation = Unbound;
    }
    else
    {
        generation = the_generation;
    }

    return cons(generation, parsetree);
}


// Treeify a list of tokens (runparsed or not).
// This essentially tokenizes the list and builds a parse tree according
// to how many inputs each procedure currently expects.
//
// On success is_tree(newtree) will be non-zero.
// On some errors, stopping_node is set to THROWING.
//
// Note that NIL cannot be tree'ified, but it can be run.
void treeify_line(NODE *newtree)
{
    if (newtree == NIL ||
        (is_tree(newtree) && generation__tree(newtree) == the_generation))
    {
        // already a tree
        return;
    }

    NODE * treepair = compute_treepair(newtree);
    if (treepair != NIL)
    {
        settype(newtree, TREE);
        settreepair__tree(newtree, treepair);
    }
}


// Calculate the priority of a procedure.
// This is called by paren_infix() to determine when it should stop
// trying to gather tokens.
static
int priority(NODE *proc_obj)
{
    // If proc_obj is not an infix procedure, then don't gather inputs for it.
    // This includes when it's not even a procedure.
    NODE *proc;
    if (nodetype(proc_obj) != CASEOBJ ||
        (proc = procnode__caseobj(proc_obj)) == UNDEFINED ||
        nodetype(proc) != INFIX)
    {
        return STOP_PRIORITY;
    }

    return is_prim(proc) ? getprimpri(proc) : PREFIX_PRIORITY;
}

// Parenthesize a expression with infix operators.
//
// left_arg - the expression on the left (already parenthesized).
// rest     - A pointer to the list starting with the infix operator, if
//            it exists.  The operator and its right side are gathered from
//            this list.  This pointer is set to the point where this
//            function stops parenthesizing (the portion of *rest that isn't
//            returned).
// old_pri  - the priority of the procedure in left_arg.  Parenthesizing
//            continues until the priority of the infix operators in *rest
//            have a lower priority than old_pri.
//            -1 is lower than all priorities, so it indicates to parse all
//            infix operations in *rest.
// inparen  - if the infix operation is parenthesized.
//
// Returns a parenthesized tree in prefix form [operator left right].
// The right node is also a parenthesized tree.
//
// If *rest is not an infix operator, then *rest is not changed and
// left_arg is returned.
static
NODE *paren_infix(NODE *left_arg, NODE **rest, int old_pri, bool inparen)
{
    while (*rest != NIL) // not end of expression
    {
        NODE * infix_proc = car(*rest);
        int    new_pri    = priority(infix_proc);

        if (new_pri == STOP_PRIORITY || new_pri <= old_pri)
        {
            // We've either encountered a non-infix procedure
            // or infix_proc has a lower binding priority than the
            // previous node (for example + when parsing "2 * 3 + 1").
            // Either way, we want to stop.
            break;
        }

        if (cdr(*rest) == NIL)
        {
            // We have an infix procedure without a right operand.
            // Delay the "not enough inputs" error until this expression is
            // evaluated, so that instructions prior to this error are still
            // evaluated.
            gcref(left_arg);
            left_arg = cons_list(
                infix_proc,
                g_ParseError,
                make_intnode(NOT_ENOUGH));
            break;
        }

        // Pop infix_proc from the list, careful not to free it
        // if the list is the only thing that held a reference to it.
        ref(infix_proc);
        pop(*rest);

        // Parenthesize the right input.
        // The following paren_infix honors the operator precedence for any
        // infix operator with a higher precedence than infix_proc that may
        // follow the next expression.  For example, when this is called on
        // "1 + 2 * 3", paren_expr returns 2 and paren_infix returns (* 2 3),
        // making the overall expression (+ 1 (* 2 3)).  When this is called
        // on "1 * 2 + 3", paren_expr returns 2 and paren_infix does nothing.
        // leaving the "+ 3" in *rest, to be parenthesized in the next
        // iteration.
        NODE * right_arg = paren_expr(rest, inparen);
        right_arg = paren_infix(right_arg, rest, new_pri, inparen);

        // Change the infix notation to prefix notation and move on
        // to the next possible infix expression at the same precedence.
        // (Operators at a higher precedence should have been parsed above).
        //
        // For example when parenthesizing [1 + 2 + 3], left_arg is
        //         1        on the first iteration
        //      (+ 1 2)     on the second iteration
        //   (+ (+ 1 2) 3)  on the third iteration
        left_arg = cons_list(infix_proc, left_arg, right_arg);

        // Now that infix_proc is reference by the right_arg list,
        // we can release the reference that we took above.
        deref(infix_proc);
    }

    return left_arg;
}
