#pragma once

#include <Core/Containers/Array.h>
#include <Core/Strings/AString.h>


class BuildingType
{

};

class Building
{

};

class Settlement
{
    uint64_t mId;
    AString mName;


    Array<Building> mBuildings;
};

class Land
{
    float mForested = 0;
    float mSoil = 0;
    float mGold = 0;
    float mIron = 0;
    float mFarmed = 0;
};




class World
{



    Array<Settlement> mSettlements;
};