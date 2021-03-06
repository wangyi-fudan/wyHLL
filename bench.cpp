#include	"wyhll.hpp"
#include	"wyset.h"
#include	<algorithm>
#include	<iostream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x10000;
const	size_t	bits=4;

int	main(void){
	uint64_t	seed=0,	r;
	uint8_t	*s;	wyset_alloc(&s,bits);	wyhll<bits>	t;
	double	rmse0=0,	rmse1=0;
	for(size_t	it=0;	it<N;	it++){
		uint64_t	n=pow(2,wy2u01(wyrand(&seed))*16);
		wyset_clear(s,bits);	t.clear();
		for(size_t	i=0;	i<n;	i++){	r=wyrand(&seed);	wyset_add(s,bits,&r,8);	t.add(&r,8);	}
		double	x=wyset_estimator(s,bits);
		double	y=t.estimate();
		double	z=n;
		x=log(x),	y=log(y);	z=log(z);
		rmse0+=(x-z)*(x-z);	rmse1+=(y-z)*(y-z);
	}
	cerr<<sqrt(rmse0/N)<<'\t'<<sqrt(rmse1/N)<<'\t'<<rmse0/rmse1<<'\n';
	return	0;
}
