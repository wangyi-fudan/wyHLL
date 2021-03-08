/*	This is free and unencumbered software released into the public domain under The Unlicense (http://unlicense.org/)
	main repo: https://github.com/wangyi-fudan/wycard
	author: 王一 Wang Yi <godspeed_china@yeah.net>
	This cardinality estimation is based on multilayer Bloom filter
	With <=12 layers, wycard is more accurate than Redis HyperLogLog.
	wycard is the choice when number of items < 2048*bytes.

	simple example:
	wycard s;	//	define the structure
	wycard_alloc(&s,11,6);	//	allocate 6 layers, each layer is 2^11 bytes
	wycard_add(&s, item, item_len);	//	insert item
	double card=wycard_cardinality(&s);	//	estimate the cardinality
	wycard_free(&s);	//	free the data
*/
#ifndef	wycard_version_0
#define	wycard_version_0
#include	"wyhash.h"
#include	<math.h>
struct	wycard{	uint8_t	*data,	bits,	layers;	};

static	inline	void	wycard_alloc(wycard	*s,	uint8_t	bits,	uint8_t	layers){	
	s->bits=bits;	
	s->layers=layers;	
	s->data=(uint8_t*)calloc(((uint64_t)layers)<<bits,1);	
}

static	inline	void	wycard_free(wycard	*s){	free(s->data);	}

static	inline	uint64_t	wycard_capacity(wycard	*s){	return	1ull<<(s->bits+s->layers+3);	}

static	inline	void	wycard_clear(wycard	*s){	memset(s->data,0,((uint64_t)s->layers)<<s->bits);	}

static	inline	void	wycard_add(wycard	*s,	void	*item,	uint64_t	item_size){
	uint64_t	h=wyhash(item,item_size,0,_wyp);
	uint64_t	lz=__builtin_clzll(h);
	if(lz>=s->layers)	lz=s->layers-1;
	h&=(8ull<<s->bits)-1;
	s->data[(lz<<s->bits)+(h>>3)]|=1u<<(h&7);
}

static	inline	double	wycard_solve(wycard	*s,	uint64_t	*m){
	double	n=8ull<<s->bits,	p[64]={},	N=(s->bits+s->layers+6)*M_LN2;
	for(uint8_t	l=0;	l<s->layers;	l++)	p[l]=l==s->layers-1?1.0/n/(1ull<<l):0.5/n/(1ull<<l);
	for(size_t	it=0;	it<256;	it++){	
		double	dn=0,	dnn=0,	en=expf(N);
		for(uint8_t	l=0;	l<s->layers;	l++){	
			double	ep=en*p[l],	enp=expf(-ep);
			dn+=-ep*(n-m[l])+m[l]*ep*enp/(1-enp);
			dnn+=-ep/(1-enp)/(1-enp)*(n*enp*enp+(m[l]-2*n+ep*m[l])*enp+n-m[l]);
		}
		N-=dn/dnn;
		if(fabs(dn/dnn)<1e-6)	break;
	}
	return	expf(N);
}

static	inline	double	wycard_cardinality(wycard	*s){
	uint64_t	m[64]={};	bool	empty=true,	full=true;
	for(uint64_t	l=0;	l<s->layers;	l++){
		uint64_t	sum=0;	uint8_t	*p=s->data+(l<<s->bits);
		for (uint64_t	i=0;	i<(1ull<<s->bits);	i+=8)	sum+=__builtin_popcountll(*(uint64_t*)(p+i));
		m[l]=sum;	
		if(m[l]!=(8ull<<s->bits))	full=false;
		if(m[l])	empty=false;
	}
	return	empty?0:(full?-1.0:wycard_solve(s,	m));
}
#endif
