// A6-A7: analytical 2D cell checks across periodicity masks,
// spatial grids and insertion orders.
#include <voro++_2d.hh>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
struct Cell {int id;double measure,xmin,xmax;};
struct Counts {unsigned long cases, failures, controls;Counts():cases(0),failures(0),controls(0){}};

bool near(double a,double b) {return std::isfinite(a)&&std::fabs(a-b)<=2e-9*std::max(1.,std::fabs(b));}
std::vector<Cell> collect(voro::container_2d& con){
    std::vector<Cell> cells; voro::voronoicell_neighbor_2d c;voro::c_loop_all_2d it(con);
    if(it.start())do{
        int id;double x,y,r;it.pos(id,x,y,r);
        if(!con.compute_cell(c,it))continue;
        std::vector<double> v;c.vertices(x,y,v);
        double lo=1e300,hi=-1e300;
        for(std::size_t i=0;i+1<v.size();i+=2){lo=std::min(lo,v[i]);hi=std::max(hi,v[i]);}
        cells.push_back(Cell{id,c.area(),lo,hi});
    }while(it.inc());
    std::sort(cells.begin(),cells.end(),[](const Cell&a,const Cell&b){return a.id<b.id;});return cells;
}
std::vector<Cell> collect(voro::container_poly_2d& con){
    std::vector<Cell> cells; voro::voronoicell_neighbor_2d c;voro::c_loop_all_2d it(con);
    if(it.start())do{
        int id;double x,y,r;it.pos(id,x,y,r);
        if(!con.compute_cell(c,it))continue;
        std::vector<double> v;c.vertices(x,y,v);
        double lo=1e300,hi=-1e300;
        for(std::size_t i=0;i+1<v.size();i+=2){lo=std::min(lo,v[i]);hi=std::max(hi,v[i]);}
        cells.push_back(Cell{id,c.area(),lo,hi});
    }while(it.inc());
    std::sort(cells.begin(),cells.end(),[](const Cell&a,const Cell&b){return a.id<b.id;});return cells;
}

bool check(const std::vector<Cell>& cells,int n,bool px,double& maxerr){
    const double width1[]={1.};
    const double width2[]={.5,.5};
    const double width3[]={.35,.3,.35};
    const double* widths=n==1?width1:n==2?width2:width3;
    bool ok=cells.size()==static_cast<std::size_t>(n);double cumulative=0,total=0;
    if(!ok){maxerr=1e9;return false;}
    for(int i=0;i<n;++i){
        const Cell& c=cells[i];
        const double lo=(n==1 && px ? .25 : cumulative);
        const double hi=lo+widths[i];cumulative+=widths[i];total+=c.measure;
        maxerr=std::max(maxerr,std::fabs(c.measure-widths[i]));
        maxerr=std::max(maxerr,std::fabs(c.xmin-lo));
        maxerr=std::max(maxerr,std::fabs(c.xmax-hi));
        ok=ok&&c.id==i&&near(c.measure,widths[i])&&near(c.xmin,lo)&&near(c.xmax,hi);
    }
    maxerr=std::max(maxerr,std::fabs(total-1.));return ok&&near(total,1.);
}
void experiment(int mask,int blocks,int n,bool reversed,int variant,Counts& counts){
    const bool px=(mask&1)!=0,py=(mask&2)!=0;
    const double p1[]={.75},p2[]={.25,.75},p3[]={.2,.5,.8};
    const double* p=n==1?p1:n==2?p2:p3;
    std::vector<Cell> result;
    if(variant==0){
        voro::container_2d con(0,1,0,1,blocks,blocks,px,py,8);
        for(int step=0;step<n;++step){const int id=reversed?n-1-step:step;con.put(id,p[id],.5);}
        result=collect(con);
    }else{
        voro::container_poly_2d con(0,1,0,1,blocks,blocks,px,py,8);
        const double radius=variant==1?1.:1e8;
        for(int step=0;step<n;++step){const int id=reversed?n-1-step:step;con.put(id,p[id],.5,radius);}
        result=collect(con);
    }
    double error=0;const bool ok=check(result,n,px,error);++counts.cases;
    if(!ok){++counts.failures;if(variant!=2)++counts.controls;}
    std::cout<<"CASE A6_A7 dim=2 container="<<(variant==0?"standard":"power")
        <<" radius="<<(variant==0?0.:variant==1?1.:1e8)
        <<" periodic="<<int(px)<<int(py)<<" blocks="<<blocks
        <<" sites="<<n<<" order="<<(reversed?"reverse":"forward")
        <<" status="<<(ok?"PASS":"FAIL")<<" count="<<result.size()
        <<" maxerror="<<error<<'\n';
}
}
int main(){
    std::cout<<std::setprecision(17);
    Counts counts;
    for(int mask=0;mask<4;++mask)for(int blocks=1;blocks<=4;blocks*=2)
        for(int n=1;n<=3;++n)for(int order=0;order<2;++order)
            for(int variant=0;variant<3;++variant)experiment(mask,blocks,n,order!=0,variant,counts);
    std::cout<<"SUMMARY A6_A7 dim=2 cases="<<counts.cases<<" failures="<<counts.failures
        <<" control_failures="<<counts.controls<<'\n';
    return counts.controls?2:counts.failures?1:0;
}
