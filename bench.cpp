#include	"wyhll.hpp"
#include	"wyset.h"
#include	<algorithm>
#include	<iostream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x10000;

int	main(void){
	uint64_t	seed=0;	double	rmse0=0,	rmse1=0;
	for(size_t	it=0;	it<N;	it++){
		uint64_t	n=pow(2,wy2u01(wyrand(&seed))*16);
		wyhll<14> as;
		for(size_t	i=0;	i<n;	i++){	uint64_t	r=wyrand(&seed);	as.add(&r,8);	}
		double	x=log(as.estimate());
		double	y=log(as.old_estimate());
		double	z=log(n);
		rmse0+=(x-z)*(x-z);
		rmse1+=(y-z)*(y-z);
	}
	cerr<<sqrt(rmse0/N)<<'\t'<<sqrt(rmse1/N)<<'\t'<<rmse0/rmse1<<'\n';
	return	0;
}
