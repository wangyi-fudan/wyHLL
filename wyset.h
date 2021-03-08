//	author: 王一 Wang Yi <godspeed_china@yeah.net>
#ifndef	wyset_version_0
#define	wyset_version_0
#include	"wyhash.h"
#include	<math.h>
#include	<iostream>
using	namespace	std;
struct	wyset{	uint64_t	*data;	uint32_t	size,	level,	ones;	};
static	inline	void	wyset_clear(wyset	*s){	memset(s->data,0,s->size<<3);	s->level=s->ones=0;	}
static	inline	void	wyset_alloc(wyset	*s,	uint32_t	size){	s->size=size>>3;	s->data=(uint64_t*)malloc(s->size<<3);	wyset_clear(s);	}
static	inline	void	wyset_free(wyset	*s){	free(s->data);	}
static	inline	uint32_t	wyset_ones(wyset	*s){	uint32_t	m=0;	for(uint32_t	i=0;	i<s->size;	i++)	m+=__builtin_popcountll(s->data[i]);	return	m;	}
static	inline	void	wyset_upscale(wyset	*s,	uint32_t	level){
	unsigned	m=(1-pow((double)((s->size<<6)-s->ones)/(s->size<<6),	1.0/(1ull<<(level-s->level))))*(s->size<<6);
	uint64_t	rng=wyhash(s->data,s->size<<3,s->level,_wyp);
	while(s->ones>m){
		uint32_t	b=wy2u0k(wyrand(&rng),(s->size<<6));
		if(s->data[b>>6]&(1ull<<(b&63))){	s->data[b>>6]&=~(1ull<<(b&63));	s->ones--;	}
	}
	s->level=level;
}
static	inline	void	wyset_add(wyset	*s,	void	*item,	uint64_t	item_size){
	uint64_t	h=wyhash(item,item_size,0,_wyp);
	if((uint32_t)h>=(1ull<<(32-s->level)))	return;	//	randomly block insertions according to level
	uint32_t	b=((h>>32)*(s->size<<6))>>32;
	if(s->data[b>>6]&(1ull<<(b&63)))	return;	//	already set
	s->ones++;	s->data[b>>6]|=1ull<<(b&63);
	if(s->ones>=3*(s->size<<4))	wyset_upscale(s,	s->level+1);
}
static	inline	double	wyset_estimator(wyset	*s){
	double	m=wyset_ones(s),	n=s->size<<6;
	return	log((double)(n-m)/n)/log(1-1.0/n)*(1ull<<s->level);
}
static	inline	bool	wyset_copy(wyset	*des,	wyset	*src){
	if(des->size!=src->size)	return	false;
	des->level=src->level;	des->ones=src->ones;
	memcpy(des->data,src->data,src->size<<3);
	return	true;
}
static	inline	void	wyset_union(wyset	*a,	wyset	*b,	wyset	*out){
	if(a->level>b->level){
		wyset_copy(out,b);	wyset_upscale(out,a->level);
		for(uint32_t	i=0;	i<out->size;	i++)	out->data[i]|=a->data[i];
	}
	else{
		wyset_copy(out,a);	wyset_upscale(out,b->level);
		for(uint32_t	i=0;	i<out->size;	i++)	out->data[i]|=b->data[i];
	}
}
#endif
