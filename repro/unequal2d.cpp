// Stage 6: weighted multi-site 2D cell geometry, with an analytic 1D power-cell oracle.
// Source Voro++ is not changed. This is test-only C++11.
#include <voro++_2d.hh>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
const double L=10.;
struct Site {int id;double x,r;};
struct Expected {bool present;long double xmin,xmax,area;};
struct Actual {int id;double xmin,xmax,area;};
struct Counts {int cases, wrong, controls;Counts():cases(0),wrong(0),controls(0){}};
struct Scenario {const char* label;int count;double r[3];bool control;};

// Each site has the same y=5. For an orthogonal box (all PBC masks),
// the power cell is the Cartesian product of the one-dimensional power
// interval and a full y interval of length 10. Unlike sum checks, this
// computes each site interval using independent pairwise inequalities.
Expected oracle(const Site* pts,int count,int index,bool periodic_x){
    const long double xi=pts[index].x,ri=pts[index].r;
    long double lo=periodic_x ? xi-L/2 : 0;
    long double hi=periodic_x ? xi+L/2 : L;
    for(int j=0;j<count;++j){
        for(int k=(periodic_x?-2:0);k<=(periodic_x?2:0);++k){
            if(j==index&&k==0)continue;
            const long double xj=static_cast<long double>(pts[j].x)+k*L;
            const long double dx=xj-xi;
            if(dx==0)continue;
            const long double rj=pts[j].r;
            const long double delta=(ri-rj)*(ri+rj);
            const long double boundary=(xi+xj)/2+delta/(2*dx);
            if(dx>0)hi=std::min(hi,boundary);
            else lo=std::max(lo,boundary);
        }
    }
    const bool present=hi>lo+1e-12L;
    return Expected{present,lo,hi,present?(hi-lo)*L:0};
}

std::vector<Actual> measure(voro::container_poly_2d& con){
    std::vector<Actual> out;voro::voronoicell_neighbor_2d cell;voro::c_loop_all_2d it(con);
    if(it.start())do{
        int id;double x,y,r;it.pos(id,x,y,r);
        if(!con.compute_cell(cell,it))continue;
        std::vector<double> v;cell.vertices(x,y,v);
        double lo=std::numeric_limits<double>::infinity(),hi=-lo;
        for(std::size_t q=0;q+1<v.size();q+=2){lo=std::min(lo,v[q]);hi=std::max(hi,v[q]);}
        out.push_back(Actual{id,lo,hi,cell.area()});
    }while(it.inc());
    std::sort(out.begin(),out.end(),[](const Actual&a,const Actual&b){return a.id<b.id;});
    return out;
}

bool near(double a,long double b,double atol=2e-7){
    return std::isfinite(a)&&std::fabs(static_cast<long double>(a)-b)<atol;
}

void check(const Scenario& s,int pbc,int blocks,bool reverse,Counts& c){
    const double xs[3]={2,5,8};
    Site pts[3];
    if(s.count==2){pts[0]=Site{0,2,s.r[0]};pts[1]=Site{1,8,s.r[1]};}
    else for(int i=0;i<3;++i)pts[i]=Site{i,xs[i],s.r[i]};
    voro::container_poly_2d con(0,L,0,L,blocks,blocks,(pbc&1)!=0,(pbc&2)!=0,8);
    for(int k=0;k<s.count;++k){int i=reverse?s.count-1-k:k;con.put(pts[i].id,pts[i].x,L/2,pts[i].r);}
    std::vector<Actual> out=measure(con);
    bool good=true;double largest_error=0;double sum=0;long double expected_sum=0;
    for(int i=0;i<s.count;++i){
        const Expected exp=oracle(pts,s.count,i,pbc&1);
        expected_sum+=exp.area;
        std::vector<Actual>::const_iterator a=std::find_if(out.begin(),out.end(),[i](const Actual& x){return x.id==i;});
        if(!exp.present){
            if(a!=out.end()&&a->area>1e-7){good=false;largest_error=std::max(largest_error,a->area);}
            if(a!=out.end())sum+=a->area;
        }else{
            if(a==out.end()){good=false;largest_error=std::max(largest_error,static_cast<double>(exp.area));}
            else{
                sum+=a->area;
                largest_error=std::max(largest_error,static_cast<double>(std::fabs(a->area-exp.area)));
                largest_error=std::max(largest_error,static_cast<double>(std::fabs(a->xmin-exp.xmin)));
                largest_error=std::max(largest_error,static_cast<double>(std::fabs(a->xmax-exp.xmax)));
                good=good&&near(a->area,exp.area)&&near(a->xmin,exp.xmin)&&near(a->xmax,exp.xmax);
            }
        }
    }
    if(!near(static_cast<double>(expected_sum),L*L,1e-8)){
        std::cerr<<"ORACLE_FAIL 2D "<<s.label<<" total="<<static_cast<double>(expected_sum)<<"\n";
        std::exit(2);
    }
    good=good&&near(sum,L*L,2e-7);
    ++c.cases;if(!good){++c.wrong;if(s.control)++c.controls;}
    std::cout<<"CASE A8 dim=2 scenario="<<s.label<<" sites="<<s.count
        <<" periodic="<<((pbc&1)?1:0)<<((pbc&2)?1:0)
        <<" blocks="<<blocks<<" reverse="<<reverse<<" status="<<(good?"PASS":"FAIL")
        <<" measured="<<sum<<" expected="<<L*L<<" maxerror="<<largest_error<<"\n";
}
}

int main(){
    const double r=1e8;
    const double up=std::nextafter(r,std::numeric_limits<double>::infinity());
    const double dn=std::nextafter(r,-std::numeric_limits<double>::infinity());
    const double farup=r+4*(up-r),fardown=r-2*(r-dn);
    const Scenario tests[]={
        {"moderate-3",3,{3.,1.,2.},true},
        {"adjacent-2",2,{r,up,0.},false},
        {"adjacent-3",3,{up,r,dn},false},
        {"ulps-3",3,{farup,r,fardown},false},
        {"dominating-3",3,{r,0.,0.},false},
        {"empty-middle-3",3,{r,0.,r},false},
        {"medium-weight-3",3,{10000.,9999.999,9999.998},true}
    };
    std::cout<<std::setprecision(17);
    Counts c;
    for(std::size_t s=0;s<sizeof(tests)/sizeof(tests[0]);++s)
        for(int pbc=0;pbc<4;++pbc)for(int blocks=1;blocks<=4;blocks*=2)
            for(int rev=0;rev<2;++rev)check(tests[s],pbc,blocks,rev,c);
    std::cout<<"SUMMARY A8 dim=2 cases="<<c.cases<<" mismatches="<<c.wrong
             <<" control_failures="<<c.controls<<"\n";
    return c.controls?2:c.wrong?1:0;
}
