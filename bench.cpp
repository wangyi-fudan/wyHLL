#include	"wyhll.hpp"
#include	"wyset.h"
#include	<algorithm>
#include	<iostream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x10000;

int	main(void){
	uint64_t	seed=0;
	double	xx0=0,	xx1=0,	xy0=0,	xy1=0,	yy0=0,	yy1=0;
	for(size_t	it=0;	it<N;	it++){
		uint64_t	n=pow(2,wy2u01(wyrand(&seed))*20);
		wyhll<8> as;
		for(size_t	i=0;	i<n;	i++){	uint64_t	r=wyrand(&seed);	as.add(&r,8);	}
		double	x=log(as.estimate());
		double	y=log(as.old_estimate());
		double	z=log(n);
		xx0+=x*x;	xx1+=y*y;	xy0+=x*z;	xy1+=y*z;	yy0+=z*z;	yy1+=z*z;
	}
	double	r0=xy0/sqrt(xx0*yy0),	r1=xy1/sqrt(xx1*yy1);
	cerr<<(1-r0)/(1-r1)<<'\n';
	return	0;
}
