/*	author: 王一 Wang Yi <godspeed_china@yeah.net>
	This algorithm combines HyperLogLog with Bloom Filter.
	The HyperLogLof is scaleable while the Bloom Filter is accurate for small set
	For every byte register, we use the 3 low bit for Bloom filter while the high 5 bits for HyperLogLog:
    10101      010
HyperLogLog  Bloom Filter
*/
#ifndef	wyset53_version_0
#define	wyset53_version_0
#include	"wyhash.h"
#include	<math.h>
//	alloc 1<<bits bytes register, data can be released by free()
static	inline	uint32_t	wyset53_alloc(uint8_t	**data,	uint64_t	bits){	*data=(uint8_t*)calloc(1ull<<bits,1);	return	data!=NULL;	}

//	reset the registers
static	inline	void	wyset53_clear(uint8_t	*data,	uint64_t	bits){	memset(data,0,1ull<<bits);	}

//	merge two sets
static	inline	void	wyset53_merge(uint8_t	*des,	uint8_t	*src,	uint64_t	bits){
	for(uint64_t	i=0;	i<(1ull<<bits);	i++){
		uint8_t	hd=des[i]>>3,	hs=src[i]>>3,	hn=hd>hs?hd:hs;	//	HyperLogLog merge
		uint8_t	bd=des[i]&7,	bs=src[i]&7,	bn=bd|bs;	//	Bloom Filter merge
		des[i]=(hn<<3)|bn;	//	write to des
	}
}

//	add an element
static	inline	void	wyset53_add(uint8_t	*data,	uint64_t	bits,	void	*item,	uint64_t	item_size){
	uint64_t	h=wyhash(item,item_size,0,_wyp);	//	generate a 64 bit hash
	uint64_t	b=h>>(64-bits);	//	the data register
	uint8_t	z=__builtin_clzll(h<<bits)+1;	//	leading zeros+1
	if(z>31)	z=31;	// restrict to 5 bits
	if(z>(data[b]>>3))	data[b]=(data[b]&7u)|(z<<3);	//	update the HyperLogLog part
	h=((h<<bits)>>bits)%(3ull<<bits);	//	choose a bit to set
	data[h/3]|=1u<<(h%3);	//	update the Bloom Filter
}

//	Bloom Filter estimator
static	inline	double	wyset53_bf_estimator(uint8_t	*data,	uint64_t	bits){
	uint8_t	pc[8]={0,1,1,2,1,2,2,3};	//	3 bit popcount table
	uint64_t	m=0,	n=3ull<<bits;
	for(uint64_t	i=0;	i<(1ull<<bits);	i++)	m+=pc[data[i]&7u];
	return	m==n?-1:log((double)(n-m)/n)/log(1-1.0/n);
}

//	HLL estimaator
static	inline	double	wyset53_hll_estimator(uint8_t	*data,	uint64_t	bits){
	double x=0,	l,	dl=0,	dll=0;
	for(uint64_t	i=0;	i<(1ull<<bits);	i++)	x+=1.0/(1ull<<(data[i]>>3));
	l=x;	x/=(1ull<<bits);
	l=0.7213/(1+1.079/(1ull<<bits))*(1ull<<bits)*(1ull<<bits)/l;	//	approximate start value
	for(uint64_t	i=0;	i<(1ull<<bits);	i++)	if(data[i]>>3){	//	Newton's Method
		double	z=l/(1ull<<bits)/(1ull<<(data[i]>>3)),	ez=exp(z);
		dl+=z/(ez-1);	dll+=z/(ez-1)/l-z*z/l/(ez-1)/(ez-1)*ez;
	}
	dl-=l*x;	dll-=x;	if(fabs(dl)>1e-8)	l-=dl/dll;	//	Newton step
	return	l;
}

//	the combined main estimator
static	inline	double	wyset53_estimator(uint8_t	*data,	uint64_t	bits){
	double	e=wyset53_hll_estimator(data,bits);
	if(e<log1p(bits)*(3ull<<bits)){//	for small set, we return Bloom Filter estimator
		double	e1=wyset53_bf_estimator(data,bits);	
		if(e1>-0.5)	return	e1;
	}
	return	e;
}

//	the original HLL estimator (Flajolet et al, 2007) to serve as baseline
static	inline	double	wyset53_baseline_estimator(uint8_t	*data,	uint64_t	bits){
	double sum=0,	alpha,	est;
	for(uint64_t	i=0;	i<(1ull<<bits);	i++)	sum+=1.0/(1ull<<(data[i]>>3));
	switch (bits) {
		case 4:	alpha=0.673;	break;
		case 5:	alpha=0.697;	break;
		case 6:	alpha=0.709;	break;
		default:	alpha=0.7213/(1+1.079/(1ull<<bits)); break;
	}
	est=alpha*(1ull<<bits)*(1ull<<bits)/sum;
	if(est<=2.5*(1ull<<bits)){
		uint64_t	zeros=0;
		for(uint64_t	i=0;	i<(1ull<<bits);	i++)	zeros+=!(data[i]>>3);
		if(zeros)	est=(1ull<<bits)*log((double)((1ull<<bits))/zeros);
	} 
	return est;
}
#endif
