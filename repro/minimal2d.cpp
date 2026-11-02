// Minimal native Voro++ 2D power-diagram reproduction.
// Two equal radii must cancel in both cell construction and site lookup.
#include <voro++_2d.hh>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

namespace {
struct Result {bool present; double area,xmin,xmax;};
bool near(double x,double target) {return std::isfinite(x)&&std::fabs(x-target)<1e-9;}
}

int main() {
    const double radius=1e8;
    voro::container_poly_2d con(0,1,0,1,1,1,false,false,8);
    con.put(0,.25,.5,radius);
    con.put(1,.75,.5,radius);

    Result cells[2]={{false,0,0,0},{false,0,0,0}};
    voro::voronoicell_neighbor_2d cell;
    voro::c_loop_all_2d loop(con);
    if(loop.start()) do {
        int id; double x,y,r;
        loop.pos(id,x,y,r);
        if(id<0||id>1) {std::cerr<<"Unexpected site id\n";return 2;}
        if(!con.compute_cell(cell,loop)) continue;
        std::vector<double> vertices;
        cell.vertices(x,y,vertices);
        double lo=std::numeric_limits<double>::infinity(),hi=-lo;
        for(std::size_t i=0;i+1<vertices.size();i+=2) {
            lo=std::min(lo,vertices[i]);hi=std::max(hi,vertices[i]);
        }
        cells[id]=Result{true,cell.area(),lo,hi};
    } while(loop.inc());
    const double total=cells[0].area+cells[1].area;
    const bool geometry=cells[0].present&&cells[1].present
        &&near(cells[0].area,.5)&&near(cells[1].area,.5)
        &&near(total,1.)&&near(cells[0].xmin,0.)&&near(cells[0].xmax,.5)
        &&near(cells[1].xmin,.5)&&near(cells[1].xmax,1.);

    double rx=0,ry=0;int pid=-1;
    const bool found=con.find_voronoi_cell(.74,.5,rx,ry,pid);
    const bool owner=found&&pid==1&&near(rx,.75)&&near(ry,.5);

    std::cout<<std::setprecision(17);
    std::cout<<"RESULT dim=2 check=geometry status="<<(geometry?"PASS":"FAIL")
             <<" expected_total=1 observed_total="<<total
             <<" expected_cell_areas=0.5,0.5 observed_cell_areas="<<cells[0].area<<","<<cells[1].area<<"\n";
    std::cout<<"RESULT dim=2 check=owner status="<<(owner?"PASS":"FAIL")
             <<" query_x=0.74 expected_id=1 observed_id="<<pid
             <<" found="<<int(found)<<" expected_site_x=0.75 observed_site_x="<<rx<<"\n";
    const int failures=int(!geometry)+int(!owner);
    std::cout<<"SUMMARY minimal dim=2 failed_checks="<<failures<<"\n";
    return failures?1:0;
}
