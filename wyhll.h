/*  This is free and unencumbered software released into the public domain under The Unlicense (http://unlicense.org/)
  main repo: https://github.com/wangyi-fudan/wyhll
  author: 王一 Wang Yi <godspeed_china@yeah.net>

  simple example:
  wyhll s;
  wyhll_configure(&s,12<<10,1000000);  //  configure 12kb data to count at most 1 million objects
  uint8_t  *data;
  wyhll_alloc(s,&data);
  wyhll_add(s, data,  item, item_len);  //  insert item
  cout<<wyhll_cardinality(s,data)<<'\n';  //  estimate the cardinality
  free(data);  //  free the data
*/
#ifndef  wyhll_V0
#define  wyhll_V0
#include <inttypes.h>
#include <stdlib.h>
#include "wyhash.h"
#include <stdio.h>
#include <math.h>

typedef  struct  wyhll_t{  uint64_t bins, p[6]; }wyhll;

static  inline  void  wyhll_configure(wyhll  *s,  uint64_t  bytes,  uint64_t  capacity){  
  s->bins=(bytes-1)*8/3;
  double p=(double)s->bins/capacity;
  if(p>0.5) p=0.5;
  for(uint32_t	i=0;	i<6;	i++) s->p[i]=pow(p,(i+1)/6.0)*~0ull;
}

static  inline  uint64_t  wyhll_bytes(wyhll  s){  return s.bins*3/8+1; }

static  inline  uint64_t  wyhll_alloc(wyhll  s,  uint8_t  **data){  *data=(uint8_t*)calloc(wyhll_bytes(s),1);  return  data!=NULL; }

static  inline  void  wyhll_clear(wyhll  s,  uint8_t  *data){  memset(data,0,wyhll_bytes(s));  }

static  inline  uint8_t wyhll_get(uint8_t *data, uint64_t i){ 
  uint64_t b=(i*3)>>3, f=(i*3)&7,f8=8-f;
  return ((data[b]>>f)|(data[b+1]<<f8))&7;
}

static  inline  void wyhll_set(uint8_t *data, uint64_t i, uint8_t v){  
  uint64_t b=(i*3)>>3, f=(i*3)&7,f8=8-f;
  data[b]&=~(7<<f); data[b]|=v<<f; data[b+1]&=~(7>>f8); data[b+1]|=v>>f8;
}

static  inline  void  wyhll_add(wyhll  s,  uint8_t  *data,  void  *item,  uint64_t  item_size){
  uint64_t h=wyhash(item,item_size,0,_wyp);
  uint64_t b=wy2u0k(wyhash64(h,h),s.bins);
  uint8_t v;
  if(h>=s.p[0]) v=1;
  else if(h>=s.p[1]) v=2;
  else if(h>=s.p[2]) v=3;
  else if(h>=s.p[3]) v=4;
  else if(h>=s.p[4]) v=5;
  else if(h>=s.p[5]) v=6;
  else v=7;
  if(v>wyhll_get(data,b)) wyhll_set(data,b,v);
}

static  inline  double  wyhll_loglike(wyhll  s,  uint32_t  *m,  double  N){
  double sum=0;
  sum-=m[0]*N/s.bins;
  sum+=m[1]*log(exp(-N/s.bins*s.p[0]/~0ull)-exp(-N/s.bins));
  sum+=m[2]*log(exp(-N/s.bins*s.p[1]/~0ull)-exp(-N/s.bins*s.p[0]/~0ull));
  sum+=m[3]*log(exp(-N/s.bins*s.p[2]/~0ull)-exp(-N/s.bins*s.p[1]/~0ull));
  sum+=m[4]*log(exp(-N/s.bins*s.p[3]/~0ull)-exp(-N/s.bins*s.p[2]/~0ull));
  sum+=m[5]*log(exp(-N/s.bins*s.p[4]/~0ull)-exp(-N/s.bins*s.p[3]/~0ull));
  sum+=m[6]*log(exp(-N/s.bins*s.p[5]/~0ull)-exp(-N/s.bins*s.p[4]/~0ull));
  sum+=m[7]*log1p(-exp(-N/s.bins*s.p[5]/~0ull));
  return sum;
}

static  inline  uint64_t  wyhll_solve(wyhll  s, uint32_t  *m){
  if(m[0]==s.bins)  return  0;
  double  r=(sqrt(5.0)+1)/2, a=0, b=log(s.bins*((double)~0ull/s.p[5])), c=b-(b-a)/r, d=a+(b-a)/r;
  while(fabs(a-b)>1e-5){
    if(wyhll_loglike(s,m,exp(c))>wyhll_loglike(s,m,exp(d)))   b=d;  
    else  a=c;
    c=b-(b-a)/r;  d=a+(b-a)/r;
  }
  return exp((a+b)/2)+0.5;
}

static  inline  uint64_t  wyhll_cardinality(wyhll  s, uint8_t *data){
  uint32_t m[8]={};
  for(uint64_t  i=0; i<s.bins; i++)  m[wyhll_get(data,i)]++;
  return wyhll_solve(s,m);
}

static  inline  uint64_t  wyhll_union_cardinality(wyhll  s, uint8_t *data_a, uint8_t *data_b){
  uint32_t m[8]={};
  for(uint64_t  i=0; i<s.bins; i++){
    uint8_t va=wyhll_get(data_a,i), vb=wyhll_get(data_b,i);
  	m[va>vb?va:vb]++;
  }
  return wyhll_solve(s,m);
}
#endif
