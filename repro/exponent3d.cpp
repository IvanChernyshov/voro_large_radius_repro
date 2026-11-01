// A12: equal but extreme finite radii, native power-cell construction.
// 1e308 is deliberately diagnostic, NOT an accepted numerical regime.
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

int main() {
    const double radii[]={1e8,1e154,1e200,1e308};
    int necessary_failures=0,extreme_failures=0,checks=0;
    std::cout<<std::setprecision(17);
    for(unsigned int ir=0;ir<sizeof(radii)/sizeof(radii[0]);++ir){
      const double r=radii[ir];
      for(int blocks:{1,2,4}){
        voro::container_poly c(0,1,0,1,0,1,blocks,blocks,blocks,false,false,false,8);
        c.put(0,.25,.5,.5,r);c.put(1,.75,.5,.5,r);
        voro::c_loop_all loop(c);voro::voronoicell_neighbor cell;
        bool pass=true;int count=0;double total=0;
        if(loop.start())do{
            int id;double x,y,z,rr;loop.pos(id,x,y,z,rr);
            if(!c.compute_cell(cell,loop)){pass=false;continue;}
            const double volume=cell.volume();
            std::vector<double> verts;cell.vertices(x,y,z,verts);
            double xmin=std::numeric_limits<double>::infinity(),xmax=-xmin;
            for(std::size_t i=0;i+2<verts.size();i+=3){xmin=std::min(xmin,verts[i]);xmax=std::max(xmax,verts[i]);}
            const double expected_min=id==0?0:.5,expected_max=id==0?.5:1;
            const bool good=std::isfinite(volume)&&std::fabs(volume-.5)<1e-8
                &&std::isfinite(xmin)&&std::isfinite(xmax)
                &&std::fabs(xmin-expected_min)<1e-8&&std::fabs(xmax-expected_max)<1e-8;
            pass=pass&&good;total+=volume;++count;
        }while(loop.inc());
        pass=pass&&count==2&&std::isfinite(total)&&std::fabs(total-1)<1e-8;
        ++checks;if(!pass){if(ir==3)++extreme_failures;else ++necessary_failures;}
        std::cout<<"CASE A12 dim=3 radius="<<r<<" blocks="<<blocks<<" status="<<(pass?"PASS":"FAIL")
                 <<" total="<<total<<" high_exponent_diagnostic="<<(ir==3)<<"\n";
      }
    }
    std::cout<<"SUMMARY A12 dim=3 cases="<<checks<<" required_failures="<<necessary_failures
             <<" high_exponent_failures="<<extreme_failures<<"\n";
    return necessary_failures?1:0;
}
