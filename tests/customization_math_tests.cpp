#include "../CustomizationMath.h"
#include <cstdio>
#include <cstdlib>
using namespace CustomizationMath;
void near(float a,float b) { if(std::fabs(a-b)>0.0001f) { std::fprintf(stderr,"%f != %f\n",a,b);std::exit(1); } }
int main() {
    auto body=Identity();body.m[3][0]=13;body.m[3][1]=-8;body.m[3][2]=2;
    auto part=body;part.m[3][0]+=1.5f;
    auto moved=MoveBodyPart(part,body,HeightTransform(1.5f,-1.5f,-.1f,-.1f));
    near(moved.m[3][0],14.5f);near(moved.m[3][2],1.9f);
    auto pitched=HeightTransform(1.5f,-1.5f,-.1f,.1f);
    auto front=Identity();front.m[3][0]=1.5f;
    auto rear=Identity();rear.m[3][0]=-1.5f;
    near(Multiply(front,pitched).m[3][2],-.1f);
    near(Multiply(rear,pitched).m[3][2],.1f);
    body.m[0][0]=2; // Unknown non-rigid body leaves the original part untouched.
    near(MoveBodyPart(part,body,pitched).m[3][2],part.m[3][2]);
    auto inverse=Identity();body=Identity();body.m[0][0]=NAN;
    if(RigidInverse(body,inverse)) return 1;
    body=Identity();body.m[3][0]=INFINITY;
    if(RigidInverse(body,inverse)) return 1;
    body=Identity();body.m[0][3]=.1f;
    if(RigidInverse(body,inverse)) return 1;
    // Attachments and animated panels retain their marker transform under rake
    // in the exact biased root, including a translated and rotated car.
    auto root=Identity();root.m[0][0]=0;root.m[0][1]=1;
    root.m[1][0]=-1;root.m[1][1]=0;
    root.m[3][0]=13;root.m[3][1]=-8;root.m[3][2]=2.03f;
    auto marker=Identity();marker.m[3][0]=1.2f;marker.m[3][1]=.3f;marker.m[3][2]=.4f;
    auto delta=HeightTransform(1.5f,-1.5f,-.1f,.1f);
    auto nativePart=Multiply(marker,root);
    auto expected=Multiply(Multiply(marker,delta),root);
    auto actual=MoveBodyPart(nativePart,root,delta);
    for(int i=0;i<4;++i) for(int j=0;j<4;++j) near(actual.m[i][j],expected.m[i][j]);
    return 0;
}
