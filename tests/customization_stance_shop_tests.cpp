#include "../CustomizationStanceShop.h"
#include <cstdlib>
#include <iostream>
using namespace CustomizationStance;
static void Check(bool value,const char* message) {
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}
static Shop::Snapshot Height(float height) {
    Shop::Snapshot value;value.enabled=true;value.values.mask=3;
    value.values.values[0]=value.values.values[1]=height;return value;
}
int main() {
    Shop shop;shop.Enter(1,Height(-.1f));
    Check(shop.Begin(1)&&shop.Preview(1,Height(-.2f)),"Edit previews immediately");
    Check(shop.Visible(1)->values.values[0]==-.2f&&shop.Purchased(1)->values.values[0]==-.1f,
          "Preview cannot enter purchased/save snapshot");
    Check(shop.Cancel(1)&&shop.Visible(1)->values.values[0]==-.1f,"Cancel restores purchased");
    Check(shop.Begin(1)&&shop.Preview(1,Height(-.25f)),"First cart selection");
    const auto first=shop.Accept(1);Check(first&&shop.Cart(1),"Accept creates cart item");
    shop.Enter(1,Height(0));
    Check(shop.Initial(1)->values.values[0]==-.25f&&shop.Purchased(1)->values.values[0]==-.1f,
          "Reopen retains absolute cart value and original purchased state");
    Check(shop.Begin(1)&&shop.Preview(1,Height(-.3f))&&shop.Cancel(1),"Cancel re-edit");
    Check(shop.Visible(1)->values.values[0]==-.25f,"Cancel re-edit restores existing cart");
    Check(shop.Begin(1)&&shop.Preview(1,Height(-.3f)),"Replace queued value");
    const auto second=shop.Accept(1);Check(second!=first&&!shop.Remove(1,first),"Stale removal cannot remove replacement");
    Check(shop.BeginCheckout(1,second),"Begin attempted purchase");
    Check(!shop.EndCheckout(1,false)&&shop.Cart(1)&&shop.Purchased(1)->values.values[0]==-.1f,
          "Failed purchase keeps cart and purchased state");
    Check(shop.BeginCheckout(1,second)&&!shop.Begin(1)&&!shop.Remove(1,second),"Checkout freezes selected revision");
    auto result=shop.EndCheckout(1,true);
    Check(result&&result->values.values[0]==-.3f&&!shop.Cart(1),"Successful purchase promotes exact absolute values");
    Check(!shop.EndCheckout(1,true),"Checkout cannot commit twice");
    Check(shop.Begin(1)&&shop.Preview(1,Height(-.35f)),"Adjust purchased state");
    const auto third=shop.Accept(1);
    Check(shop.Remove(1,third)&&shop.Visible(1)->values.values[0]==-.3f,"Removing cart restores purchased stance");
    Check(shop.Begin(1)&&shop.Preview(1,Height(-.4f))&&shop.Accept(1),"Queue before switching cars");
    shop.Enter(2,Height(.1f));
    Check(!shop.Owns(1)&&!shop.Cart(2)&&shop.Visible(2)->values.values[0]==.1f,"Owner switch clears transient state");
    Check(!shop.BeginCheckout(1,third)&&!shop.Cancel(1),"Other owner cannot mutate shop");
    Check(shop.Begin(2),"Begin validation case");
    auto invalid=Height(NAN);Check(!shop.Preview(2,invalid),"Reject nonfinite values");
    invalid=Height(0);invalid.values.mask=0x1000;Check(!shop.Preview(2,invalid),"Reject unsupported channels");
    shop.Clear();Check(!shop.Visible(2)&&!shop.Cart(2),"Shop exit clears every transient layer");
    // Reproduce accept -> reopen -> adjust. Inherited values remain native and
    // the explicit queued value is what initializes the slider on every entry.
    shop.Enter(3,Height(0));
    MenuInfo info;info.token=3;info.enabled=1;info.baselineMask=0xFFF;
    for(unsigned c=0;c<6;++c){info.minimum[c]=-1;info.maximum[c]=1;info.step[c]=.25f;}
    info.minimum[0]=-.75f;
    Session session;
    for(unsigned pass=0;pass<8;++pass) {
        const auto initial=*shop.Initial(3);
        Check(shop.Begin(3)&&session.Begin(info,initial.values),"Reopen queued absolute stance");
        Check(session.Value(0,0)==initial.values.values[0],"Slider starts at queued value, never zero");
        session.Step(0,-1);
        Check(shop.Preview(3,{session.active,true})&&shop.Accept(3),"Queue next bounded adjustment");
    }
    Check(shop.Initial(3)->values.values[0]==-.75f&&shop.Purchased(3)->values.values[0]==0,
          "Repeated accept cannot bypass absolute limit or purchase early");
    const auto queued=shop.Cart(3)->revision;
    Check(shop.BeginCheckout(3,queued)&&shop.EndCheckout(3,true).has_value(),"Purchase adjusted cart");
    Check(session.Begin(info,shop.Initial(3)->values)&&session.Value(0,0)==-.75f,
          "Reopen after purchase retains displayed absolute value");
    Check(!shop.Enter(0,Height(0))&&!shop.Enter(4,Height(NAN))&&shop.Owns(3),
          "Invalid entry cannot discard a valid shop");
    std::cout<<"Stance shop transactions: reopen, cancel, remove, replace, buy and ownership passed\n";
}
