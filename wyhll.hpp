#include	"wyhash.h"
#include	<cmath>
using	namespace	std;
#define	wyhll_size	(1ull<<bits)
template<uint64_t	bits>
struct wyhll{
	uint8_t	s[wyhll_size]={};
	void add(void	*data,	uint64_t len){
		uint64_t	h=wyhash(data, len, 0, _wyp),	i=h>>(64-bits),	lz=__builtin_clzll(h<<bits);
		uint8_t	r=(lz<(64-bits)?lz:64-bits)+1;
		if(r>s[i])	s[i]=r;
	}
    double estimate(void){
		double sum=0,	alpha;
		for(uint64_t	i=0;	i<wyhll_size;	i++)	sum+=1.0/(1ull<<s[i]);
		switch (bits) {
			case 4:	alpha=0.673;	break;
			case 5:	alpha=0.697;	break;
			case 6:	alpha=0.709;	break;
			default:	alpha=0.7213/(1+1.079/wyhll_size); break;
		}
		sum=alpha*wyhll_size*wyhll_size/sum;
		if(sum<log1p(bits)*wyhll_size){
			uint64_t	zeros=0;
			for(uint64_t	i=0;	i<wyhll_size;	i++)	zeros+=!s[i];
			if(zeros)	sum=log((double)zeros/wyhll_size)/log(1-1.0/wyhll_size);
		} 
		return	sum;
    }
    void clear(void){	memset(s,0,wyhll_size);	}
    void merge(const wyhll&	h){	for (uint64_t	i=0;	i<wyhll_size;	i++)	s[i]=max(s[i],h.s[i]);	}
};
