/*

gbmhelp.h - Internal helpers for GBM file I/O stuff

*/

#ifndef GBMHELP_H
#define	GBMHELP_H

#include <stddef.h>
#include <stdio.h>

BOOLEAN gbm_same(const wchar_t *s1, const wchar_t *s2, int n);

FILE* gbm_file_open  (const wchar_t *fn, const wchar_t * mode);
FILE* gbm_file_create(const wchar_t *fn, const wchar_t * mode);
void  gbm_file_close (FILE* fp);
long  gbm_file_lseek (FILE* fp, long pos, int whence);
int   gbm_file_read  (FILE* fp, void *buf, int len);
int   gbm_file_write (FILE* fp, const void *buf, int len);

#define	AHEAD_BUF 0x4000

struct AHEAD
{
    byte buf[AHEAD_BUF];
    int inx, cnt;
    FILE* fp;
};

AHEAD *gbm_create_ahead(FILE * fp);
void   gbm_destroy_ahead(AHEAD *ahead);
int    gbm_read_ahead(AHEAD *ahead);

#endif
