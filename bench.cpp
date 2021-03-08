#include	"hyperloglog.h"
#include	"wyset.h"
#include	<algorithm>
#include	<iostream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x1000;
int	main(void){
	uint64_t	seed=0,	r;
	wyset	s;	wyset_alloc(&s,3<<12);
	cout.precision(3);	cout.setf(ios::fixed);
	cout<<"|set_size|wycard32_error%|wycard64__error%|error_ratio|\n";
	cout<<"|----|----|----|----|\n";
	for(uint64_t	n=1;	n<0x10000;	n+=(n/2)+1){
		double	rmse0=0,	rmse1=0;
		for(size_t	it=0;	it<N;	it++){
			wyset_clear(&s);
			hllhdr *hdr = createHLLObject();
			hdr = hllSparseToDense(hdr);
			for(size_t	i=0;	i<n;	i++){	
				r=wyrand(&seed);	
				wyset_add(&s,&r,8);	
				pfaddCommand(&hdr,(const unsigned char *)&r,8);
			}
			double	x=wyset_estimator(&s);
			uint64_t	ret;
			pfcountCommand(hdr,&ret);
			double	y=ret;
			double	z=n;
//			cerr<<n<<'\t'<<x<<'\t'<<y<<'\n';
			x=log(x);
			y=log(y);	
			z=log(z);
			rmse0+=(x-z)*(x-z);	rmse1+=(y-z)*(y-z);
			deleteHLLObject(hdr);
		}
		cout<<'|'<<n<<'|'<<100*sqrt(rmse0/N)<<'|'<<100*sqrt(rmse1/N)<<'|'<<sqrt(rmse0/rmse1)<<"|\n";
	}
	wyset_free(&s);
	return	0;
}
