/*

gbmbmp.h - OS/2 1.1, 1.2, 2.0 and Windows 3.0 support

*/

GBM_ERR bmp_qft(GBMFT *gbmft);
GBM_ERR bmp_rhdr(const wchar_t *fn, FILE* fp, GBM *gbm);
GBM_ERR bmp_rpal(FILE* fp, GBM *gbm, GBMRGB *gbmrgb);
GBM_ERR bmp_rdata(FILE* fp, GBM *gbm, byte *data);
GBM_ERR bmp_w(const wchar_t *fn, FILE* fp, const GBM *gbm, const GBMRGB *gbmrgb, const byte *data);
