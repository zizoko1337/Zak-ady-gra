#include "game.hpp"
#include "presentation.hpp"
#include <raylib.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#if defined(_WIN32)
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void*,wchar_t*,unsigned long);
#endif

using namespace orbital;
namespace {
constexpr int Width=1440,Height=940;
// Temporary economy helper for development. Set this to false to remove the Add 10k button.
constexpr bool ShowDebugAddCoins=true;
const Color Background{10,14,22,255},Panel{17,23,34,255},PanelAlt{23,31,44,255};
const Color Border{40,52,69,255},Muted{135,152,173,255},White{234,241,249,255};
const Color Accent{211,248,117,255},Cyan{76,217,242,255},Coral{255,126,144,255};
const Color Team[2]={Cyan,Coral};
Font uiFont{},displayFont{}; Vector2 mouse{}; bool inputBlocked=false;
AudioBank* audioBank=nullptr;
Vector2 point(Vec v) { return {720+v.x,427+v.y}; }
std::string number(float value,int decimals=0) { char b[64]; std::snprintf(b,sizeof b,decimals==0?"%.0f":decimals==1?"%.1f":"%.2f",value); return b; }
float fontSize(float size) { return size<17?size+4:size; }
Font fontFor(float size) { return size>=48?displayFont:uiFont; }
void text(const std::string& value,float x,float y,float size,Color color=White) { DrawTextEx(fontFor(size),value.c_str(),{x,y},fontSize(size),0.3f,color); }
float textWidth(const std::string& s,float size) { return MeasureTextEx(fontFor(size),s.c_str(),fontSize(size),0.3f).x; }
void fitted(const std::string& value,float x,float y,float width,float size,Color color=White) {
    float actual=fontSize(size); float measured=MeasureTextEx(uiFont,value.c_str(),actual,0.3f).x;
    if(measured>width) actual*=width/measured;
    DrawTextEx(uiFont,value.c_str(),{x,y},actual,0.3f,color);
}
void centered(const std::string& value,float x,float y,float size,Color color=White) { text(value,x-textWidth(value,size)/2,y,size,color); }
void box(Rectangle r,Color fill=Panel,Color line=Border,float round=0.08f) { DrawRectangleRounded(r,round,8,fill); DrawRectangleRoundedLinesEx(r,round,8,1,line); }
bool inside(Rectangle r) { return CheckCollisionPointRec(mouse,r); }
bool button(Rectangle r,const std::string& label,bool selected=false,bool enabled=true,Color accent=Accent,float size=16) {
    bool hover=enabled && !inputBlocked && inside(r);
    Color fill=selected?accent:hover?Color{39,51,66,255}:PanelAlt;
    box(r,enabled?fill:Panel,selected?accent:hover?Muted:Border,0.15f);
    centered(label,r.x+r.width/2,r.y+(r.height-size)/2-1,size,!enabled?Color{76,88,104,255}:selected?Background:White);
    if(hover) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    bool clicked=hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if(clicked && audioBank) audioBank->play(Cue::Click);
    return clicked;
}
void wrapped(const std::string& s,float x,float y,float width,float size,Color color,int maxLines=3) {
    std::istringstream words(s); std::string word,line; int lines=0;
    while(words>>word) {
        std::string candidate=line.empty()?word:line+" "+word;
        if(textWidth(candidate,size)>width && !line.empty()) { text(line,x,y,size,color); y+=size+5; if(++lines>=maxLines) return; line=word; }
        else line=candidate;
    }
    if(!line.empty() && lines<maxLines) text(line,x,y,size,color);
}
std::string detail(const Item& i) {
    if(i.id=="weapon.orbit_shield") return "heavy  /  covers 1/3 body  /  deflects";
    if(i.effect=="sword") return i.meleeClass+"  /  "+number(i.get("damage"))+" DMG"+(i.get("lifesteal")>0?"  /  +"+number(i.get("lifesteal"))+" HP":"")+"  /  reach "+number(i.get("reach"));
    if(i.effect=="bow" || i.effect=="pistol" || i.effect=="shotgun") return number(i.get("damage"))+" DMG"+(i.effect=="shotgun"?" ×3":"")+"  /  every "+number(i.get("cooldown"),1)+" s";
    if(i.effect=="mine") return number(i.get("damage"))+" DMG  /  every "+number(i.get("cooldown"),1)+" s  /  blast "+number(i.get("blast_radius"));
    if(i.effect=="flute") return number(i.get("count"))+" mice  /  "+number(i.get("damage"))+" DMG each  /  every "+number(i.get("cooldown"),1)+" s";
    if(i.effect=="minion") return number(i.get("hp"))+" HP helper  /  random weapon";
    if(i.effect=="spikes") return number(i.get("damage"))+" DMG  /  lasts "+number(i.get("lifetime"))+" s";
    if(i.effect=="tape") return number(i.get("chance")*100)+"% on wall, +1% per miss  /  "+number(i.get("duration"))+" s slow";
    if(i.effect=="dash" || i.effect=="dodge") return "Every "+number(i.get("cooldown"),1)+" s  /  speed ×"+number(i.get("speed_multiplier"),1);
    if(i.effect=="regen") return "+"+number(i.get("heal"))+" HP  /  every "+number(i.get("cooldown"),1)+" s";
    if(i.effect=="shield") return number(i.get("absorb"))+" shield  /  "+number(i.get("duration"),1)+" s every "+number(i.get("cooldown"),1)+" s";
    if(i.effect=="vampire") return number(i.get("lifesteal")*100)+"% damage returned as HP";
    return i.description;
}
void icon(const std::string& kind,Vector2 p,Color c,float scale=1) {
    auto line=[&](float x,float y,float xx,float yy,float thick=2.5f) { DrawLineEx({p.x+x*scale,p.y+y*scale},{p.x+xx*scale,p.y+yy*scale},thick*scale,c); };
    if(kind=="sword") { line(-8,9,8,-9,3); line(-8,0,0,8); line(-10,11,-6,7); }
    else if(kind=="bow") { DrawRingLines(p,10*scale,11*scale,-70,70,20,c); line(3,-10,3,10,1); line(-9,0,12,0,2); line(8,-4,12,0); line(8,4,12,0); }
    else if(kind=="mine" || kind=="spikes") { DrawCircleV(p,6*scale,c); for(int n=0;n<6;++n) {float a=n*Pi/3; line(std::cos(a)*7,std::sin(a)*7,std::cos(a)*12,std::sin(a)*12,2);} }
    else if(kind=="pistol" || kind=="shotgun") { line(-10,-4,11,-4,6); line(-6,0,-9,10,5); }
    else if(kind=="poison_dart") { line(-10,0,10,0,2); line(5,-4,10,0); line(5,4,10,0); DrawCircleV({p.x-8*scale,p.y},3*scale,Color{118,226,106,255}); }
    else if(kind=="shuriken") { for(int n=0;n<4;++n) { float a=n*Pi/2; DrawTriangle({p.x+std::cos(a)*12*scale,p.y+std::sin(a)*12*scale},{p.x+std::cos(a+1.7f)*5*scale,p.y+std::sin(a+1.7f)*5*scale},{p.x+std::cos(a-1.7f)*5*scale,p.y+std::sin(a-1.7f)*5*scale},c); } DrawCircleV(p,2*scale,White); }
    else if(kind=="flute") { line(-3,-11,-3,11,3); line(-3,-7,10,-7,3); line(4,-7,4,-2,1.5f); DrawCircleV({p.x+8*scale,p.y-7*scale},2*scale,c); }
    else if(kind=="minion") { DrawCircleV(p,8*scale,c); DrawCircleV({p.x-3*scale,p.y-2*scale},1.6f*scale,Background); DrawCircleV({p.x+3*scale,p.y-2*scale},1.6f*scale,Background); line(-10,9,10,9,2); }
    else if(kind=="tape") { DrawRingLines({p.x-6*scale,p.y},4*scale,7*scale,0,360,16,c); line(-2,0,13,0,3); line(7,-4,13,0,1); line(7,4,13,0,1); }
    else if(kind=="shield") { line(-9,-8,0,-11); line(0,-11,9,-8); line(-9,-8,-7,4); line(9,-8,7,4); line(-7,4,0,11); line(7,4,0,11); }
    else if(kind=="regen" || kind=="vampire") { line(-9,0,9,0,4); line(0,-9,0,9,4); }
    else if(kind=="dash" || kind=="dodge") { line(-10,-8,0,0); line(0,0,-10,8); line(0,-8,10,0); line(10,0,0,8); }
    else { DrawCircleLinesV(p,10*scale,c); line(-5,0,5,0); line(0,-5,0,5); }
}
void drawMinionWeapon(const Item& weapon,Vec holder,Vec velocity,float radius,Color color) {
    if(weapon.effect=="sword") return; // Melee weapons use their visible orbit drawn in the arena.
    Vec forward=normalized(velocity),side{-forward.y,forward.x}; Vec hand=holder+forward*(radius*0.7f);
    auto line=[&](Vec a,Vec b,float width,Color c) { DrawLineEx(point(a),point(b),width,c); };
    if(weapon.effect=="bow") {
        DrawRingLines(point(hand),6,8,-65,65,16,color); line(hand-side*7,hand+side*7,1.2f,White); line(hand-forward*4,hand+forward*10,1.5f,White);
    } else if(weapon.effect=="pistol" || weapon.effect=="shotgun") {
        float width=weapon.effect=="shotgun"?4.5f:3.2f;
        line(hand-forward*3,hand+forward*11,width,White); line(hand-forward*1-side*2,hand-forward*4-side*5,width*0.8f,color);
    } else if(weapon.effect=="poison_dart") {
        line(hand-forward*4,hand+forward*11,2.4f,Color{118,226,106,255}); DrawCircleV(point(hand+forward*11),2.2f,White);
    } else if(weapon.effect=="shuriken") {
        for(int n=0;n<4;++n) { float angle=n*Pi/2; Vec a=hand+Vec{std::cos(angle),std::sin(angle)}*7,b=hand+Vec{std::cos(angle+1.8f),std::sin(angle+1.8f)}*2,c=hand+Vec{std::cos(angle-1.8f),std::sin(angle-1.8f)}*2; DrawTriangle(point(a),point(b),point(c),White); }
    }
}
struct App {
    struct FloatingText { std::string message; Vec position; int owner; bool heal,critical=false; float age=0; };
    struct Impact { Vec position,direction; float age,total,strength; uint64_t seed; bool sparks; };
    std::filesystem::path root;
    Catalog catalog;
    Wallet wallet;
    Match preview;
    std::vector<Fighter> ownedBallCache;
    std::array<Fighter,3> marketBallCache{};
    std::unique_ptr<Simulation> sim;
    std::mt19937 random{std::random_device{}()};
    int selected=0,stake=100,speed=1,returned=0;
    bool paused=false,catalogOpen=false,rulesOpen=false,collectionOpen=false,inspectionOpen=false;
    bool volumeDragging=false,stakeDragging=false;
    int catalogTab=0,catalogScroll=0,pendingRemove=-1,inspectionMarket=-1,inspectionOwned=-1;
    uint32_t inspectionSeed=0;
    Fighter inspectedBall;
    double accumulator=0;
    Countdown countdown;
    std::array<Appearance,2> appearances{};
    std::vector<FloatingText> floating;
    std::vector<Impact> impacts;
    std::array<float,2> messageCooldown{};
    uint64_t lastEvent=0;
    float realClock=0,resultAge=0,hitStop=0,tiltTimer=0,tiltTotal=0,tiltKick=0,screenTilt=0;
    std::string error;
    const Item* hovered=nullptr;
    explicit App(std::filesystem::path path):root(std::move(path)),catalog(Catalog::load(root/"data/catalog.json")),wallet(Wallet::load(root/"save.json",catalog.rules.startingCoins)) {
        ensureMarket(); rebuildCollectionCache(); wallet.save(root/"save.json"); reroll();
        if(audioBank && std::filesystem::exists(root/"settings.json")) {
            try {
                std::ifstream input(root/"settings.json"); nlohmann::json settings; input>>settings;
                float volume=settings.value("volume",0.55f);
                if(std::isfinite(volume)) audioBank->setVolume(volume);
                audioBank->setMuted(settings.value("muted",false));
            } catch(const std::exception&) { error="Audio settings could not be loaded. Using defaults."; }
        }
    }
    bool live() const { return sim && !sim->finished; }
    void persist() { try { wallet.save(root/"save.json"); } catch(const std::exception& e) { error=e.what(); } }
    std::array<uint32_t,3> freshMarketSeeds() {
        std::array<uint32_t,3> seeds{};
        for(size_t n=0;n<seeds.size();++n) {
            do seeds[n]=random(); while(seeds[n]==0 || std::find(seeds.begin(),seeds.begin()+static_cast<std::ptrdiff_t>(n),seeds[n])!=seeds.begin()+static_cast<std::ptrdiff_t>(n) || std::find(wallet.ownedBallSeeds.begin(),wallet.ownedBallSeeds.end(),seeds[n])!=wallet.ownedBallSeeds.end());
        }
        return seeds;
    }
    void ensureMarket() {
        if(wallet.marketInitialized) return;
        wallet.marketBallSeeds=freshMarketSeeds(); wallet.marketInitialized=true;
    }
    void rebuildCollectionCache() {
        ownedBallCache.clear(); ownedBallCache.reserve(wallet.ownedBallSeeds.size());
        for(uint32_t seed:wallet.ownedBallSeeds) ownedBallCache.push_back(generateBall(catalog,seed));
        for(size_t n=0;n<marketBallCache.size();++n) marketBallCache[n]=wallet.marketBallSeeds[n]?generateBall(catalog,wallet.marketBallSeeds[n]):Fighter{};
    }
    void openInspector(const Fighter& fighter,uint32_t seed,int marketIndex=-1,int ownedIndex=-1) {
        inspectedBall=fighter; inspectionSeed=seed; inspectionMarket=marketIndex; inspectionOwned=ownedIndex; inspectionOpen=true; pendingRemove=-1;
    }
    void resetSeason() {
        wallet.resetSeason(catalog.rules.startingCoins); persist(); stake=1+(wallet.coins-1)/2;
    }
    void reroll() {
        bool completedMatch=sim && sim->finished;
        sim.reset(); preview=generateMatch(catalog,random()); accumulator=0; paused=false; returned=0;
        countdown.reset(); resultAge=0; lastEvent=0; floating.clear(); impacts.clear(); messageCooldown={};
        hitStop=tiltTimer=tiltTotal=tiltKick=screenTilt=0;
        for(int k=0;k<2;++k) appearances[k]=appearanceFor(preview.seed,k);
        stakeDragging=false;
        stake=completedMatch && wallet.coins>0?1+(wallet.coins-1)/2:std::max(1,std::min(stake,wallet.coins));
    }
    void saveAudio() {
        if(!audioBank) return;
        std::ofstream output(root/"settings.json");
        output<<nlohmann::json{{"volume",audioBank->volume},{"muted",audioBank->muted}}.dump(2);
        if(!output.good()) error="Unable to save audio settings.";
    }
    void updateImpactFeedback(float dt) {
        for(auto& impact:impacts) impact.age+=dt;
        std::erase_if(impacts,[](const Impact& impact){ return impact.age>=impact.total; });
        if(tiltTimer<=0) { screenTilt=0; return; }
        tiltTimer=std::max(0.0f,tiltTimer-dt);
        float progress=1-tiltTimer/std::max(0.001f,tiltTotal);
        screenTilt=tiltKick*std::sin(progress*Pi);
    }
    void consumeEvents() {
        if(!sim) return;
        for(const auto& event:sim->events) {
            if(event.serial<=lastEvent) continue;
            lastEvent=event.serial;
            if(audioBank) audioBank->play(event.cue,0.5f+event.position.x/900);
            // A stable hash gives each impact a visually random left or right screen tilt.
            float tiltSide=((event.serial*0x9e3779b97f4a7c15ULL)>>63)?1.0f:-1.0f;
            bool directHit=event.amount>0 && (event.cue==Cue::Hit || event.cue==Cue::Sword || event.cue==Cue::Hammer);
            if(directHit) {
                float strength=std::clamp(event.amount,1.0f,45.0f);
                Vec direction=event.owner>=0&&event.owner<2?normalized(event.position-sim->match.fighters[event.owner].position):Vec{};
                if(length(direction)<0.01f) { float phase=static_cast<float>(event.serial%360)*Pi/180; direction={std::cos(phase),std::sin(phase)}; }
                impacts.push_back({event.position,direction,0,0.28f+0.004f*strength,strength,event.serial,false});
                if(impacts.size()>42) impacts.erase(impacts.begin());
                if(event.amount>=6 && live() && !countdown.active()) {
                    float scale=std::clamp((event.amount-6.0f)/34.0f,0.0f,1.0f);
                    hitStop=std::max(hitStop,0.035f+0.165f*scale);
                    tiltTotal=std::max(tiltTotal,0.10f+0.16f*scale); tiltTimer=tiltTotal;
                    tiltKick=tiltSide*(0.8f+3.2f*scale);
                }
            }
            if(event.cue==Cue::Clash && event.text=="MELEE CLASH") {
                float phase=static_cast<float>(event.serial%360)*Pi/180;
                impacts.push_back({event.position,{std::cos(phase),std::sin(phase)},0,0.24f,1,event.serial,true});
                if(impacts.size()>42) impacts.erase(impacts.begin());
                hitStop=std::max(hitStop,0.028f);
                tiltTotal=std::max(tiltTotal,0.09f); tiltTimer=tiltTotal;
                tiltKick=tiltSide*0.7f;
            }
            // Text above a fighter is reserved for actual healing and damage numbers, never debug/event logs.
            if(event.amount>0 && event.owner>=0 && messageCooldown[event.owner]<=0) {
                std::string label=(event.cue==Cue::Heal?"+":"-")+number(std::ceil(event.amount));
                floating.push_back({label,event.position,event.owner,event.cue==Cue::Heal,event.critical});
                if(floating.size()>12) floating.erase(floating.begin());
                messageCooldown[event.owner]=0.12f;
            }
        }
    }
    void start() {
        if(sim || wallet.coins<stake || stake<1) return;
        // Persist the debit before the fight is revealed; roll back on an I/O failure.
        Wallet before=wallet;
        if(!wallet.place(selected,stake)) return;
        try { wallet.save(root/"save.json"); }
        catch(const std::exception& e) { wallet=before; error=e.what(); return; }
        sim=std::make_unique<Simulation>(preview,catalog.rules); accumulator=0;
        countdown.start(); floating.clear();
        if(audioBank) audioBank->play(Cue::Tick);
    }
    void settle() {
        if(sim && sim->finished && wallet.active) {
            returned=wallet.settle(sim->winner,catalog.rules.payout); persist(); resultAge=0;
            floating.clear();
            if(audioBank) audioBank->play(sim->winner<0?Cue::Draw:sim->winner==selected?Cue::Win:Cue::Lose);
        }
    }
    void update(float dt) {
        dt=std::clamp(dt,0.0f,0.1f); realClock+=dt;
        updateImpactFeedback(dt);
        if(audioBank) {
            audioBank->update(dt);
            if(IsKeyPressed(KEY_M)) { audioBank->setMuted(!audioBank->muted); saveAudio(); }
        }
        if(!paused && !catalogOpen && !rulesOpen && !collectionOpen && !inspectionOpen) {
            for(auto& label:floating) label.age+=dt;
            for(auto& cooldown:messageCooldown) cooldown=std::max(0.0f,cooldown-dt);
            std::erase_if(floating,[](const FloatingText& t){return t.age>=0.95f;});
            if(sim && sim->finished) resultAge+=dt;
        }
        if(IsKeyPressed(KEY_F5) && !live()) {
            try { Catalog fresh=Catalog::load(root/"data/catalog.json"); catalog=std::move(fresh); rebuildCollectionCache(); reroll(); error="Catalog reloaded. New settings are active."; }
            catch(const std::exception& e) { error=std::string("Catalog error: ")+e.what(); }
        }
        if(IsKeyPressed(KEY_ESCAPE) && inspectionOpen) { inspectionOpen=false; pendingRemove=-1; }
        else if(IsKeyPressed(KEY_ESCAPE) && collectionOpen && !catalogOpen && !rulesOpen) collectionOpen=false;
        if(!catalogOpen && !rulesOpen && !collectionOpen && !inspectionOpen) {
            if(IsKeyPressed(KEY_SPACE)) { if(live()) paused=!paused; else if(!sim) start(); }
            if(IsKeyPressed(KEY_R) && !live()) reroll();
            if(IsKeyPressed(KEY_ONE)) speed=1;
            if(IsKeyPressed(KEY_TWO)) speed=2;
            if(IsKeyPressed(KEY_THREE)) speed=4;
            if(IsKeyPressed(KEY_FOUR)) speed=8;
            if(IsKeyPressed(KEY_FIVE)) speed=16;
        }
        if(hitStop>0) {
            hitStop=std::max(0.0f,hitStop-dt);
            if(sim && sim->finished) settle();
            return;
        }
        if(live() && !paused && !catalogOpen && !rulesOpen && !collectionOpen && !inspectionOpen) {
            if(countdown.active()) {
                int previous=countdown.number(); countdown.advance(dt);
                if(countdown.number()!=previous && audioBank) audioBank->play(countdown.number()==0?Cue::Fight:Cue::Tick);
                return;
            }
            accumulator+=dt*speed;
            while(accumulator>=Step && live()) {
                sim->step(); accumulator-=Step; consumeEvents();
                if(hitStop>0) break;
            }
            settle();
        }
    }
    void fighterPanel(int side) {
        const auto& f=sim?sim->match.fighters[side]:preview.fighters[side];
        float x=side==0?28.0f:1100.0f; Color color=Team[side];
        box({x,171,312,565},Panel,selected==side?Fade(color,0.7f):Border,0.04f);
        DrawRectangleRounded({x+1,172,310,4},0.5f,6,color);
        text(side==0?"FIGHTER A":"FIGHTER B",x+20,191,12,color);
        fitted(f.name+" "+f.title,x+20,216,166,24);
        drawPortrait({x+199,233},21,color,appearances[side],realClock,sim && sim->finished && sim->winner==side);
        box({x+236,197,56,42},PanelAlt,Border,0.18f);
        centered(std::to_string(f.points),x+264,201,21,color); centered("PTS",x+264,222,9,Muted);
        float hp=std::clamp(f.hp/f.maxHp,0.0f,1.0f);
        text("HEALTH",x+20,259,11,Muted); text(number(f.hp)+" / "+number(f.maxHp),x+192,255,16,color);
        DrawRectangleRounded({x+20,281,272,7},1,8,Border);
        if(hp>0) DrawRectangleRounded({x+20,281,272*hp,7},1,8,color);
        const std::array<std::pair<std::string,std::string>,3> stats={{{"SPEED",number(f.speed)},{"RADIUS",number(f.radius,1)},{"ARMOR",number(f.armor*100)+"%"}}};
        for(int n=0;n<3;++n) { text(stats[n].first,x+20+n*94,303,9,Muted); text(stats[n].second,x+20+n*94,321,22); }
        text("POWER ×"+number(f.damageMultiplier,2),x+20,354,11,Muted);
        if(f.stunTime>0) text("STUNNED "+number(f.stunTime,1)+" s",x+160,354,11,Coral);
        else if(f.shield>0) text("SHIELD "+number(f.shield),x+160,354,11,color);
        if(f.poisonDps>0) text("POISON -"+number(f.poisonDps,0)+"/s",x+160,369,10,Color{118,226,106,255});
        if(f.comboWindow>0) text("COMBO "+number(f.comboWindow,1)+" s",x+20,369,10,Accent);
        DrawLineEx({x+20,377},{x+292,377},1,Border);
        text("LOADOUT & TRAITS",x+20,393,10,Muted);
        float y=417;
        for(size_t n=0;n<f.items.size();++n) {
            const auto& i=f.items[n]; Rectangle row{x+12,y-4,288,51}; bool hover=inside(row) && !catalogOpen && !rulesOpen;
            if(hover) { DrawRectangleRounded(row,0.15f,6,PanelAlt); hovered=&i; }
            icon(i.effect,{x+34,y+15},i.category=="modifier"?Muted:color,0.75f);
            fitted(i.name,x+57,y,235,15);
            std::string rowDetail=detail(i);
            fitted(rowDetail,x+57,y+22,235,10,Muted);
            if(live() && i.params.contains("cooldown")) {
                float reload=i.get("cooldown")+(f.effect("slow_reload")?1.0f:0.0f); if(f.effect("fast_reload")) reload*=0.65f; reload=std::max(0.1f,reload);
                float progress=1-std::clamp(f.cooldowns[n]/reload,0.0f,1.0f);
                DrawRectangleRec({x+57,y+39,226*progress,2},Fade(color,0.45f));
            }
            y+=55;
        }
        std::string label=sim?(selected==side?"YOUR PICK":"OPPONENT"):(selected==side?"SELECTED  ·  "+number(catalog.rules.payout,2)+"×":"PICK  ·  "+number(catalog.rules.payout,2)+"×");
        if(button({x+20,688,272,32},label,selected==side,!sim,color,12)) selected=side;
    }
    void arena() {
        box({360,171,720,487},Color{12,18,28,255},Border,0.03f);
        std::string arenaEffect=preview.arena.effect=="spiked_arena"?"SPIKED":preview.arena.effect=="center_gravity"?"CENTER GRAVITY":preview.arena.effect=="healing_arena"?"HEALING CENTER":"";
        text("ARENA / "+preview.arena.name+(arenaEffect.empty()?"":"  ·  "+arenaEffect),380,189,12,arenaEffect.empty()?Muted:Accent);
        std::string status=!sim?"STANDBY":sim->finished?"FINISHED":paused?"PAUSED":countdown.active()?"GET READY":"LIVE";
        DrawCircle(956,196,3,!sim?Muted:sim->finished?Accent:Coral); text(status,968,188,11,!sim?Muted:sim->finished?Accent:Coral);
        BeginScissorMode(365,212,710,441);
        Camera2D arenaCamera{{720,432},{720,432},screenTilt,std::abs(screenTilt)>0.01f?1.025f:1.0f};
        BeginMode2D(arenaCamera);
        auto drawStunStar=[](Vector2 center,float radius,Color color) {
            DrawLineEx({center.x-radius,center.y},{center.x+radius,center.y},2.4f,color);
            DrawLineEx({center.x,center.y-radius},{center.x,center.y+radius},2.4f,color);
            DrawLineEx({center.x-radius*0.65f,center.y-radius*0.65f},{center.x+radius*0.65f,center.y+radius*0.65f},1.4f,color);
            DrawLineEx({center.x-radius*0.65f,center.y+radius*0.65f},{center.x+radius*0.65f,center.y-radius*0.65f},1.4f,color);
            DrawCircleV(center,radius*0.25f,White);
        };
        for(int x=390;x<1060;x+=24) for(int y=226;y<653;y+=24) DrawCircle(x,y,0.8f,Color{28,39,54,255});
        const auto& vertices=preview.arena.vertices;
        Color wall=sim && catalog.rules.suddenDps>0 && sim->time>=catalog.rules.sudden?Coral:Color{84,106,133,255};
        for(size_t n=0;n<vertices.size();++n) {
            Vector2 a=point(vertices[n]),b=point(vertices[(n+1)%vertices.size()]);
            DrawTriangle(point({0,0}),b,a,Color{16,25,37,255});
        }
        for(size_t n=0;n<vertices.size();++n) {
            Vector2 a=point(vertices[n]),b=point(vertices[(n+1)%vertices.size()]);
            DrawLineEx(a,b,9,Fade(wall,0.07f)); DrawLineEx(a,b,2,wall);
        }
        if(preview.arena.effect=="spiked_arena") {
            for(size_t n=0;n<vertices.size();++n) {
                Vec a=vertices[n],b=vertices[(n+1)%vertices.size()],edge=b-a;
                float span=length(edge); Vec side=normalized(edge),inward=normalized(Vec{-edge.y,edge.x});
                for(float at=8;at<span-6;at+=18) {
                    Vec base=a+side*at;
                    Vec tip=base+inward*12,left=base-side*5,right=base+side*5;
                    DrawTriangle(point(tip),point(left),point(right),Color{120,39,57,255});
                    DrawTriangle(point(tip-inward*2),point(left+inward*1),point(right+inward*1),Coral);
                    DrawLineEx(point(left),point(tip),1.1f,Fade(White,0.72f));
                }
            }
        } else if(preview.arena.effect=="center_gravity") {
            DrawCircleV(point({0,0}),35,Fade(Cyan,0.06f)); DrawCircleLinesV(point({0,0}),35,Fade(Cyan,0.52f));
            for(int n=0;n<6;++n) {
                float angle=n*Pi/3;
                Vec outer{std::cos(angle)*52,std::sin(angle)*52},inner{std::cos(angle)*38,std::sin(angle)*38};
                DrawLineEx(point(outer),point(inner),1.8f,Fade(Cyan,0.62f));
            }
            text("PULL",704,422,9,Fade(Cyan,0.8f));
        } else if(preview.arena.effect=="healing_arena") {
            float radius=catalog.rules.radius*1.4f;
            DrawCircleV(point({0,0}),radius,Fade(Accent,0.16f)); DrawCircleLinesV(point({0,0}),radius,Fade(Accent,0.85f));
            centered("+15",720,418,11,Accent);
        }
        DrawLineEx(point({-16,0}),point({16,0}),1,Color{40,55,73,255});
        DrawLineEx(point({0,-16}),point({0,16}),1,Color{40,55,73,255});
        if(sim) {
            for(const auto& h:sim->hazards) {
                Vector2 p=point(h.position); Color c=Team[h.owner];
                if(h.mine) {
                    DrawCircleLinesV(p,h.radius,Fade(c,h.arm>0?0.1f:0.28f));
                    DrawCircleV(p,9,Panel); icon("mine",p,Fade(c,h.arm>0?0.4f:1),0.7f);
                    DrawCircleV(p,2,std::sin(sim->time*9)>0?White:c);
                } else {
                    Vec inward=length(h.normal)>0.01f?h.normal:Vec{0,-1};
                    Vec side{-inward.y,inward.x};
                    // Three narrow teeth sit on the wall and point toward the arena centre.
                    DrawLineEx(point(h.position-side*h.radius),point(h.position+side*h.radius),4,Fade(c,0.45f));
                    for(float offset:{-0.58f,0.0f,0.58f}) {
                        Vec base=h.position+side*(offset*h.radius);
                        float width=h.radius*0.25f;
                        Vec left=base-side*width,right=base+side*width,tip=base+inward*(h.radius+13);
                        DrawTriangle(point(tip),point(left),point(right),Fade(c,0.32f));
                        DrawTriangle(point(tip),point(left+inward*2),point(right+inward*2),c);
                        DrawLineEx(point(left),point(tip),1.2f,White);
                    }
                }
            }
            for(const auto& shape:sim->fieldShapes) {
                float alpha=std::clamp(shape.life/shape.total,0.0f,1.0f); Color c=Team[shape.owner];
                if(shape.vertices.size()>=3) {
                    for(size_t n=1;n+1<shape.vertices.size();++n) DrawTriangle(point(shape.vertices[0]),point(shape.vertices[n]),point(shape.vertices[n+1]),Fade(c,0.20f*alpha));
                    for(size_t n=0;n<shape.vertices.size();++n) DrawLineEx(point(shape.vertices[n]),point(shape.vertices[(n+1)%shape.vertices.size()]),2.4f,Fade(c,alpha));
                }
            }
            for(int owner=0;owner<2;++owner) {
                const auto& field=sim->damageFields[owner]; if(field.points.empty()) continue;
                Color c=Team[owner];
                auto fieldLine=[&](Vec a,Vec b) { DrawLineEx(point(a),point(b),5,Fade(c,0.12f)); DrawLineEx(point(a),point(b),2.1f,Fade(c,0.72f)); };
                for(size_t point=1;point<field.points.size();++point) fieldLine(field.points[point-1],field.points[point]);
                if(field.drawing) fieldLine(field.points.back(),sim->match.fighters[owner].position);
            }
            for(const auto& tape:sim->tapes) {
                Vector2 start=point(tape.start),end=point(tape.end); Color tapeColor{255,202,82,255};
                DrawLineEx(start,end,7,Fade(Background,0.9f)); DrawLineEx(start,end,3,Fade(tapeColor,tape.deploying?0.72f:0.95f));
                Vec span=tape.end-tape.start; float distance=length(span);
                if(distance>1) {
                    Vec direction=span/distance,side{-direction.y,direction.x};
                    for(float mark=std::fmod(sim->time*52,14.0f);mark<distance;mark+=14) {
                        Vec at=tape.start+direction*mark;
                        DrawLineEx(point(at-side*3),point(at+side*3),1.3f,Fade(White,tape.deploying?0.65f:0.9f));
                    }
                }
                if(tape.deploying) { DrawCircleV(end,6,Fade(tapeColor,0.25f)); DrawCircleLinesV(end,4,tapeColor); }
            }
            for(const auto& minion:sim->minions) {
                Color c=Team[minion.owner]; Vector2 p=point(minion.position);
                if(minion.weapon.effect=="sword") {
                    Vec dir{std::cos(minion.meleeAngle),std::sin(minion.meleeAngle)};
                    Vec start=minion.position+dir*(minion.radius+3),tip=minion.position+dir*(minion.radius+minion.weapon.get("reach"));
                    DrawLineEx(point(start),point(tip),5,Fade(c,0.3f)); DrawLineEx(point(start),point(tip),2.5f,White);
                    if(minion.weapon.id=="weapon.scythe") {
                        Vec side{-dir.y,dir.x},hook=tip-dir*7,mid=tip+side*18-dir*3,blade=tip+side*27-dir*15;
                        DrawLineEx(point(hook),point(mid),8,Fade(c,0.9f)); DrawLineEx(point(mid),point(blade),8,Fade(c,0.9f));
                        DrawLineEx(point(hook),point(mid),2.5f,White); DrawLineEx(point(mid),point(blade),2.5f,White);
                    }
                }
                DrawCircleV(p,minion.radius+4,Fade(c,0.18f)); DrawCircleV(p,minion.radius,c); DrawCircleLinesV(p,minion.radius,White);
                Appearance look=appearanceFor(sim->match.seed^0x51A7u,minion.owner); look.hat=0; look.beard=0;
                Vec gaze=normalized(sim->match.fighters[1-minion.owner].position-minion.position);
                drawFace(p,minion.radius,look,sim->time,{gaze.x,gaze.y},0,false);
                drawMinionWeapon(minion.weapon,minion.position,minion.velocity,minion.radius,c);
                float hp=std::clamp(minion.hp/minion.maxHp,0.0f,1.0f);
                DrawRectangleRec({p.x-minion.radius,p.y-minion.radius-9,minion.radius*2,3},Panel);
                DrawRectangleRec({p.x-minion.radius,p.y-minion.radius-9,minion.radius*2*hp,3},c);
            }
            for(const auto& mouse:sim->mice) {
                Vec forward=normalized(mouse.velocity),side{-forward.y,forward.x}; Vector2 p=point(mouse.position);
                Color fur=mouse.owner==0?Color{179,193,207,255}:Color{207,178,176,255};
                Color innerEar=mouse.owner==0?Color{244,171,190,255}:Color{246,158,177,255};
                // A tiny round body, ears, whiskers, nose and tail make every summoned mouse readable in the arena.
                DrawLineEx(point(mouse.position-forward*(mouse.radius+3)),point(mouse.position-forward*(mouse.radius+12)+side*4),1.4f,Fade(fur,0.8f));
                DrawCircleV(point(mouse.position-forward*2+side*(mouse.radius*0.55f)),mouse.radius*0.5f,fur);
                DrawCircleV(point(mouse.position-forward*2-side*(mouse.radius*0.55f)),mouse.radius*0.5f,fur);
                DrawCircleV(point(mouse.position-forward*2+side*(mouse.radius*0.55f)),mouse.radius*0.22f,innerEar);
                DrawCircleV(point(mouse.position-forward*2-side*(mouse.radius*0.55f)),mouse.radius*0.22f,innerEar);
                DrawCircleV(p,mouse.radius,fur); DrawCircleLinesV(p,mouse.radius,Fade(White,0.35f));
                DrawCircleV(point(mouse.position+forward*(mouse.radius*0.62f)),1.6f,Color{72,50,61,255});
                DrawCircleV(point(mouse.position+forward*2+side*(mouse.radius*0.38f)),1.15f,Color{24,30,39,255});
                DrawCircleV(point(mouse.position+forward*2-side*(mouse.radius*0.38f)),1.15f,Color{24,30,39,255});
                Vec nose=mouse.position+forward*(mouse.radius*0.66f);
                for(float offset:{-1.0f,1.0f}) DrawLineEx(point(nose+side*(offset*2)),point(nose+forward*2+side*(offset*(mouse.radius+3))),0.8f,Fade(White,0.88f));
            }
            for(const auto& p:sim->projectiles) {
                Vec dir=normalized(p.velocity); Color c=Team[p.owner];
                if(p.kind!="shuriken" && p.kind!="bouncy_ball") DrawLineEx(point(p.position-dir*20),point(p.position),p.radius*1.3f,Fade(c,0.45f));
                if(p.kind=="bow" || p.kind=="poison_dart") { DrawLineEx(point(p.position-dir*12),point(p.position+dir*5),2,p.kind=="poison_dart"?Color{118,226,106,255}:White); Vec side{-dir.y,dir.x}; DrawTriangle(point(p.position+dir*7),point(p.position-dir*1+side*4),point(p.position-dir*1-side*4),c); }
                else if(p.kind=="shuriken") {
                    // A large four-point throwing star spins independently while it flies.
                    float spin=sim->time*24+std::atan2(p.velocity.y,p.velocity.x);
                    for(int blade=0;blade<4;++blade) {
                        float a=spin+blade*Pi/2;
                        Vec tip{std::cos(a),std::sin(a)};
                        // The heavy outline and white inner edge keep every arm visible at combat speed.
                        DrawLineEx(point(p.position),point(p.position+tip*12),7,Color{12,23,37,255});
                        DrawLineEx(point(p.position),point(p.position+tip*12),5,c);
                        DrawLineEx(point(p.position+tip*2),point(p.position+tip*12),1.5f,White);
                    }
                    DrawCircleV(point(p.position),3,Color{12,23,37,255}); DrawCircleV(point(p.position),1.5f,White);
                }
                else if(p.kind=="bouncy_ball") {
                    DrawCircleV(point(p.position),p.radius+4,Fade(c,0.18f));
                    DrawCircleV(point(p.position),p.radius+1,Color{15,24,38,255});
                    DrawCircleV(point(p.position),p.radius,c);
                    DrawCircleV(point(p.position-dir*1.6f-Vec{1.5f,1.5f}),std::max(1.5f,p.radius*0.32f),White);
                    DrawCircleLinesV(point(p.position),p.radius,Fade(White,0.85f));
                }
                else if(p.kind=="boomerang") {
                    float spin=sim->time*5.5f+std::atan2(p.velocity.y,p.velocity.x);
                    Vec armA{std::cos(spin+0.72f),std::sin(spin+0.72f)},armB{std::cos(spin-0.72f),std::sin(spin-0.72f)};
                    DrawLineEx(point(p.position),point(p.position+armA*13),7,Color{15,24,38,255});
                    DrawLineEx(point(p.position),point(p.position+armB*13),7,Color{15,24,38,255});
                    DrawLineEx(point(p.position),point(p.position+armA*13),4.5f,c);
                    DrawLineEx(point(p.position),point(p.position+armB*13),4.5f,c);
                    DrawCircleV(point(p.position),2.5f,White);
                }
                else { DrawCircleV(point(p.position),p.radius+3,Fade(c,0.15f)); DrawCircleV(point(p.position),p.radius,c); }
            }
        }
        if(!sim) for(int k=0;k<2;++k) {
            const auto& owner=preview.fighters[k]; const auto* helper=owner.effect("minion");
            if(!helper || owner.minionWeapon.id.empty()) continue;
            Vec forward=normalized(preview.fighters[1-k].position-owner.position),side{-forward.y,forward.x};
            float offset=((preview.seed^static_cast<uint32_t>(k*419))&1u)?1.0f:-1.0f;
            float radius=helper->get("radius"); Vec position=owner.position+side*offset*(owner.radius+radius+8); Vector2 p=point(position);
            DrawCircleV(p,radius+4,Fade(Team[k],0.18f)); DrawCircleV(p,radius,Team[k]); DrawCircleLinesV(p,radius,White);
            Appearance look=appearanceFor(preview.seed^0x51A7u,k); look.hat=0; look.beard=0;
            drawFace(p,radius,look,0,{forward.x,forward.y},0,false);
            drawMinionWeapon(owner.minionWeapon,position,forward*helper->get("speed"),radius,Team[k]);
            centered("RANDOM WEAPON",p.x,p.y+radius+12,9,Muted);
        }
        for(int k=0;k<2;++k) {
            const auto& f=sim?sim->match.fighters[k]:preview.fighters[k]; Vector2 p=point(f.position); Color c=Team[k];
            if(f.hp<=0) { DrawCircleLinesV(p,f.radius+12,Fade(c,0.3f)); continue; }
            if(sim) {
                Vec dir=normalized(f.velocity);
                for(int t=6;t>0;--t) DrawCircleV(point(f.position-dir*(t*7.0f)),f.radius*(1-t*0.11f),Fade(c,0.02f*(7-t)));
            }
            for(size_t n=0;n<f.items.size();++n) { const auto& i=f.items[n]; if(i.effect=="sword") {
                float a=sim?f.meleeAngles[n]:f.angle; Vec dir{std::cos(a),std::sin(a)},perp{-dir.y,dir.x};
                Vec base=f.position+dir*(f.radius+5),tip=f.position+dir*(f.radius+i.get("reach"));
                if(i.id=="weapon.orbit_shield") {
                    float degrees=a*180/Pi;
                    // A wide 120-degree plate hugs the ball and visibly covers one third of it.
                    DrawRing(p,f.radius+3,f.radius+14,degrees-60,degrees+60,28,Fade(c,0.92f));
                    DrawRingLines(p,f.radius+3,f.radius+14,degrees-60,degrees+60,28,White);
                    Vec edgeA{std::cos(a-Pi/3),std::sin(a-Pi/3)},edgeB{std::cos(a+Pi/3),std::sin(a+Pi/3)};
                    DrawLineEx(point(f.position+edgeA*(f.radius+5)),point(f.position+edgeA*(f.radius+13)),3,White);
                    DrawLineEx(point(f.position+edgeB*(f.radius+5)),point(f.position+edgeB*(f.radius+13)),3,White);
                } else {
                    DrawCircleLinesV(p,f.radius+i.get("reach"),Fade(c,0.06f));
                    float width=i.meleeClass=="heavy"?11:i.meleeClass=="medium"?8:6;
                    DrawLineEx(point(base),point(tip),width,Fade(c,0.2f)); DrawLineEx(point(base),point(tip),width*0.43f,White);
                    if(i.id=="weapon.scythe") {
                        Vec hook=tip-dir*7,mid=tip+perp*22-dir*3,blade=tip+perp*32-dir*18;
                        DrawLineEx(point(hook),point(mid),width+2,Fade(c,0.95f)); DrawLineEx(point(mid),point(blade),width+2,Fade(c,0.95f));
                        DrawLineEx(point(hook),point(mid),width*0.43f,White); DrawLineEx(point(mid),point(blade),width*0.43f,White);
                    }
                    DrawLineEx(point(base+perp*8),point(base-perp*8),3,c);
                }
                if(i.id=="weapon.hammer") {
                    Vec head=tip-dir*5;
                    DrawLineEx(point(head+perp*11),point(head-perp*11),8,Fade(c,0.95f));
                    DrawLineEx(point(head+perp*11),point(head-perp*11),3,White);
                }
            }}
            const auto& other=sim?sim->match.fighters[1-k]:preview.fighters[1-k];
            Vec aim=normalized(other.position-f.position),side{-aim.y,aim.x}; int mount=0;
            for(const auto& i:f.items) if(i.effect=="bow" || i.effect=="pistol" || i.effect=="shotgun" || i.effect=="poison_dart" || i.effect=="shuriken") {
                Vec p0=f.position+aim*(f.radius+9)+side*(mount++*15.0f);
                if(i.effect=="bow" || i.effect=="poison_dart") {
                    float angle=std::atan2(aim.y,aim.x)*180/Pi;
                    DrawRingLines(point(p0),11,12,angle-70,angle+70,16,c);
                    DrawLineEx(point(p0+aim*4+side*10),point(p0+aim*4-side*10),1,i.effect=="poison_dart"?Color{118,226,106,255}:White);
                    DrawLineEx(point(p0-aim*5),point(p0+aim*15),2,c);
                } else if(i.effect=="shuriken") {
                    for(int blade=0;blade<4;++blade) { float a=blade*Pi/2; DrawLineEx(point(p0),point(p0+Vec{std::cos(a),std::sin(a)}*10),3,c); }
                    DrawCircleV(point(p0),2,White);
                } else {
                    DrawLineEx(point(p0-aim*4),point(p0+aim*13),i.effect=="pistol"?7:5,c);
                    DrawLineEx(point(p0),point(p0+side*9),4,White);
                }
            }
            if(f.effect("spiked_skin")) {
                for(int spike=0;spike<12;++spike) {
                    float angle=spike*2*Pi/12+0.12f*std::sin(realClock*3+spike);
                    Vec dir{std::cos(angle),std::sin(angle)},side{-dir.y,dir.x};
                    Vec base=f.position+dir*(f.radius-2),tip=f.position+dir*(f.radius+9);
                    DrawTriangle(point(tip),point(base+side*4),point(base-side*4),Color{232,240,248,255});
                    DrawLineEx(point(base),point(tip),1.2f,Fade(c,0.9f));
                }
            }
            DrawCircleV(p,f.radius+7,Fade(c,0.09f));
            DrawCircleV({p.x+2,p.y+5},f.radius,Color{4,8,14,200});
            DrawCircleV(p,f.radius,f.flash>0?White:Fade(c,f.invulnerable>0?0.4f:1));
            drawFace(p,f.radius,appearances[k],realClock,{aim.x,aim.y},f.flash,sim && sim->finished && sim->winner==k);
            drawHat(p,f.radius,appearances[k],realClock);
            if(sim && f.stunTime>0) {
                float bob=std::sin(sim->time*10)*2;
                float top=p.y-f.radius*(appearances[k].hat?2.55f:1.65f);
                for(int star=0;star<3;++star) {
                    float phase=sim->time*5+star*2*Pi/3;
                    Vector2 at{p.x+std::cos(phase)*19,top+std::sin(phase)*6+bob};
                    drawStunStar(at,4.5f,Accent);
                }
            }
            if(f.shield>0) DrawCircleLinesV(p,f.radius+11,Accent);
            if(f.invulnerable>0) DrawCircleLinesV(p,f.radius+15,White);
            if(f.slowTime>0) { DrawCircleLinesV(p,f.radius+13,Color{255,202,82,255}); DrawCircleV({p.x-f.radius*0.75f,p.y-f.radius*0.8f},3,Color{255,202,82,255}); }
            if(f.poisonDps>0) { DrawCircleLinesV(p,f.radius+18,Color{118,226,106,255}); DrawCircleV({p.x+f.radius*0.55f,p.y-f.radius*0.8f},3,Color{118,226,106,255}); }
            if(sim) {
                float healthY=p.y-f.radius*(appearances[k].hat?2.2f:1.0f)-11;
                DrawRectangleRec({p.x-22,healthY,44,4},Border);
                DrawRectangleRec({p.x-22,healthY,44*std::clamp(f.hp/f.maxHp,0.0f,1.0f),4},c);
                if(f.comboWindow>0) {
                    DrawRectangleRec({p.x-18,healthY+7,36,2},Border);
                    DrawRectangleRec({p.x-18,healthY+7,36*std::clamp(f.comboWindow/ComboWindow,0.0f,1.0f),2},Accent);
                }
            }
        }
        for(const auto& impact:impacts) {
            float progress=impact.age/impact.total,alpha=std::clamp(1-progress*1.35f,0.0f,1.0f);
            int count=impact.sparks?14:7+static_cast<int>(impact.strength/7);
            float phase=static_cast<float>(impact.seed%360)*Pi/180;
            if(!impact.sparks) {
                float streakLength=(27+impact.strength*2.7f)*(1-progress*0.55f);
                float halfWidth=1.8f+impact.strength*0.1125f;
                Vec base=impact.position-impact.direction*streakLength,side{-impact.direction.y,impact.direction.x};
                Vec tip=impact.position+impact.direction*(3+impact.strength*0.08f);
                DrawTriangle(point(tip),point(base+side*halfWidth),point(base-side*halfWidth),Fade(White,alpha*0.8f));
                DrawTriangle(point(tip),point(base+side*(halfWidth*0.42f)),point(base-side*(halfWidth*0.42f)),Fade(White,alpha));
                DrawCircleV(point(impact.position),halfWidth*0.65f,Fade(White,alpha));
            }
            for(int n=0;n<count;++n) {
                float angle=phase+n*2.39996f;
                Vec direction{std::cos(angle),std::sin(angle)};
                if(impact.sparks) {
                    float reach=(10+(n%5)*4)*progress;
                    Vector2 from=point(impact.position+direction*std::max(0.0f,reach-7));
                    Vector2 to=point(impact.position+direction*reach);
                    Color spark=n%3==0?White:Color{255,202,82,255};
                    DrawLineEx(from,to,2.2f*(1-progress)+0.4f,Fade(spark,alpha));
                    DrawCircleV(to,1.4f*(1-progress)+0.5f,Fade(spark,alpha));
                } else {
                    float reach=(7+(n%5)*4+impact.strength*0.35f)*progress;
                    Vec drop=impact.position+direction*reach+Vec{0,progress*progress*9};
                    float radius=1.2f+(n%3)*0.65f+impact.strength*0.025f;
                    DrawCircleV(point(drop),radius,Fade(Coral,alpha*0.9f));
                }
            }
        }
        if(sim) for(const auto& b:sim->bursts) {
            float progress=1-b.life/b.total; Vector2 p=point(b.position);
            if(b.text.empty()) { DrawCircleLinesV(p,b.radius*(0.3f+0.7f*progress),Fade(Team[b.owner],1-progress)); DrawCircleV(p,b.radius*(0.3f+0.7f*progress),Fade(Team[b.owner],0.07f*(1-progress))); }
        }
        for(const auto& label:floating) {
            float progress=label.age/0.95f,alpha=std::min(1.0f,(1-progress)*3);
            Vector2 p=point(label.position);
            p.x=std::clamp(p.x,475.0f,965.0f); p.y=std::clamp(p.y-32-progress*38,235.0f,612.0f);
            float size=(label.message.size()<6?21+5*std::exp(-label.age*18):12)*(label.critical?1.45f:1);
            centered(label.message,p.x+1,p.y+2,size,Fade(Background,alpha));
            centered(label.message,p.x,p.y,size,Fade(label.heal?Accent:(label.critical?Color{255,218,72,255}:White),alpha));
        }
        if(!sim) {
            centered("READ THE MATCH. MAKE YOUR PICK.",720,588,13,Muted);
            centered("Every loadout has an edge.",720,610,12,Color{86,105,127,255});
        }
        if(sim && sim->finished) {
            float enter=1-std::pow(1-std::min(1.0f,resultAge/0.45f),3),offset=(1-enter)*35;
            Color c=sim->winner<0?Accent:Team[sim->winner];
            for(int n=0;n<40 && resultAge<2.4f;++n) {
                float angle=n*2.39996f,velocity=75+(n%7)*19.0f;
                float x=720+std::cos(angle)*velocity*resultAge,y=422+std::sin(angle)*velocity*resultAge+55*resultAge*resultAge;
                DrawRectanglePro({x,y,5,9},{2,4},n*17+resultAge*140,Fade(n%3?c:Accent,std::max(0.0f,1-resultAge/2.4f)));
            }
            box({459,347+offset,522,159},Fade(Background,0.95f*enter),Fade(c,enter),0.08f);
            centered(sim->winner<0?"DRAW":sim->match.fighters[sim->winner].name+" WINS!",720,367+offset,34+8*(1-enter),Fade(c,enter));
            centered(sim->winner<0?"Your stake has been refunded.":sim->winner==selected?"Good call. +"+std::to_string(returned-wallet.stake)+" coins":"The arena takes this one. -"+std::to_string(wallet.stake)+" coins",720,425+offset,17,Fade(White,enter));
            centered("Damage dealt: "+number(sim->match.fighters[0].damageDealt)+"  /  "+number(sim->match.fighters[1].damageDealt),720,468+offset,12,Fade(Muted,enter));
        } else if(paused) centered("PAUSED",720,406,32,Accent);
        else if(countdown.active()) {
            float progress=countdown.phase(); bool fight=countdown.number()==0;
            DrawRectangleRec({365,212,710,441},Color{7,12,20,170});
            Color c=fight?Accent:White;
            DrawRing({720,416},76+progress*24,78+progress*24,-90,270,80,Fade(fight?Accent:Cyan,(1-progress)*0.7f));
            std::string label=fight?"FIGHT!":std::to_string(countdown.number());
            float size=(fight?76.0f:104.0f)*(1+0.2f*std::exp(-progress*12));
            centered(fight?"LET THE ODDS PLAY OUT":"BET LOCKED · GET READY",720,308,12,Muted);
            centered(label,723,367,size,Fade(Background,0.8f));
            centered(label,720,364,size,c);
            for(int n=0;n<4;++n) DrawCircle(690+n*20,504,3,n<=3-countdown.number()?Accent:Border);
            centered("GOOD LUCK!",720,535,10,Muted);
        }
        EndMode2D();
        EndScissorMode();
    }
    void controls() {
        box({360,674,720,62});
        text(sim?number(sim->time,1)+" s":"0.0 s",380,684,24);
        text("MATCH TIME",381,714,9,Muted);
        if(button({494,689,80,32},paused?"Resume":"Pause",false,live(),Accent,12)) paused=!paused;
        text("SPEED",595,700,10,Muted);
        const int speeds[]={1,2,4,8,16};
        for(int n=0;n<5;++n) if(button({653.0f+n*80,689,69,32},std::to_string(speeds[n])+"×",speed==speeds[n],true,Accent,14)) speed=speeds[n];
    }
    void betSlip() {
        box({28,758,1384,125},Panel,Border,0.08f);
        DrawRectangleRounded({29,759,4,123},0.5f,6,Accent);
        text("YOUR BET SLIP",51,778,11,Accent);
        text(preview.fighters[selected].name+" "+preview.fighters[selected].title,51,802,24,Team[selected]);
        text("Match winner  ·  odds "+number(catalog.rules.payout,2),51,842,12,Muted);
        text("STAKE",352,778,10,Muted);
        text(std::to_string(stake)+" coins",765,772,20,Accent);
        Rectangle stakeSlider{352,809,490,8};
        Rectangle stakeHitArea{stakeSlider.x,stakeSlider.y-15,stakeSlider.width,38};
        bool canSetStake=!sim && wallet.coins>0;
        if(canSetStake && inside(stakeHitArea)) {
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) stakeDragging=true;
        }
        if(stakeDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float fraction=std::clamp((mouse.x-stakeSlider.x)/stakeSlider.width,0.0f,1.0f);
            stake=1+static_cast<int>(std::lround(fraction*(wallet.coins-1)));
        }
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) stakeDragging=false;
        float stakeFraction=wallet.coins>1?static_cast<float>(stake-1)/(wallet.coins-1):0;
        DrawRectangleRounded(stakeSlider,1,8,Border);
        if(stakeFraction>0) DrawRectangleRounded({stakeSlider.x,stakeSlider.y,stakeSlider.width*stakeFraction,stakeSlider.height},1,8,Accent);
        DrawCircleV({stakeSlider.x+stakeSlider.width*stakeFraction,stakeSlider.y+stakeSlider.height/2},9,canSetStake?Accent:Muted);
        text("1",352,832,10,Muted); text("MAX "+std::to_string(wallet.coins),764,832,10,Muted);
        text(sim?"BET LOCKED":"VIRTUAL COINS",352,848,10,Muted);
        text("POTENTIAL RETURN",871,778,10,Muted);
        text(std::to_string(payoutFor(stake,catalog.rules.payout))+" coins",871,805,23,Accent);
        text("Includes your original stake",871,842,11,Muted);
        if(!sim) {
            if(button({1102,784,286,53},"PLACE BET & FIGHT  →",true,wallet.coins>=stake && stake>0,Accent,16)) start();
            centered("SPACE / start",1245,848,10,Muted);
        } else if(sim->finished) {
            if(button({1102,784,286,53},wallet.coins>0?"NEXT MATCH  →":"NEW SEASON  →",true,true,Accent,16)) {
                if(wallet.coins==0) resetSeason();
                reroll();
            }
            centered(wallet.coins>0?"New arena. New loadouts.":"Fresh wallet and season stats",1245,848,10,Muted);
        } else {
            button({1102,784,286,53},countdown.active()?"GET READY...":"MATCH IN PROGRESS",false,false,Accent,16);
            centered("Your bet has been saved",1245,848,10,Muted);
        }
        if(!sim && wallet.coins==0) {
            if(button({1102,840,286,28},"NEW SEASON · "+std::to_string(catalog.rules.startingCoins)+" COINS",true,true,Accent,11)) { resetSeason(); reroll(); }
        }
    }
    void collectionScreen() {
        text("MY COLLECTION",28,99,30);
        text(std::to_string(wallet.ownedBallSeeds.size())+" owned  /  "+std::to_string(wallet.collectionSlots)+" slots unlocked  /  12 max",29,135,12,Muted);
        text(std::to_string(wallet.wins)+" W  /  "+std::to_string(wallet.losses)+" L  /  "+std::to_string(wallet.draws)+" D",1182,118,12,Muted);
        for(int slot=0;slot<12;++slot) {
            float x=28+(slot%6)*230.0f,y=166+(slot/6)*204.0f; Rectangle card{x,y,214,188};
            bool unlocked=slot<wallet.collectionSlots;
            box(card,unlocked?Panel:Color{14,19,28,255},unlocked?Border:Color{29,38,51,255},0.05f);
            text("SLOT "+std::to_string(slot+1),x+12,y+10,9,unlocked?Accent:Muted);
            if(slot<static_cast<int>(ownedBallCache.size())) {
                const auto& fighter=ownedBallCache[slot]; uint32_t seed=wallet.ownedBallSeeds[slot]; Color color=Team[seed&1u];
                drawPortrait({x+46,y+75},28,color,appearanceFor(seed,0),realClock);
                fitted(fighter.name+" "+fighter.title,x+84,y+40,118,14);
                text(std::to_string(fighter.points)+" PTS",x+84,y+68,11,color);
                fitted(fighter.items.empty()?"No loadout":fighter.items[0].name,x+84,y+94,118,10,Muted);
                if(button({x+12,y+145,91,29},"INSPECT",false,true,Accent,10)) openInspector(fighter,seed,-1,slot);
                std::string removeLabel=pendingRemove==slot?"CONFIRM":"REMOVE";
                if(button({x+111,y+145,91,29},removeLabel,false,true,Coral,10)) {
                    if(pendingRemove==slot) { wallet.removeOwnedBall(slot); persist(); rebuildCollectionCache(); pendingRemove=-1; return; }
                    pendingRemove=slot;
                }
            } else if(unlocked) {
                centered("EMPTY SLOT",x+107,y+69,16,Muted);
                centered("Buy a ball from the market",x+107,y+99,10,Color{86,105,127,255});
            } else if(slot==wallet.collectionSlots) {
                int cost=wallet.nextCollectionSlotCost();
                centered("NEXT SLOT",x+107,y+46,13,Accent);
                centered(std::to_string(cost)+" COINS",x+107,y+78,20,White);
                centered("Prices double each time",x+107,y+110,10,Muted);
                if(button({x+22,y+145,170,29},"UNLOCK",true,wallet.coins>=cost,Accent,11)) { wallet.unlockCollectionSlot(); persist(); }
            } else {
                centered("LOCKED",x+107,y+70,15,Color{70,82,98,255});
                centered("Unlock previous slot first",x+107,y+101,9,Color{70,82,98,255});
            }
        }
        text("BALL MARKET",28,594,25);
        text("Three persistent offers. Inspect before buying, or replace all three.",215,603,11,Muted);
        for(int market=0;market<3;++market) {
            float x=28+market*337.0f,y=635; Rectangle card{x,y,321,222};
            box(card,Panel,Border,0.05f); text("OFFER "+std::to_string(market+1),x+14,y+12,9,Accent);
            uint32_t seed=wallet.marketBallSeeds[market];
            if(seed==0) {
                centered("SOLD",x+160,y+82,24,Muted); centered("Refresh the market for a new offer",x+160,y+122,10,Color{86,105,127,255});
                continue;
            }
            const auto& fighter=marketBallCache[market]; Color color=Team[seed&1u]; int price=marketPrice(fighter);
            drawPortrait({x+58,y+94},32,color,appearanceFor(seed,0),realClock);
            fitted(fighter.name+" "+fighter.title,x+109,y+47,196,16);
            text(std::to_string(fighter.points)+" PTS",x+109,y+78,11,color);
            std::string loadout=fighter.items.empty()?"No loadout":fighter.items[0].name;
            if(fighter.items.size()>1) loadout+="  /  "+fighter.items[1].name;
            fitted(loadout,x+109,y+103,196,10,Muted);
            text(std::to_string(price)+" COINS",x+109,y+129,16,Accent);
            if(button({x+14,y+175,104,31},"INSPECT",false,true,Accent,11)) openInspector(fighter,seed,market,-1);
            bool room=wallet.ownedBallSeeds.size()<static_cast<size_t>(wallet.collectionSlots);
            if(button({x+128,y+175,179,31},room?"BUY":"NO FREE SLOT",true,room&&wallet.coins>=price,Accent,11)) {
                if(wallet.buyMarketBall(market,price)) { persist(); rebuildCollectionCache(); pendingRemove=-1; return; }
            }
        }
        box({1039,635,373,222},PanelAlt,Border,0.05f);
        text("REFRESH MARKET",1062,657,18,Accent);
        wrapped("Replace every offer with three newly generated balls. Purchased offers are replaced too.",1062,692,323,12,Muted,3);
        centered("COST  1000 COINS",1225,759,16,White);
        if(button({1070,802,310,36},"REFRESH ALL  /  1000",true,wallet.coins>=1000,Accent,12)) {
            if(wallet.refreshMarket(freshMarketSeeds())) { persist(); rebuildCollectionCache(); inspectionOpen=false; pendingRemove=-1; }
        }
        text("Owned balls are permanent until you confirm removal. Arena deployment will be added in the next stage.",28,895,11,Muted);
    }
    void ballInspector() {
        if(!inspectionOpen) return;
        DrawRectangle(0,0,Width,Height,Color{3,7,12,222}); inputBlocked=false;
        box({205,86,1030,768},Panel,Border,0.025f);
        text("BALL INSPECTION",237,112,28);
        if(button({1171,107,36,34},"×",false,true,Accent,22) || IsKeyPressed(KEY_ESCAPE)) { inspectionOpen=false; pendingRemove=-1; return; }
        Color color=Team[inspectionSeed&1u]; Appearance look=appearanceFor(inspectionSeed,0);
        box({237,170,280,314},PanelAlt,Border,0.05f);
        drawPortrait({377,293},58,color,look,realClock);
        centered(inspectedBall.name+" "+inspectedBall.title,377,389,19,White);
        centered(std::to_string(inspectedBall.points)+" BALANCE PTS",377,426,12,color);
        text("HP",257,456,9,Muted); text(number(inspectedBall.maxHp),292,450,15);
        text("SPEED",364,456,9,Muted); text(number(inspectedBall.speed),418,450,15);
        text("SIZE",257,476,9,Muted); text(number(inspectedBall.radius,1),301,470,15);
        text("ARMOR",364,476,9,Muted); text(number(inspectedBall.armor*100)+"%",420,470,15);
        text("LOADOUT & TRAITS",555,171,12,Accent);
        float y=205;
        for(const auto& item:inspectedBall.items) {
            box({548,y,650,91},PanelAlt,Border,0.04f); icon(item.effect,{578,y+30},item.category=="modifier"?Muted:color,0.9f);
            text(item.name,608,y+12,17); text(item.category+"  /  "+std::to_string(item.points)+" pts",608,y+38,10,Muted);
            wrapped(detail(item),608,y+59,570,10,White,2); y+=101;
        }
        if(inspectionMarket>=0) {
            int price=marketPrice(inspectedBall); bool room=wallet.ownedBallSeeds.size()<static_cast<size_t>(wallet.collectionSlots);
            text("MARKET PRICE",237,536,10,Muted); text(std::to_string(price)+" COINS",237,558,25,Accent);
            if(button({237,611,280,48},room?"BUY THIS BALL":"UNLOCK A SLOT FIRST",true,room&&wallet.coins>=price,Accent,14)) {
                if(wallet.buyMarketBall(static_cast<size_t>(inspectionMarket),price)) { persist(); rebuildCollectionCache(); inspectionOpen=false; pendingRemove=-1; }
            }
        } else if(inspectionOwned>=0) {
            text("OWNED BALL",237,536,10,Muted);
            text("This ball is stored in slot "+std::to_string(inspectionOwned+1)+".",237,558,14,White);
            std::string label=pendingRemove==inspectionOwned?"CONFIRM PERMANENT REMOVAL":"REMOVE FROM COLLECTION";
            if(button({237,611,280,48},label,false,true,Coral,12)) {
                if(pendingRemove==inspectionOwned) { wallet.removeOwnedBall(static_cast<size_t>(inspectionOwned)); persist(); rebuildCollectionCache(); inspectionOpen=false; pendingRemove=-1; }
                else pendingRemove=inspectionOwned;
            }
        }
        text("Seed "+std::to_string(inspectionSeed),237,811,10,Color{75,89,108,255});
    }
    void tooltip() {
        if(!hovered || catalogOpen || rulesOpen || collectionOpen || inspectionOpen) return;
        const Item& i=*hovered; float x=std::clamp(mouse.x+18,20.0f,1050.0f),y=std::clamp(mouse.y+16,20.0f,Height-222.0f);
        box({x,y,365,202},Color{25,34,47,255},Muted,0.06f);
        text(i.name,x+16,y+14,20,Accent);
        text(i.id+"  /  "+std::to_string(i.points)+" pts",x+16,y+42,12,Muted);
        wrapped(i.description,x+16,y+66,335,14,White,2);
        std::string params;
        for(const auto& [key,value]:i.params) { if(!params.empty()) params+="  ·  "; params+=key+": "+number(value,2); }
        wrapped(params,x+16,y+114,335,11,Muted,4);
    }
    void modal() {
        if(!catalogOpen && !rulesOpen) return;
        DrawRectangle(0,0,Width,Height,Color{3,7,12,218});
        inputBlocked=false;
        box({185,90,1070,748},Panel,Border,0.025f);
        text(catalogOpen?"Loadout catalog":"How to play",215,113,30);
        if(button({1190,110,36,34},"×",false,true,Accent,22) || IsKeyPressed(KEY_ESCAPE)) { catalogOpen=false; rulesOpen=false; return; }
        if(rulesOpen) {
            const std::vector<std::pair<std::string,std::string>> paragraphs={
                {"01 / Read the loadouts","Both fighters roll weapons, an ability and stat modifiers. Hover over any item to see its ID, balance cost and parameters. Radius uses arena units; speed is units per second. Faces and hats are purely cosmetic."},
                {"02 / Pick your winner","Pick A or B, set a stake and lock in your bet. A 3, 2, 1, FIGHT! countdown starts the match. All coins are virtual. Fixed odds are not a prediction of the outcome. Returns include your stake."},
                {"03 / Watch the fight","Fighters move automatically. Projectiles and traps cannot hurt their owner. Contact deals minor damage. Dash closes the gap; dodge reacts to incoming projectiles. Speed changes never affect the countdown."},
                {"04 / Draw rule","There is no overtime damage. The fight lasts at most "+number(catalog.rules.limit)+" s. If both fighters survive the limit or die in the same step, the match is a draw and your stake is refunded."},
                {"05 / Balance is a starting point","Matchmaking pairs loadouts with similar point totals. Equal costs do not guarantee equal odds: weapon combinations and arena geometry matter. Edit data/catalog.json and press F5 outside a match."},
                {"06 / Collection and saving","Spend winnings in My Collection. Unlock up to 12 slots, inspect or buy one of three persistent market offers, and refresh all offers for 1000 coins. Space starts or pauses; 1–5 changes speed; M mutes. Your collection survives a new season."}
            };
            float y=177;
            for(const auto& [title,body]:paragraphs) { text(title,218,y,18,Accent); wrapped(body,218,y+29,986,15,Muted,3); y+=104; }
            return;
        }
        text("Values are loaded from data/catalog.json. Press F5 outside a match to reload.",215,156,14,Muted);
        const std::string tabs[]={"Weapons","Abilities","Stats","Visuals","Cosmetics"};
        for(int n=0;n<5;++n) if(button({215.0f+n*123,188,112,34},tabs[n],catalogTab==n,true,Accent,14)) { catalogTab=n; catalogScroll=0; }
        if(catalogTab==3) {
            struct Visual { const char* kind; const char* name; const char* id; const char* description; };
            const Visual visuals[]={
                {"minion","Pocket Minion","visual.minion.body","Small face-bearing ball. It appears beside its owner before the bet and carries a 40 HP bar."},
                {"sword","Minion Weapon Badge","visual.minion.weapon","The icon above a minion shows its exact randomly rolled weapon before the fight starts."},
                {"flute","Mouse Swarm","visual.mouse.swarm","Round mice have ears, inner ears, eyes, noses, whiskers and tails. Their fur follows their summoner's team."},
                {"shield","Fighter Cosmetics","visual.fighter.cosmetics","Six eye styles, six mouths, ten hats plus no hat, and five beard styles. These never affect balance."}
            };
            for(int row=0;row<4;++row) {
                const auto& visual=visuals[row]; float y=244+row*133.0f;
                box({215,y,1010,121},PanelAlt,Border,0.04f); icon(visual.kind,{245,y+29},Accent);
                text(visual.name,272,y+14,20); text(visual.id,272,y+42,12,Muted);
                wrapped(visual.description,234,y+66,950,14,White,2);
            }
            text("These cards describe the visual elements currently used in the arena.",215,798,12,Muted);
            return;
        }
        if(catalogTab==4) {
            const char* eyes[]={"Bright","Determined","Sleepy","Happy","Googly","Shades"};
            const char* mouths[]={"Smile","Grin","Smirk","Surprised","Toothy Smile","Deadpan"};
            const char* basicHats[]={"No Hat","Top Hat","Crown","Beanie","Party Hat","Cap"};
            const char* extraHats[]={"Cowboy","Headband","Viking","Wizard","Bandana","No Hat"};
            const char* beards[]={"Clean","Stubble","Moustache","Curled Moustache","Grey Full Beard","Split Beard"};
            const char* headings[]={"EYE STYLES","MOUTH STYLES","HATS 1 / 2","HATS 2 / 2","BEARD STYLES"};
            catalogScroll=std::clamp(catalogScroll,0,4);
            text(headings[catalogScroll],215,246,18,Accent);
            text("Cosmetics are visual only. Use the arrows to browse every variant.",215,272,12,Muted);
            for(int n=0;n<6;++n) {
                float x=215+n*166.0f,y=300; box({x,y,154,390},PanelAlt,Border,0.04f);
                Appearance appearance{}; appearance.palette=n%5;
                const char* label="";
                if(catalogScroll==0) { appearance.eyes=n; label=eyes[n]; }
                else if(catalogScroll==1) { appearance.mouth=n; label=mouths[n]; }
                else if(catalogScroll==2) { appearance.hat=n; label=basicHats[n]; }
                else if(catalogScroll==3) { appearance.hat=n<5?n+6:0; label=extraHats[n]; }
                else { appearance.beard=n; label=beards[n]; }
                drawPortrait({x+77,y+162},40,Team[n%2],appearance,0.5f);
                centered(label,x+77,y+286,14,White);
                text(catalogScroll<2?"FACE":catalogScroll<4?"HEADWEAR":"FACIAL HAIR",x+16,y+326,9,Muted);
            }
            if(button({1060,790,74,30},"←",false,catalogScroll>0,Accent,17)) --catalogScroll;
            if(button({1144,790,74,30},"→",false,catalogScroll<4,Accent,17)) ++catalogScroll;
            centered(std::to_string(catalogScroll+1)+" / 5",1139,760,11,Muted);
            return;
        }
        std::vector<const Item*> filtered;
        for(const auto& i:catalog.items) if(i.category==(catalogTab==0?"weapon":catalogTab==1?"ability":"modifier")) filtered.push_back(&i);
        catalogScroll=std::clamp(catalogScroll-static_cast<int>(GetMouseWheelMove()),0,std::max(0,static_cast<int>(filtered.size())-4));
        for(int row=0;row<4 && row+catalogScroll<static_cast<int>(filtered.size());++row) {
            const auto& i=*filtered[row+catalogScroll]; float y=244+row*133.0f;
            box({215,y,1010,121},PanelAlt,Border,0.04f); icon(i.effect,{245,y+29},Accent);
            text(i.name,272,y+14,20); text(i.id,272,y+42,12,Muted);
            text(std::to_string(i.points)+" PTS",1110,y+18,20,Accent);
            text(i.description,234,y+66,14,White);
            std::string params;
            for(const auto& [key,value]:i.params) { if(!params.empty()) params+="   /   "; params+=key+": "+number(value,2); }
            wrapped(params,234,y+93,968,11,Muted,1);
        }
        if(button({1060,790,74,30},"↑",false,catalogScroll>0,Accent,17)) --catalogScroll;
        if(button({1144,790,74,30},"↓",false,catalogScroll+4<static_cast<int>(filtered.size()),Accent,17)) ++catalogScroll;
        text("Keep IDs unique. Names, point costs and parameters are editable.",215,798,12,Muted);
    }
    void draw() {
        inputBlocked=catalogOpen || rulesOpen || inspectionOpen;
        hovered=nullptr; SetMouseCursor(MOUSE_CURSOR_DEFAULT); ClearBackground(Background);
        DrawCircle(43,40,12,Accent); DrawCircle(43,40,5,Background);
        text("ORBITAL",65,24,26); text("ODDS",182,24,26,Accent);
        if(button({276,23,124,34},"ARENA",!collectionOpen,!live(),Accent,11)) { collectionOpen=false; inspectionOpen=false; pendingRemove=-1; }
        if(button({408,23,148,34},"MY COLLECTION",collectionOpen,!live(),Accent,11)) { collectionOpen=true; catalogOpen=false; rulesOpen=false; }
        if(audioBank) {
            if(button({570,24,90,32},!audioBank->ready?"NO AUDIO":audioBank->muted?"MUTED":"SFX ON",false,audioBank->ready,Accent,10)) {
                audioBank->setMuted(!audioBank->muted); saveAudio(); audioBank->play(Cue::Click);
            }
            Rectangle slider{672,24,82,32};
            if(!inputBlocked && audioBank->ready && inside(slider)) {
                SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
                if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) volumeDragging=true;
            }
            if(volumeDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { audioBank->setVolume((mouse.x-slider.x)/slider.width); audioBank->setMuted(false); }
            if(volumeDragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) { volumeDragging=false; saveAudio(); }
            DrawRectangleRounded({672,38,82,4},1,6,Border);
            DrawRectangleRounded({672,38,82*audioBank->volume,4},1,6,audioBank->muted?Muted:Accent);
            DrawCircleV({672+82*audioBank->volume,40},5,audioBank->muted?Muted:Accent);
        }
        if(button({770,24,83,32},"Rules",rulesOpen,true,Accent,11)) rulesOpen=!rulesOpen;
        if(button({862,24,93,32},"Catalog",catalogOpen,true,Accent,11)) catalogOpen=!catalogOpen;
        if constexpr(ShowDebugAddCoins) if(button({966,24,111,32},"ADD 10K",false,true,Coral,11)) { wallet.coins=std::min(1000000000,wallet.coins+10000); persist(); }
        box({1169,18,243,45},PanelAlt,Border,0.2f);
        DrawCircle(1193,40,7,Accent); text(std::to_string(wallet.coins),1212,25,23,Accent); text("COINS",1348,34,10,Muted);
        DrawLine(28,77,1412,77,Border);
        if(collectionOpen) collectionScreen();
        else {
            text("MATCH #"+std::to_string(wallet.rounds+(sim&&sim->finished?0:1)),28,99,12,Accent);
            text("Small arena. Big personalities.",28,120,29);
            text("SEED "+std::to_string(preview.seed),690,108,10,Muted);
            text("Balance gap: "+std::to_string(std::abs(preview.fighters[0].points-preview.fighters[1].points))+" pts",690,130,12,Muted);
            text(std::to_string(wallet.wins)+" W  /  "+std::to_string(wallet.losses)+" L  /  "+std::to_string(wallet.draws)+" D",968,118,12,Muted);
            if(button({1169,107,243,36},"REROLL MATCH  /  R",false,!live(),Accent,12)) reroll();
            fighterPanel(0); fighterPanel(1); arena(); controls(); betSlip();
            text("F5  reload balance   ·   SPACE  start / pause   ·   1–5  speed   ·   M  mute",28,907,11,Muted);
            text("Virtual coins only.",1210,907,10,Muted);
        }
        tooltip(); modal(); ballInspector();
        if(!error.empty()) {
            box({340,84,760,66},PanelAlt,Accent,0.08f); wrapped(error,355,95,704,14,White,2);
            if(button({1061,98,27,28},"×",false,true,Accent,18)) error.clear();
        }
    }
};
std::filesystem::path locateRoot() {
    auto cwd=std::filesystem::current_path(); if(std::filesystem::exists(cwd/"data/catalog.json")) return cwd;
#if defined(_WIN32)
    std::array<wchar_t,32768> executable{};
    GetModuleFileNameW(nullptr,executable.data(),static_cast<unsigned long>(executable.size()));
    auto dir=std::filesystem::path(executable.data()).parent_path();
#else
    auto dir=std::filesystem::path(reinterpret_cast<const char8_t*>(GetApplicationDirectory()));
#endif
    for(int n=0;n<4;++n) { if(std::filesystem::exists(dir/"data/catalog.json")) return dir; dir=dir.parent_path(); }
    return cwd;
}
Font loadFont(int resolution=48,bool bold=false) {
    std::vector<int> glyphs; for(int c=32;c<=382;++c) glyphs.push_back(c);
    for(int c:{8211,8212,8217,8226,8592,8593,8594,8595,8722}) glyphs.push_back(c);
    const char* paths[]={bold?"C:/Windows/Fonts/segoeuib.ttf":"C:/Windows/Fonts/segoeui.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/System/Library/Fonts/Supplemental/Arial.ttf"};
    for(const char* path:paths) if(FileExists(path)) {
        Font f=LoadFontEx(path,resolution,glyphs.data(),static_cast<int>(glyphs.size())); SetTextureFilter(f.texture,TEXTURE_FILTER_BILINEAR); return f;
    }
    return GetFontDefault();
}
void styleSheet() {
    ClearBackground(Background); text("THE PERSONALITY DEPARTMENT",50,34,30,Accent);
    text("6 eye styles / 6 mouths / 10 hats + no hat / 5 beard styles. Cosmetics do not affect balance.",50,80,15,Muted);
    const char* eyes[]={"Bright","Determined","Sleepy","Happy","Googly","Shades"};
    const char* mouths[]={"Smile","Grin","Smirk","Surprised","Toothy Smile","Deadpan"};
    const char* hats[]={"No hat","Cowboy","Headband","Viking","Wizard","Bandana"};
    const int hatStyles[]={0,6,7,8,9,10};
    for(int row=0;row<3;++row) {
        text(row==0?"EYES":row==1?"MOUTHS":"HEADWEAR",50,132+row*252.0f,12,Muted);
        for(int n=0;n<6;++n) {
            float x=50+n*226.0f,y=163+row*252.0f;
            box({x,y,210,203}); Appearance a{}; a.palette=n%5;
            if(row==0) a.eyes=n;
            if(row==1) a.mouth=n;
            if(row==2) { a.hat=hatStyles[n]; a.mouth=n%6; a.beard=n%6; }
            drawPortrait({x+105,y+103},42,Team[n%2],a,1.5f);
            centered(row==0?eyes[n]:row==1?mouths[n]:hats[n],x+105,y+172,15);
        }
    }
}
}
int main(int argc,char** argv) {
    bool smoke=false; for(int n=1;n<argc;++n) if(std::string(argv[n])=="--smoke") smoke=true;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_RESIZABLE|(smoke?FLAG_WINDOW_HIDDEN:0));
    InitWindow(Width,Height,"Orbital Odds | Betting Arena"); SetWindowMinSize(1008,658); SetExitKey(KEY_NULL); SetTargetFPS(60);
    uiFont=loadFont(); displayFont=loadFont(128,true);
    RenderTexture2D canvas=LoadRenderTexture(Width,Height); SetTextureFilter(canvas.texture,TEXTURE_FILTER_BILINEAR);
    auto sounds=std::make_unique<AudioBank>(); audioBank=sounds.get();
    int result=0;
    try {
        auto root=locateRoot();
        // Smoke rendering uses an isolated wallet and never touches the player's save.
        if(smoke) {
            auto smokeRoot=root/"build/smoke"; std::filesystem::create_directories(smokeRoot/"data");
            std::filesystem::copy_file(root/"data/catalog.json",smokeRoot/"data/catalog.json",std::filesystem::copy_options::overwrite_existing);
            root=smokeRoot;
        }
        App app(root); int frame=0;
        if(smoke) audioBank->setMuted(true);
        auto verify=[](bool condition,const char* message) { if(!condition) throw std::runtime_error(message); };
        if(smoke && audioBank->ready) {
            verify(audioBank->loaded==static_cast<int>(Cue::Count)-1,"Some sound buffers failed to load");
            audioBank->play(Cue::Click); verify(!audioBank->isPlaying(Cue::Click),"Mute failed to suppress a sound");
            // Exercise playback with the actual audio backend, silently during automated checks.
            audioBank->setMuted(false); SetMasterVolume(0); audioBank->play(Cue::Fight);
            verify(audioBank->isPlaying(Cue::Fight),"Audio backend did not start playback"); audioBank->setMuted(true);
        }
        const char* smokeFiles[]={"preview.png","countdown-3.png","countdown-2.png","countdown-1.png","fight.png","live.png","result.png","catalog.png","styles.png","rules.png","collection.png","ball-inspection.png"};
        while(!WindowShouldClose()) {
            float scale=std::min(GetScreenWidth()/static_cast<float>(Width),GetScreenHeight()/static_cast<float>(Height));
            float ox=(GetScreenWidth()-Width*scale)/2,oy=(GetScreenHeight()-Height*scale)/2;
            Vector2 raw=GetMousePosition(); mouse={(raw.x-ox)/scale,(raw.y-oy)/scale};
            if(smoke) {
                mouse={-100,-100};
                if(frame==0) {
                    app.wallet=Wallet{}; app.wallet.coins=app.catalog.rules.startingCoins; app.persist();
                    app.ensureMarket(); app.rebuildCollectionCache(); app.persist();
                    app.preview=generateMatch(app.catalog,482031); app.stake=100;
                    for(int k=0;k<2;++k) app.appearances[k]=appearanceFor(app.preview.seed,k);
                }
                if(frame==1) {
                    app.start(); app.speed=16; app.update(0.1f);
                    verify(app.countdown.number()==3 && app.sim->time==0,"Countdown did not lock physics");
                    float before=app.countdown.elapsed; app.paused=true; app.update(0.1f);
                    verify(app.countdown.elapsed==before,"Pause advanced countdown"); app.paused=false;
                    app.rulesOpen=true; app.update(0.1f); verify(app.countdown.elapsed==before,"Rules advanced countdown"); app.rulesOpen=false;
                }
                if(frame>=2 && frame<=4) {
                    for(int n=0;n<60;++n) app.update(1.0f/60);
                    verify(app.countdown.number()==4-frame && app.sim->time==0,"16x speed changed countdown duration");
                }
                if(frame==5) {
                    for(int n=0;n<70;++n) app.update(1.0f/60);
                    verify(!app.countdown.active() && app.sim->time>0,"Fight failed to start after countdown");
                }
                if(frame==6) {
                    while(app.live()) app.update(1.0f/60);
                    int coins=app.wallet.coins,rounds=app.wallet.rounds;
                    app.update(0.1f); app.settle(); verify(coins==app.wallet.coins && rounds==app.wallet.rounds,"Repeated payout after animated result");
                    app.resultAge=0.55f;
                }
                if(frame==7) {
                    int midpoint=1+(app.wallet.coins-1)/2;
                    app.reroll(); verify(app.stake==midpoint,"The next match did not reset the stake to the slider midpoint");
                    app.catalogOpen=true;
                }
                if(frame==9) { app.catalogOpen=false; app.rulesOpen=true; }
                if(frame==10) {
                    app.rulesOpen=false; app.collectionOpen=true; app.wallet.coins=20000;
                    verify(app.wallet.unlockCollectionSlot() && app.wallet.unlockCollectionSlot(),"Collection slots did not unlock");
                    int price=marketPrice(app.marketBallCache[0]); verify(app.wallet.buyMarketBall(0,price),"Market ball could not be purchased");
                    app.rebuildCollectionCache(); app.persist();
                }
                if(frame==11) { app.openInspector(app.marketBallCache[1],app.wallet.marketBallSeeds[1],1,-1); }
            } else app.update(GetFrameTime());
            BeginTextureMode(canvas); if(smoke && frame==8) styleSheet(); else app.draw(); EndTextureMode();
            BeginDrawing(); ClearBackground(BLACK);
            DrawTexturePro(canvas.texture,{0,0,static_cast<float>(Width),-static_cast<float>(Height)},
                {ox,oy,Width*scale,Height*scale},{0,0},0,WHITE);
            EndDrawing();
            if(smoke) {
                Image shot=LoadImageFromTexture(canvas.texture); ImageFlipVertical(&shot);
                auto file=root/smokeFiles[frame];
                int bytes=0; unsigned char* png=ExportImageToMemory(shot,".png",&bytes);
                UnloadImage(shot);
                if(!png || bytes<=0) throw std::runtime_error("Screenshot encoding failed");
                std::ofstream output(file,std::ios::binary); output.write(reinterpret_cast<const char*>(png),bytes); MemFree(png);
                if(!output.good()) throw std::runtime_error("Screenshot write failed");
                if(++frame==12) { std::cout<<"Smoke render OK: 12 screens; countdown, pause, 16x, settlement, collection OK; audio device "<<(audioBank->ready?"ready":"unavailable")<<"; "<<root.string()<<"\n"; break; }
            }
        }
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n"; result=1;
        if(!smoke) while(!WindowShouldClose()) {
            BeginDrawing(); ClearBackground(Background); text("Unable to start the game",40,40,28,Coral);
            wrapped(e.what(),40,100,GetScreenWidth()-80.0f,18,White,12);
            text("Check data/catalog.json and restart the game.",40,380,18,Muted); EndDrawing();
        }
    }
    sounds.reset(); audioBank=nullptr;
    UnloadRenderTexture(canvas);
    if(uiFont.texture.id!=GetFontDefault().texture.id) UnloadFont(uiFont);
    if(displayFont.texture.id!=GetFontDefault().texture.id) UnloadFont(displayFont);
    CloseWindow(); return result;
}
