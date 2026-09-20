#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace orbital {
enum class Cue { None, Click, Tick, Fight, Bounce, Arrow, Gun, Sword, Hit, Mine, Explosion, Dash, Dodge, Heal, Shield, Win, Lose, Draw, Overtime, Clash, Hammer, Stun, Flute, Count };
struct Appearance {
    int eyes=0,mouth=0,hat=0,beard=0,palette=0;
    float phase=0;
    bool operator==(const Appearance&) const = default;
};
// A separate deterministic stream keeps appearance choices out of gameplay RNG.
inline Appearance appearanceFor(uint32_t seed,int side) {
    uint32_t state=seed^(side==0?0xB5297A4Du:0x68E31DA4u);
    auto next=[&]() { state+=0x9e3779b9u; uint32_t x=state; x=(x^(x>>16))*0x85ebca6bu; x=(x^(x>>13))*0xc2b2ae35u; return x^(x>>16); };
    Appearance a; a.eyes=next()%6; a.mouth=next()%6;
    int roll=next()%20; a.hat=roll<8?0:1+(roll-8)%10;
    roll=next()%16; a.beard=roll<6?0:1+(roll-6)%5;
    a.palette=next()%5; a.phase=(next()%1000)/1000.0f*6.2831853f; return a;
}
class Countdown {
public:
    void start() { elapsed=0; running=true; }
    void reset() { running=false; elapsed=0; }
    void advance(float realSeconds) { if(running) { elapsed=std::min(3.65f,elapsed+std::max(0.0f,realSeconds)); if(elapsed>=3.65f) running=false; } }
    bool active() const { return running; }
    int number() const { return std::max(0,3-static_cast<int>(elapsed)); }
    float phase() const { return elapsed<3?elapsed-std::floor(elapsed):(elapsed-3)/0.65f; }
    float elapsed=0;
private:
    bool running=false;
};
std::vector<float> synthesize(Cue cue,int sampleRate=44100);
} // namespace orbital
