// A1-A3: direct 3D radius_poly arithmetic and two-site geometry checks on dev.
// Return 1 for numerical mismatches, 2 for an invalid test reference.
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

namespace {
const double R = 1e8;

bool close(double actual, double expected, double tolerance) {
    return std::isfinite(actual) && std::fabs(actual - expected) <= tolerance;
}

void report(const char* id, bool passed, double measured, double expected) {
    std::cout << "CASE " << id << " " << (passed ? "PASS" : "FAIL")
              << " observed=" << measured << " expected=" << expected << '\n';
}

class RadiusProbe : public voro::radius_poly_3d {
    double values[8];
    double* blocks[1];
    double r_current, r_mul;
public:
    RadiusProbe(double ri, double rj) {
        std::fill(values, values + 8, 0.0);
        values[3] = ri;
        values[7] = rj;
        blocks[0] = values;
        ppr = blocks;
        max_radius = std::max(ri, rj);
        r_init(0, 0, r_current, r_mul);
    }
    double scale(double squared_distance) {
        return r_scale(squared_distance, 0, 1, r_current);
    }
    bool scale_check(double& squared_distance, double mrs) {
        return r_scale_check(squared_distance, mrs, 0, 1, r_current);
    }
};

struct MeasuredCell {
    int id;
    double volume;
    double xmin;
    double xmax;
};

std::vector<MeasuredCell> measure_pair(double ri, double rj) {
    voro::container_poly_3d con(0, 10, 0, 10, 0, 10, 1, 1, 1,
                             false, false, false, 8);
    con.put(0, 2, 5, 5, ri);
    con.put(1, 8, 5, 5, rj);
    std::vector<MeasuredCell> result;
    voro::voronoicell_neighbor_3d cell;
    for(auto it=con.begin();it!=con.end();++it) {
        const int id=con.pid(it);
        double x,y,z;
        con.pos(it,x,y,z);
        if(!con.compute_cell(cell,it)) continue;
        std::vector<double> verts;
        cell.vertices(x,y,z,verts);
        double xmin=std::numeric_limits<double>::infinity();
        double xmax=-xmin;
        for(std::size_t i=0; i+2<verts.size(); i+=3) {
            xmin=std::min(xmin, verts[i]);
            xmax=std::max(xmax, verts[i]);
        }
        result.push_back(MeasuredCell{id, cell.volume(), xmin, xmax});
    }
    std::sort(result.begin(),result.end(),
              [](const MeasuredCell& a, const MeasuredCell& b){return a.id<b.id;});
    return result;
}

bool check_pair(double expected_boundary, double& measured_boundary) {
    const double adjacent=std::nextafter(R, std::numeric_limits<double>::infinity());
    const std::vector<MeasuredCell> cells=measure_pair(R, adjacent);
    measured_boundary=std::numeric_limits<double>::quiet_NaN();
    if(cells.size()!=2 || cells[0].id!=0 || cells[1].id!=1) return false;
    measured_boundary=cells[0].xmax;
    const double tol_x=1e-9, tol_volume=1e-6;
    return close(cells[0].xmin, 0, tol_x)
        && close(cells[0].xmax, expected_boundary, tol_x)
        && close(cells[1].xmin, expected_boundary, tol_x)
        && close(cells[1].xmax, 10, tol_x)
        && close(cells[0].volume, expected_boundary*100, tol_volume)
        && close(cells[1].volume, (10-expected_boundary)*100, tol_volume);
}
}

int main() {
    const double adjacent=std::nextafter(R, std::numeric_limits<double>::infinity());
    const double delta=(R-adjacent)*(R+adjacent);
    const double expected_boundary=5+delta/12.;
    std::cout << std::setprecision(17);
    std::cout << "PARAM dimension=3 r0=" << R << " r1=" << adjacent
              << " exact_r1_minus_r0=" << adjacent-R << "\n";

    RadiusProbe equal(R,R);
    const double equal_scaled=equal.scale(0.25);
    const bool a1=close(equal_scaled,0.25,1e-12);
    report("A1_equal_radii_r_scale",a1,equal_scaled,0.25);

    RadiusProbe near(R,adjacent);
    const double scaled=near.scale(36.);
    const bool a2a=close(scaled,36+delta,1e-9);
    report("A2_adjacent_radii_r_scale",a2a,scaled,36+delta);

    double boundary=0;
    const bool a2b=check_pair(expected_boundary,boundary);
    report("A2_adjacent_radii_cell_boundary",a2b,boundary,expected_boundary);

    double scaled_for_check=36.;
    const bool candidate_possible=near.scale_check(scaled_for_check,31.);
    const double expected_scaled=36+delta;
    const bool expected_possible=expected_scaled<=0 || expected_scaled*expected_scaled<31.*36.;
    if(!expected_possible) {
        std::cerr << "Internal oracle error: expected the candidate plane to be retained\n";
        return 2;
    }
    const bool a3=candidate_possible && close(scaled_for_check,expected_scaled,1e-9);
    report("A3_r_scale_check_value",close(scaled_for_check,expected_scaled,1e-9),
           scaled_for_check,expected_scaled);
    report("A3_r_scale_check_decision",candidate_possible,
           candidate_possible?1.:0.,1.);
    const int failures=(!a1)+(!a2a)+(!a2b)+(!a3);
    std::cout << "SUMMARY dimension=3 failures=" << failures << '\n';
    return failures ? 1 : 0;
}
