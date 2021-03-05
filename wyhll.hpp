#include	<algorithm>
#include	"wyhash.h"
#include	<cmath>
using	namespace	std;
const	double	wyhll_table[65]={1.3913,0.695652,0.347826,0.173913,0.0869565,0.0434783,0.0217391,0.0108696,0.00543478,0.00271739,0.0013587,0.000679348,0.000339674,0.000169837,8.49185e-05,4.24592e-05,2.12296e-05,1.06148e-05,5.3074e-06,2.6537e-06,1.32685e-06,6.63425e-07,3.31712e-07,1.65856e-07,8.29284e-08,4.14642e-08,2.0732e-08,1.0366e-08,5.18301e-09,2.59151e-09,1.29575e-09,6.47878e-10,3.23938e-10,1.61969e-10,8.09848e-11,4.04922e-11,2.02462e-11,1.01231e-11,5.06155e-12,2.53077e-12,1.26538e-12,6.32691e-13,3.16346e-13,1.58173e-13,7.90864e-14,3.95432e-14,1.97716e-14,9.88582e-15,4.94291e-15,2.47145e-15,1.23573e-15,6.17864e-16,3.08932e-16,1.54466e-16,7.72332e-17,3.86164e-17,1.93082e-17,9.65409e-18,4.82705e-18,2.41353e-18,1.20676e-18,6.03384e-19,3.01691e-19,1.50845e-19,7.54227e-20};
#define	wyhll_size	(1ull<<bits)
template<uint64_t	bits>
struct wyhll{
	uint8_t	s[wyhll_size]={};
    void clear(void){	memset(s,0,wyhll_size);	}
    void merge(const wyhll&	h){	for (uint64_t	i=0;	i<wyhll_size;	i++)	s[i]=max(s[i],h.s[i]);	}
	void add(void	*data,	uint64_t len){
		uint64_t	h=wyhash(data, len, 0, _wyp),	i=h>>(64-bits);
		uint8_t	r=std::min((uint8_t)(64-bits),(uint8_t)__builtin_clzll(h<<bits))+1;
		if(r>s[i])	s[i]=r;
	}
    double	estimate(void){
		double y=0;	
		for(uint64_t	i=0;	i<wyhll_size;	i++)	y+=wyhll_table[s[i]];
		y=wyhll_size*(wyhll_size/y);
		if(y<log1p(bits)*wyhll_size){
			uint64_t	zeros=0;
			for(uint64_t	i=0;	i<wyhll_size;	i++)	zeros+=!s[i];
			if(zeros)	y=log((double)zeros/wyhll_size)/log(1-1.0/wyhll_size);
		}	
		return	y;
	}
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
