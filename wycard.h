/*	This is free and unencumbered software released into the public domain under The Unlicense (http://unlicense.org/)
	main repo: https://github.com/wangyi-fudan/wycard
	author: 王一 Wang Yi <godspeed_china@yeah.net>

	This cardinality estimation is based on multilayer Bloom filter	
	The more layers the more capacity. The less layers the more accuracy.
	With <=12 layers, wycard is more accurate than Redis HyperLogLog, which means the cardinality < 3072*bytes.

	simple example:
	wycard s;	//	define the structure
	wycard_alloc(&s,10,12);	//	allocate 12 layers, each layer is 2^10 bytes
	wycard_add(&s, item, item_len);	//	insert item
	double card=wycard_cardinality(&s);	//	estimate the cardinality
	wycard_free(&s);	//	free the data
*/
#ifndef	wycard_version_0
#define	wycard_version_0
#include	"wyhash.h"
#include	<stdlib.h>
#include	<math.h>
typedef	struct	wycard_t{	uint32_t	bytes;	uint8_t	*data,	layers;}	wycard;

static	inline	uint64_t	wycard_alloc(wycard	*s,	uint32_t	bytes,	uint64_t	capacity){
	for(s->layers=1;	s->layers<=32;	s->layers++){
		s->bytes=bytes/s->layers;
		if((uint64_t)s->bytes<<(s->layers+2)>=capacity)	break;
	}
	if(s->layers>32)	return	0;
	s->data=(uint8_t*)calloc((uint64_t)s->layers*s->bytes,1);
	if(s->data==NULL)	return	0;
	return	(uint64_t)s->bytes<<(s->layers+2);
}

static	inline	void	wycard_free(wycard	*s){	free(s->data);	}

static	inline	void	wycard_clear(wycard	*s){	memset(s->data,0,(uint64_t)s->layers*s->bytes);	}

static	inline	void	wycard_add(wycard	*s,	void	*item,	uint64_t	item_size){
	uint64_t	h=wyhash(item,item_size,0,_wyp);
	uint64_t	lz=__builtin_clzll(h);
	if(lz>=s->layers)	lz=s->layers-1;
	h=(((unsigned)h)*(8ull*s->bytes))>>32;
	s->data[lz*s->bytes+(h>>3)]|=1u<<(h&7);
}
static	inline	uint64_t	wycard_solve(wycard	*s,	uint64_t	*m){
	double	n=8ull*s->bytes,	p[64]={},	N=logf((uint64_t)s->bytes<<(s->layers+2));
	for(uint8_t	l=0;	l<s->layers;	l++)	p[l]=l==s->layers-1?1.0/n/(1ull<<l):0.5/n/(1ull<<l);
	for(size_t	it=0;	it<256;	it++){	
		double	dn=0,	dnn=0,	en=exp(N);
		for(uint8_t	l=0;	l<s->layers;	l++){	
			double	ep=en*p[l],	enp=exp(-ep);
			dn+=-ep*(n-m[l])+m[l]*ep*enp/(1-enp);
			dnn+=-ep/(1-enp)/(1-enp)*(n*enp*enp+(m[l]-2*n+ep*m[l])*enp+n-m[l]);
		}
		N-=dn/dnn;
		if(fabs(dn/dnn)<1e-6)	break;
	}
	return	exp(N)+0.5;
}

static	inline	uint64_t	wycard_cardinality(wycard	*s){
	uint64_t	m[64]={};	uint8_t	empty=1,	full=1;
	for(uint64_t	l=0;	l<s->layers;	l++){
		uint64_t	sum=0;	uint8_t	*p=s->data+l*s->bytes;
		for (uint64_t	i=0;	i<s->bytes;	i++)	sum+=__builtin_popcount(p[i]);
		m[l]=sum;	
		if(m[l]!=8ull*s->bytes)	full=0;
		if(m[l])	empty=0;
	}
	return	empty?0:(full?~0ull:wycard_solve(s,	m));
}

static	inline	uint64_t	wycard_union_cardinality(wycard	*a,	wycard	*b){
	if(a->bytes!=b->bytes||a->layers!=b->layers)	return	~0ull;
	uint64_t	m[64]={};	uint8_t	empty=1,	full=1;
	for(uint64_t	l=0;	l<a->layers;	l++){
		uint64_t	sum=0;	uint8_t	*p=a->data+l*a->bytes,	*q=b->data+l*b->bytes;
		for (uint64_t	i=0;	i<a->bytes;	i++)	sum+=__builtin_popcount(p[i]|q[i]);
		m[l]=sum;	
		if(m[l]!=8ull*a->bytes)	full=0;
		if(m[l])	empty=0;
	}
	return	empty?0:(full?~0ull:wycard_solve(a,	m));
}
#endif
