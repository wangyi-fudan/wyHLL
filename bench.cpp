#include	"hyperloglog.h"
#include	"wycard.h"
#include	<algorithm>
#include	<iostream>
#include	<fstream>
#include	<cmath>
#include	<vector>
using	namespace	std;
const	size_t	N=0x1000;
int	main(void){
	uint64_t	seed=0,	r;
	wycard	s;
	uint64_t	c=wycard_configure(&s,12<<10,10000000);
	wycard_print_configure(s,stderr);
	uint8_t	*data;
	wycard_alloc(s,&data);
	ofstream	fo("compare.xls");
	fo.precision(3);	fo.setf(ios::fixed);
	fo<<"size\twycard\tredis_HLL\n";
	cout.precision(3);	cout.setf(ios::fixed);
	cout<<"size\twycard\tredis_HLL\n";
	for(uint64_t	n=1;	n<=c;	n+=(n/2)+1){
		double	rmse0=0,	rmse1=0,	eb=0;
		for(size_t	it=0;	it<N;	it++){
			wycard_clear(s,data);
			hllhdr *hdr = createHLLObject();
			hdr = hllSparseToDense(hdr);
			for(size_t	i=0;	i<n;	i++){	
				r=wyrand(&seed);	
				wycard_add(s,data,&r,8);	
				pfaddCommand(&hdr,(const unsigned char *)&r,8);
			}
			double	x=wycard_cardinality(s,data);
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
		fo<<n<<'\t'<<100*sqrt(rmse0)<<'\t'<<100*sqrt(rmse1)<<'\n';
		cout<<n<<'\t'<<100*sqrt(rmse0)<<'\t'<<100*sqrt(rmse1)<<'\n';
	}
	fo.close();
	free(data);
	return	0;
}
