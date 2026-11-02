// A10: native locate() stress vs exhaustive, factorized, long-double power oracle.
// Includes all rectangular periodicity masks, block-grid/order invariance.
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
using namespace voro;
struct Pt{double x,y,r;};
struct Query{double x,y;};
struct Oracle{int id;double x,y,margin;};
uint32_t step(uint32_t& s){return s=1664525u*s+1013904223u;}
std::vector<Pt> sites(int seed,int pat){
 double R=1e8, u=std::nextafter(R,std::numeric_limits<double>::infinity())-R;
 const double offsets[]={0,2,-3,4,-5,1,-1,3,-2};
 uint32_t rng=0x9e3779b9u+uint32_t(seed+1)*1529u;
 std::vector<Pt> out;
 for(int i=0;i<9;++i){Pt p={0,0,0};
 for(int k=0;k<500;++k){p.x=1.+(step(rng)%8000)/1000.;p.y=1.+(step(rng)%8000)/1000.;
 bool ok=true;for(const auto& t:out){double dx=t.x-p.x,dy=t.y-p.y;if(dx*dx+dy*dy<0.81){ok=false;break;}}
 if(ok)break;if(k==499){std::cerr<<"BAD_SOURCE";std::exit(2);}}
 if(pat==0)p.r=1+(i*7+seed*3)%5;
 if(pat==1)p.r=R;
 if(pat==2)p.r=R+offsets[(i+seed)%9]*u;
 if(pat==3)p.r=R+offsets[(2*i+seed)%9]*u*3;
 if(pat==4)p.r=R+(i==0?12:-3-i)*u;
 if(pat==5)p.r=i==0?R:0;
 out.push_back(p);
 }return out;
}
Oracle calc(const std::vector<Pt>& ps,int mask,Query q,int K){
 long double R=0;for(const auto& p:ps)R=std::max(R,(long double)p.r);
 long double best=1e300L,second=1e300L;
 Oracle o={-1,0,0,0};
 for(size_t j=0;j<ps.size();++j)for(int ky=(mask&2)?-K:0;ky<=((mask&2)?K:0);++ky)
 for(int kx=(mask&1)?-K:0;kx<=((mask&1)?K:0);++kx){
 long double x=ps[j].x+10*kx,y=ps[j].y+10*ky, r=ps[j].r;
 long double dx=x-q.x,dy=y-q.y;
 long double v=dx*dx+dy*dy+(R-r)*(R+r);
 if(v<best){second=best;best=v;o.id=int(j);o.x=double(x);o.y=double(y);}
 else if(v<second)second=v;
 }
 o.margin=double(second-best);return o;
}
int main(){
 int errors=0,controls=0,queries=0,skipped=0;
 for(int seed=0;seed<4;++seed)for(int pat=0;pat<6;++pat){
  const auto pts=sites(seed,pat);
  for(int mask=0;mask<4;++mask)for(int blocks: {1,2,4})for(int rev=0;rev<2;++rev){
   container_poly_2d c(0,10,0,10,blocks,blocks,mask&1,mask&2,8);
   for(int i=0;i<9;++i){int j=rev?8-i:i;c.put(j,pts[j].x,pts[j].y,pts[j].r);}
   for(int iq=0;iq<9;++iq)for(int jq=0;jq<7;++jq){
      Query q={0.35+iq*1.13,0.40+jq*1.31};
      Oracle o=calc(pts,mask,q,2);
      Oracle check=calc(pts,mask,q,1);
      if(o.id!=check.id || std::fabs(o.x-check.x)>1e-10 || std::fabs(o.y-check.y)>1e-10){
        std::cerr<<"ORACLE_NOT_CONVERGED dim=2 seed="<<seed<<" pattern="<<pat<<" mask="<<mask<<"\n";return 2;
      }
      if(o.margin<1e-7){skipped++;continue;}
      double rx=0,ry=0;int id=-1;bool found=c.find_voronoi_cell(q.x,q.y,rx,ry,id);
      bool ok=found&&id==o.id&&std::fabs(rx-o.x)<1e-8&&std::fabs(ry-o.y)<1e-8;
      ++queries;
      if(!ok){++errors;if(pat==0)++controls;
        if(errors<=12)std::cout<<std::setprecision(17)<<"WRONG seed="<<seed<<" pat="<<pat<<" mask="<<mask<<" blocks="<<blocks<<" rev="<<rev<<" query="<<q.x<<","<<q.y<<" expected="<<o.id<<" @"<<o.x<<","<<o.y<<" actual="<<id<<" @"<<rx<<","<<ry<<" margin="<<o.margin<<"\n";
      }
   }
  }
 }
 std::cout<<"SUMMARY A10 dim=2 queries="<<queries<<" mismatches="<<errors<<" control_failures="<<controls<<" tie_skips="<<skipped<<"\n";
 return controls?2:errors?1:0;
}
