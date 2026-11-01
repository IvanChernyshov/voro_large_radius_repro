// Non-blocking numerical scope diagnostics for Voro++ radius_poly.
// Works with original 2D or 3D headers, without changing library code.
#include "rad_option.hh"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

#ifdef AUDIT_2D
static const int RAD_INDEX=2;
static const char* DIM="2";
#else
static const int RAD_INDEX=3;
static const char* DIM="3";
#endif

class RadiusAccess : public voro::radius_poly {
    double particle[4], *blocks[1];
public:
    RadiusAccess(double current, double max) {
        for(int i=0;i<4;++i)particle[i]=0.;
        particle[RAD_INDEX]=current;
        blocks[0]=particle;
        ppr=blocks;
        max_radius=max;
        r_init(0,0);
    }
    bool cutoffTest(double crs,double mrs){return r_ctest(crs,mrs);}
    double faceTest(double rv,double lrs){r_prime(rv); return r_cutoff(lrs);}
};

void ctest(const char* name,double r,double R,double crs,double mrs,const char* reference){
    RadiusAccess access(r,R);
    const bool result=access.cutoffTest(crs,mrs);
    std::cout << "PROBE ctest " << name << " r="<<r<<" R="<<R<<" crs="<<crs<<" mrs="<<mrs
       <<" skip="<<int(result)<<" reference="<<reference<<"\n";
}
void face(const char* name,double r,double R,double rv,double lrs){
    RadiusAccess access(r,R);
    const double v=access.faceTest(rv,lrs);
    std::cout << "PROBE face "<<name<<" r="<<r<<" R="<<R<<" rv="<<rv<<" lrs="<<lrs
        <<" cutoff="<<v<<" finite="<<int(std::isfinite(v))
        <<" nan="<<int(std::isnan(v))<<" inf="<<int(std::isinf(v))<<"\n";
}

int main(){
    std::cout<<std::setprecision(17)<<"AUDIT radius_poly dim="<<DIM<<"\n";
    ctest("normal_true",1e8,1e8,16,8,"true");
    ctest("normal_false",1e8,1e8,16,32,"false");
    ctest("weight_dominates",0,1e8,16,8,"false");
    ctest("boundary_rounding_unit",0.8,1.,1.,0.40960000000000013,"false-exact-binary64-inputs");
    ctest("boundary_rounding_large",0.,1e8,1.1805916207174113e21,1.1805716208021146e21,"false-exact-binary64-inputs");
    ctest("square_overflow_conservative",1e8,1e8,1e160,5e159,"true-exact");
    ctest("square_overflow_no_prune",1e8,1e8,1e160,2e160,"false-exact");
    face("equal_regular",1e8,1e8,1,1);
    face("equal_zero_denom",1e8,1e8,0,1);
    face("distinct_zero_denom",0,1e8,0,1);
    face("distinct_zero_product",0,1e8,0,0);
    face("huge_distinct",0,1e200,1,1);
    face("huge_distinct_zero_product",0,1e200,1,0);
    face("huge_equal_radius",1e308,1e308,1,1);
    face("signed_negative_scope_only",-1e8,0,1,1);
    std::cout << "SUMMARY radius_poly dim="<<DIM<<" category=scope-only no_acceptance_claim=1\n";
}
