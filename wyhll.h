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

typedef  struct  wyhll_t{  
  uint64_t bins;
  uint32_t p0, p1; 
}wyhll;

static  inline  void  wyhll_configure(wyhll  *s,  uint64_t  bytes,  uint64_t  capacity){  
  s->bins=bytes<<2;
  double p=(double)s->bins/capacity;
  if(p>0.25) p=0.25;
  s->p0=p*(1ull<<32);
  s->p1=sqrt(p)*(1ull<<32);
}

static  inline  uint64_t  wyhll_alloc(wyhll  s,  uint8_t  **data){  *data=(uint8_t*)calloc(s.bins>>2,1);  return  data!=NULL; }

static  inline  void  wyhll_clear(wyhll  s,  uint8_t  *data){  memset(data,0,s.bins>>2);  }

static  inline  uint8_t wyhll_get(uint8_t *data, uint64_t i){ return (data[i>>2]>>((i&3)<<1))&3; }

static  inline  void wyhll_set(uint8_t *data, uint64_t i, uint8_t v){ data[i>>2]&=~(3<<((i&3)<<1)); data[i>>2]|=v<<((i&3)<<1); }

static  inline  void  wyhll_add(wyhll  s,  uint8_t  *data,  void  *item,  uint64_t  item_size){
  uint64_t h=wyhash(item,item_size,0,_wyp);
  uint64_t b=((h>>32)*s.bins)>>32;
  uint8_t o;
  if((uint32_t)h>=s.p1) o=1;  else if((uint32_t)h>=s.p0) o=2; else o=3;
  if(o>wyhll_get(data,b)) wyhll_set(data,b,o);
}

static  inline  double  wyhll_loglike(wyhll  s,  uint32_t  *m,  double  N){
  double sum=0, p0=(double)s.p0/(1ull<<32), p1=(double)s.p1/(1ull<<32);
  sum+=m[3]*log1p(-exp(-p0*N/s.bins));
  sum+=m[2]*log(exp(-p0*N/s.bins)-exp(-p1*N/s.bins));
  sum+=m[1]*log(exp(-p1*N/s.bins)-exp(-N/s.bins));
  sum-=m[0]*N/s.bins;
  return sum;
}

static  inline  uint64_t  wyhll_solve(wyhll  s, uint32_t  *m){
  if(m[0]==s.bins)  return  0;
  double  r=(sqrt(5.0)+1)/2, a=0, b=log(s.bins<<32)-log(s.p0), c=b-(b-a)/r, d=a+(b-a)/r;
  while(fabs(a-b)>1e-5){
    if(wyhll_loglike(s,m,exp(c))>wyhll_loglike(s,m,exp(d)))   b=d;  
    else  a=c;
    c=b-(b-a)/r;  d=a+(b-a)/r;
  }
  return exp((a+b)/2)+0.5;
}

static  inline  uint64_t  wyhll_cardinality(wyhll  s, uint8_t *data){
  uint32_t m[4]={};
  for(uint64_t  i=0; i<s.bins; i++)  m[wyhll_get(data,i)]++;
  return wyhll_solve(s,m);
}

static  inline  uint64_t  wyhll_union_cardinality(wyhll  s, uint8_t *data_a, uint8_t *data_b){
  uint32_t m[4]={};
  for(uint64_t  i=0; i<s.bins; i++){
    uint8_t va=wyhll_get(data_a,i), vb=wyhll_get(data_b,i);
  	m[va>vb?va:vb]++;
  }
  return wyhll_solve(s,m);
}
#endif
