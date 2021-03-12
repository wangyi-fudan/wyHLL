#include	"hyperloglog.h"
#include	"wycard.h"
#include	<algorithm>
#include	<iostream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x100;
int	main(void){
	uint64_t	seed=0,	r;
	wycard	s;	
	uint64_t	cap=wycard_alloc(&s,12<<10,0x100000);
	cerr<<"capacity:\t"<<cap<<'\n';
	cout.precision(3);	cout.setf(ios::fixed);
	cout<<"|set_size|wycard_RMSE%|redis_HLL_RMSE%|RMSE_ratio|\n";
	cout<<"|----|----|----|----|\n";
	for(uint64_t	n=1;	n<=cap;	n+=(n/2)+1){
		double	rmse0=0,	rmse1=0;
		for(size_t	it=0;	it<N;	it++){
			wycard_clear(&s);
			hllhdr *hdr = createHLLObject();
			hdr = hllSparseToDense(hdr);
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
