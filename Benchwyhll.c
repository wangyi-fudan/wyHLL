#include	"wyhll.h"
const	size_t	N=10000;

int	main(void){
	FILE	*f=fopen("wyhll.xls","wt");
	uint64_t	seed=0,	r;
	wyhll	s;
	wyhll_configure(&s,12<<10,15000000ull);
	uint8_t	*data;
	wyhll_alloc(s,&data);
	for(uint32_t	n=1;	n<=15000000;	n+=(n/2)+1){
		double	rmse=0;
		for(size_t	it=0;	it<N;	it++){
			wyhll_clear(s,data);
			for(size_t	i=0;	i<n;	i++){	
				r=wyrand(&seed);	
				wyhll_add(s,data,&r,8);	
			}
			double	x=log(wyhll_cardinality(s,data));
			double	y=log(n);
			rmse+=(x-y)*(x-y);
		}
		fprintf(f,"%u\t%.3f\n",n,100*sqrt(rmse/N));
		fflush(f);
		printf("%u\t%.3f\n",n,100*sqrt(rmse/N));
	}
	fclose(f);
	free(data);
	return	0;
}
