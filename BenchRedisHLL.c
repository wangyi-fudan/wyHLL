#include	"hyperloglog.h"
#include	"wyhash.h"
#include	<stdio.h>
const	size_t	N=10000;

int	main(void){
	FILE	*f=fopen("RedisHLL.xls","wt");
	uint64_t	seed=0,	r;
	for(uint32_t	n=1;	n<=15000000;	n+=(n/2)+1){
		double	rmse=0;
		for(size_t	it=0;	it<N;	it++){
			hllhdr *hdr = createHLLObject();
			hdr = hllSparseToDense(hdr);
			for(size_t	i=0;	i<n;	i++){	
				r=wyrand(&seed);	
				pfaddCommand(&hdr,(const unsigned char *)&r,8);
			}
			uint64_t	ret;
			pfcountCommand(hdr,&ret);
			double	x=log(ret);
			double	y=log(n);
			rmse+=(x-y)*(x-y);
			deleteHLLObject(hdr);
		}
		fprintf(f,"%u\t%.3f\n",n,100*sqrt(rmse/N));
		printf("%u\t%.3f\n",n,100*sqrt(rmse/N));
		fflush(f);
	}
	fclose(f);
	return	0;
}
