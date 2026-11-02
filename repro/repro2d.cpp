// 2D cell-construction reproduction for the pinned Voro++ master (C++11).
#include <voro++_2d.hh>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

using namespace voro;

struct CellInfo {int id; double area,xmin,xmax;};

template<class Container>
std::vector<CellInfo> collect(Container& con) {
    std::vector<CellInfo> result;
    voronoicell_neighbor_2d cell;
    c_loop_all_2d loop(con);
    if(loop.start()) do {
        int id; double x,y,r;
        loop.pos(id,x,y,r);
        if(con.compute_cell(cell,loop)) {
            std::vector<double> vs;
            cell.vertices(x,y,vs);
            double xmin=std::numeric_limits<double>::infinity();
            double xmax=-xmin;
            for(std::size_t i=0;i+1<vs.size();i+=2){
                xmin=std::min(xmin,vs[i]); xmax=std::max(xmax,vs[i]);
            }
            result.push_back(CellInfo{id,cell.area(),xmin,xmax});
        } else std::cout << "EMPTY site=" << id << "\n";
    } while(loop.inc());
    std::sort(result.begin(),result.end(),[](const CellInfo& a,const CellInfo& b){return a.id<b.id;});
    return result;
}

void print(const char* name,const std::vector<CellInfo>& cells,double target) {
    double total=0;
    for(std::size_t i=0;i<cells.size();++i) {
        const CellInfo& c=cells[i];
        total+=c.area;
        std::cout << "CELL " << name << " id=" << c.id << " area=" << c.area
                  << " xmin=" << c.xmin << " xmax=" << c.xmax << "\n";
    }
    std::cout << "TOTAL " << name << " got=" << total << " expected=" << target
              << " fraction=" << total/target << "\n";
    if(cells.size()==2) {
        std::cout << "GAP " << name << " got=" << cells[1].xmin-cells[0].xmax
                  << " expected=0\n";
    }
}


static bool near_value(double a,double b,double scale) {
    return std::isfinite(a) && std::fabs(a-b)<=1e-9*std::max(1.0,scale);
}

static bool check_pair(const char* name,const std::vector<CellInfo>& cells,double L) {
    const double measure=L*L;
    bool ok=cells.size()==2;
    if(ok) {
        ok=cells[0].id==0 && cells[1].id==1
            && near_value(cells[0].area,measure/2.,measure)
            && near_value(cells[1].area,measure/2.,measure)
            && near_value(cells[0].xmin,0,L)
            && near_value(cells[0].xmax,L/2.,L)
            && near_value(cells[1].xmin,L/2.,L)
            && near_value(cells[1].xmax,L,L);
    }
    std::cout << "CHECK " << name << " " << (ok?"PASS":"FAIL") << "\n";
    return ok;
}

static bool check_periodic(const char* name,const std::vector<CellInfo>& cells,double L) {
    bool ok=cells.size()==1 && cells[0].id==0
        && near_value(cells[0].area,L*L,L*L)
        && near_value(cells[0].xmax-cells[0].xmin,L,L);
    std::cout << "CHECK " << name << " " << (ok?"PASS":"FAIL") << "\n";
    return ok;
}

int main(int argc,char** argv) {
    const double r=argc>1?std::atof(argv[1]):1e8;
    const double L=argc>2?std::atof(argv[2]):1.;
    const int blocks=argc>3?std::atoi(argv[3]):1;
    if(!(std::isfinite(r)&&r>=0&&std::isfinite(L)&&L>0&&blocks>=1)) return 2;
    std::cout << std::setprecision(17) << "PARAM dim=2 r=" << r
              << " L=" << L << " pair_blocks=1 periodic_blocks=" << blocks << "\n";
    // Nonperiodic control: mathematically equal radii => ordinary bisector x=L/2.
    container_2d ordinary(0,L,0,L,1,1,false,false,8);
    ordinary.put(0,.25*L,.5*L);ordinary.put(1,.75*L,.5*L);
    const std::vector<CellInfo> standard=collect(ordinary);
    print("two-standard",standard,L*L);
    int failures=!check_pair("two-standard",standard,L);
    container_poly_2d weighted(0,L,0,L,1,1,false,false,8);
    weighted.put(0,.25*L,.5*L,r);weighted.put(1,.75*L,.5*L,r);
    const std::vector<CellInfo> power=collect(weighted);
    print("two-power-equal-radii",power,L*L);
    failures+=!check_pair("two-power-equal-radii",power,L);
    // One periodic generator must own the full fundamental area for ANY radius.
    container_poly_2d periodic(0,L,0,L,blocks,blocks,true,true,8);
    periodic.put(0,.75*L,.5*L,r);
    const std::vector<CellInfo> periodic_cells=collect(periodic);
    print("one-power-periodic",periodic_cells,L*L);
    failures+=!check_periodic("one-power-periodic",periodic_cells,L);
    std::cout << "SUMMARY dim=2 failed_checks=" << failures << "\n";
    return failures?1:0;
}
