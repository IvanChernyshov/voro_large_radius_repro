// Diagnostic counterexample: a remote maximum radius hides a local score gap.
// Compile for either pinned Voro++ branch with OWNER_SHIFT_DEV and OWNER_SHIFT_2D.
// The baseline should be correct; shifted owner scores can introduce a tie.
#if defined(OWNER_SHIFT_DEV) || !defined(OWNER_SHIFT_2D)
#include <voro++.hh>
#else
#include <voro++_2d.hh>
#endif
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc,char** argv) {
    if(argc!=2) return 2;
    const std::string scenario=argv[1];
    if(scenario!="large" && scenario!="scaled") return 2;
    const bool scaled=scenario=="scaled";
    const double L=scaled?3.:3e8;
    const double p0=scaled?2.5e-9:.25;
    const double p1=scaled?7.5e-9:.75;
    const double p2=scaled?2.:2e8;
    const double R=scaled?1.:1e8;
    const long double gap=static_cast<long double>(p1)-p0;
    const long double far=static_cast<long double>(p2)-p1;
    // At the query site, this uniquely determines the exact power owner.
    if(!(gap*gap>0 && far*far-static_cast<long double>(R)*R>0)) return 2;

#if defined(OWNER_SHIFT_2D)
    voro::container_poly_2d con(0,L,0,1,1,1,false,false,8);
    con.put(0,p0,.5,0);
    con.put(1,p1,.5,0);
    con.put(2,p2,.5,R);
    double rx=0,ry=0;
    int id=-1;
    const bool found=con.find_voronoi_cell(p1,.5,rx,ry,id);
    const int dim=2;
#else
#if defined(OWNER_SHIFT_DEV)
    voro::container_poly_3d con(0,L,0,1,0,1,1,1,1,false,false,false,8);
#else
    voro::container_poly con(0,L,0,1,0,1,1,1,1,false,false,false,8);
#endif
    con.put(0,p0,.5,.5,0);
    con.put(1,p1,.5,.5,0);
    con.put(2,p2,.5,.5,R);
    double rx=0,ry=0,rz=0;
    int id=-1;
    const bool found=con.find_voronoi_cell(p1,.5,.5,rx,ry,rz,id);
    const int dim=3;
#endif
    if(!found) {
        std::cerr<<"OWNER_SHIFT_ERROR: native search returned no owner\n";
        return 2;
    }
    std::cout<<std::setprecision(17)
             <<"RESULT owner_shift branch="
#if defined(OWNER_SHIFT_DEV)
             <<"dev"
#else
             <<"master"
#endif
             <<" dim="<<dim<<" case="<<scenario
             <<" status="<<(id==1?"CORRECT":"WRONG")
             <<" expected_id=1 observed_id="<<id
             <<" query_x="<<p1<<" found_x="<<rx<<"\n";
    return 0; // A wrong owner is a diagnostic result, not a runner crash.
}
