#include	"wyset.h"
#include	"wyset53.h"
#include	<algorithm>
#include	<iostream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x10000;
const	size_t	bits0=7;
const	size_t	bits1=10;
int	main(void){
	uint64_t	seed=0,	r;
	wyset	s;	wyset_alloc(&s,bits0,8);
	uint8_t	*t;	wyset53_alloc(&t,bits1);
	cout<<"|set_size|wyset_error%|Flajolet_error%|error_ratio|\n";
	cout<<"|----|----|----|----|\n";
	for(uint64_t	n=1;	n<0x10000;	n+=(n/2)+1){
		double	rmse0=0,	rmse1=0;
		for(size_t	it=0;	it<N;	it++){
			wyset_clear(&s);	
			wyset53_clear(t,bits1);	
			for(size_t	i=0;	i<n;	i++){	
				r=wyrand(&seed);	
				wyset_add(&s,&r,8);	
				wyset53_add(t,bits1,&r,8);	
			}
			double	x=wyset_estimator(&s);
			double	y=wyset53_baseline_estimator(t,bits1);
			double	z=n;
//			cerr<<n<<'\t'<<x<<'\t'<<y<<'\n';
			x=log(x),	y=log(y);	z=log(z);
			rmse0+=(x-z)*(x-z);	rmse1+=(y-z)*(y-z);
		}
		cout<<'|'<<n<<'|'<<100*sqrt(rmse0/N)<<'|'<<100*sqrt(rmse1/N)<<'|'<<sqrt(rmse0/rmse1)<<"|\n";
	}
	wyset_free(&s);	free(t);
	return	0;
}
