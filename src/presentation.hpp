#pragma once
#include "feedback.hpp"
#include <raylib.h>

namespace orbital {
class AudioBank {
public:
    AudioBank();
    ~AudioBank();
    AudioBank(const AudioBank&)=delete;
    AudioBank& operator=(const AudioBank&)=delete;
    void update(float dt);
    void play(Cue cue,float pan=0.5f);
    bool isPlaying(Cue cue) const;
    void setMuted(bool value);
    void setVolume(float value);
    bool ready=false,muted=false;
    int loaded=0;
    float volume=0.55f;
private:
    static constexpr int Count=static_cast<int>(Cue::Count);
    std::array<std::array<Sound,3>,Count> sounds{};
    std::array<int,Count> nextVoice{};
    std::array<float,Count> cooldown{};
};
void drawFace(Vector2 center,float radius,const Appearance& appearance,float clock,Vector2 look,float hurt=0,bool celebrating=false);
void drawHat(Vector2 center,float radius,const Appearance& appearance,float clock);
void drawPortrait(Vector2 center,float radius,Color color,const Appearance& appearance,float clock,bool celebrating=false);
} // namespace orbital
