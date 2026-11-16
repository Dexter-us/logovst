/* SciLexer features - not in standard Scintilla */

#ifndef SCILEXER_H
#define SCILEXER_H

#define SCLEX_CONTAINER 0
#define SCLEX_NULL      1
#define SCLEX_FMSLOGO   200 // beyond all of the built-in ones
#define SCLEX_AUTOMATIC 1000

// FMS lexer styles
#define SCE_FMS_DEFAULT                     0
#define SCE_FMS_STRING                      1
#define SCE_FMS_STRING_VBAR                 2

#define SCE_FMS_VBAR                        3

#define SCE_FMS_VARIABLE                    4
#define SCE_FMS_VARIABLE_VBAR               5

#define SCE_FMS_COMMENT                     6
#define SCE_FMS_COMMENT_IN_STRING           7
#define SCE_FMS_COMMENT_IN_VARIABLE         8

#define SCE_FMS_OPERATOR                    9
#define SCE_FMS_NUMBER                      10
#define SCE_FMS_IDENTIFIER                  11

#define SCE_FMS_DEFAULT_BACKSLASH           12

#endif // SCILEXER_H
