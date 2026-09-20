#include "feedback.hpp"

namespace orbital {
std::vector<float> synthesize(Cue cue,int sampleRate) {
    if(cue==Cue::None || cue==Cue::Count || sampleRate<8000) return {};
    float duration=0.18f,from=600,to=200,noise=0,volume=0.4f;
    switch(cue) {
        case Cue::Click: duration=0.055f; from=1000; to=700; break;
        case Cue::Tick: duration=0.19f; from=740; to=680; break;
        case Cue::Fight: duration=0.55f; from=220; to=880; volume=0.5f; break;
        case Cue::Bounce: duration=0.075f; from=280; to=130; volume=0.18f; break;
        case Cue::Arrow: duration=0.17f; from=1500; to=500; noise=0.35f; volume=0.24f; break;
        case Cue::Gun: duration=0.2f; from=190; to=45; noise=0.65f; break;
        case Cue::Sword: duration=0.24f; from=1800; to=480; noise=0.15f; volume=0.25f; break;
        case Cue::Hit: duration=0.13f; from=200; to=65; noise=0.2f; break;
        case Cue::Mine: duration=0.15f; from=400; to=600; volume=0.25f; break;
        case Cue::Explosion: duration=0.55f; from=100; to=25; noise=0.8f; volume=0.55f; break;
        case Cue::Dash: case Cue::Dodge: duration=0.24f; from=180; to=1150; noise=0.15f; volume=0.25f; break;
        case Cue::Heal: duration=0.45f; from=520; to=1040; volume=0.25f; break;
        case Cue::Shield: duration=0.42f; from=320; to=800; volume=0.25f; break;
        case Cue::Win: duration=0.95f; from=523.25f; to=1046.5f; break;
        case Cue::Lose: duration=0.75f; from=392; to=196; break;
        case Cue::Draw: duration=0.65f; from=440; to=440; break;
        case Cue::Overtime: duration=0.65f; from=440; to=880; break;
        case Cue::Clash: duration=0.18f; from=1650; to=720; noise=0.22f; volume=0.27f; break;
        case Cue::Hammer: duration=0.28f; from=165; to=48; noise=0.42f; volume=0.42f; break;
        case Cue::Stun: duration=0.62f; from=980; to=620; noise=0.08f; volume=0.3f; break;
        case Cue::Flute: duration=0.34f; from=1450; to=2600; volume=0.2f; break;
        default: break;
    }
    const int count=static_cast<int>(duration*sampleRate);
    std::vector<float> samples(count); double phase=0; uint32_t state=static_cast<uint32_t>(cue)+1234;
    for(int n=0;n<count;++n) {
        float t=static_cast<float>(n)/sampleRate,u=static_cast<float>(n)/(count-1);
        float frequency=from+(to-from)*u;
        if(cue==Cue::Win || cue==Cue::Fight) {
            constexpr float notes[]={1,1.25f,1.5f,2}; frequency=from*notes[std::min(3,static_cast<int>(u*4))];
        }
        if(cue==Cue::Overtime) frequency=(static_cast<int>(t*8)%2)?660:440;
        if(cue==Cue::Stun) frequency=(static_cast<int>(t*11)%2)?980:620;
        if(cue==Cue::Flute) frequency=1900+std::sin(t*75)*430+u*500;
        phase+=6.283185307179586*frequency/sampleRate;
        state=1664525u*state+1013904223u; float whiteNoise=(state>>8)/8388607.5f-1;
        float tone=static_cast<float>(std::sin(phase));
        if(cue==Cue::Sword || cue==Cue::Shield) tone=0.7f*tone+0.3f*static_cast<float>(std::sin(phase*2.73));
        float attack=std::min(1.0f,t/0.008f),release=std::min(1.0f,(duration-t)/0.035f);
        float envelope=attack*release*std::pow(1-u,0.8f);
        samples[n]=volume*envelope*(tone*(1-noise)+whiteNoise*noise);
    }
    samples.front()=samples.back()=0;
    return samples;
}
} // namespace orbital
