// 3D cell-construction reproduction for the pinned Voro++ master (C++11).
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

using namespace voro;
struct CellInfo {int id;double volume,xmin,xmax;};

template<class Container,class Loop>
std::vector<CellInfo> collect(Container& con,Loop& loop){
    std::vector<CellInfo> result;
    voronoicell_neighbor cell;
    if(loop.start()) do {
        int id;double x,y,z,r;
        loop.pos(id,x,y,z,r);
        if(con.compute_cell(cell,loop)) {
            std::vector<double> vs;
            cell.vertices(x,y,z,vs);
            double xmin=std::numeric_limits<double>::infinity();
            double xmax=-xmin;
            for(std::size_t i=0;i+2<vs.size();i+=3){
                xmin=std::min(xmin,vs[i]);xmax=std::max(xmax,vs[i]);
            }
            result.push_back(CellInfo{id,cell.volume(),xmin,xmax});
        } else std::cout << "EMPTY site=" << id << "\n";
    }while(loop.inc());
    std::sort(result.begin(),result.end(),[](const CellInfo& a,const CellInfo& b){return a.id<b.id;});
    return result;
}

void print(const char* name,const std::vector<CellInfo>& cells,double expected){
    double total=0;
    for(std::size_t i=0;i<cells.size();++i){
        const CellInfo& c=cells[i];total+=c.volume;
        std::cout << "CELL " << name << " id=" << c.id << " volume=" << c.volume
                  << " xmin=" << c.xmin << " xmax=" << c.xmax << "\n";
    }
    std::cout << "TOTAL " << name << " got=" << total << " expected=" << expected
              << " fraction=" << total/expected << "\n";
    if(cells.size()==2) std::cout << "GAP " << name << " got=" << cells[1].xmin-cells[0].xmax
                                 << " expected=0\n";
}


static bool near_value(double a,double b,double scale) {
    return std::isfinite(a) && std::fabs(a-b)<=1e-9*std::max(1.0,scale);
}

static bool check_pair(const char* name,const std::vector<CellInfo>& cells,double L) {
    const double measure=L*L*L;
    bool ok=cells.size()==2;
    if(ok) {
        ok=cells[0].id==0 && cells[1].id==1
            && near_value(cells[0].volume,measure/2.,measure)
            && near_value(cells[1].volume,measure/2.,measure)
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
        && near_value(cells[0].volume,L*L*L,L*L*L)
        && near_value(cells[0].xmax-cells[0].xmin,L,L);
    std::cout << "CHECK " << name << " " << (ok?"PASS":"FAIL") << "\n";
    return ok;
}

int main(int argc,char** argv){
    const double r=argc>1?std::atof(argv[1]):1e8;
    const double L=argc>2?std::atof(argv[2]):1.;
    const int blocks=argc>3?std::atoi(argv[3]):1;
    if(!(std::isfinite(r)&&r>=0&&std::isfinite(L)&&L>0&&blocks>=1)) return 2;
    std::cout << std::setprecision(17) << "PARAM dim=3 r=" << r
              << " L=" << L << " blocks=" << blocks << "\n";
    container ordinary(0,L,0,L,0,L,1,1,1,false,false,false,8);
    ordinary.put(0,.25*L,.5*L,.5*L);ordinary.put(1,.75*L,.5*L,.5*L);
    c_loop_all plain_loop(ordinary);
    const std::vector<CellInfo> standard=collect(ordinary,plain_loop);
    print("two-standard",standard,L*L*L);
    int failures=!check_pair("two-standard",standard,L);

    container_poly weighted(0,L,0,L,0,L,1,1,1,false,false,false,8);
    weighted.put(0,.25*L,.5*L,.5*L,r);weighted.put(1,.75*L,.5*L,.5*L,r);
    c_loop_all box_loop(weighted);
    const std::vector<CellInfo> power=collect(weighted,box_loop);
    print("two-power-equal-radii",power,L*L*L);
    failures+=!check_pair("two-power-equal-radii",power,L);

    container_poly periodic_box(0,L,0,L,0,L,blocks,blocks,blocks,true,true,true,8);
    periodic_box.put(0,.75*L,.5*L,.5*L,r);
    c_loop_all pbox_loop(periodic_box);
    const std::vector<CellInfo> periodic_box_cells=collect(periodic_box,pbox_loop);
    print("one-power-periodic-box",periodic_box_cells,L*L*L);
    failures+=!check_periodic("one-power-periodic-box",periodic_box_cells,L);

    container_periodic_poly periodic_triclinic(L,0,L,0,0,L,blocks,blocks,blocks,8);
    periodic_triclinic.put(0,.75*L,.5*L,.5*L,r);
    c_loop_all_periodic tric_loop(periodic_triclinic);
    const std::vector<CellInfo> periodic_tri_cells=collect(periodic_triclinic,tric_loop);
    print("one-power-periodic-triclinic",periodic_tri_cells,L*L*L);
    failures+=!check_periodic("one-power-periodic-triclinic",periodic_tri_cells,L);
    std::cout << "SUMMARY dim=3 failed_checks=" << failures << "\n";
    return failures?1:0;
}
