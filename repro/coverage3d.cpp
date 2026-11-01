// A6-A7: 3D rectangular (all periodic masks) and orthogonal/skew periodic
// containers, block counts and insertion order. Original Voro++ C++11 only.
#include <voro++.hh>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
struct Cell {int id;double measure,xmin,xmax;};
struct Counts {unsigned long cases,failures,controls;Counts():cases(0),failures(0),controls(0){}};
bool near(double a,double b){return std::isfinite(a)&&std::fabs(a-b)<=3e-9*std::max(1.,std::fabs(b));}
template<class Container,class Loop>
std::vector<Cell> collect(Container& con,Loop& loop){
    std::vector<Cell> cells;voro::voronoicell_neighbor c;
    if(loop.start())do{
        int id;double x,y,z,r;loop.pos(id,x,y,z,r);
        if(!con.compute_cell(c,loop))continue;
        std::vector<double> v;c.vertices(x,y,z,v);
        double lo=1e300,hi=-1e300;
        for(std::size_t i=0;i+2<v.size();i+=3){lo=std::min(lo,v[i]);hi=std::max(hi,v[i]);}
        cells.push_back(Cell{id,c.volume(),lo,hi});
    }while(loop.inc());
    std::sort(cells.begin(),cells.end(),[](const Cell&a,const Cell&b){return a.id<b.id;});return cells;
}
const double site1[]={.75};const double site2[]={.25,.75};const double site3[]={.2,.5,.8};
const double* site_pos(int n){return n==1?site1:n==2?site2:site3;}
const double expected1[]={1.};const double expected2[]={.5,.5};const double expected3[]={.35,.3,.35};
const double* widths(int n){return n==1?expected1:n==2?expected2:expected3;}
bool validate_rect(const std::vector<Cell>& cells,int n,bool px,double& err){
    if(cells.size()!=static_cast<std::size_t>(n)){err=1e9;return false;}
    const double* width=widths(n);double x=0,volume=0;bool ok=true;
    for(int i=0;i<n;++i){
        const Cell& c=cells[i];const double lo=(n==1&&px)?.25:x,hi=lo+width[i];
        err=std::max(err,std::fabs(c.measure-width[i]));
        err=std::max(err,std::fabs(c.xmin-lo));err=std::max(err,std::fabs(c.xmax-hi));
        ok=ok&&c.id==i&&near(c.measure,width[i])&&near(c.xmin,lo)&&near(c.xmax,hi);
        volume+=c.measure;x+=width[i];
    }
    err=std::max(err,std::fabs(volume-1.));return ok&&near(volume,1.);
}
bool validate_periodic(const std::vector<Cell>& cells,int n,bool skew,
                       const std::vector<Cell>& reference,double& err){
    if(cells.size()!=static_cast<std::size_t>(n)){err=1e9;return false;}
    double total=0;bool ok=true;
    for(int i=0;i<n;++i){
        const Cell& c=cells[i];total+=c.measure;
        ok=ok&&c.id==i&&std::isfinite(c.measure)&&c.measure>0;
        if(n==1||n==2){const double expected=1./n;
            err=std::max(err,std::fabs(c.measure-expected));ok=ok&&near(c.measure,expected);}
        if(!reference.empty()){
            if(reference.size()!=cells.size())return false;
            err=std::max(err,std::fabs(c.measure-reference[i].measure));
            ok=ok&&c.id==reference[i].id&&near(c.measure,reference[i].measure);
        }
    }
    err=std::max(err,std::fabs(total-1.));ok=ok&&near(total,1.);
    // The input x=0.2, 0.5, 0.8 has inversion symmetry (also on a
    // skew periodic lattice). Its outer sites must have equal measures.
    if(n==3){err=std::max(err,std::fabs(cells[0].measure-cells[2].measure));
        ok=ok&&near(cells[0].measure,cells[2].measure);}
    (void)skew;return ok;
}
void rectangular(int mask,int blocks,int n,bool reverse,int variant,Counts& counts){
    const bool px=mask&1,py=mask&2,pz=mask&4;
    const double* p=site_pos(n);std::vector<Cell> cells;
    if(variant==0){
        voro::container con(0,1,0,1,0,1,blocks,blocks,blocks,px,py,pz,8);
        for(int i=0;i<n;++i){const int id=reverse?n-1-i:i;con.put(id,p[id],.5,.5);}
        voro::c_loop_all loop(con);cells=collect(con,loop);
    }else{
        voro::container_poly con(0,1,0,1,0,1,blocks,blocks,blocks,px,py,pz,8);
        const double r=variant==1?1.:1e8;
        for(int i=0;i<n;++i){const int id=reverse?n-1-i:i;con.put(id,p[id],.5,.5,r);}
        voro::c_loop_all loop(con);cells=collect(con,loop);
    }
    double err=0;const bool ok=validate_rect(cells,n,px,err);++counts.cases;
    if(!ok){++counts.failures;if(variant!=2)++counts.controls;}
    std::cout<<"CASE A6_A7 dim=3 container="<<(variant==0?"standard-box":"power-box")
        <<" radius="<<(variant==0?0.:variant==1?1.:1e8)
        <<" periodic="<<int(px)<<int(py)<<int(pz)<<" blocks="<<blocks
        <<" sites="<<n<<" order="<<(reverse?"reverse":"forward")
        <<" status="<<(ok?"PASS":"FAIL")<<" count="<<cells.size()
        <<" maxerror="<<err<<'\n';
}
std::vector<Cell> periodic_cells(int skew,int blocks,int n,bool reverse,int variant){
    const double a=skew?.23:0., b=skew?-.17:0.,c=skew?.31:0.;
    const double* p=site_pos(n);
    if(variant==0){
        voro::container_periodic con(1,a,1,b,c,1,blocks,blocks,blocks,8);
        for(int i=0;i<n;++i){const int id=reverse?n-1-i:i;con.put(id,p[id],.5,.5);}
        voro::c_loop_all_periodic loop(con);return collect(con,loop);
    }
    voro::container_periodic_poly con(1,a,1,b,c,1,blocks,blocks,blocks,8);
    const double r=variant==1?1.:1e8;
    for(int i=0;i<n;++i){const int id=reverse?n-1-i:i;con.put(id,p[id],.5,.5,r);}
    voro::c_loop_all_periodic loop(con);return collect(con,loop);
}
void triclinic(int skew,int blocks,int n,bool reverse,Counts& counts){
    const std::vector<Cell> reference=periodic_cells(skew,blocks,n,reverse,0);
    for(int variant=0;variant<3;++variant){
        const std::vector<Cell> cells=variant==0?reference:periodic_cells(skew,blocks,n,reverse,variant);
        double err=0;const bool ok=validate_periodic(cells,n,skew,reference,err);++counts.cases;
        if(!ok){++counts.failures;if(variant!=2)++counts.controls;}
        std::cout<<"CASE A6_A7 dim=3 container="<<(variant==0?"standard-periodic":"power-periodic")
            <<" lattice="<<(skew?"skew":"orthogonal")<<" radius="<<(variant==0?0.:variant==1?1.:1e8)
            <<" blocks="<<blocks<<" sites="<<n<<" order="<<(reverse?"reverse":"forward")
            <<" status="<<(ok?"PASS":"FAIL")<<" count="<<cells.size()
            <<" maxerror="<<err<<'\n';
    }
}
}
int main(){
    std::cout<<std::setprecision(17);Counts counts;
    for(int mask=0;mask<8;++mask)for(int blocks=1;blocks<=4;blocks*=2)
        for(int n=1;n<=3;++n)for(int order=0;order<2;++order)
            for(int variant=0;variant<3;++variant)rectangular(mask,blocks,n,order!=0,variant,counts);
    for(int skew=0;skew<2;++skew)for(int blocks=1;blocks<=4;blocks*=2)
        for(int n=1;n<=3;++n)for(int order=0;order<2;++order)triclinic(skew,blocks,n,order!=0,counts);
    std::cout<<"SUMMARY A6_A7 dim=3 cases="<<counts.cases<<" failures="<<counts.failures
        <<" control_failures="<<counts.controls<<'\n';
    return counts.controls?2:counts.failures?1:0;
}
