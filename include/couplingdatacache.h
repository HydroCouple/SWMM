#ifndef COUPLINGDATACACHE_H
#define COUPLINGDATACACHE_H

#include <unordered_map>

struct CouplingDataCache
{
    CouplingDataCache(){}
    std::unordered_map<int,double> NodeLateralInflows;
    std::unordered_map<int,double> NodeDepths;
    std::unordered_map<int,double> SubcatchRainfall;
    std::unordered_map<int,double> XSections;
};

#endif // COUPLINGDATACACHE_H
