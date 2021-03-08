/*	This is free and unencumbered software released into the public domain under The Unlicense (http://unlicense.org/)
	main repo: https://github.com/wangyi-fudan/wycard
	author: 王一 Wang Yi <godspeed_china@yeah.net>
	This cardinality estimation is based on a Bloom filter that upscales automaticly
	
	simple example:
	wycard s;
	wycard_alloc(&s,12<<10);
	wycard_add(&s, item, item_len);
	double card=wycard_cardinality(&s);
	wycard_free(&s);
*/
#ifndef	wycard_version_0
#define	wycard_version_0
#include	"wyhash.h"
#include	<math.h>
//	main data structure
struct	wycard{	
	uint64_t	*data;	//	64 bit block
	uint64_t	blocks;	//	number of 64 bit blocks
	uint64_t	level;	//	down sampling rate=2^-level
	uint64_t	ones;	//	number of "on" bits
};
//	clear the registers
static	inline	void	wycard_clear(wycard	*s){	
	memset(s->data,0,s->blocks<<3);	
	s->level=s->ones=0;	
}
//	actual memory size=(bytes>>3)<<3
static	inline	bool	wycard_alloc(wycard	*s,	uint64_t	bytes){
	if(bytes<8)	return	false;
	s->blocks=bytes>>3;	
	s->data=(uint64_t*)malloc(s->blocks<<3);
	if(s->data==NULL)	return	false;
	wycard_clear(s);
	return	true;
}
//	free the data
static	inline	void	wycard_free(wycard	*s){	
	free(s->data);	
}
//	copy the data from src to des
static	inline	bool	wycard_copy(wycard	*des,	wycard	*src){
	if(des->blocks!=src->blocks)	return	false;
	memcpy(des->data,src->data,src->blocks<<3);
	des->level=src->level;	
	des->ones=src->ones;
	return	true;
}
//	count the number of ones in data
static	inline	uint64_t	wycard_ones(wycard	*s){	
	uint64_t	m=0;	
	for(uint64_t	i=0;	i<s->blocks;	i++){
#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
		m+=__builtin_popcountll(s->data[i]);	
#elif defined(_MSC_VER) && defined(_M_X64)
		m+=_mm_popcnt_u64(s->data[i]);
#else
		uint64_t	x=s->data[i];
		x-=(x>>1)&0x5555555555555555;
		x=(x&0x3333333333333333)+((x>>2)&0x3333333333333333);
		x=(x+(x>>4))&0x0f0f0f0f0f0f0f0f;
		x=(x*0x0101010101010101)>>56;
		m+=x;
#endif
	}
	return	m;	
}
//	upscale the data to level
static	inline	void	wycard_upscale(wycard	*s,	uint64_t	level){
	if(level<=s->level)	return;
	unsigned	m=(1-pow((double)((s->blocks<<6)-s->ones)/(s->blocks<<6), 1.0/(1ull<<(level-s->level))))*(s->blocks<<6);
	uint64_t	rng=wyhash(s->data,s->blocks<<3,level,_wyp);
	while(s->ones>m){
		uint64_t	b=wy2u0k(wyrand(&rng),(s->blocks<<6));
		if(s->data[b>>6]&(1ull<<(b&63))){	
			s->data[b>>6]&=~(1ull<<(b&63));	
			s->ones--;	
		}
	}
	s->level=level;
}
//	add an element
static	inline	void	wycard_add(wycard	*s,	void	*item,	uint64_t	item_blocks){
	uint64_t	h=wyhash(item,item_blocks,0,_wyp);
	if((uint32_t)h>=(1ull<<(32-s->level)))	return;	//	randomly block insertions according to level
	uint64_t	b=((h>>32)*(s->blocks<<6))>>32;	//	choose a bit to update
	if(s->data[b>>6]&(1ull<<(b&63)))	return;	//	already set
	s->data[b>>6]|=1ull<<(b&63);	//	update the bit
	s->ones++;	//	update the ones counter
	if(s->ones>=3*(s->blocks<<4))	wycard_upscale(s,	s->level+1);	//	upscale if 3/4 bits are set
}
//	estimate the cardinality
static	inline	double	wycard_cardinality(wycard	*s){
	double	m=wycard_ones(s),	n=s->blocks<<6;
	return	log((double)(n-m)/n)/log(1-1.0/n)*(1ull<<s->level);
}

static	inline	void	wycard_union(wycard	*a,	wycard	*b,	wycard	*out){
	if(a->level>b->level){
		wycard_copy(out,b);	
		wycard_upscale(out,a->level);
		for(uint64_t	i=0;	i<out->blocks;	i++)	out->data[i]|=a->data[i];
	}
	else	if(a->level<b->level){
		wycard_copy(out,a);	
		wycard_upscale(out,b->level);
		for(uint64_t	i=0;	i<out->blocks;	i++)	out->data[i]|=b->data[i];
	}
	else{
		for(uint64_t	i=0;	i<out->blocks;	i++)	out->data[i]=a->data[i]|b->data[i];
	}
}
#endif

/* The Unlisence
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/
