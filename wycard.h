/*  This is free and unencumbered software released into the public domain under The Unlicense (http://unlicense.org/)
  main repo: https://github.com/wangyi-fudan/wycard
  author: 王一 Wang Yi <godspeed_china@yeah.net>

  This cardinality estimation is based on multilayer Bloom filter  

  simple example:
  wycard s;
  wycard_configure(&s,12<<10,0x100000);  //  configure 12kb data to count at least 1 million objects
  wycard_print_configure(s,stdout);
  uint8_t  *data;
  wycard_alloc(s,&data);
  wycard_add(s, data,  item, item_len);  //  insert item
  cout<<wycard_cardinality(s,data)<<'\n';  //  estimate the cardinality
  free(data);  //  free the data
*/
#ifndef  wycard_0_0
#define  wycard_0_0
#include <inttypes.h>
#include <stdlib.h>
#include "wyhash.h"
#include <stdio.h>
#include <math.h>

typedef  struct  wycard_t{
  uint64_t  layers:6;  //  number of layers
  uint64_t  bytes:26;  //  bytes per layer
}wycard;

static  inline  uint64_t  wycard_configure(wycard  *s,  uint64_t  bytes,  uint64_t  capacity){
  for(s->layers=1;  s->layers<=32;  s->layers++){
    s->bytes=bytes/s->layers;
    if(s->bytes<<(s->layers+2)>=capacity)  break;
  }
  return  s->layers>32?0:s->bytes<<(s->layers+2);
}

static  inline  uint64_t  wycard_bytes(wycard  s){  return  s.layers*s.bytes;  }

static  inline  uint64_t  wycard_bits(wycard  s){  return  s.bytes<<3;  }

static  inline  uint64_t  wycard_capacity(wycard  s){  return  s.bytes<<(s.layers+2);  }

static	inline	void	wycard_print_configure(wycard	s,	FILE	*f){
  fprintf(f,"number of layers:\t%"PRIu64"\n",s.layers);
  fprintf(f,"bytes per layers:\t%"PRIu64"\n",s.bytes);
  fprintf(f,"#bits per layer:\t%"PRIu64"\n",wycard_bits(s));
  fprintf(f,"actual data bytes:\t%"PRIu64"\n",wycard_bytes(s));
  fprintf(f,"cardinality capacity:\t%"PRIu64"\n",wycard_capacity(s));
}

static  inline  uint64_t  wycard_alloc(wycard  s,  uint8_t  **data){  *data=(uint8_t*)calloc(wycard_bytes(s),1);  return  data!=NULL; }

static  inline  void  wycard_clear(wycard  s,  uint8_t  *data){  memset(data,0,wycard_bytes(s));  }

static  inline  void  wycard_add(wycard  s,  uint8_t  *data,  void  *item,  uint64_t  item_size){
  uint64_t  h=wyhash(item,item_size,0,_wyp),  lz=__builtin_clzll(h);
  if(lz>=s.layers)  lz=s.layers-1;
  h=(((unsigned)h)*wycard_bits(s))>>32;
  data[lz*s.bytes+(h>>3)]|=1<<(h&7);
}

static  inline  double  wycard_loglike(wycard  s,  uint32_t  *m,  double  N){
   double sum=0;  uint64_t n=wycard_bits(s);
   for(uint64_t  i=0;  i<s.layers;  i++){
      double  l=N/(n*(i==s.layers-1?1ull<<i:2ull<<i));
      sum+=-l*(n-m[i])+m[i]*log1pf(-expf(-l));
   }
   return sum;
}

static  inline  uint64_t  wycard_solve(wycard  s,  uint32_t  *m){
  double  r=(sqrt(5.0)+1)/2, a=0, b=logf(wycard_capacity(s)), c=b-(b-a)/r, d=a+(b-a)/r;
  unsigned ev=0;
  while(fabs(b-a)>1e-4){
    if(wycard_loglike(s,m,expf(c))>wycard_loglike(s,m,expf(d)))	 b=d;	else	a=c;
    c=b-(b-a)/r;	d=a+(b-a)/r;
  }
  return expf((a+b)/2)+0.5;
/*
  do{  
    dn=dnn=0;  en=exp(N);
    for(uint64_t  l=0;  l<s.layers;  l++){  
      ep=en*p[l];  enp=exp(-ep);
      dn+=-ep*(n-m[l])+m[l]*ep*enp/(1-enp);
      dnn+=-ep/(1-enp)/(1-enp)*(n*enp*enp+(m[l]-2*n+ep*m[l])*enp+n-m[l]);
    }
	dn/=dnn;  N-=dn;
  }while(fabs(dn)>1e-6);
*/
//  return  en+0.5;
}

static  inline  uint64_t  wycard_cardinality(wycard  s,	uint8_t	*data){
  uint32_t  m[32]={}, empty=1, full=1;
  for(uint64_t  l=0;  l<s.layers;  l++){
    uint32_t  sum=0;  uint8_t  *p=data+l*s.bytes;
    for (uint64_t  i=0;  i<s.bytes;  i++)  sum+=__builtin_popcount(p[i]);
    m[l]=sum;  
    if(m[l]<wycard_bits(s))  full=0;
    if(m[l])  empty=0;
  }
  return  empty?0:(full?~0ull:wycard_solve(s, m));
}

static  inline  uint64_t  wycard_union_cardinality(wycard s,  uint8_t *a,  uint8_t  *b){
  uint32_t  m[32]={},  empty=1,  full=1;
  for(uint64_t  l=0;  l<s.layers;  l++){
    uint32_t  sum=0;  uint8_t  *p=a+l*s.bytes,  *q=b+l*s.bytes;
    for (uint64_t  i=0;  i<s.bytes;  i++)  sum+=__builtin_popcount(p[i]|q[i]);
    m[l]=sum;  
    if(m[l]<wycard_bits(s))  full=0;
    if(m[l])  empty=0;
  }
  return  empty?0:(full?~0ull:wycard_solve(s, m));
}
#endif
