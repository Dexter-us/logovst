/*

gbmhelp.c - Helpers for GBM file I/O stuff

*/

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include <wx/crt.h>

   #include <ctype.h>
   #include <stddef.h>
   #include <stdio.h>
   #include <stdlib.h>

   #include "gbm.h"
   #include "gbmhelp.h"
   #include "debugheap.h"
#endif


BOOLEAN gbm_same(const wchar_t *s1, const wchar_t *s2, int n)
{
    for ( ; n--; s1++, s2++ )
        if ( tolower(*s1) != tolower(*s2) )
            return FALSE;
    return TRUE;
}

FILE* gbm_file_open(const wchar_t *fn, const wchar_t * mode)
{
    return wxFopen(fn, mode);
}
FILE* gbm_file_create(const wchar_t *fn, const wchar_t * mode)
{
    return wxFopen(fn, mode);
}
void gbm_file_close(FILE * fp)
{
    fclose(fp);
}
long gbm_file_lseek(FILE* fp, long pos, int whence)
{
    fseek(fp, pos, whence);
    return ftell(fp);
}
int gbm_file_read(FILE* fp, void *buf, int len)
{
    return fread(buf, 1, len, fp);
}
int gbm_file_write(FILE* fp, const void *buf, int len)
{
    return fwrite(buf, 1, len, fp);
}

AHEAD *gbm_create_ahead(FILE* fp)
{
    AHEAD *ahead;

    if ( (ahead = (AHEAD *)malloc((size_t) sizeof(AHEAD))) == NULL )
        return NULL;

    ahead->inx = 0;
    ahead->cnt = 0;
    ahead->fp  = fp;

    return ahead;
}

void gbm_destroy_ahead(AHEAD *ahead)
{
    free(ahead);
}

int gbm_read_ahead(AHEAD *ahead)
{
    if ( ahead->inx >= ahead->cnt )
    {
        ahead->cnt = gbm_file_read(ahead->fp, (char *) ahead->buf, AHEAD_BUF);
        if ( ahead->cnt <= 0 )
            return -1;
        ahead->inx = 0;
    }
    return (int) (unsigned int) ahead->buf[ahead->inx++];
}
