// Stage 2 (A4/A5): 2D analytic-owner regressions for find_voronoi_cell.
// Uses the original Voro++ C++ API, without Python or a second geometry engine.
#include <voro++_2d.hh>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>

namespace {
struct Scenario {
    const char* label;
    double length, site0, site1, radius0, radius1;
    const double* queries;
    unsigned int count;
    bool standard;
};

struct Counts {
    unsigned long queries, errors;
    Counts() : queries(0), errors(0) {}
};

// For input binary64 radii, this factorization avoids subtracting two
// separately rounded, very large squares. All tested queries are separated
// from the resulting boundary by much more than our numeric tolerance.
double analytic_boundary(const Scenario& s) {
    return (s.site0+s.site1)*0.5
        + (s.radius0-s.radius1)*(s.radius0+s.radius1)
          / (2*(s.site1-s.site0));
}

bool correct_coordinates(int id, double rx, double ry, const Scenario& s) {
    const double site_x = id == 0 ? s.site0 : s.site1;
    return std::isfinite(rx) && std::isfinite(ry)
        && std::fabs(rx-site_x) < 1e-12
        && std::fabs(ry-s.length*0.5) < 1e-12;
}

template<class Container>
void query_all(Container& con, const Scenario& s, int blocks,
               bool reverse_insertion, Counts& total) {
    unsigned int failed=0;
    const double boundary=analytic_boundary(s);
    for(unsigned int q=0; q<s.count; ++q) {
        const double x=s.queries[q];
        if(!(x>0 && x<s.length && std::fabs(x-boundary)>1e-4)) {
            std::cerr << "INVALID_ORACLE: query must be strictly inside domain and away from the bisector\n";
            std::exit(2);
        }
        const int expected=x<boundary ? 0 : 1;
        int got=-1;
        double rx=std::numeric_limits<double>::quiet_NaN();
        double ry=std::numeric_limits<double>::quiet_NaN();
        const bool found=con.find_voronoi_cell(x,s.length*0.5,rx,ry,got);
        const bool ok=found && got==expected && correct_coordinates(got,rx,ry,s);
        ++total.queries;
        if(!ok) {
            ++failed; ++total.errors;
            std::cout << "MISMATCH " << s.label << " blocks=" << blocks
                << " order=" << (reverse_insertion ? "reversed" : "forward")
                << " x=" << x << " boundary=" << boundary
                << " expected=" << expected << " found=" << found
                << " actual=" << got << " returned_site=(" << rx << "," << ry << ")\n";
        }
    }
    std::cout << "CASE " << s.label << " " << (failed ? "FAIL" : "PASS")
        << " blocks=" << blocks
        << " order=" << (reverse_insertion ? "reversed" : "forward")
        << " queries=" << s.count << " mismatches=" << failed
        << " boundary=" << boundary << '\n';
}

void run_case(const Scenario& s, Counts& total) {
    for(int blocks=1;blocks<=4;blocks*=2) {
        for(int reversed=0;reversed<2;++reversed) {
            if(s.standard) {
                voro::container_2d con(0,s.length,0,s.length,blocks,blocks,false,false,8);
                if(reversed) {
                    con.put(1,s.site1,s.length*0.5);
                    con.put(0,s.site0,s.length*0.5);
                } else {
                    con.put(0,s.site0,s.length*0.5);
                    con.put(1,s.site1,s.length*0.5);
                }
                query_all(con,s,blocks,reversed!=0,total);
            } else {
                voro::container_poly_2d con(0,s.length,0,s.length,blocks,blocks,false,false,8);
                if(reversed) {
                    con.put(1,s.site1,s.length*0.5,s.radius1);
                    con.put(0,s.site0,s.length*0.5,s.radius0);
                } else {
                    con.put(0,s.site0,s.length*0.5,s.radius0);
                    con.put(1,s.site1,s.length*0.5,s.radius1);
                }
                query_all(con,s,blocks,reversed!=0,total);
            }
        }
    }
}
} // namespace

int main() {
    const double r=1e8;
    const double adjacent=std::nextafter(r,std::numeric_limits<double>::infinity());
    const double equal_queries[]={0.10,0.26,0.49,0.51,0.74,0.90};
    const double adjacent_queries[]={1.5,4.5,4.7,4.75,4.76,4.8,5.0,8.5,9.5};
    const Scenario scenarios[]={
        {"A4_standard_control",1,0.25,0.75,0,0,equal_queries,6,true},
        {"A4_small_equal_radii",1,0.25,0.75,1,1,equal_queries,6,false},
        {"A4_large_equal_radii",1,0.25,0.75,r,r,equal_queries,6,false},
        {"A5_large_adjacent_radii",10,2,8,r,adjacent,adjacent_queries,9,false}
    };
    if(!(adjacent>r && std::fabs(analytic_boundary(scenarios[3])-4.751647313435872)<1e-12)) {
        std::cerr << "INVALID_ORACLE: adjacent binary64 radius reference\n";
        return 2;
    }
    std::cout << std::setprecision(17)
        << "PARAM dim=2 r0=" << r << " next_radius=" << adjacent
        << " adjacent_boundary=" << analytic_boundary(scenarios[3]) << '\n';
    Counts per_case[sizeof(scenarios)/sizeof(scenarios[0])];
    unsigned long tested=0, failed=0;
    for(unsigned int s=0;s<sizeof(scenarios)/sizeof(scenarios[0]);++s) {
        run_case(scenarios[s],per_case[s]);
        tested+=per_case[s].queries;
        failed+=per_case[s].errors;
        std::cout << "SCENARIO " << scenarios[s].label
                  << " queries=" << per_case[s].queries
                  << " mismatches=" << per_case[s].errors << '\n';
    }
    const unsigned long control_failures=per_case[0].errors+per_case[1].errors;
    std::cout << "SUMMARY A4_A5 dim=2 queries=" << tested
              << " mismatches=" << failed
              << " control_failures=" << control_failures << '\n';
    // Control failures are infrastructure/regression failures, not
    // intentional evidence of the large-radius limitation.
    return control_failures ? 2 : failed ? 1 : 0;
}
