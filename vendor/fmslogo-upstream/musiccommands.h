#ifndef __MUSICCOMMANDS_H_
#define __MUSICCOMMANDS_H_

struct NODE;

NODE *lmusicnote(NODE *args);
NODE *lmusicchord(NODE *args);
NODE *lmusicsteal(NODE *args);
NODE *lmusicanticipate(NODE *args);
NODE *lmusicrest(NODE *args);
NODE *lmusiccc(NODE *args);
NODE *lmusicprogram(NODE *args);
NODE *lmusicvelocity(NODE *args);
NODE *lmusicchannel(NODE *args);
NODE *lmusicgate(NODE *args);
NODE *lmusictempo(NODE *args);

#endif // __MUSICCOMMANDS_H_
