
#ifndef _HYPERLOGLOG_H
#define _HYPERLOGLOG_H

#include <stdint.h>
static const char *hll_magic_char = "HYLL";

typedef struct hllhdr hllhdr;

hllhdr *createHLLObject();
int pfaddCommand(hllhdr **ptr, const unsigned char *key, int len);
int pfcountCommand(hllhdr *hdr, uint64_t *ret);
int pfmergeCommand(hllhdr **dest, hllhdr *src);
hllhdr* hllSparseToDense(hllhdr *ptr);
void deleteHLLObject(hllhdr* hdr);

#endif