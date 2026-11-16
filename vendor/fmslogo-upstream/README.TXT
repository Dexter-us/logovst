                                   FMSLogo 8.5.0
--------------------------------------------------------------------------------

Kit Description (https://sourceforge.net/projects/fmslogo)

 fmslogo-8.5.0.exe - Installer for the Logo environment
 fmslogo-8.5.0.zip - ZIP of the Logo environment

Changes From Previous Version

  o Fix Bug #385; Deleting the window from the setup callback of
    WINDOWCREATE no longer reads freed memory.
  o Fix Bug #392; RUNPARSE no longer leaks memory.
  o Fix Bug #505; FIRST now handles empty arrays.
  o Fix Bug #595; GENSYM now buries the variable "gensym.number".
  o Implement Feature Request #136; the PAUSE dialog now explains
    what pause mode is and what you can do in it.
  o Implement Feature Request #143; MACHINE now includes the current
    scale of the screen window as the 18th element.
  o Fix Bug #598; QUEUE, DEQUEUE, and NAME now behave correctly when the
    queue/variable name is shadowed by one of their own local variables.
  o Fix Bug #599; The output list of macros that are run as input
    to a procedure and which define local variables are now evaluated
    in the caller's context, not the context of the macro.
  o Fix Bug #600; A procedure which runs a tail-called APPLY no longer
    leaks its parameters into the caller's scope.
  o Fix Bug #601; PO now displays lines in procedures defined with
    DEFINE correctly in the Commander History.
  o Fix Bug #602; APPLY with a primitive now copies the inputlist so that
    it can't be modified.
  o Fix Bug #606; Running a procedure with parse error no longer leaks memory.
  o Fix Bug #608; SETLABELFONT no longer leaks memory when given an
    unknown font.  This was a regression introduced in version 8.3.0.
  o Minor improvements to the manual.

 For the history of changes across all versions, see
 https://sourceforge.net/p/fmslogo/source/HEAD/tree/logo/trunk/src/CHANGELOG.TXT

Source Code

 The source code is available under the GNU GPL.  Get it from:

   https://sourceforge.net/p/fmslogo/source/HEAD/tree/

Minimum System Requirements

 Windows XP with 512 MB RAM
