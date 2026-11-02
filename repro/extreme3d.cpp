// Stage 9: independent long-double shifted-power oracle for 3D owner search.
// Nonnegative radii are acceptance tests; negative radii and near ties diagnostic.
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
// Promote the exact native binary64 lattice inputs for the reference.
constexpr double skew_bxy=.23,skew_bxz=-.17,skew_byz=.31;
struct Site {double x,y,z,r;};
struct Query {double x,y,z;};
struct Owner {bool found;int id;long double x,y,z,margin;};
struct Totals {unsigned long tested=0,wrong=0,controls=0,tie_skips=0,negative_tested=0,negative_wrong=0,outside_tested=0,outside_wrong=0,near_tested=0,near_wrong=0,exact_ties=0;};
Owner reference(const Site* ss,Query q,int mask,int K){
  const bool dedicated=mask>=8,skew=mask==9;
  const bool px=dedicated||bool(mask&1),py=dedicated||bool(mask&2),pz=dedicated||bool(mask&4);
  if((!px&&(q.x<0||q.x>=1))||(!py&&(q.y<0||q.y>=1))||(!pz&&(q.z<0||q.z>=1)))return Owner{false,-1,0,0,0,0};
  const long double bxy=skew?static_cast<long double>(skew_bxy):0,
       bxz=skew?static_cast<long double>(skew_bxz):0,
       byz=skew?static_cast<long double>(skew_byz):0;
  const long double R=std::max(std::fabs((long double)ss[0].r),std::fabs((long double)ss[1].r));
  long double best=std::numeric_limits<long double>::infinity(),next=best;
  Owner out={false,-1,0,0,0,0};
  for(int j=0;j<2;++j)for(int kz=pz?-K:0;kz<=(pz?K:0);++kz)
    for(int ky=py?-K:0;ky<=(py?K:0);++ky)
      for(int kx=px?-K:0;kx<=(px?K:0);++kx){
        const long double x=(long double)ss[j].x+kx+ky*bxy+kz*bxz;
        const long double y=(long double)ss[j].y+ky+kz*byz;
        const long double z=(long double)ss[j].z+kz,r=ss[j].r;
        const long double dx=x-q.x,dy=y-q.y,dz=z-q.z;
        const long double ar=std::fabs(r);
        const long double radius_gap=(ar==R)?0:(R-ar)*(R+ar);
        const long double val=dx*dx+dy*dy+dz*dz+radius_gap;
        if(val<best){next=best;best=val;out=Owner{true,j,x,y,z,0};}
        else if(val<next)next=val;
      }
  out.margin=next-best;return out;
}
template<class Container> bool native(Container& c,Query q,const Owner& expected){
 double rx=0,ry=0,rz=0;int id=-1;const bool found=c.find_voronoi_cell(q.x,q.y,q.z,rx,ry,rz,id);
 return found==expected.found&&(!found||(id==expected.id&&std::fabs((long double)rx-expected.x)<1e-9L&&std::fabs((long double)ry-expected.y)<1e-9L&&std::fabs((long double)rz-expected.z)<1e-9L));
}
int main(){
 const double R=1e8,R150=1e150,R308=1e308;
 const double INF=std::numeric_limits<double>::infinity();
 const double radii[][2]={{1,1},{R,R},{1e154,1e154},{1e200,1e200},{R308,R308},
 {R,std::nextafter(R,INF)},{R150,std::nextafter(R150,INF)},{R308,std::nextafter(R308,INF)},
 {0,R150},{R150,0},{0,R},{R,0},{1e-150,0},{1e-8,1e-8},
 {-R,-R},{-2*R,R},{R,-2*R},{-2*R,-3*R}};
 const int npos=14,ns=int(sizeof(radii)/sizeof(radii[0]));
 const double xx[]={.1,.49,.5,.51,.74,-.1,1.2,2.3};
 const double yy[]={.37,.64,1.19};
 const double zz[]={.31,-.1,1.42};
 Totals all;int trace=0;
 std::cout<<std::setprecision(17);
 for(int sc=0;sc<ns;++sc){
   Site ss[2]={{.25,.5,.5,radii[sc][0]},{.75,.5,.5,radii[sc][1]}};
   unsigned long tested=0,wrong=0,skipped_ties=0;
   for(int mask=0;mask<10;++mask){
    const bool dedicated=mask>=8,skew=mask==9;
    std::vector<Query> qs;std::vector<Owner> refs;std::vector<bool> skipped;
    for(double x:xx)for(double y:yy)for(double z:zz){
      Query q={x,y,z};const Owner o=reference(ss,q,mask,6),p=reference(ss,q,mask,5);
      if(o.found!=p.found||(o.found&&(o.id!=p.id||o.x!=p.x||o.y!=p.y||o.z!=p.z))){
        std::cerr<<"ORACLE_NOT_CONVERGED 3D sc="<<sc<<" mask="<<mask<<" x="<<x<<" y="<<y<<" z="<<z<<"\n";return 2;
      }
      qs.push_back(q);refs.push_back(o);skipped.push_back(o.found&&o.margin<=1e-12L);
    }
    for(int blocks:{1,2,4})for(int reverse=0;reverse<2;++reverse){
      if(dedicated){
        voro::container_periodic_poly c(1,skew?skew_bxy:0,1,skew?skew_bxz:0,skew?skew_byz:0,1,blocks,blocks,blocks,8);
        for(int i=0;i<2;++i){int id=reverse?1-i:i;c.put(id,ss[id].x,ss[id].y,ss[id].z,ss[id].r);}
        for(size_t iq=0;iq<qs.size();++iq){
          if(skipped[iq]){
            ++all.tie_skips;++skipped_ties;
            if(sc<npos){
              if(refs[iq].margin==0)++all.exact_ties;
              else{++all.near_tested;if(!native(c,qs[iq],refs[iq])){++all.near_wrong;if(all.near_wrong<=16)std::cout<<"NEAR_MISMATCH sc="<<sc<<" mask="<<mask<<" grid="<<blocks<<" reverse="<<reverse<<" query="<<qs[iq].x<<","<<qs[iq].y<<","<<qs[iq].z<<" margin="<<double(refs[iq].margin)<<" expected="<<refs[iq].id<<" @"<<double(refs[iq].x)<<","<<double(refs[iq].y)<<","<<double(refs[iq].z)<<"\n";}}
            }
            continue;
          }
          const Query q=qs[iq];const Owner o=refs[iq];const bool good=native(c,q,o);
          ++tested;
          if(sc<npos){++all.tested;if(!good){++all.wrong;++wrong;if(sc==0)++all.controls;}}
          else{++all.negative_tested;if(!good){++all.negative_wrong;++wrong;}}
          if(!o.found){++all.outside_tested;if(!good)++all.outside_wrong;}
          if(!good&&trace++<10)std::cout<<"MISMATCH dim=3 sc="<<sc<<" mask="<<mask<<" blocks="<<blocks<<" reverse="<<reverse<<" x="<<q.x<<" y="<<q.y<<" z="<<q.z<<" expected="<<o.found<<","<<o.id<<" @"<<double(o.x)<<","<<double(o.y)<<","<<double(o.z)<<" margin="<<double(o.margin)<<"\n";
        }
      }else{
        voro::container_poly c(0,1,0,1,0,1,blocks,blocks,blocks,bool(mask&1),bool(mask&2),bool(mask&4),8);
        for(int i=0;i<2;++i){int id=reverse?1-i:i;c.put(id,ss[id].x,ss[id].y,ss[id].z,ss[id].r);}
        for(size_t iq=0;iq<qs.size();++iq){
          if(skipped[iq]){
            ++all.tie_skips;++skipped_ties;
            if(sc<npos){
              if(refs[iq].margin==0)++all.exact_ties;
              else{++all.near_tested;if(!native(c,qs[iq],refs[iq])){++all.near_wrong;if(all.near_wrong<=16)std::cout<<"NEAR_MISMATCH sc="<<sc<<" mask="<<mask<<" grid="<<blocks<<" reverse="<<reverse<<" query="<<qs[iq].x<<","<<qs[iq].y<<","<<qs[iq].z<<" margin="<<double(refs[iq].margin)<<" expected="<<refs[iq].id<<" @"<<double(refs[iq].x)<<","<<double(refs[iq].y)<<","<<double(refs[iq].z)<<"\n";}}
            }
            continue;
          }
          const Query q=qs[iq];const Owner o=refs[iq];const bool good=native(c,q,o);
          ++tested;
          if(sc<npos){++all.tested;if(!good){++all.wrong;++wrong;if(sc==0)++all.controls;}}
          else{++all.negative_tested;if(!good){++all.negative_wrong;++wrong;}}
          if(!o.found){++all.outside_tested;if(!good)++all.outside_wrong;}
          if(!good&&trace++<10)std::cout<<"MISMATCH dim=3 sc="<<sc<<" mask="<<mask<<" blocks="<<blocks<<" reverse="<<reverse<<" x="<<q.x<<" y="<<q.y<<" z="<<q.z<<" expected="<<o.found<<","<<o.id<<" @"<<double(o.x)<<","<<double(o.y)<<","<<double(o.z)<<" margin="<<double(o.margin)<<"\n";
        }
      }
    }
   }
   std::cout<<"SCENARIO A11 dim=3 id="<<sc<<" r0="<<ss[0].r<<" r1="<<ss[1].r<<" "<<(sc<npos?"accepted":"negative-diagnostic")<<" tested="<<tested<<" wrong="<<wrong<<" skipped_ties="<<skipped_ties<<"\n";
 }
 std::cout<<"SUMMARY A11 dim=3 tested="<<all.tested<<" mismatches="<<all.wrong<<" controls="<<all.controls<<" skipped_ties="<<all.tie_skips<<" outside_queries="<<all.outside_tested<<" outside_wrong="<<all.outside_wrong<<" near_tested="<<all.near_tested<<" near_wrong="<<all.near_wrong<<" exact_ties="<<all.exact_ties<<" negative_tested="<<all.negative_tested<<" negative_wrong="<<all.negative_wrong<<"\n";
 return all.controls?2:all.wrong?1:0;
}
