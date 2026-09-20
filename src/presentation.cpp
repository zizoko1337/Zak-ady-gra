#include "presentation.hpp"

namespace orbital {
AudioBank::AudioBank() {
    InitAudioDevice(); ready=IsAudioDeviceReady();
    if(!ready) return;
    for(int n=1;n<Count;++n) {
        auto samples=synthesize(static_cast<Cue>(n));
        Wave wave{static_cast<unsigned int>(samples.size()),44100,32,1,samples.data()};
        sounds[n][0]=LoadSoundFromWave(wave);
        if(IsSoundValid(sounds[n][0])) {
            for(int voice=1;voice<3;++voice) sounds[n][voice]=LoadSoundAlias(sounds[n][0]);
            if(IsSoundValid(sounds[n][1]) && IsSoundValid(sounds[n][2])) ++loaded;
        }
    }
    setVolume(volume);
}
AudioBank::~AudioBank() {
    if(!ready) return;
    for(auto& voices:sounds) {
        for(int n=1;n<3;++n) if(IsSoundValid(voices[n])) UnloadSoundAlias(voices[n]);
        if(IsSoundValid(voices[0])) UnloadSound(voices[0]);
    }
    CloseAudioDevice();
}
void AudioBank::update(float dt) { for(auto& value:cooldown) value=std::max(0.0f,value-dt); }
void AudioBank::setVolume(float value) { volume=std::clamp(value,0.0f,1.0f); if(ready) SetMasterVolume(muted?0:volume); }
void AudioBank::setMuted(bool value) { muted=value; setVolume(volume); }
bool AudioBank::isPlaying(Cue cue) const {
    int index=static_cast<int>(cue);
    if(!ready || index<=0 || index>=Count) return false;
    for(const Sound& sound:sounds[index]) if(IsSoundValid(sound) && IsSoundPlaying(sound)) return true;
    return false;
}
void AudioBank::play(Cue cue,float pan) {
    int n=static_cast<int>(cue);
    if(!ready || muted || volume==0 || n<=0 || n>=Count || cooldown[n]>0) return;
    // Wall-clock throttling keeps 16x playback from becoming a wall of sound.
    cooldown[n]=cue==Cue::Bounce?0.12f:cue==Cue::Hit?0.075f:0.05f;
    Sound sound=sounds[n][nextVoice[n]++%3];
    // raylib's pan parameter represents left-channel gain.
    if(IsSoundValid(sound)) { SetSoundPan(sound,1-std::clamp(pan,0.15f,0.85f)); PlaySound(sound); }
}
namespace {
const Color Ink{12,23,37,255},Paper{251,251,239,255};
const Color HatColors[]={{124,96,213,255},{243,181,83,255},{63,158,129,255},{239,116,136,255},{80,137,220,255}};
const Color BeardColors[]={{72,44,29,255},{42,32,26,255},{116,72,43,255},{53,46,40,255},{92,55,35,255}};
void stroke(Vector2 a,Vector2 b,float width,Color color) { DrawLineEx(a,b,width,color); }
}
void drawFace(Vector2 p,float r,const Appearance& a,float clock,Vector2 look,float hurt,bool celebrating) {
    // Work at a 24-unit reference size; small fighters retain the same silhouette.
    float s=r/24.0f;
    auto v=[&](float x,float y){return Vector2{p.x+x*s,p.y+y*s};};
    auto line=[&](float x,float y,float xx,float yy,float w=2.1f){stroke(v(x,y),v(xx,yy),w*s,Ink);};
    auto ellipse=[&](float x,float y,float rx,float ry,Color c){DrawEllipse(static_cast<int>(p.x+x*s),static_cast<int>(p.y+y*s),rx*s,ry*s,c);};
    float blinkCycle=std::fmod(clock+a.phase,3.9f);
    bool blink=blinkCycle>3.76f && hurt<=0 && !celebrating;
    int eyes=celebrating?3:a.eyes;
    for(int eye=0;eye<2;++eye) {
        float x=eye==0?-8:8;
        if(hurt>0) { line(x-3,-8,x+3,-2); line(x+3,-8,x-3,-2); continue; }
        if(blink) { line(x-4,-4,x+4,-4); continue; }
        if(eyes==0 || eyes==1 || eyes==4) {
            float h=eyes==4?(eye==0?7.5f:5):6.5f;
            ellipse(x,-5,eyes==4?5.2f:5,h,Paper);
            ellipse(x+std::clamp(look.x,-1.0f,1.0f)*1.6f,-4+std::clamp(look.y,-1.0f,1.0f),2.2f,3,Ink);
            if(eyes==1) { line(x-5,-12+(eye==0?-1:3),x+5,-12+(eye==0?3:-1),2.8f); }
        } else if(eyes==2) {
            ellipse(x,-4,5,4,Paper); ellipse(x,-3,2,2.8f,Ink);
            stroke(v(x-5,-6),v(x+5,-6),3.6f*s,Ink);
        } else if(eyes==3) {
            line(x-4,-3,x,-7,2.4f); line(x,-7,x+4,-3,2.4f);
        } else {
            DrawRectangleRounded({p.x+(x-6)*s,p.y-9*s,12*s,8*s},0.35f,4,Ink);
            stroke(v(x-3,-7),v(x+1,-7),1.3f*s,Paper);
        }
    }
    if(eyes==5 && !blink && hurt<=0) line(-3,-6,3,-6,2);
    Color beard=BeardColors[a.palette%5];
    if(a.beard==1) { // Stubble
        for(int x=-6;x<=6;x+=4) for(int y=7;y<=12;y+=3) DrawCircleV(v(static_cast<float>(x),static_cast<float>(y)),0.9f*s,beard);
    } else if(a.beard==2) { // Moustache
        ellipse(-4,6,5,2.5f,beard); ellipse(4,6,5,2.5f,beard); DrawCircleV(v(0,6),1.7f*s,beard);
    } else if(a.beard==3) { // Long curled moustache
        line(-2,7,-8,5,2.7f); line(-8,5,-14,8,2.7f); line(-14,8,-15,13,2.7f); line(-15,13,-11,15,2.7f);
        line(2,7,8,5,2.7f); line(8,5,14,8,2.7f); line(14,8,15,13,2.7f); line(15,13,11,15,2.7f);
        DrawCircleV(v(-2,7),1.8f*s,beard); DrawCircleV(v(2,7),1.8f*s,beard);
    } else if(a.beard==4) { // Full beard
        Color greyBeard{176,184,194,150}; ellipse(0,11,10,9,greyBeard); DrawCircleV(v(0,5),8*s,greyBeard);
    } else if(a.beard==5) { // Split beard
        DrawTriangle(v(-8,8),v(-1,8),v(-4,18),beard); DrawTriangle(v(1,8),v(8,8),v(4,18),beard);
    }
    int mouth=hurt>0?3:celebrating?1:a.mouth;
    if(mouth==0) { line(-6,7,-3,10); line(-3,10,3,10); line(3,10,6,7); }
    else if(mouth==1) {
        ellipse(0,9,7,5,Ink); stroke(v(-4,7),v(4,7),2.4f*s,Paper);
    } else if(mouth==2) { line(-5,10,4,9); line(4,9,7,6); }
    else if(mouth==3) { ellipse(0,9,3.7f,4.6f,Ink); }
    else if(mouth==4) {
        ellipse(0,9,8,5.2f,Ink); DrawRectangleRounded({p.x-6*s,p.y+5*s,12*s,6*s},0.28f,4,Paper);
        line(-2,5,-2,11,0.75f); line(2,5,2,11,0.75f);
    } else { line(-6,9,6,9,2.5f); }
}
void drawHat(Vector2 center,float r,const Appearance& a,float clock) {
    if(a.hat==0) return;
    float s=r/24,tilt=std::sin(clock*2+a.phase)*0.04f;
    auto p=[&](float x,float y) { return Vector2{center.x+(x*std::cos(tilt)-y*std::sin(tilt))*s,center.y+(x*std::sin(tilt)+y*std::cos(tilt))*s}; };
    auto line=[&](float x,float y,float xx,float yy,float width,Color c){stroke(p(x,y),p(xx,yy),width*s,c);};
    auto tri=[&](float x,float y,float xx,float yy,float xxx,float yyy,Color c){DrawTriangle(p(x,y),p(xx,yy),p(xxx,yyy),c);};
    Color color=HatColors[a.palette%5];
    if(a.hat==1) { // Top hat
        line(-13,-25,13,-25,25,color); line(-20,-17,20,-17,5,Ink); line(-12,-20,12,-20,4,Paper);
    } else if(a.hat==2) { // Crown
        tri(-16,-16,0,-16,-18,-34,color); tri(-12,-16,12,-16,0,-38,color); tri(0,-16,16,-16,18,-34,color);
        line(-16,-17,16,-17,5,color); DrawCircleV(p(0,-21),2.6f*s,Paper);
    } else if(a.hat==3) { // Beanie
        DrawCircleSector(p(0,-16),18*s,180,360,24,color); line(-18,-17,18,-17,6,Paper);
        DrawCircleV(p(0,-36),5*s,color); line(-8,-22,-7,-29,1.2f,Paper); line(7,-22,6,-29,1.2f,Paper);
    } else if(a.hat==4) { // Party cone
        tri(-14,-17,14,-17,-4,-48,color); line(-14,-17,14,-17,4,Paper);
        DrawCircleV(p(-4,-48),3.4f*s,Paper); DrawCircleV(p(-3,-34),2*s,Paper); DrawCircleV(p(3,-24),2*s,Paper);
    } else if(a.hat==5) { // Baseball cap
        DrawCircleSector(p(0,-17),16*s,180,360,20,color); line(-14,-17,24,-17,5,color); line(0,-29,0,-19,1.2f,Paper);
    } else if(a.hat==6) { // Cowboy hat
        DrawEllipse(static_cast<int>(p(0,-19).x),static_cast<int>(p(0,-19).y),25*s,6*s,color);
        line(-25,-19,25,-19,3,Ink); line(-11,-34,11,-34,16,color); line(-12,-27,12,-27,3,Ink);
    } else if(a.hat==7) { // Headband
        line(-19,-21,19,-21,7,color); line(-19,-21,19,-21,1.5f,Paper);
        tri(15,-21,29,-15,16,-12,color); line(17,-20,27,-15,1.3f,Paper);
    } else if(a.hat==8) { // Viking helmet
        DrawCircleSector(p(0,-18),18*s,180,360,24,color); line(-18,-18,18,-18,4,Ink);
        tri(-14,-24,-29,-36,-22,-16,Paper); tri(14,-24,29,-36,22,-16,Paper);
    } else if(a.hat==9) { // Wizard hat
        tri(-15,-17,15,-17,5,-51,color); line(-18,-17,18,-17,6,Paper); DrawCircleV(p(5,-36),2*s,Paper);
    } else { // Pirate bandana
        DrawCircleSector(p(0,-18),18*s,180,360,20,color); line(-19,-18,19,-18,6,color);
        tri(13,-18,28,-11,15,-7,color); line(-16,-18,15,-18,1.5f,Paper);
    }
}
void drawPortrait(Vector2 p,float r,Color color,const Appearance& a,float clock,bool celebrating) {
    DrawCircleV({p.x+2,p.y+4},r,Fade(Ink,0.8f)); DrawCircleV(p,r,color);
    drawFace(p,r,a,clock,{0.15f,0},0,celebrating); drawHat(p,r,a,clock);
}
} // namespace orbital
