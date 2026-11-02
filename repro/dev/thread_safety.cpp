// dev-only OpenMP regression: simultaneous cell construction and owner lookups.
// The combined patch must retain per-invocation radius state, not shared members.
// Run only with -fopenmp; ordinary serial correctness is checked elsewhere.
#include <voro++.hh>
#include <omp.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

struct RefCell {int ijk,q,id;bool present;double measure;};
struct Owner2 {bool found;int id;double x,y;};
struct Owner3 {bool found;int id;double x,y,z;};
struct Query2 {double x,y;};
struct Query3 {double x,y,z;};

double amount(voro::voronoicell_neighbor_2d& cell){return cell.area();}
double amount(voro::voronoicell_neighbor_3d& cell){return cell.volume();}
bool close(double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::fabs(a-b)<1e-7;}

template<class Container,class Cell>
int verify_cells(Container& con,int trials,int& evaluated){
    std::vector<RefCell> refs;
    for(auto it=con.begin();it!=con.end();++it){
        const int id=con.pid(it);
        Cell cell;
        const bool present=con.compute_cell(cell,it);
        const double value=present?amount(cell):0;
        if(present&&(!std::isfinite(value)||value<0)){std::cerr<<"INVALID_SERIAL_CELL id="<<id<<"\n";return -1;}
        refs.push_back(RefCell{it->ijk,it->q,id,present,value});
    }
    if(refs.size()!=9){std::cerr<<"WRONG_SITE_COUNT "<<refs.size()<<"\n";return -1;}
    int mismatches=0;
    const int total=int(refs.size())*trials;
#pragma omp parallel for schedule(dynamic,1) reduction(+:mismatches)
    for(int t=0;t<total;++t){
        const RefCell& r=refs[t%refs.size()];
        Cell cell;
        const bool got=con.compute_cell(cell,r.ijk,r.q);
        const double value=got?amount(cell):0;
        if(got!=r.present||(got&&!close(value,r.measure)))++mismatches;
    }
    evaluated+=total;
    return mismatches;
}

int test2d(){
    const double R=1e8;
    const double ulp=std::nextafter(R,std::numeric_limits<double>::infinity())-R;
    voro::container_poly_2d con(0,10,0,10,3,3,false,false,8,4);
    for(int i=0;i<9;++i)
        con.put(i,1.5+3.*(i%3)+.08*(i/3),1.3+3.2*(i/3)+.09*(i%3),R+(i%5-2)*ulp);
    int evaluated=0,bad=verify_cells<voro::container_poly_2d,voro::voronoicell_neighbor_2d>(con,36,evaluated);
    if(bad<0)return 2;
    std::vector<Query2> queries;
    std::vector<Owner2> refs;
    for(int i=0;i<10;++i)for(int j=0;j<10;++j){
        Query2 q={.2+.96*i,.2+.96*j};
        double x=0,y=0;int id=-1;
        bool ok=con.find_voronoi_cell(q.x,q.y,x,y,id);
        queries.push_back(q);refs.push_back(Owner2{ok,id,x,y});
    }
    const int trials=8,nt=int(queries.size())*trials;
#pragma omp parallel for schedule(dynamic,1) reduction(+:bad)
    for(int i=0;i<nt;++i){
        const int idx=i%queries.size();
        const Query2 q=queries[idx];const Owner2& ref=refs[idx];
        double x=0,y=0;int id=-1;
        bool found=con.find_voronoi_cell(q.x,q.y,x,y,id);
        if(found!=ref.found||(found&&(id!=ref.id||!close(x,ref.x)||!close(y,ref.y))))++bad;
    }
    std::cout<<"SUMMARY DEV_OMP dim=2 cells="<<evaluated<<" owner_queries="<<nt<<" mismatches="<<bad<<"\n";
    return bad?1:0;
}
int test3d(){
    const double R=1e8;
    const double ulp=std::nextafter(R,std::numeric_limits<double>::infinity())-R;
    voro::container_poly_3d con(0,10,0,10,0,10,3,3,3,false,false,false,8,4);
    for(int i=0;i<9;++i)
        con.put(i,1.5+3.*(i%3)+.08*(i/3),1.3+3.2*(i/3)+.09*(i%3),
                1.1+1.07*((i*5)%7),R+(i%5-2)*ulp);
    int evaluated=0,bad=verify_cells<voro::container_poly_3d,voro::voronoicell_neighbor_3d>(con,36,evaluated);
    if(bad<0)return 2;
    std::vector<Query3> queries;
    std::vector<Owner3> refs;
    for(int i=0;i<5;++i)for(int j=0;j<5;++j)for(int k=0;k<5;++k){
        Query3 q={.4+1.91*i,.5+1.91*j,.6+1.91*k};
        double x=0,y=0,z=0;int id=-1;
        bool ok=con.find_voronoi_cell(q.x,q.y,q.z,x,y,z,id);
        queries.push_back(q);refs.push_back(Owner3{ok,id,x,y,z});
    }
    const int trials=8,nt=int(queries.size())*trials;
#pragma omp parallel for schedule(dynamic,1) reduction(+:bad)
    for(int i=0;i<nt;++i){
        const int idx=i%queries.size();
        const Query3 q=queries[idx];const Owner3& ref=refs[idx];
        double x=0,y=0,z=0;int id=-1;
        bool found=con.find_voronoi_cell(q.x,q.y,q.z,x,y,z,id);
        if(found!=ref.found||(found&&(id!=ref.id||!close(x,ref.x)||!close(y,ref.y)||!close(z,ref.z))))++bad;
    }
    std::cout<<"SUMMARY DEV_OMP dim=3 cells="<<evaluated<<" owner_queries="<<nt<<" mismatches="<<bad<<"\n";
    return bad?1:0;
}
int main(){
#ifndef _OPENMP
    std::cerr<<"OpenMP is required for this test.\n";
    return 2;
#else
    omp_set_dynamic(0);
    omp_set_num_threads(4);
    int nthreads=0;
#pragma omp parallel reduction(+:nthreads)
    {nthreads+=1;}
    if(nthreads<2){std::cerr<<"ERROR OpenMP did not create multiple threads.\n";return 2;}
    std::cout<<"OpenMP threads="<<nthreads<<"\n";
    const int a=test2d(),b=test3d();
    std::cout<<"SUMMARY DEV_OMP total_failures="<<(a!=0)+(b!=0)<<"\n";
    return a>1||b>1?2:(a||b)?1:0;
#endif
}
