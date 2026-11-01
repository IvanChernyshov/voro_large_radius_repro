// Stage 9: independent binary64-input / extended-arithmetic owner oracle for 2D.
// Native Voro++ location API; nonnegative radii are acceptance tests.
// Negative radii and near ties are explicitly diagnostic.
#include <voro++_2d.hh>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

struct Site { double x,y,r; };
struct Query { double x,y; };
struct Owner { bool found; int id; long double x,y,margin; };
struct Totals { unsigned long tested=0,wrong=0,controls=0,skipped_tie=0,negative_tested=0,negative_wrong=0,outside_tested=0,outside_wrong=0,near_tested=0,near_wrong=0,exact_ties=0; };

Owner reference(const Site* sites,Query q,int mask,int K) {
    if((!(mask&1) && (q.x<0 || q.x>=1)) || (!(mask&2) && (q.y<0 || q.y>=1)))
        return Owner{false,-1,0,0,0};
    const long double R=std::max(std::fabs((long double)sites[0].r),std::fabs((long double)sites[1].r));
    long double best=std::numeric_limits<long double>::infinity(),second=best;
    Owner o={false,-1,0,0,0};
    for(int j=0;j<2;++j) for(int ky=(mask&2)?-K:0;ky<=((mask&2)?K:0);++ky)
        for(int kx=(mask&1)?-K:0;kx<=((mask&1)?K:0);++kx) {
            const long double px=(long double)sites[j].x+kx,py=(long double)sites[j].y+ky;
            const long double dx=px-q.x,dy=py-q.y,r=sites[j].r;
            const long double ar=std::fabs(r);
            const long double radius_gap=(ar==R)?0:(R-ar)*(R+ar);
            const long double v=dx*dx+dy*dy+radius_gap;
            if(v<best){second=best;best=v;o=Owner{true,j,px,py,0};}
            else if(v<second)second=v;
        }
    o.margin=second-best;
    return o;
}

int main(){
    const double R=1e8, R150=1e150, R308=1e308;
    const double pInf=std::numeric_limits<double>::infinity();
    const double radii[][2]={
       {1,1},{R,R},{1e154,1e154},{1e200,1e200},{R308,R308},
       {R,std::nextafter(R,pInf)},{R150,std::nextafter(R150,pInf)},
       {R308,std::nextafter(R308,pInf)},
       {0,R150},{R150,0},{0,R},{R,0},{1e-150,0},{1e-8,1e-8},
       {-R,-R},{-2*R,R},{R,-2*R},{-2*R,-3*R}
    };
    const int nPositive=14,nScenarios=int(sizeof(radii)/sizeof(radii[0]));
    const double xs[]={.1,.25,.49,std::nextafter(.5,0.),.5,std::nextafter(.5,1.),.500000000001,.51,.74,.9,-.1,1.2,-2.1,2.3};
    const double ys[]={.37,.64,-.31,1.19,2.24};
    Totals all;int trace=0;
    std::cout<<std::setprecision(17);
    for(int sc=0;sc<nScenarios;++sc){
      Site sites[2]={{.25,.5,radii[sc][0]},{.75,.5,radii[sc][1]}};
      unsigned long swrong=0,stested=0,sskip=0;
      for(int mask=0;mask<4;++mask)for(int blocks:{1,2,4})for(int reverse=0;reverse<2;++reverse){
        voro::container_poly_2d c(0,1,0,1,blocks,blocks,bool(mask&1),bool(mask&2),8);
        for(int k=0;k<2;++k){const int id=reverse?1-k:k;c.put(id,sites[id].x,sites[id].y,sites[id].r);}
        for(double x:xs)for(double y:ys){
          Query q={x,y};
          const Owner expected=reference(sites,q,mask,6),check=reference(sites,q,mask,5);
          if(expected.found!=check.found || (expected.found && (expected.id!=check.id || expected.x!=check.x || expected.y!=check.y))){
            std::cerr<<"ORACLE_NONCONVERGED 2d scenario="<<sc<<" mask="<<mask<<" query="<<x<<","<<y<<"\n";return 2;
          }
          if(expected.found&&expected.margin<=1e-12L){
            ++sskip;++all.skipped_tie;
            if(sc<nPositive){
              if(expected.margin==0)++all.exact_ties;
              else{
                double rrx=0,rry=0;int iid=-1;bool ff=c.find_voronoi_cell(x,y,rrx,rry,iid);
                ++all.near_tested;
                if(!(ff&&iid==expected.id&&std::fabs((long double)rrx-expected.x)<1e-9L&&std::fabs((long double)rry-expected.y)<1e-9L))++all.near_wrong;
              }
            }
            continue;
          }
          double rx=0,ry=0;int id=-1;const bool found=c.find_voronoi_cell(x,y,rx,ry,id);
          const bool good=found==expected.found && (!found || (id==expected.id && std::fabs((long double)rx-expected.x)<1e-9L && std::fabs((long double)ry-expected.y)<1e-9L));
          ++stested;
          if(sc<nPositive){++all.tested;if(!good){++all.wrong;++swrong;if(sc==0)++all.controls;}}
          else{++all.negative_tested;if(!good){++all.negative_wrong;++swrong;}}
          if(!expected.found){++all.outside_tested;if(!good)++all.outside_wrong;}
          if(!good && trace++<18)std::cout<<"MISMATCH dim=2 scenario="<<sc<<" mask="<<mask<<" blocks="<<blocks<<" reverse="<<reverse<<" query="<<x<<","<<y<<" expected="<<expected.found<<","<<expected.id<<" @"<<double(expected.x)<<","<<double(expected.y)<<" got="<<found<<","<<id<<" @"<<rx<<","<<ry<<" margin="<<double(expected.margin)<<"\n";
        }
      }
      std::cout<<"SCENARIO A11 dim=2 id="<<sc<<" radii="<<sites[0].r<<","<<sites[1].r<<" "<<(sc<nPositive?"accepted":"negative-diagnostic")<<" tested="<<stested<<" wrong="<<swrong<<" skipped_ties="<<sskip<<"\n";
    }
    std::cout<<"SUMMARY A11 dim=2 tested="<<all.tested<<" mismatches="<<all.wrong<<" controls="<<all.controls<<" skipped_ties="<<all.skipped_tie<<" outside_queries="<<all.outside_tested<<" outside_wrong="<<all.outside_wrong<<" near_tested="<<all.near_tested<<" near_wrong="<<all.near_wrong<<" exact_ties="<<all.exact_ties<<" negative_tested="<<all.negative_tested<<" negative_wrong="<<all.negative_wrong<<"\n";
    return all.controls?2:all.wrong?1:0;
}
