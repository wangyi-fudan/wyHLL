#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "hyperloglog.h"

/*
* 一些工具函数
*/

/* debug宏 */
#ifdef DEBUG
	#define xprintf(fmt, ...)	printf("FILE:<%s>,FUN:<%s>,LINE:<%d>,"fmt"\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
	#define xprintf(fmt, ...)
#endif

/* assert宏, assert失败时打印表达式 */
#define xassert(X) 		if(!(X)){										\
								xprintf(" (%s) assert fail!", #X);		\
								assert(0);								\
							}


enum 
{
	_Little_endian = 0,
	_Big_endian
};

/*判断大小端的代码*/
inline static int byte_order()
{
	union
	{
		short a;
		char b;
	}w;
	
	w.a = 1;
	
	return (w.b == 1) ? _Little_endian : _Big_endian;
}


const uint16_t hll_sparse_max_bytes = 3000;				/*sparse编码方式最大占用空间*/

/*
* 关于sds内存分配的一些简单代码
*/
typedef struct sdshdr sdshdr;
typedef char *sds;

struct __attribute__ ((__packed__)) sdshdr
{
    uint16_t len; 			/* 已使用的空间 */
    uint16_t alloc; 		/* 分配的总空间 */
    char buf[];
};

#define sdsHdrSize sizeof(sdshdr)

static inline size_t sdsavail(const sds s)
{
	sdshdr *sh = (sdshdr*)(s - sdsHdrSize);
	return (sh->alloc - sh->len);
}

static inline void sdssetlen(void *ptr, size_t len)
{
	sds s = (sds)ptr;
	sdshdr *sh = (sdshdr*)(s - sdsHdrSize);
	sh->len = len;
}

static inline void sdssetalloc(void *ptr, size_t len)
{
	sds s = (sds)ptr;
	sdshdr *sh = (sdshdr*)(s - sdsHdrSize);
	sh->alloc = len;
}

static inline size_t sdslen(const void *ptr)
{
	sds s = (sds)ptr;
	sdshdr *sh = (sdshdr*)(s - sdsHdrSize);
	return sh->len;
}

static inline void sdsIncrLen(void *ptr, int inc)
{
	sds s = (sds)ptr;
	sdshdr *sh = (sdshdr*)(s - sdsHdrSize);
	sh->len += inc;
}

sds sdsnewlen(const void *init, size_t initlen)
{
	
	sdshdr *sh = (sdshdr*)calloc(sdsHdrSize + initlen + 1, 1);
	sds s = (char*)sh + sdsHdrSize;
	if(init)
	{
		memcpy(s, init, initlen);
	}
	sh->alloc 	= initlen;
	sh->len   	= initlen;
	s[initlen] 	= '\0';
	return s;
}

void sdsfree(void *s)
{
	if (s == NULL) return;
    free(((char*)s - sdsHdrSize));
}

/*
* 小于3/4直接翻倍, 超过直接等于最大值,
* 达到最大值不再分配内存
*/
sds sdsMakeRoomFor(void *ptr, size_t addlen)
{
	void *sh, *newsh;
	sds s = (sds)ptr;
	size_t avail = sdsavail(s);
	size_t len = sdslen(s);
	size_t newlen = len + addlen;
	
	if(avail >= addlen || len >= hll_sparse_max_bytes)
	{
		return s;
	}
	
	if(newlen * 2 < (hll_sparse_max_bytes / 4 * 3))
	{
		newlen *= 2;
	}
	else
	{
		newlen = hll_sparse_max_bytes;
	}
	
	sh = (char*)s - sdsHdrSize;
	newsh = realloc(sh, sdsHdrSize + newlen + 1);
	
	if(!newsh)
	{
		return NULL;
	}
	
	s = (char*)newsh + sdsHdrSize;
	sdssetlen(s, len);
	sdssetalloc(s, newlen);
	
	return ((char*)newsh + sdsHdrSize);
	
}

/*
* hyperloglog的核心函数
*/

/*除去特别明显说明的函数, 比如: is_xxx
* 其他均返回 0 表示正确, 其余表示错误
*/


struct hllhdr 
{
    char magic[4];     
    /* 字符串"HYLL",表示这是一个HLL对象, 因为可以对普通字符串进行操作, 用于校验 */
    uint8_t encoding;   		/* 存储格式 */
    uint8_t notused[3]; 		/* 保留字段,对齐 */
    uint8_t card[8];    		/* 基数的缓存结果 */
    uint8_t registers[]; 		/* Data bytes. 视存储方式的不同有所不同*/
};


#define HLL_INVALIDATE_CACHE(hdr) (hdr)->card[7] |= (1<<7)
#define HLL_VALID_CACHE(hdr) (((hdr)->card[7] & (1<<7)) == 0)
/*card最高位(共64)表明数据是否可用, 如果进行了数据修改,将card[7]第七位设置为1,表示不可用;
如果经过查询后设置为0,下次再取时可用, 桶的计数值发生变化, 并不会立即触发刷新缓存
*/

#define HLL_P 14    										/* 14位作为桶 */
#define HLL_Q (64-HLL_P) 									/* 用来实际计数的位数(50位) */
#define HLL_REGISTERS (1<<HLL_P) 							/* 桶的数量 16384 registers. */
#define HLL_P_MASK (HLL_REGISTERS-1) 						/* 桶的掩码 */
#define HLL_BITS 6 											/* 每个桶需要6位来计数, 2^6 > 50 */
#define HLL_REGISTER_MAX ((1<<HLL_BITS)-1)  				/*63*/
#define HLL_HDR_SIZE sizeof(struct hllhdr)  				/*hllhdr的长度*/
#define HLL_DENSE_SIZE (HLL_HDR_SIZE+((HLL_REGISTERS*HLL_BITS+7)/8))
															/*稠密结构时的总大小，12K*/
#define HLL_DENSE 0 										/* Dense encoding. */
#define HLL_SPARSE 1 										/* Sparse encoding. */
#define HLL_RAW 255 										/* 也是一种编码方式,仅内部使用 */
#define HLL_MAX_ENCODING 1									/* 最大的编码数字 */

static char *invalid_hll_err = "-INVALIDOBJ Corrupted HLL object detected\r\n";

#define HLL_DENSE_GET_REGISTER(target,p,regnum) do { \
    uint8_t *_p = (uint8_t*) p; \
    unsigned long _byte = regnum*HLL_BITS/8; \
    unsigned long _fb = regnum*HLL_BITS&7; \
    unsigned long _fb8 = 8 - _fb; \
    unsigned long b0 = _p[_byte]; \
    unsigned long b1 = _p[_byte+1]; \
    target = ((b0 >> _fb) | (b1 << _fb8)) & HLL_REGISTER_MAX; \
} while(0)

/*Dense编码方式下,取第regnum个桶值*/

#define HLL_DENSE_SET_REGISTER(p,regnum,val) do { \
    uint8_t *_p = (uint8_t*) p; \
    unsigned long _byte = regnum*HLL_BITS/8; \
    unsigned long _fb = regnum*HLL_BITS&7; \
    unsigned long _fb8 = 8 - _fb; \
    unsigned long _v = val; \
    _p[_byte] &= ~(HLL_REGISTER_MAX << _fb); \
    _p[_byte] |= _v << _fb; \
    _p[_byte+1] &= ~(HLL_REGISTER_MAX >> _fb8); \
    _p[_byte+1] |= _v >> _fb8; \
} while(0)
/*Dense编码方式下, 存储第regnum个桶的值 */


/***************************************sparse的一些宏定义***************************/

/* val 	1vvvvvxx 表示连续xx个桶的基数为(vvvvv + 1)
*  zero 00xxxxxx 连续xxxxxx + 1个桶的值为0
*  xzero 01xxxxxx yyyyyyyy	 后(14位+1)表示连续多少个桶位0
*/

#define HLL_SPARSE_XZERO_BIT 0x40   		/* 01xxxxxx xzero类型*/
#define HLL_SPARSE_VAL_BIT 0x80     		/* 1vvvvvxx val类型*/

#define HLL_SPARSE_IS_ZERO(p) (((*(p)) & 0xc0) == 0) 
											/*oxc0=0x11000000 判断是否时zero类型 */
#define HLL_SPARSE_IS_XZERO(p) (((*(p)) & 0xc0) == HLL_SPARSE_XZERO_BIT)
											/*是否时xzero类型*/
#define HLL_SPARSE_IS_VAL(p) ((*(p)) & HLL_SPARSE_VAL_BIT)
											/*判断是否是val类型*/

#define HLL_SPARSE_ZERO_LEN(p) (((*(p)) & 0x3f)+1)
/*00xxxxxx & 0x00111111 获得后6位的值,即zero的长度*/
#define HLL_SPARSE_XZERO_LEN(p) (((((*(p)) & 0x3f) << 8) | (*((p)+1)))+1)
/* 01xxxxxx yyyyyyy 获取xzero的长度  */
#define HLL_SPARSE_VAL_VALUE(p) ((((*(p)) >> 2) & 0x1f)+1)
/*001vvvvv & 值0x00011111 获得中间5位的值即*/
#define HLL_SPARSE_VAL_LEN(p) (((*(p)) & 0x3)+1)
/*获得VAL类型长度*/
#define HLL_SPARSE_VAL_MAX_VALUE 32
/*spase值5bit最大32*/
#define HLL_SPARSE_VAL_MAX_LEN 4
/*长度2bit 最大4*/
#define HLL_SPARSE_ZERO_MAX_LEN 64
/*zero类型6位表示长度, 64*/
#define HLL_SPARSE_XZERO_MAX_LEN 16384
/*xzero类型14bit, 最大16384*/
#define HLL_SPARSE_VAL_SET(p,val,len) do { \
    *(p) = (((val)-1)<<2|((len)-1))|HLL_SPARSE_VAL_BIT; \
} while(0)
/*sparse编码 设置val的值, 同时传入len和val*/

#define HLL_SPARSE_ZERO_SET(p,len) do { \
    *(p) = (len)-1; \
} while(0)
/*设置zero的长度, 由于是再后6位, 头两位为0*/
#define HLL_SPARSE_XZERO_SET(p,len) do { \
    int _l = (len)-1; \
    *(p) = (_l>>8) | HLL_SPARSE_XZERO_BIT; \
    *((p)+1) = (_l&0xff); \
} while(0)
/*sparse编码设置xzero的长度*/

#define HLL_ALPHA_INF 0.721347520444481703680 /* constant for 0.5/ln(2)  用来修正的一个宏 */



/*hash算法*/
uint64_t MurmurHash64A (const void * key, int len, unsigned int seed)
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;
    uint64_t h = seed ^ (len * m);
    const uint8_t *data = (const uint8_t *)key;
    const uint8_t *end = data + (len-(len&7));

    while(data != end) {
        uint64_t k;
	
		if(byte_order() == _Little_endian)
		{
			k = *((uint64_t*)data);
		}
		else
		{
			k = (uint64_t) data[0];
			k |= (uint64_t) data[1] << 8;
			k |= (uint64_t) data[2] << 16;
			k |= (uint64_t) data[3] << 24;
			k |= (uint64_t) data[4] << 32;
			k |= (uint64_t) data[5] << 40;
			k |= (uint64_t) data[6] << 48;
			k |= (uint64_t) data[7] << 56;
		}
		
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
        data += 8;
    }

    switch(len & 7) {
    case 7: h ^= (uint64_t)data[6] << 48; /* fall-thru */
    case 6: h ^= (uint64_t)data[5] << 40; /* fall-thru */
    case 5: h ^= (uint64_t)data[4] << 32; /* fall-thru */
    case 4: h ^= (uint64_t)data[3] << 24; /* fall-thru */
    case 3: h ^= (uint64_t)data[2] << 16; /* fall-thru */
    case 2: h ^= (uint64_t)data[1] << 8; /* fall-thru */
    case 1: h ^= (uint64_t)data[0];
            h *= m; /* fall-thru */
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

/*根据hash算法,计算出index(桶的下标), count(后50的计数值)*/
int hllPatLen(const unsigned char *ele, size_t elesize, long *regp)
{
    uint64_t hash, bit, index;
    int count;

    hash = MurmurHash64A(ele,elesize,0xadc83b19ULL);
    index = hash & HLL_P_MASK; 		/* Register index. */
    hash >>= HLL_P; 				/* Remove bits used to address the register. */
    hash |= ((uint64_t)1<<HLL_Q); 	/* Make sure the loop terminates and count will be <= Q+1. */
    bit = 1;
    count = 1; 						/* Initialized to 1 since we count the "00000...1" pattern. */
    while((hash & bit) == 0) {
        count++;
        bit <<= 1;
    }
    *regp = (int) index;
    return count;
}

/*对于dense方式, 只更新更大的数值*/
int hllDenseSet(uint8_t *registers, long index, uint8_t count) 
{
    uint8_t oldcount;

    HLL_DENSE_GET_REGISTER(oldcount,registers,index);
    if (count > oldcount)
	{
        HLL_DENSE_SET_REGISTER(registers,index,count);
        return 1;
    } 
	else 
	{
        return 0;
    }
}

/*往data数据中放入一个新的值*/
int hllDenseAdd(uint8_t *registers, const unsigned char *ele, size_t elesize)
{
    long index;
    uint8_t count = hllPatLen(ele,elesize,&index);
    return hllDenseSet(registers,index,count);
}


/*统计基数值的函数,传入reghisto, 每个位置存储index对应计数出现的次数*/
void hllDenseRegHisto(uint8_t *registers, int* reghisto)
{
    int j;

    if (HLL_REGISTERS == 16384 && HLL_BITS == 6)
	{
        uint8_t *r = registers;
        unsigned long r0, r1, r2, r3, r4, r5, r6, r7, r8, r9,
                      r10, r11, r12, r13, r14, r15;
        for (j = 0; j < 1024; j++)
		{
            r0 = r[0] & 63;
            r1 = (r[0] >> 6 | r[1] << 2) & 63;
            r2 = (r[1] >> 4 | r[2] << 4) & 63;
            r3 = (r[2] >> 2) & 63;
            r4 = r[3] & 63;
            r5 = (r[3] >> 6 | r[4] << 2) & 63;
            r6 = (r[4] >> 4 | r[5] << 4) & 63;
            r7 = (r[5] >> 2) & 63;
            r8 = r[6] & 63;
            r9 = (r[6] >> 6 | r[7] << 2) & 63;
            r10 = (r[7] >> 4 | r[8] << 4) & 63;
            r11 = (r[8] >> 2) & 63;
            r12 = r[9] & 63;
            r13 = (r[9] >> 6 | r[10] << 2) & 63;
            r14 = (r[10] >> 4 | r[11] << 4) & 63;
            r15 = (r[11] >> 2) & 63;

            reghisto[r0]++;
            reghisto[r1]++;
            reghisto[r2]++;
            reghisto[r3]++;
            reghisto[r4]++;
            reghisto[r5]++;
            reghisto[r6]++;
            reghisto[r7]++;
            reghisto[r8]++;
            reghisto[r9]++;
            reghisto[r10]++;
            reghisto[r11]++;
            reghisto[r12]++;
            reghisto[r13]++;
            reghisto[r14]++;
            reghisto[r15]++;

            r += 12;
        }
		/*对于我们自定义的分桶方法, 采用这种方法更高效*/
    }
	else
	{
        for(j = 0; j < HLL_REGISTERS; j++) {
            unsigned long reg;
            HLL_DENSE_GET_REGISTER(reg,registers,j);
            reghisto[reg]++;
        }
    }
}


/*进行编码转换*/
hllhdr* hllSparseToDense(hllhdr *ptr)
{
	if(!ptr)
	{
		return NULL;
	}
	
    sds sparse = (sds)ptr;
    hllhdr *hdr = NULL;
    int idx = 0, runlen, regval;
    uint8_t *p = (uint8_t*)sparse, *end = p + sdslen(sparse);

    if (ptr->encoding == HLL_DENSE) return ptr;

    hdr = (hllhdr*)sdsnewlen(NULL,HLL_DENSE_SIZE);
	/*dense方式占用的空间是固定的*/
    *hdr = *ptr; 				/*结构体赋值*/
    hdr->encoding = HLL_DENSE;

    p += HLL_HDR_SIZE;
    while(p < end)
	{
        if (HLL_SPARSE_IS_ZERO(p))
		{
            runlen = HLL_SPARSE_ZERO_LEN(p);
            idx += runlen;
            p++;
        }
		else if (HLL_SPARSE_IS_XZERO(p))
		{
            runlen = HLL_SPARSE_XZERO_LEN(p);
            idx += runlen;
            p += 2;
        }
		/*对于xzero和zero直接就是0, 不需要对data进行操作*/
		else
		{
            runlen = HLL_SPARSE_VAL_LEN(p);
            regval = HLL_SPARSE_VAL_VALUE(p);
            if ((runlen + idx) > HLL_REGISTERS) break; /* Overflow. */
            while(runlen--)
			{
                HLL_DENSE_SET_REGISTER(hdr->registers, idx, regval);
                idx++;
            }
            p++;
        }
    }
    if (idx != HLL_REGISTERS)
	{
        sdsfree(hdr);
        return NULL;
    }
	
    sdsfree(ptr);
    return hdr;
}

/* 对于sparse编码方式赋值, 当数据超过3000或者一个VAL超过32 
*  由于会进行扩展空间, 所以需要传入指向指针的指针
*  
*/
int hllSparseSet(hllhdr **ptr, long index, uint8_t count) 
{
	hllhdr *hdr = *ptr;
    uint8_t oldcount, *sparse, *end, *p, *prev, *next;
    long first, span;
    long is_zero = 0, is_xzero = 0, is_val = 0, runlen = 0;

    if (count > HLL_SPARSE_VAL_MAX_VALUE) goto promote;

	hdr = (hllhdr*)sdsMakeRoomFor(hdr, 3);
	/* 一次最多可能扩展3个字节, 先一步分配空间 */
	if(!hdr)
	{
		xprintf("memory rellac error: %s!", sterrror(errno));
		return -1;
	}

	
	/* 第一步需要找到将要更改的那个数据(zero xzero val) */
    sparse = p = ((uint8_t*)hdr) + HLL_HDR_SIZE;
    end = p + sdslen(hdr) - HLL_HDR_SIZE;

    first = 0;
    prev = NULL; 
    next = NULL; 
    span = 0;
    while(p < end)
	{
        long oplen;

        oplen = 1;
        if (HLL_SPARSE_IS_ZERO(p))
		{
            span = HLL_SPARSE_ZERO_LEN(p);
        } 
		else if (HLL_SPARSE_IS_VAL(p)) 
		{
            span = HLL_SPARSE_VAL_LEN(p);
        } 
		else
		{ 
            span = HLL_SPARSE_XZERO_LEN(p);
            oplen = 2;
        }

        if (index <= first + span - 1) break;
        prev = p;
        p += oplen;
        first += span;
    }
    if (span == 0) return -1; 

    next = HLL_SPARSE_IS_XZERO(p) ? (p + 2) : (p + 1);
    if (next >= end) next = NULL;

    if (HLL_SPARSE_IS_ZERO(p)) 
	{
        is_zero = 1;
        runlen = HLL_SPARSE_ZERO_LEN(p);
    }
	else if (HLL_SPARSE_IS_XZERO(p))
	{
        is_xzero = 1;
        runlen = HLL_SPARSE_XZERO_LEN(p);
    }
	else
	{
        is_val = 1;
        runlen = HLL_SPARSE_VAL_LEN(p);
    }

	/*first和p指向的是将要更改的前一个数据,*/

	/*先处理几种简单的情况*/
    if (is_val)
	{
        oldcount = HLL_SPARSE_VAL_VALUE(p);
        if (oldcount >= count) return 0;

        if (runlen == 1)
		{
            HLL_SPARSE_VAL_SET(p,count,1);
            goto updated;
        }
    }
	/*对于VAL数据, 新的计数值更小, 直接返回
	* 如果长度为1, 表示之前是没有值, 直接设置值更新数据即可
	*/

	
    if (is_zero && runlen == 1)
	{
        HLL_SPARSE_VAL_SET(p,count,1);
        goto updated;
    }


	/*更可能的情况,需要拆分移动数据*/
    uint8_t seq[5], *n = seq;
    int last = first+span-1; 
    int len;

    if (is_zero || is_xzero)
	{
		/*对于XZERO和ZERO,最大可能XZERO-VAL-XZERO*/
        if (index != first)
		{
            len = index-first;
            if (len > HLL_SPARSE_ZERO_MAX_LEN)
			{
                HLL_SPARSE_XZERO_SET(n,len);
                n += 2;
            }
			else
			{
                HLL_SPARSE_ZERO_SET(n,len);
                n++;
            }
        }
        HLL_SPARSE_VAL_SET(n,count,1);
        n++;
        if (index != last)
		{
            len = last-index;
            if (len > HLL_SPARSE_ZERO_MAX_LEN)
			{
                HLL_SPARSE_XZERO_SET(n,len);
                n += 2;
            }
			else
			{
                HLL_SPARSE_ZERO_SET(n,len);
                n++;
            }
        }
    }
	else
	{
		/*对于一个VAL, 可能拆分为最大VAL-VAL-VAL*/
        int curval = HLL_SPARSE_VAL_VALUE(p);

        if (index != first)
		{
            len = index-first;
            HLL_SPARSE_VAL_SET(n,curval,len);
            n++;
        }
        HLL_SPARSE_VAL_SET(n, count, 1);
        n++;
        if (index != last)
		{
            len = last-index;
            HLL_SPARSE_VAL_SET(n,curval,len);
            n++;
        }
    }

	/* 修改数据,移动内容 */
     int seqlen = n-seq;
     int oldlen = is_xzero ? 2 : 1;
     int deltalen = seqlen-oldlen;

     if (deltalen > 0 &&
         sdslen(hdr)+deltalen > hll_sparse_max_bytes) goto promote;
     if (deltalen && next) memmove(next+deltalen,next,end-next);
     sdsIncrLen(hdr,deltalen);
     memcpy(p, seq, seqlen);
     end += deltalen;

updated:
    /*在更新后,有可能左右可以合并*/
    p = prev ? prev : sparse;
    int scanlen = 5; /* Scan up to 5 upcodes starting from prev. */
    while (p < end && scanlen--)
	{
        if (HLL_SPARSE_IS_XZERO(p))
		{
            p += 2;
            continue;
        } 
		else if (HLL_SPARSE_IS_ZERO(p))
		{
            p++;
            continue;
        }
        if (p + 1 < end && HLL_SPARSE_IS_VAL(p+1))
		{
            int v1 = HLL_SPARSE_VAL_VALUE(p);
            int v2 = HLL_SPARSE_VAL_VALUE(p+1);
            if (v1 == v2)
			{
                int len = HLL_SPARSE_VAL_LEN(p)+HLL_SPARSE_VAL_LEN(p+1);
                if (len <= HLL_SPARSE_VAL_MAX_LEN)
				{
                    HLL_SPARSE_VAL_SET(p+1,v1,len);
                    memmove(p, p+1, end-p);
                    sdsIncrLen(hdr, -1);
                    end--;
                    continue;
                }
            }
        }
        p++;
    }

    HLL_INVALIDATE_CACHE(hdr);
	*ptr = hdr;
    return 1;

promote: 
    if((hdr = hllSparseToDense(hdr)) == NULL) return -1; /* 更新编码 */

    int dense_retval = hllDenseSet(hdr->registers, index, count);
    assert(dense_retval == 1);
	*ptr = hdr;
    return dense_retval;
}


int hllSparseAdd(hllhdr **ptr, const unsigned char *ele, size_t elesize)
{
    long index;
    uint8_t count = hllPatLen(ele, elesize, &index);
    return hllSparseSet(ptr,index,count);
}

/*sparse方式count*/
void hllSparseRegHisto(uint8_t *sparse, int sparselen, int *invalid, int* reghisto)
{
    int idx = 0, runlen, regval;
    uint8_t *end = sparse+sparselen, *p = sparse;

    while(p < end)
	{
        if (HLL_SPARSE_IS_ZERO(p))
		{
            runlen = HLL_SPARSE_ZERO_LEN(p);
            idx += runlen;
            reghisto[0] += runlen;
            p++;
        }
		else if (HLL_SPARSE_IS_XZERO(p))
		{
            runlen = HLL_SPARSE_XZERO_LEN(p);
            idx += runlen;
            reghisto[0] += runlen;
            p += 2;
        }
		else
		{
            runlen = HLL_SPARSE_VAL_LEN(p);
            regval = HLL_SPARSE_VAL_VALUE(p);
            idx += runlen;
            reghisto[regval] += runlen;
            p++;
        }
    }
    if (idx != HLL_REGISTERS && invalid) *invalid = 1;
}

/*Raw编码方式获取count*/
void hllRawRegHisto(uint8_t *registers, int* reghisto)
{
    uint64_t *word = (uint64_t*) registers;
    uint8_t *bytes;
    int j;

    for (j = 0; j < HLL_REGISTERS/8; j++)
	{
        if (*word == 0)
		{
            reghisto[0] += 8;
        }
		else
		{
            bytes = (uint8_t*) word;
            reghisto[bytes[0]]++;
            reghisto[bytes[1]]++;
            reghisto[bytes[2]]++;
            reghisto[bytes[3]]++;
            reghisto[bytes[4]]++;
            reghisto[bytes[5]]++;
            reghisto[bytes[6]]++;
            reghisto[bytes[7]]++;
        }
        word++;
    }
}

/* Helper function sigma as defined in
 * "New cardinality estimation algorithms for HyperLogLog sketches"
 * Otmar Ertl, arXiv:1702.01284 */
double hllSigma(double x)
{
    if (x == 1.) return INFINITY;
    double zPrime;
    double y = 1;
    double z = x;
    do
	{
        x *= x;
        zPrime = z;
        z += x * y;
        y += y;
    } while(zPrime != z);
    return z;
}

/* Helper function tau as defined in
 * "New cardinality estimation algorithms for HyperLogLog sketches"
 * Otmar Ertl, arXiv:1702.01284 */
double hllTau(double x)
{
    if (x == 0. || x == 1.) return 0.;
    double zPrime;
    double y = 1.0;
    double z = 1 - x;
    do
	{
        x = sqrt(x);
        zPrime = z;
        y *= 0.5;
        z -= pow(1 - x, 2)*y;
    } while(zPrime != z);
    return z / 3;
}


/*hllhdr 获取count值*/
uint64_t hllCount(struct hllhdr *hdr, int *invalid) {
    double m = HLL_REGISTERS;
    double E;
    int j;
    /* 最大是 HLL_Q + 1*/
    int reghisto[64] = {0};

	/*根据编码方式进行编码*/
    if (hdr->encoding == HLL_DENSE)
	{
        hllDenseRegHisto(hdr->registers,reghisto);
    }
	else if (hdr->encoding == HLL_SPARSE)
	{
        hllSparseRegHisto(hdr->registers,
                         sdslen((sds)hdr)-HLL_HDR_SIZE,invalid,reghisto);
    }
	else if (hdr->encoding == HLL_RAW)
	{
        hllRawRegHisto(hdr->registers,reghisto);
    }
	else
	{
        *invalid = 1;
		return 0;
    }

    /* Estimate cardinality form register histogram. See:
     * "New cardinality estimation algorithms for HyperLogLog sketches"
     * Otmar Ertl, arXiv:1702.01284 */
    double z = m * hllTau((m-reghisto[HLL_Q+1])/(double)m);
    for (j = HLL_Q; j >= 1; --j) {
        z += reghisto[j];
        z *= 0.5;
    }
    z += m * hllSigma(reghisto[0]/(double)m);
    E = llroundl(HLL_ALPHA_INF*m*m/z);
	
	/*进行计算和修正, 在这里无法说明*/
	
    return (uint64_t) E;
}

/*根据编码方式进行添加
* @return 1 表示进行了修改
* @return 0 表示未进行修改
* @return -1 表示发生错误
*/
int hllAdd(hllhdr **ptr, const unsigned char *ele, size_t elesize)
{
    switch((*ptr)->encoding)
	{
    case HLL_DENSE: 
		{
			if(hllDenseAdd((*ptr)->registers,ele,elesize))
			{
				HLL_INVALIDATE_CACHE(*ptr);
				return 1;
			}
			return 0;
		}
	case HLL_SPARSE: 
		{
			return hllSparseAdd(ptr,ele,elesize);
		}
	default: return -1; /* Invalid representation. */
    }
}


/*这里的max就是一个简单的数组, 不需要考虑什么编码*/
int hllMerge(uint8_t *max, hllhdr *ptr)
{
    int i;
	
    if (ptr->encoding == HLL_DENSE)
	{
        uint8_t val;

        for (i = 0; i < HLL_REGISTERS; i++)
		{
            HLL_DENSE_GET_REGISTER(val, ptr->registers,i);
            if (val > max[i]) max[i] = val;
        }
    }
	else
	{
        uint8_t *p = (uint8_t*)ptr, *end = p + sdslen(ptr);
        long runlen, regval;

        p += HLL_HDR_SIZE;
        i = 0;
        while(p < end)
		{
            if (HLL_SPARSE_IS_ZERO(p))
			{
                runlen = HLL_SPARSE_ZERO_LEN(p);
                i += runlen;
                p++;
            } else if (HLL_SPARSE_IS_XZERO(p)) {
                runlen = HLL_SPARSE_XZERO_LEN(p);
                i += runlen;
                p += 2;
            } else {
                runlen = HLL_SPARSE_VAL_LEN(p);
                regval = HLL_SPARSE_VAL_VALUE(p);
                if ((runlen + i) > HLL_REGISTERS) break; /* Overflow. */
                while(runlen--) {
                    if (regval > max[i]) max[i] = regval;
                    i++;
                }
                p++;
            }
        }
        if (i != HLL_REGISTERS) return -1;
    }
    return 0;
}


/*
* 对外部提供的接口
*/

void deleteHLLObject(hllhdr* hdr)
{
	sdsfree(hdr);
}

/*创建结构体*/
hllhdr *createHLLObject(void)
{
    struct hllhdr *hdr;
    sds s;
    uint8_t *p;
    int sparselen = HLL_HDR_SIZE +
                    (((HLL_REGISTERS+(HLL_SPARSE_XZERO_MAX_LEN-1)) /
                     HLL_SPARSE_XZERO_MAX_LEN)*2);
	/* HLL_SPARSE_XZERO_MAX_LEN 个值需要2字节, 哪怕超过1个也需要2字节 */
					 
    int aux;

    aux = HLL_REGISTERS;
    s = sdsnewlen(NULL,sparselen);
	hdr = (hllhdr*)s;
    p = (uint8_t*)s + HLL_HDR_SIZE;
    while(aux)
	{
        int xzero = HLL_SPARSE_XZERO_MAX_LEN;
        if (xzero > aux) xzero = aux;
        HLL_SPARSE_XZERO_SET(p,xzero);
        p += 2;
        aux -= xzero;
    }
    assert((p-(uint8_t*)s) == sparselen);


    memcpy(hdr->magic, hll_magic_char, 4);
    hdr->encoding = HLL_SPARSE;
    return hdr;
}

/* 确认字符串是否是有效的hll字符串,还记得那个HYLL魔数吗 */
int isHLLObjectOrReply(void *ptr) {
    struct hllhdr *hdr;

    hdr = ptr;

    if (hdr->magic[0] != 'H' || hdr->magic[1] != 'Y' ||
        hdr->magic[2] != 'L' || hdr->magic[3] != 'L') goto invalid;

    if (hdr->encoding > HLL_MAX_ENCODING) goto invalid;

    if (hdr->encoding == HLL_DENSE &&
        sdslen(ptr) != HLL_DENSE_SIZE) goto invalid;

    return 0;

invalid:
    return -1;
}

/*add命令*/
int pfaddCommand(hllhdr **ptr, const unsigned char *key, int len)
{
	if(!ptr || !*ptr || !key)
	{
		return -1;
	}
	
	len = (len > 0) ? len : strlen(key);
	return hllAdd(ptr, key, len);
		
}


/* 统计基数的命令接口 */
int pfcountCommand(hllhdr *hdr, uint64_t *ret)
{
	uint64_t card;
	if (HLL_VALID_CACHE(hdr))
	{
        card = (uint64_t)hdr->card[0];
        card |= (uint64_t)hdr->card[1] << 8;
        card |= (uint64_t)hdr->card[2] << 16;
        card |= (uint64_t)hdr->card[3] << 24;
        card |= (uint64_t)hdr->card[4] << 32;
        card |= (uint64_t)hdr->card[5] << 40;
		card |= (uint64_t)hdr->card[6] << 48;
        card |= (uint64_t)hdr->card[7] << 56;
    }
	else {
        int invalid = 0;
		card = hllCount(hdr,&invalid);
        if (invalid)
		{
			return -1;
        }
        hdr->card[0] = card & 0xff;
        hdr->card[1] = (card >> 8) & 0xff;
        hdr->card[2] = (card >> 16) & 0xff;
        hdr->card[3] = (card >> 24) & 0xff;
        hdr->card[4] = (card >> 32) & 0xff;
        hdr->card[5] = (card >> 40) & 0xff;
        hdr->card[6] = (card >> 48) & 0xff;
        hdr->card[7] = (card >> 56) & 0xff;
    }
	
	*ret = card;
	return 0;
}


/* 合并两个hllhdr结构, 合并多个的情况应该很少见 */
int pfmergeCommand(hllhdr **hdr, hllhdr *src)
{
    uint8_t max[HLL_REGISTERS];
    int j;
    int use_dense = 0; 

	if(!hdr || !*hdr || !src)
	{
		return -1;
	}

    memset(max,0,sizeof(max));
	
	if ((*hdr)->encoding == HLL_DENSE) use_dense = 1;
	
	if(hllMerge(max, src))
	{
		return -1;
	}
	
	for(j = 0; j < HLL_REGISTERS; j++)
	{
		if(use_dense)
		{
			hllDenseSet((*hdr)->registers, j, max[j]);
		}
		else
		{
			hllSparseSet(hdr, j, max[j]);
		}
	}
	
	HLL_INVALIDATE_CACHE(*hdr);
	return 0;
}




