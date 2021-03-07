//	author: 王一 Wang Yi <godspeed_china@yeah.net>
#ifndef	wyset_version_0
#define	wyset_version_0
#include	"wyhash.h"
#include	<math.h>
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
static	inline	double	wyset_loglike(wyset	*s,	uint64_t	*m,	double	N){
	uint64_t	n=8ull<<s->bits;	double	lk=0;
	N=exp(N)/n;
	for(uint8_t	l=0;	l<s->layer;	l++){
		double	lambda=l==s->layer-1?N/(1ull<<l):N/(1ull<<l)*0.5;
		lk+=-lambda*(n-m[l])+m[l]*log(1-exp(-lambda));
	}
	return	lk;
}
static	inline	double	wyset_solve(wyset	*s,	uint64_t	*m){
	const	double	gr=(sqrt(5.0)+1)/2;
	double	a=log(1.0/16),	b=log(~0ull),	c=b-(b-a)/gr,	d=a+(b-a)/gr;
 	while(fabs(b-a)>1e-4){
        if(wyset_loglike(s,m,c)>wyset_loglike(s,m,d))	 b=d;	else	a=c;
        c=b-(b-a)/gr;	d=a+(b-a)/gr;
	}
    return exp((b+a)/2);
}
static	inline	double	wyset_estimator(wyset	*s){
	uint64_t	m[64]={};
	for(uint64_t	l=0;	l<s->layer;	l++){
		uint64_t	sum=0;	uint8_t	*p=s->data+(l<<s->bits);
		for (uint64_t	i=0;	i<(1ull<<s->bits);	i+=8)	sum+=__builtin_popcountll(*(uint64_t*)(p+i));
		m[l]=sum;
	}
	return	wyset_solve(s,	m);
}
#endif
