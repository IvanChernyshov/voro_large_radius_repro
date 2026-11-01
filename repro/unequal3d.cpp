// Stage 6: nonuniform-radius power cells across rectangular and skew 3D lattices.
// Analytic 1D oracle for rectangular cells; exhaustive direct-plane oracle
// (no radius_poly or block-pruning) for the skew periodic container.
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
const double L=10.;
struct Site {int id;double x,y,z,r;};
struct Expected1D {bool present;long double lo,hi,volume;};
struct Cell {int id;double vol,xmin,xmax,ymin,ymax,zmin,zmax;};
struct Counts {int cases,wrong,controls;Counts():cases(0),wrong(0),controls(0){}};
struct Scenario {const char* name;int count;double r[3];bool control;};

Expected1D interval(const Site* pts,int count,int idx,bool px){
    const long double xi=pts[idx].x,ri=pts[idx].r;
    long double lo=px?xi-L/2:0,hi=px?xi+L/2:L;
    for(int j=0;j<count;++j)for(int k=(px?-2:0);k<=(px?2:0);++k){
        if(j==idx&&k==0)continue;
        const long double xj=static_cast<long double>(pts[j].x)+k*L;
        const long double dx=xj-xi;
        if(dx==0)continue;
        const long double rj=pts[j].r;
        const long double boundary=(xi+xj)/2+(ri-rj)*(ri+rj)/(2*dx);
        if(dx>0)hi=std::min(hi,boundary);
        else lo=std::max(lo,boundary);
    }
    const bool present=hi>lo+1e-12L;
    return Expected1D{present,lo,hi,present?(hi-lo)*L*L:0};
}

bool near(double val,long double expected,double tol=5e-6){
    return std::isfinite(val)&&std::fabs(static_cast<long double>(val)-expected)<tol;
}

template<class Container,class Loop>
std::vector<Cell> collect(Container& con,Loop& loop){
    std::vector<Cell> out;voro::voronoicell_neighbor cell;
    if(loop.start())do{
        int id;double x,y,z,r;loop.pos(id,x,y,z,r);
        if(!con.compute_cell(cell,loop))continue;
        std::vector<double> v;cell.vertices(x,y,z,v);
        double lo[3]={1e99,1e99,1e99},hi[3]={-1e99,-1e99,-1e99};
        for(std::size_t j=0;j+2<v.size();j+=3)for(int k=0;k<3;++k){lo[k]=std::min(lo[k],v[j+k]);hi[k]=std::max(hi[k],v[j+k]);}
        out.push_back(Cell{id,cell.volume(),lo[0],hi[0],lo[1],hi[1],lo[2],hi[2]});
    }while(loop.inc());
    std::sort(out.begin(),out.end(),[](const Cell&a,const Cell&b){return a.id<b.id;});
    return out;
}

void init_sites(const Scenario& s,Site* sites){
    const double xs[3]={2,5,8};
    for(int i=0;i<s.count;++i){
        const double x=s.count==2?(i==0?2.:8.):xs[i];
        sites[i]=Site{i,x,5.,5.,s.r[i]};
    }
}

void rectangular(const Scenario& s,int mask,int blocks,bool reversed,Counts& counts){
    Site sites[3];init_sites(s,sites);
    voro::container_poly con(0,L,0,L,0,L,blocks,blocks,blocks,mask&1,mask&2,mask&4,8);
    for(int k=0;k<s.count;++k){int j=reversed?s.count-1-k:k;con.put(j,sites[j].x,sites[j].y,sites[j].z,sites[j].r);}
    voro::c_loop_all it(con);
    const std::vector<Cell> cells=collect(con,it);
    bool good=true;double sum=0,err=0;long double refsum=0;
    for(int i=0;i<s.count;++i){
        const Expected1D e=interval(sites,s.count,i,mask&1);refsum+=e.volume;
        const std::vector<Cell>::const_iterator a=std::find_if(cells.begin(),cells.end(),[i](const Cell& c){return c.id==i;});
        if(e.present){
            if(a==cells.end()){good=false;err=std::max(err,static_cast<double>(e.volume));}
            else{
                sum+=a->vol;
                err=std::max(err,static_cast<double>(std::fabs(a->vol-e.volume)));
                err=std::max(err,static_cast<double>(std::fabs(a->xmin-e.lo)));
                err=std::max(err,static_cast<double>(std::fabs(a->xmax-e.hi)));
                good=good&&near(a->vol,e.volume)&&near(a->xmin,e.lo)&&near(a->xmax,e.hi);
            }
        }else if(a!=cells.end()){
            sum+=a->vol;
            if(a->vol>1e-6){good=false;err=std::max(err,a->vol);}
        }
    }
    if(!near(static_cast<double>(refsum),L*L*L,1e-8)){
        std::cerr<<"ORACLE_FAIL rectangular="<<s.name<<" total="<<static_cast<double>(refsum)<<'\n';std::exit(2);
    }
    good=good&&near(sum,L*L*L);++counts.cases;
    if(!good){++counts.wrong;if(s.control)++counts.controls;}
    std::cout<<"CASE A8 dim=3 container=rect scenario="<<s.name
             <<" periodic="<<((mask&1)?1:0)<<((mask&2)?1:0)<<((mask&4)?1:0)
             <<" sites="<<s.count<<" blocks="<<blocks<<" reverse="<<reversed
             <<" status="<<(good?"PASS":"FAIL")<<" count="<<cells.size()
             <<" total="<<sum<<" maxerror="<<err<<'\n';
}

struct RefCell {bool present;Cell c;};
RefCell brute_clip(const Site* pts,int n,int i,bool skew,int K){
    // For each particle, clip a generous box by ALL planes from a full
    // neighbourhood of period images, without container search/pruning.
    // This shares Voro++'s primitive convex clipping algorithm (not an
    // arbitrary-precision fully independent polyhedral geometry oracle).
    const double bxy=skew?2.3:0,bxz=skew?-1.7:0,byz=skew?3.1:0;
    voro::voronoicell cell;cell.init(-40,40,-40,40,-40,40);
    bool alive=true;
    for(int kz=-K;kz<=K&&alive;++kz)
      for(int ky=-K;ky<=K&&alive;++ky)
        for(int kx=-K;kx<=K&&alive;++kx)
          for(int j=0;j<n&&alive;++j){
            if(i==j&&kx==0&&ky==0&&kz==0)continue;
            const double dx=pts[j].x+kx*L+ky*bxy+kz*bxz-pts[i].x;
            const double dy=pts[j].y+ky*L+kz*byz-pts[i].y;
            const double dz=pts[j].z+kz*L-pts[i].z;
            const long double ri=pts[i].r,rj=pts[j].r;
            const long double plane=static_cast<long double>(dx)*dx+static_cast<long double>(dy)*dy+
                 static_cast<long double>(dz)*dz+(ri-rj)*(ri+rj);
            alive=cell.nplane(dx,dy,dz,static_cast<double>(plane),j);
          }
    if(!alive)return RefCell{false,Cell{pts[i].id,0,0,0,0,0,0,0}};
    std::vector<double> v;cell.vertices(pts[i].x,pts[i].y,pts[i].z,v);
    double lo[3]={1e99,1e99,1e99},hi[3]={-1e99,-1e99,-1e99};
    for(std::size_t j=0;j+2<v.size();j+=3)for(int k=0;k<3;++k){lo[k]=std::min(lo[k],v[j+k]);hi[k]=std::max(hi[k],v[j+k]);}
    const double vol=cell.volume();
    return RefCell{vol>1e-8,Cell{pts[i].id,vol,lo[0],hi[0],lo[1],hi[1],lo[2],hi[2]}};
}

void triclinic(const Scenario& s,bool skew,int blocks,bool reversed,Counts& cnt){
    Site sites[3];init_sites(s,sites);
    const double bxy=skew?2.3:0,bxz=skew?-1.7:0,byz=skew?3.1:0;
    voro::container_periodic_poly con(L,bxy,L,bxz,byz,L,blocks,blocks,blocks,8);
    for(int k=0;k<s.count;++k){int j=reversed?s.count-1-k:k;con.put(j,sites[j].x,sites[j].y,sites[j].z,sites[j].r);}
    voro::c_loop_all_periodic it(con);
    const std::vector<Cell> cells=collect(con,it);
    double sum=0,maxerr=0;long double refsum=0;bool good=true;
    for(int i=0;i<s.count;++i){
        const RefCell ref=brute_clip(sites,s.count,i,skew,2);
        const RefCell ref1=brute_clip(sites,s.count,i,skew,1);
        if(ref.present!=ref1.present||(ref.present&&std::fabs(ref.c.vol-ref1.c.vol)>5e-6)){
            std::cerr<<"ORACLE_DID_NOT_CONVERGE "<<s.name<<" skew="<<skew<<" site="<<i
                     <<" K1="<<ref1.c.vol<<" K2="<<ref.c.vol<<'\n';std::exit(2);
        }
        refsum+=ref.c.vol;
        std::vector<Cell>::const_iterator a=std::find_if(cells.begin(),cells.end(),[i](const Cell& c){return c.id==i;});
        if(!ref.present){
            if(a!=cells.end()){sum+=a->vol;if(a->vol>1e-6){good=false;maxerr=std::max(maxerr,a->vol);}}
        }else if(a==cells.end()){
            good=false;maxerr=std::max(maxerr,ref.c.vol);
        }else{
            sum+=a->vol;
            const double d=std::fabs(a->vol-ref.c.vol);
            maxerr=std::max(maxerr,d);good=good&&near(a->vol,ref.c.vol);
            // Individual vertex-coordinate extents catch wrong shape even
            // when volume appears deceptively plausible.
            for(int k=0;k<3;++k){
                double avlo=k==0?a->xmin:k==1?a->ymin:a->zmin;
                double avhi=k==0?a->xmax:k==1?a->ymax:a->zmax;
                double elo=k==0?ref.c.xmin:k==1?ref.c.ymin:ref.c.zmin;
                double ehi=k==0?ref.c.xmax:k==1?ref.c.ymax:ref.c.zmax;
                maxerr=std::max(maxerr,std::fabs(avlo-elo));
                maxerr=std::max(maxerr,std::fabs(avhi-ehi));
                good=good&&near(avlo,elo)&&near(avhi,ehi);
            }
        }
    }
    if(!near(static_cast<double>(refsum),L*L*L,5e-5)){
        std::cerr<<"ORACLE_VOLUME_ERROR "<<s.name<<" skew="<<skew<<" volume="<<static_cast<double>(refsum)<<"\n";
        std::exit(2);
    }
    good=good&&near(sum,L*L*L,5e-5);++cnt.cases;
    if(!good){++cnt.wrong;if(s.control)++cnt.controls;}
    std::cout<<"CASE A8 dim=3 container=periodic lattice="<<(skew?"skew":"orthogonal")
             <<" scenario="<<s.name<<" sites="<<s.count<<" blocks="<<blocks
             <<" reverse="<<reversed<<" status="<<(good?"PASS":"FAIL")
             <<" count="<<cells.size()<<" total="<<sum<<" maxerror="<<maxerr<<'\n';
}
}

int main(){
    const double r=1e8;
    const double up=std::nextafter(r,std::numeric_limits<double>::infinity());
    const double dn=std::nextafter(r,-std::numeric_limits<double>::infinity());
    const Scenario scenarios[]={
        {"moderate-3",3,{3.,1.,2.},true},
        {"adjacent-2",2,{r,up,0.},false},
        {"adjacent-3",3,{up,r,dn},false},
        {"ulps-3",3,{r+4*(up-r),r,r-2*(r-dn)},false},
        {"dominating-3",3,{r,0.,0.},false},
        {"empty-middle-3",3,{r,0.,r},false},
        {"medium-weight-3",3,{10000.,9999.999,9999.998},true}
    };
    Counts cnt;std::cout<<std::setprecision(17);
    for(std::size_t q=0;q<sizeof(scenarios)/sizeof(scenarios[0]);++q){
        for(int mask=0;mask<8;++mask)for(int b=1;b<=4;b*=2)for(int rev=0;rev<2;++rev)
            rectangular(scenarios[q],mask,b,rev,cnt);
        for(int skew=0;skew<2;++skew)for(int b=1;b<=4;b*=2)
            for(int rev=0;rev<2;++rev)triclinic(scenarios[q],skew,b,rev,cnt);
    }
    std::cout<<"SUMMARY A8 dim=3 cases="<<cnt.cases<<" mismatches="<<cnt.wrong
             <<" control_failures="<<cnt.controls<<'\n';
    return cnt.controls?2:cnt.wrong?1:0;
}
