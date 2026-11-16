/*

gbmgif.h - Graphics Interchange Format support

*/

GBM_ERR gif_qft(GBMFT *gbmft);
GBM_ERR gif_rhdr(const wchar_t *fn, FILE* fp, GBM *gbm);
GBM_ERR gif_rpal(FILE* fp, GBM *gbm, GBMRGB *gbmrgb);
GBM_ERR gif_rdata(FILE* fp, GBM *gbm, byte *data);
GBM_ERR gif_w(const wchar_t *fn, FILE* fp, const GBM *gbm, const GBMRGB *gbmrgb, const byte *data);
