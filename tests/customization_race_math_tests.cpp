#include <initializer_list>
#include "../CustomizationRaceMath.h"
#include <iostream>
#include <limits>
int main() {
    using namespace CustomizationRace;
    Adjustment a{};
    const Wheel stock{-.8f,.2f,.3f},wide{-.9f,.4f,.35f};
    if(!Adjust(.8f,.3f,stock,wide,a)||std::fabs(a.positionY-.9f)>.0001f||a.skidScale!=2||a.radius!=.35f)return 1;
    if(!Adjust(-.8f,.3f,{.8f,.2f,.3f},{.9f,.4f,.35f},a)||std::fabs(a.positionY+.9f)>.0001f)return 2;
    if(!Adjust(.8f,.31f,stock,stock,a)||a.positionY!=.8f||a.radius!=.31f||a.skidScale!=1)return 3;
    if(Adjust(.8f,.3f,stock,{-.9f,std::numeric_limits<float>::quiet_NaN(),.3f},a))return 4;
    if(Adjust(.8f,.3f,stock,{-.9f,0,.3f},a))return 5;
    std::cout<<"Race wheel offsets, radius and proportional skid calibration passed\n";
}
