/*

gbm.c - Generalised Bitmap Module

*/

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   /*...sincludes:0:*/
   #include <stdio.h>
   #include <ctype.h>
   #include <stddef.h>
   #include <stdlib.h>
   #include <wchar.h>

   #include "gbm.h"
   #include "gbmhelp.h"

   #include "gbmbmp.h"
   #include "gbmgif.h"
   #include "debugheap.h"
#endif


struct FT
{
    GBM_ERR (*query_filetype)(GBMFT *gbmft);
    GBM_ERR (*read_header )(const wchar_t *fn, FILE* fp, GBM *gbm);
    GBM_ERR (*read_palette)(FILE* fp, GBM *gbm, GBMRGB *gbmrgb);
    GBM_ERR (*read_data   )(FILE* fp, GBM *gbm, byte *data);
    GBM_ERR (*write       )(const wchar_t *fn, FILE* fp, const GBM *gbm, const GBMRGB *gbmrgb, const byte *data);
};

static const FT fts[] =
{
    {bmp_qft, bmp_rhdr, bmp_rpal, bmp_rdata, bmp_w},
    {gif_qft, gif_rhdr, gif_rpal, gif_rdata, gif_w}
};

GBMEXPORT FILE* GBMENTRY gbm_io_open(const wchar_t *fn, const wchar_t * mode)
{
    return gbm_file_open(fn, mode);
}
GBMEXPORT FILE* GBMENTRY gbm_io_create(const wchar_t *fn, const wchar_t * mode)
{
    return gbm_file_create(fn, mode);
}
GBMEXPORT void  GBMENTRY gbm_io_close(FILE* fp)
{
    gbm_file_close(fp);
}
GBMEXPORT long GBMENTRY gbm_io_lseek(FILE* fp, long pos, int whence)
{
    return gbm_file_lseek(fp, pos, whence);
}
GBMEXPORT int  GBMENTRY gbm_io_read (FILE* fp, void *buf, int len)
{
    return gbm_file_read(fp, buf, len);
}
GBMEXPORT int  GBMENTRY gbm_io_write(FILE* fp, const void *buf, int len)
{
    return gbm_file_write(fp, buf, len);
}


GBMEXPORT GBM_ERR GBMENTRY gbm_query_filetype(int ft, GBMFT *gbmft)
{
    if ( gbmft == NULL )
        return GBM_ERR_BAD_ARG;
    return (*fts[ft].query_filetype)(gbmft);
}


GBMEXPORT GBM_ERR GBMENTRY gbm_read_header(const wchar_t *fn, FILE* fp, int ft, GBM *gbm)
{
    if ( fn == NULL || gbm == NULL )
        return GBM_ERR_BAD_ARG;
    gbm_file_lseek(fp, 0L, SEEK_SET);
    return (*fts[ft].read_header)(fn, fp, gbm);
}


GBMEXPORT GBM_ERR GBMENTRY gbm_read_palette(FILE* fp, int ft, GBM *gbm, GBMRGB *gbmrgb)
{
    if ( gbm == NULL || gbmrgb == NULL )
        return GBM_ERR_BAD_ARG;
    return (*fts[ft].read_palette)(fp, gbm, gbmrgb);
}


GBMEXPORT GBM_ERR GBMENTRY gbm_read_data(FILE* fp, int ft, GBM *gbm, byte *data)
{
    if ( gbm == NULL || data == NULL )
        return GBM_ERR_BAD_ARG;
    return (*fts[ft].read_data)(fp, gbm, data);
}


GBMEXPORT GBM_ERR GBMENTRY gbm_write(const wchar_t *fn, FILE* fp, int ft, const GBM *gbm, const GBMRGB *gbmrgb, const byte *data)
{
    if ( fn == NULL )
        return GBM_ERR_BAD_ARG;
    return (*fts[ft].write)(fn, fp, gbm, gbmrgb, data);
}

