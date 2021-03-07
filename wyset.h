//	author: 王一 Wang Yi <godspeed_china@yeah.net>
#ifndef	wyset_version_0
#define	wyset_version_0
#include	"wyhash.h"
#include	<math.h>
#include	<iostream>
using	namespace	std;
struct	wyset{	uint8_t	*data,	bits,	layer;	};
static	inline	void	wyset_alloc(wyset	*s,	uint8_t	bits,	uint8_t	layer){	s->bits=bits;	s->layer=layer;	s->data=(uint8_t*)calloc(((uint64_t)s->layer)<<s->bits,1);	}
static	inline	void	wyset_free(wyset	*s){	free(s->data);	}
static	inline	void	wyset_clear(wyset	*s){	memset(s->data,0,((uint64_t)s->layer)<<s->bits);	}
static	inline	void	wyset_add(wyset	*s,	void	*item,	uint64_t	item_size){
	uint64_t	h=wyhash(item,item_size,0,_wyp),	lz=__builtin_clzll(h);
	if(lz>=s->layer)	lz=s->layer-1;
	h&=(8ull<<s->bits)-1;
	s->data[(lz<<s->bits)+(h>>3)]|=1u<<(h&7);
}
static	inline	double	wyset_solve(wyset	*s,	uint64_t	*m){
	double	n=8ull<<s->bits;
	double	p[64]={},	N=(s->bits+s->layer+8)*M_LN2;
	for(uint8_t	l=0;	l<s->layer;	l++)	p[l]=l==s->layer-1?1.0/n/(1ull<<l):1.0/n/(1ull<<l)/2;
	for(size_t	it=0;	it<100;	it++){	
		double	dn=0,	dnn=0,	en=exp(N);
		for(uint8_t	l=0;	l<s->layer;	l++){	
			double	enp=exp(-en*p[l]);
			dn+=-en*p[l]*(n-m[l])+m[l]*en*p[l]*enp/(1-enp);
			dnn+=-en*p[l]/(1-enp)/(1-enp)*(n*enp*enp+(m[l]-2*n+en*p[l]*m[l])*enp+n-m[l]);
		}
		N-=dn/dnn;
		if(fabs(dn/dnn)<1e-6)	break;
	}
	return	exp(N);
}

static	inline	double	wyset_estimator(wyset	*s){
	uint64_t	m[64]={};	bool	empty=true,	full=true;;
	for(uint64_t	l=0;	l<s->layer;	l++){
		uint64_t	sum=0;	uint8_t	*p=s->data+(l<<s->bits);
		for (uint64_t	i=0;	i<(1ull<<s->bits);	i+=8)	sum+=__builtin_popcountll(*(uint64_t*)(p+i));
		m[l]=sum;	
		if(m[l]!=(8ull<<s->bits))	full=false;
		if(m[l])	empty=false;
	}
	return	empty?0:(full?-1.0:wyset_solve(s,	m));
}
#endif
