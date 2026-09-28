#include "WorEnergy.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace opp_lora_relay_official;
int main() {
    auto close=[](double a,double b) { assert(std::abs(a-b)<1e-10); };
    const double t=sx126xCadDurationSeconds(9,4);
    close(t,0.019145);
    close(worStandbyCurrentMa(1,t,1,3.84,0.0056),0.079009588);
    close(worStandbyCurrentMa(.02,t,1,3.84,0.0056),3.6760794);
    close(worStandbyCurrentMa(1,t,2,3.84,0.0056)-.0056,
          2*(worStandbyCurrentMa(1,t,1,3.84,.0056)-.0056));
    close(worStandbyCurrentMa(t,t,1,3.84,.0056),3.84);
    for(int sf=7;sf<=12;++sf) {
        double prev=0;
        for(int n:{1,2,4,8,16}) { double x=sx126xCadDurationSeconds(sf,n); assert(x>prev); prev=x; }
    }
    int rejected=0;
    for(int test=0;test<5;++test) {
        try {
            if(test==0) worStandbyCurrentMa(.02,t,2,3.84,.0056);
            if(test==1) worStandbyCurrentMa(0,t,1,3.84,.0056);
            if(test==2) sx126xCadDurationSeconds(6,4);
            if(test==3) sx126xCadDurationSeconds(9,3);
            if(test==4) worStandbyCurrentMa(std::numeric_limits<double>::quiet_NaN(),t,1,3.84,.0056);
        } catch(const std::invalid_argument&) { ++rejected; }
    }
    assert(rejected==5);
    std::cout << "PASS: charge, scaling, two channels, full duty, 30 profiles, invalid inputs\n";
}
