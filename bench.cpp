#include	"hyperloglog.h"
#include	"wycard.h"
#include	<algorithm>
#include	<iostream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x10000;
int	main(void){
	uint64_t	seed=0,	r;
	wycard	s;	wycard_alloc(&s,11,6);
	uint64_t	ca=wycard_capacity(&s);
	cout.precision(3);	cout.setf(ios::fixed);
	cout<<"wycard capacity:\t"<<ca<<'\n';
	cout<<"|set_size|wycard_error%|redis_HLL_error%|error_ratio|\n";
	cout<<"|----|----|----|----|\n";
	for(uint64_t	n=1;	n<ca;	n+=(n/2)+1){
		double	rmse0=0,	rmse1=0;
		for(size_t	it=0;	it<N;	it++){
			wycard_clear(&s);
			hllhdr *hdr = createHLLObject();
			hdr = hllSparseToDense(hdr);
			uint64_t	s0=seed;
			for(size_t	i=0;	i<n;	i++){	
				r=wyrand(&seed);	
				wycard_add(&s,&r,8);	
				pfaddCommand(&hdr,(const unsigned char *)&r,8);
			}
			double	x=wycard_cardinality(&s);
			uint64_t	ret;
			pfcountCommand(hdr,&ret);
			double	y=ret;
			double	z=n;
			x=log(x);
			y=log(y);	
			z=log(z);
			rmse0+=(x-z)*(x-z);	rmse1+=(y-z)*(y-z);
			deleteHLLObject(hdr);
		}
		rmse0/=N;	rmse1/=N;
		cout<<'|'<<n<<'|'<<100*sqrt(rmse0)<<'|'<<100*sqrt(rmse1)<<'|'<<sqrt(rmse0/rmse1)<<"|\n";
	}
	wycard_free(&s);
	return	0;
}
