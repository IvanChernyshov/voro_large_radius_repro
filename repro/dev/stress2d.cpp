// Stage 7: non-collinear weighted 2D cells, compared to a standalone
// long-double convex polygon half-plane clipping reference (no Voro++ oracle).
// Deterministic test data, C++11, no external dependencies.
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
struct Point {long double x,y;};
struct Site {int id;double x,y,r;};
struct Shape {bool active;double measure,cx,cy,xlo,xhi,ylo,yhi;};
struct Case {int seed,pattern,mask,blocks,reverse;};
struct Stat {int configurations,failures,controls,failcase;Stat():configurations(0),failures(0),controls(0),failcase(0){}};

uint32_t step(uint32_t& s){return s=1664525u*s+1013904223u;}
std::vector<Site> sites(int seed,int pattern){
    const double big=1e8;
    const double up=std::nextafter(big,std::numeric_limits<double>::infinity())-big;
    const double offsets[]={0,2,-3,4,-5,1,-1,3,-2};
    uint32_t rng=0x9e3779b9u + uint32_t(seed+1)*1529u;
    std::vector<Site> out;
    for(int i=0;i<9;++i){
        Site p={i,0,0,0};
        for(int attempt=0;attempt<500;++attempt){
            p.x=1.+(step(rng)%8000)/1000.;
            p.y=1.+(step(rng)%8000)/1000.;
            bool far=true;
            for(std::size_t k=0;k<out.size();++k){
                const double dx=p.x-out[k].x,dy=p.y-out[k].y;
                if(dx*dx+dy*dy<0.9*0.9){far=false;break;}
            }
            if(far)break;
            if(attempt==499){std::cerr<<"ORACLE_INVALID positions too close\n";std::exit(2);}
        }
        if(pattern==0) p.r=1.+(i*7+seed*3)%5;
        if(pattern==1) p.r=big;
        if(pattern==2) p.r=big+offsets[(i+seed)%9]*up;
        if(pattern==3) p.r=big+offsets[(2*i+seed)%9]*up*3;
        if(pattern==4) p.r=big+(i==0?12:-3-i)*up;
        if(pattern==5) p.r=(i==0?big:0.);
        out.push_back(p);
    }
    return out;
}

long double signed_plane(const Point& p,const Point& d,long double half_rhs){
    return d.x*p.x+d.y*p.y-half_rhs;
}
std::vector<Point> polygon_clip(const std::vector<Point>& input,const Point& d,long double half_rhs){
    std::vector<Point> result;
    if(input.empty())return result;
    const size_t sz=input.size();
    for(size_t j=0;j<sz;++j){
        const Point& a=input[j];const Point& b=input[(j+1)%sz];
        const long double fa=signed_plane(a,d,half_rhs),fb=signed_plane(b,d,half_rhs);
        const bool ia=fa<=0,ib=fb<=0;
        if(ia&&ib){result.push_back(b);}
        else if(ia!=ib){
            const long double denominator=fa-fb;
            if(denominator==0){std::cerr<<"ORACLE_INVALID zero intersection denominator\n";std::exit(2);}
            const long double t=fa/denominator;
            Point cross={a.x+t*(b.x-a.x),a.y+t*(b.y-a.y)};
            result.push_back(cross);
            if(ib)result.push_back(b);
        }
    }
    return result;
}
Shape from_polygon(const std::vector<Point>& poly,const Site& s){
    Shape sh={false,0,0,0,0,0,0,0};
    if(poly.size()<3)return sh;
    long double crosssum=0,cxsum=0,cysum=0;
    long double xlo=1e100L,xhi=-1e100L,ylo=1e100L,yhi=-1e100L;
    for(size_t j=0;j<poly.size();++j){
        const Point& a=poly[j];const Point& b=poly[(j+1)%poly.size()];
        const long double cross=a.x*b.y-b.x*a.y;
        crosssum+=cross;cxsum+=(a.x+b.x)*cross;cysum+=(a.y+b.y)*cross;
        xlo=std::min(xlo,a.x);xhi=std::max(xhi,a.x);
        ylo=std::min(ylo,a.y);yhi=std::max(yhi,a.y);
    }
    const long double area=std::fabs(crosssum)/2;
    if(area<1e-10L)return sh;
    if(crosssum==0){std::cerr<<"ORACLE_INVALID zero area centroid\n";std::exit(2);}
    sh.active=true;sh.measure=double(area);
    sh.cx=double(s.x+cxsum/(3*crosssum));sh.cy=double(s.y+cysum/(3*crosssum));
    sh.xlo=double(s.x+xlo);sh.xhi=double(s.x+xhi);
    sh.ylo=double(s.y+ylo);sh.yhi=double(s.y+yhi);
    return sh;
}
Shape reference(const std::vector<Site>& sites,int i,int mask,int K){
    const Site& s=sites[i];
    const long double xmin=(mask&1)?-L/2:-s.x, xmax=(mask&1)?L/2:L-s.x;
    const long double ymin=(mask&2)?-L/2:-s.y, ymax=(mask&2)?L/2:L-s.y;
    std::vector<Point> p={{xmin,ymin},{xmax,ymin},{xmax,ymax},{xmin,ymax}};
    for(size_t j=0;j<sites.size();++j){
        for(int ky=(mask&2)?-K:0;ky<=((mask&2)?K:0);++ky){
            for(int kx=(mask&1)?-K:0;kx<=((mask&1)?K:0);++kx){
                if(i==int(j)&&kx==0&&ky==0)continue;
                const long double dx=sites[j].x+kx*L-s.x,dy=sites[j].y+ky*L-s.y;
                const long double ri=s.r,rj=sites[j].r;
                const long double rhs=(dx*dx+dy*dy+(ri-rj)*(ri+rj))/2;
                p=polygon_clip(p,Point{dx,dy},rhs);
                if(p.empty())return Shape{false,0,0,0,0,0,0,0};
            }
        }
    }
    return from_polygon(p,s);
}

std::vector<Shape> actual(const std::vector<Site>& sites,int mask,int blocks,bool reverse){
    voro::container_poly_2d con(0,10,0,10,blocks,blocks,bool(mask&1),bool(mask&2),8);
    for(size_t k=0;k<sites.size();++k){int i=reverse?int(sites.size()-1-k):int(k);const Site& s=sites[i];con.put(s.id,s.x,s.y,s.r);}
    std::vector<Shape> out(sites.size(),Shape{false,0,0,0,0,0,0,0});
    voro::voronoicell_neighbor_2d cell;
    for(auto it=con.begin();it!=con.end();++it){
        const int id=con.pid(it);
        double x,y;con.pos(it,x,y);
        if(!con.compute_cell(cell,it))continue;
        std::vector<double> verts;cell.vertices(x,y,verts);
        if(verts.empty())continue;
        Shape sh={true,cell.area(),0,0,1e100,-1e100,1e100,-1e100};
        cell.centroid(sh.cx,sh.cy);sh.cx+=x;sh.cy+=y;
        for(size_t k=0;k+1<verts.size();k+=2){
            sh.xlo=std::min(sh.xlo,verts[k]);sh.xhi=std::max(sh.xhi,verts[k]);
            sh.ylo=std::min(sh.ylo,verts[k+1]);sh.yhi=std::max(sh.yhi,verts[k+1]);
        }
        if(id<0||id>=int(out.size())){std::cerr<<"ORACLE_INVALID unexpected id\n";std::exit(2);}
        out[id]=sh;
    }
    return out;
}

bool equivalent(const Shape& a,const Shape& b,double& maxerr){
    if(a.active!=b.active)return false;
    if(!a.active)return true;
    const double x[]={a.measure,a.cx,a.cy,a.xlo,a.xhi,a.ylo,a.yhi};
    const double y[]={b.measure,b.cx,b.cy,b.xlo,b.xhi,b.ylo,b.yhi};
    bool okay=true;
    for(int k=0;k<7;++k){const double delta=std::fabs(x[k]-y[k]);maxerr=std::max(maxerr,delta);okay=okay&&std::isfinite(x[k])&&delta<1e-5;}
    return okay;
}
void run(int seed,int pattern,int mask,int blocks,int reverse,Stat& stat){
    const std::vector<Site> ss=sites(seed,pattern);
    std::vector<Shape> ref;
    long double sum=0;
    for(size_t i=0;i<ss.size();++i){
        const Shape a=reference(ss,int(i),mask,2);
        const Shape b=reference(ss,int(i),mask,1);
        double err=0;
        if(!equivalent(a,b,err)){std::cerr<<"ORACLE_NOT_CONVERGED dim=2 seed="<<seed<<" pattern="<<pattern<<" mask="<<mask<<" id="<<i<<" err="<<err<<"\n";std::exit(2);}
        sum+=a.measure;ref.push_back(a);
    }
    if(std::fabs(sum-100.)>2e-5L){std::cerr<<"ORACLE_BAD_AREA seed="<<seed<<" pattern="<<pattern<<" mask="<<mask<<" sum="<<double(sum)<<"\n";std::exit(2);}
    const std::vector<Shape> got=actual(ss,mask,blocks,reverse!=0);
    bool pass=true;double worst=0;int bad=0;double actualsum=0;
    for(size_t i=0;i<ss.size();++i){
        if(got[i].active)actualsum+=got[i].measure;
        if(!equivalent(got[i],ref[i],worst)){pass=false;bad++;}
    }
    if(std::fabs(actualsum-100)>2e-5){pass=false;worst=std::max(worst,std::fabs(actualsum-100));}
    stat.configurations++;if(!pass){stat.failures++;if(pattern==0)stat.controls++;}
    if(!pass){
        ++stat.failcase;
        if(stat.failcase<=40)std::cout<<"FAIL_CASE dim=2 seed="<<seed<<" pattern="<<pattern<<" mask="<<mask<<" grid="<<blocks<<" rev="<<reverse<<" count="<<bad<<" worst="<<worst<<" sum="<<actualsum<<"\n";
    }
}
} // namespace
int main(){
    std::cout<<std::setprecision(17);
    Stat stat;
    for(int seed=0;seed<4;++seed)for(int pattern=0;pattern<6;++pattern)
        for(int mask=0;mask<4;++mask)for(int blocks=1;blocks<=3;blocks+=2)
            for(int rev=0;rev<2;++rev)run(seed,pattern,mask,blocks,rev,stat);
    std::cout<<"SUMMARY A9 dim=2 cases="<<stat.configurations<<" failures="<<stat.failures<<" controls="<<stat.controls<<"\n";
    return stat.controls?2:stat.failures?1:0;
}
