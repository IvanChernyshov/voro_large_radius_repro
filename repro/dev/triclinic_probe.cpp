// Dedicated triclinic cell-volume comparisons for Voro++ dev.
// Each scenario runs separately so any native error is recorded.
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
struct Site { double x,y,z,r; };
uint32_t step(uint32_t& s) { return s=1664525u*s+1013904223u; }
std::vector<Site> make_sites(const std::string& scenario) {
    const double R=1e8;
    if(scenario=="single-small") return {{2.5,3.5,4.5,2.}};
    if(scenario=="pair-small") return {{2.5,3.5,4.5,2.},{7.5,3.5,4.5,2.}};
    if(scenario=="pair-large") return {{2.5,3.5,4.5,R},{7.5,3.5,4.5,R}};
    if(scenario=="pair-adjacent") return {{2.5,3.5,4.5,R},{7.5,3.5,4.5,std::nextafter(R,std::numeric_limits<double>::infinity())}};
    if(scenario=="nine-small") {
        // Match A9's first (ordinary-radius) nine-site configuration.
        uint32_t rng=0x9e3779b9u+1529u;
        std::vector<Site> result;
        for(int i=0;i<9;++i) {
            Site p={0,0,0,1.+(i*7)%5};
            for(int attempt=0;attempt<500;++attempt) {
                p.x=1.+(step(rng)%8000)/1000.;
                p.y=1.+(step(rng)%8000)/1000.;
                p.z=1.+(step(rng)%8000)/1000.;
                bool separated=true;
                for(const Site& q:result) {
                    const double dx=p.x-q.x,dy=p.y-q.y,dz=p.z-q.z;
                    if(dx*dx+dy*dy+dz*dz<1.) { separated=false;break; }
                }
                if(separated) break;
                if(attempt==499) { std::cerr<<"ORACLE_ERROR: failed to place site\n";std::exit(2); }
            }
            result.push_back(p);
        }
        return result;
    }
    std::cerr<<"Unknown scenario: "<<scenario<<"\n";
    std::exit(2);
}

double reference_volume(const std::vector<Site>& sites,int i,bool skew,int K) {
    const Site& s=sites[i];
    const long double bxy=skew?2.3L:0.,bxz=skew?-1.7L:0.,byz=skew?3.1L:0.;
    voro::voronoicell_neighbor_3d c;
    c.init(-30,30,-30,30,-30,30);
    for(int kz=-K;kz<=K;++kz) for(int ky=-K;ky<=K;++ky)
        for(int kx=-K;kx<=K;++kx) for(std::size_t j=0;j<sites.size();++j) {
            if(int(j)==i && kx==0 && ky==0 && kz==0) continue;
            const long double dx=sites[j].x+10.L*kx+ky*bxy+kz*bxz-s.x;
            const long double dy=sites[j].y+10.L*ky+kz*byz-s.y;
            const long double dz=sites[j].z+10.L*kz-s.z;
            const long double ri=s.r,rj=sites[j].r;
            const long double rhs=dx*dx+dy*dy+dz*dz+(ri-rj)*(ri+rj);
            if(!c.nplane(double(dx),double(dy),double(dz),double(rhs),int(j))) return 0.;
        }
    return c.volume();
}
std::vector<double> native_volumes(const std::vector<Site>& sites,bool skew,int blocks,bool reverse) {
    voro::container_triclinic_poly con(10.,skew?2.3:0.,10.,skew?-1.7:0.,skew?3.1:0.,10.,blocks,blocks,blocks,8);
    for(std::size_t k=0;k<sites.size();++k) {
        const std::size_t i=reverse?sites.size()-1-k:k;
        const Site& s=sites[i];
        con.put(int(i),s.x,s.y,s.z,s.r);
    }
    // Capture only primary-domain generators before any compute_cell()
    // creates new image-block particles. The dev iterator's != end()
    // condition is not stable under image creation: it may continue into
    // generated image blocks, unlike upstream's "< end()" loops.
    struct Original {int ijk,q,id;};
    std::vector<Original> originals;
    for(int k=con.ez;k<con.wz;++k)
      for(int j=con.ey;j<con.wy;++j)
        for(int i=0;i<blocks;++i) {
            const int ijk=i+blocks*(j+con.oy*k);
            for(int q=0;q<con.co[ijk];++q) {
                const int id=con.id[ijk][q];
                if(id<0||id>=int(sites.size())) {std::cerr<<"ORACLE_ERROR: invalid primary ID\n";std::exit(2);}
                originals.push_back(Original{ijk,q,id});
            }
        }
    if(originals.size()!=sites.size()) {std::cerr<<"ORACLE_ERROR: primary site count mismatch\n";std::exit(2);}
    std::vector<double> volumes(sites.size(),0.);
    voro::voronoicell_neighbor_3d cell;
    if(std::getenv("VORO_DEV_TRICLINIC_LEGACY_ITERATOR")) {
        // Diagnostic of the OLD harness only; never used for geometry acceptance.
        for(auto it=con.begin();it!=con.end();++it) {
            const int id=con.id[it->ijk][it->q];
            if(id<0||id>=int(sites.size())) {std::cerr<<"ORACLE_ERROR: invalid legacy ID\n";std::exit(2);}
            if(con.compute_cell(cell,it)) volumes[id]=cell.volume();
        }
    } else {
        for(const Original& site:originals)
            if(con.compute_cell(cell,site.ijk,site.q)) volumes[site.id]=cell.volume();
    }
    return volumes;
}
}
int main(int argc,char** argv) {
    if(argc!=5) {std::cerr<<"Usage: triclinic_probe SCENARIO [orth|skew] BLOCKS REVERSE\n";return 2;}
    const std::string scenario=argv[1],lattice=argv[2];
    if(lattice!="orth"&&lattice!="skew") return 2;
    const bool skew=lattice=="skew";
    const int blocks=std::atoi(argv[3]),reverse=std::atoi(argv[4]);
    if((blocks!=1&&blocks!=3)||(reverse!=0&&reverse!=1)) return 2;
    const std::vector<Site> sites=make_sites(scenario);
    std::vector<double> expected;
    double reference_total=0;
    for(std::size_t i=0;i<sites.size();++i) {
        const double v1=reference_volume(sites,int(i),skew,1);
        const double v2=reference_volume(sites,int(i),skew,2);
        if(!std::isfinite(v1)||!std::isfinite(v2)||std::fabs(v1-v2)>2e-5) {
            std::cerr<<"ORACLE_ERROR: image-window mismatch id="<<i<<" K1="<<v1<<" K2="<<v2<<"\n";
            return 2;
        }
        expected.push_back(v2);
        reference_total+=v2;
    }
    if(std::fabs(reference_total-1000.)>5e-4) {
        std::cerr<<"ORACLE_ERROR: reference volume sum="<<reference_total<<"\n";
        return 2;
    }
    const std::vector<double> observed=native_volumes(sites,skew,blocks,reverse!=0);
    double actual_total=0,max_error=0;
    bool good=true;
    for(std::size_t i=0;i<sites.size();++i) {
        if(!std::isfinite(observed[i])||observed[i]<0) good=false;
        actual_total+=observed[i];
        const double error=std::fabs(observed[i]-expected[i]);
        if(!std::isfinite(error)||error>2e-5) good=false;
        max_error=std::max(max_error,error);
    }
    if(std::fabs(actual_total-1000.)>5e-4) good=false;
    std::cout<<std::setprecision(17)<<"RESULT DEV_TRICLINIC scenario="<<scenario
             <<" lattice="<<lattice<<" blocks="<<blocks<<" reverse="<<reverse
             <<" sites="<<sites.size()<<" reference_total="<<reference_total
             <<" native_total="<<actual_total<<" max_volume_error="<<max_error
             <<" status="<<(good?"PASS":"GEOMETRY_MISMATCH")<<"\n";
    return good?0:1;
}
