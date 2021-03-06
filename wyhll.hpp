#include	"wyhash.h"
#include	<math.h>
#define	wyhll_size	(1ull<<bits)
template<uint64_t	bits>
struct wyhll{
	uint8_t	s[wyhll_size]={};
	void add(void	*data,	uint64_t len){
		uint64_t	h=wyhash(data, len, 0, _wyp),	i=h>>(64-bits),	lz=__builtin_clzll(h<<bits);
		uint8_t	r=(lz<(64-bits)?lz:64-bits)+1;
		if(r>s[i])	s[i]=r;
	}
	//Otmar Ertl, New Cardinality Estimation Methods for HyperLogLog Sketches
	double estimate(void){
		double x=0,	l,	dl=0,	dll=0;
		for(uint64_t	i=0;	i<wyhll_size;	i++)	x+=1.0/(1ull<<s[i]);
		l=x;	x/=wyhll_size;
		l=0.7213/(1+1.079/wyhll_size)*wyhll_size*wyhll_size/l;
		for(uint64_t	i=0;	i<wyhll_size;	i++)	if(s[i]){
			double	z=l/wyhll_size/(1ull<<s[i]),	ez=exp(z);
			dl+=z/(ez-1);
			dll+=z/(ez-1)/l-z*z/l/(ez-1)/(ez-1)*ez;
		}
		dl-=l*x;	dll-=x;	if(fabs(dl)>1e-8)	l-=dl/dll;
		if(l<log1p(bits)*wyhll_size){
			uint64_t	zeros=0;
			for(uint64_t	i=0;	i<wyhll_size;	i++)	zeros+=!s[i];
			if(zeros)	l=log((double)zeros/wyhll_size)/log(1-1.0/wyhll_size);
		} 
		return	l;
	}
	void clear(void){	memset(s,0,wyhll_size);	}
	void merge(const wyhll&	h){	for (uint64_t	i=0;	i<wyhll_size;	i++)	s[i]=max(s[i],h.s[i]);	}
	//https://github.com/hideo55/cpp-HyperLogLog
	double old_estimate(void){
		double sum=0,	alpha,	est;
		for(uint64_t	i=0;	i<wyhll_size;	i++)	sum+=1.0/(1ull<<s[i]);
		switch (bits) {
			case 4:	alpha=0.673;	break;
			case 5:	alpha=0.697;	break;
			case 6:	alpha=0.709;	break;
			default:	alpha=0.7213/(1+1.079/wyhll_size); break;
		}
		est=alpha*wyhll_size*wyhll_size/sum;
		if(est<=2.5*wyhll_size){
			uint64_t	zeros=0;
			for(uint64_t	i=0;	i<wyhll_size;	i++)	zeros+=!s[i];
			if(zeros)	est=wyhll_size*log((double)(wyhll_size)/zeros);
		} 
		return est;
	}
};
