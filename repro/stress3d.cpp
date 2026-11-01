// Stage 7: noncoplanar weighted 3D cells vs exhaustive direct-plane clipping.
// The reference uses Voro++'s low-level convex clipping primitive, but does
// not call radius_poly, compute_cell, periodic image generators, or block pruning.
// Deterministic C++11 native test with no external dependencies.
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

namespace {
const long double L=10;
struct Site {int id;double x,y,z,r;};
struct Shape {bool active;double volume,cx,cy,cz,xlo,xhi,ylo,yhi,zlo,zhi;};
struct Stat {int configurations,failures,controls,failcase;Stat():configurations(0),failures(0),controls(0),failcase(0){}};
uint32_t step(uint32_t& s){return s=1664525u*s+1013904223u;}
std::vector<Site> sites(int seed,int pattern){
    const double big=1e8,up=std::nextafter(big,std::numeric_limits<double>::infinity())-big;
    const double offsets[]={0,2,-3,4,-5,1,-1,3,-2};
    uint32_t rng=0x9e3779b9u+uint32_t(seed+1)*1529u;
    std::vector<Site> out;
    for(int i=0;i<9;++i){
        Site p={i,0,0,0,0};
        for(int attempt=0;attempt<500;++attempt){
            p.x=1.+(step(rng)%8000)/1000.;
            p.y=1.+(step(rng)%8000)/1000.;
            p.z=1.+(step(rng)%8000)/1000.;
            bool far=true;
            for(size_t k=0;k<out.size();++k){
                const double dx=p.x-out[k].x,dy=p.y-out[k].y,dz=p.z-out[k].z;
                if(dx*dx+dy*dy+dz*dz<1.){far=false;break;}
            }
            if(far)break;
            if(attempt==499){std::cerr<<"ORACLE_INVALID positions too close\n";std::exit(2);}
        }
        if(pattern==0)p.r=1.+(i*7+seed*3)%5;
        if(pattern==1)p.r=big;
        if(pattern==2)p.r=big+offsets[(i+seed)%9]*up;
        if(pattern==3)p.r=big+offsets[(2*i+seed)%9]*up*3;
        if(pattern==4)p.r=big+(i==0?12:-3-i)*up;
        if(pattern==5)p.r=i==0?big:0.;
        out.push_back(p);
    }
    return out;
}

Shape empty(){return Shape{false,0,0,0,0,0,0,0,0,0,0};}
Shape shape(voro::voronoicell_neighbor& cell,const Site& s){
    const double volume=cell.volume();
    if(!(std::isfinite(volume)&&volume>=0)){
        std::cerr<<"ORACLE_INVALID nonfinite or negative volume\n";std::exit(2);
    }
    if(volume<=1e-9)return empty();
    std::vector<double> verts;cell.vertices(s.x,s.y,s.z,verts);
    if(verts.empty()){std::cerr<<"ORACLE_INVALID positive volume no vertices\n";std::exit(2);}
    Shape sh={true,volume,0,0,0,1e100,-1e100,1e100,-1e100,1e100,-1e100};
    cell.centroid(sh.cx,sh.cy,sh.cz);sh.cx+=s.x;sh.cy+=s.y;sh.cz+=s.z;
    for(size_t k=0;k+2<verts.size();k+=3){
        sh.xlo=std::min(sh.xlo,verts[k]);sh.xhi=std::max(sh.xhi,verts[k]);
        sh.ylo=std::min(sh.ylo,verts[k+1]);sh.yhi=std::max(sh.yhi,verts[k+1]);
        sh.zlo=std::min(sh.zlo,verts[k+2]);sh.zhi=std::max(sh.zhi,verts[k+2]);
    }
    return sh;
}

bool equivalent(const Shape& a,const Shape& b,double& maxerr){
    if(a.active!=b.active)return false;
    if(!a.active)return true;
    const double x[]={a.volume,a.cx,a.cy,a.cz,a.xlo,a.xhi,a.ylo,a.yhi,a.zlo,a.zhi};
    const double y[]={b.volume,b.cx,b.cy,b.cz,b.xlo,b.xhi,b.ylo,b.yhi,b.zlo,b.zhi};
    bool okay=true;
    for(int k=0;k<10;++k){const double delta=std::fabs(x[k]-y[k]);maxerr=std::max(maxerr,delta);okay=okay&&std::isfinite(x[k])&&delta<2e-5;}
    return okay;
}

// mask 0..7: rectangular periodic axes. mask 8: orthogonal dedicated
// periodic container; mask 9: true triclinic dedicated periodic container.
Shape reference(const std::vector<Site>& sites,int id,int mask,int K){
    const Site& s=sites[id];
    const bool triclinic=mask>=8;
    const bool skew=mask==9;
    const bool px=triclinic||(mask&1),py=triclinic||(mask&2),pz=triclinic||(mask&4);
    const double bxy=skew?2.3:0.,bxz=skew?-1.7:0.,byz=skew?3.1:0.;
    voro::voronoicell_neighbor cell;
    // Rectangular cells are bounded by box walls and nearest self images.
    // For the skew lattice, a generous initial box is made nonbinding by
    // periodic self-image planes; K=1 versus K=2 checks convergence.
    if(triclinic){cell.init(-22,22,-22,22,-22,22);}
    else{
        cell.init(px?-5:-s.x,px?5:10-s.x,py?-5:-s.y,py?5:10-s.y,pz?-5:-s.z,pz?5:10-s.z);
    }
    for(int kz=pz?-K:0;kz<=(pz?K:0);++kz)
      for(int ky=py?-K:0;ky<=(py?K:0);++ky)
        for(int kx=px?-K:0;kx<=(px?K:0);++kx)
          for(size_t j=0;j<sites.size();++j){
            if(int(j)==id&&kx==0&&ky==0&&kz==0)continue;
            const long double dx=sites[j].x+kx*L+ky*bxy+kz*bxz-s.x;
            const long double dy=sites[j].y+ky*L+kz*byz-s.y;
            const long double dz=sites[j].z+kz*L-s.z;
            const long double ri=s.r,rj=sites[j].r;
            const long double rhs=dx*dx+dy*dy+dz*dz+(ri-rj)*(ri+rj);
            if(!cell.nplane(double(dx),double(dy),double(dz),double(rhs),int(j)))return empty();
          }
    return shape(cell,s);
}

std::vector<Shape> actual(const std::vector<Site>& sites,int mask,int blocks,bool reverse){
    std::vector<Shape> out(sites.size(),empty());
    if(mask<8){
        voro::container_poly con(0,10,0,10,0,10,blocks,blocks,blocks,mask&1,mask&2,mask&4,8);
        for(size_t k=0;k<sites.size();++k){int i=reverse?int(sites.size()-1-k):int(k);const Site& s=sites[i];con.put(i,s.x,s.y,s.z,s.r);}
        voro::c_loop_all loop(con);voro::voronoicell_neighbor c;
        if(loop.start())do{
            int id;double x,y,z,r;loop.pos(id,x,y,z,r);
            if(con.compute_cell(c,loop)){if(id<0||id>=int(out.size())){std::cerr<<"ORACLE_INVALID id\n";std::exit(2);}out[id]=shape(c,sites[id]);}
        }while(loop.inc());
    }else{
        const bool skew=mask==9;
        voro::container_periodic_poly con(10,skew?2.3:0,10,skew?-1.7:0,skew?3.1:0,10,blocks,blocks,blocks,8);
        for(size_t k=0;k<sites.size();++k){int i=reverse?int(sites.size()-1-k):int(k);const Site& s=sites[i];con.put(i,s.x,s.y,s.z,s.r);}
        voro::c_loop_all_periodic loop(con);voro::voronoicell_neighbor c;
        if(loop.start())do{
            int id;double x,y,z,r;loop.pos(id,x,y,z,r);
            if(con.compute_cell(c,loop)){if(id<0||id>=int(out.size())){std::cerr<<"ORACLE_INVALID id\n";std::exit(2);}out[id]=shape(c,sites[id]);}
        }while(loop.inc());
    }
    return out;
}

void run(const std::vector<Site>& ss,int seed,int pattern,int mask,Stat& stat){
    std::vector<Shape> refs;long double total=0;
    for(size_t i=0;i<ss.size();++i){
        const Shape a=reference(ss,int(i),mask,2);
        const Shape b=reference(ss,int(i),mask,1);
        double error=0;
        if(!equivalent(a,b,error)){
            std::cerr<<"ORACLE_NOT_CONVERGED dim=3 seed="<<seed<<" pattern="<<pattern<<" mask="<<mask<<" id="<<i<<" diff="<<error<<" volumes="<<a.volume<<","<<b.volume<<"\n";std::exit(2);
        }
        refs.push_back(a);total+=a.volume;
    }
    if(std::fabs(total-1000.)>5e-4L){std::cerr<<"ORACLE_BAD_VOLUME dim=3 seed="<<seed<<" pattern="<<pattern<<" mask="<<mask<<" total="<<double(total)<<"\n";std::exit(2);}
    for(int blocks=1;blocks<=3;blocks+=2)for(int reverse=0;reverse<2;++reverse){
        const std::vector<Shape> got=actual(ss,mask,blocks,reverse!=0);
        bool pass=true;int bad=0;double maxerr=0,sum=0;
        for(size_t i=0;i<ss.size();++i){
            if(got[i].active)sum+=got[i].volume;
            if(!equivalent(got[i],refs[i],maxerr)){pass=false;++bad;}
        }
        if(std::fabs(sum-1000.)>5e-4){pass=false;maxerr=std::max(maxerr,std::fabs(sum-1000.));}
        ++stat.configurations;
        if(!pass){++stat.failures;if(pattern==0)++stat.controls;++stat.failcase;
            if(stat.failcase<=50)std::cout<<"FAIL_CASE dim=3 seed="<<seed<<" pattern="<<pattern<<" mask="<<mask<<" blocks="<<blocks<<" reverse="<<reverse<<" count="<<bad<<" maxerr="<<maxerr<<" sum="<<sum<<"\n";
        }
    }
}
} // namespace
int main(){
    std::cout<<std::setprecision(17);Stat stat;
    for(int seed=0;seed<4;++seed)for(int pattern=0;pattern<6;++pattern){
        const std::vector<Site> ss=sites(seed,pattern);
        for(int mask=0;mask<10;++mask)run(ss,seed,pattern,mask,stat);
    }
    std::cout<<"SUMMARY A9 dim=3 cases="<<stat.configurations<<" failures="<<stat.failures<<" controls="<<stat.controls<<"\n";
    return stat.controls?2:stat.failures?1:0;
}
