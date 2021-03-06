#include	"wyhll.hpp"
#include	"wyset.h"
#include	<algorithm>
#include	<iostream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x10000;

int	main(void){
	uint64_t	seed=0;	double	rmse=0;
	for(size_t	it=0;	it<N;	it++){
		uint64_t	n=pow(2,wy2u01(wyrand(&seed))*16);
		wyhll<4> as;
		for(size_t	i=0;	i<n;	i++){	uint64_t	r=wyrand(&seed);	as.add(&r,8);	}
		double	x=log(as.estimate());
		double	y=log(n);
		rmse+=(x-y)*(x-y);
	}
	cerr<<sqrt(rmse/N)<<'\n';
	return	0;
}
