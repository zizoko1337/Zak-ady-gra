#pragma once
#include "feedback.hpp"
#include <box2d/box2d.h>
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace orbital {
constexpr float Step = 1.0f / 120.0f;
constexpr float Pi = 3.14159265358979323846f;
constexpr float ComboWindow = 2.5f;
struct Vec {
    float x=0,y=0;
    Vec operator+(Vec b) const { return {x+b.x,y+b.y}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y}; }
    Vec operator*(float s) const { return {x*s,y*s}; }
    Vec operator/(float s) const { return {x/s,y/s}; }
};
float dot(Vec a, Vec b);
float length(Vec a);
Vec normalized(Vec a);
float segmentDistance(Vec p, Vec a, Vec b);
int payoutFor(int stake,float multiplier);
struct Item {
    std::string id,name,category,effect,description,group,meleeClass;
    int points=0;
    std::map<std::string,float> params;
    float get(const std::string& key, float fallback=0) const;
};
struct Arena { std::string id,name,effect; std::vector<Vec> vertices; int weight=1; };
struct Rules {
    int startingCoins=1000,weaponSlots=2,abilitySlots=1,modifierSlots=2;
    int budgetMin=75,budgetMax=115,tolerance=5,candidates=400;
    float payout=1.9f,limit=90,sudden=55,suddenDps=3;
    float hp=100,speed=165,radius=18,contactDamage=2;
};
struct Catalog {
    Rules rules;
    std::vector<Item> items;
    std::vector<Arena> arenas;
    static Catalog load(const std::filesystem::path& path);
};
struct Fighter {
    std::string name,title;
    std::vector<Item> items;
    int points=0;
    float maxHp=100,hp=100,speed=165,radius=18,damageMultiplier=1,armor=0;
    Vec position,previous,velocity;
    float angle=0,flash=0,wallCooldown=0,boostTime=0,boostMultiplier=1,invulnerable=0;
    float shield=0,shieldTime=0,damageDealt=0,poisonDps=0,poisonTick=0,slowTime=0,slowMultiplier=1,tapeChance=0,comboWindow=0;
    bool centerHealingInside=false;
    float stunTime=0,wallStunWindow=0,knockbackTime=0;
    Vec knockbackDirection{};
    Item minionWeapon;
    std::vector<float> cooldowns,hitCooldowns,meleeAngles,meleeDirections,meleeClashCooldowns;
    const Item* effect(const std::string& name) const;
};
struct Match {
    uint32_t seed=0;
    Arena arena;
    std::array<Fighter,2> fighters;
    int targetBudget=0;
};
Match generateMatch(const Catalog& catalog,uint32_t seed);
Fighter generateBall(const Catalog& catalog,uint32_t seed);
int marketPrice(const Fighter& fighter);
struct Projectile {
    Vec position,previous,velocity; int owner; float damage,radius,life; std::string kind;
    int weaponSlot=-1,sourceOwner=-1;
    // A reflected Bouncy Ball remembers the previous holder for its one return hit.
    int bouncyBounces=0,bouncyReturnTarget=-1;
    bool bouncyDeflected=false;
    float returnPull=0,cruiseSpeed=0;
    bool boomerangHit=false;
};
struct SummonedMouse {
    Vec position,previous,velocity;
    int owner=0;
    float hp=1,radius=7,life=16,damage=5,biteCooldown=2,attackCooldown=0;
    float speed=165,slowTime=0,slowMultiplier=1;
};
struct Minion {
    Vec position,previous,velocity;
    int owner=0;
    float hp=40,maxHp=40,radius=11,speed=135,cooldown=0,meleeAngle=0,hitCooldown=0;
    float stunTime=0,wallStunWindow=0,knockbackTime=0;
    float slowTime=0,slowMultiplier=1;
    Vec knockbackDirection{};
    Item weapon;
};
struct Hazard {
    Vec position;
    int owner;
    float damage,radius,blastRadius,life,arm,hitCooldown=0;
    bool mine;
    // Inward wall normal for a spike. Mines leave this at {0, 0}.
    Vec normal{};
};
struct Tape {
    Vec start,end;
    int owner=0;
    float life=0;
    bool deploying=true;
};
struct DamageField {
    std::vector<Vec> points;
    bool drawing=false;
};
struct FieldShape {
    std::vector<Vec> vertices;
    int owner=0;
    float life=0.5f,total=0.5f;
};
struct Burst { Vec position; float life,total,radius; int owner; std::string text; };
struct Event { float time; int owner; std::string text; Cue cue=Cue::None; Vec position{}; uint64_t serial=0; float amount=0; bool critical=false; };
class Simulation {
public:
    Simulation(const Match& match,const Rules& rules);
    ~Simulation();
    Simulation(const Simulation&)=delete;
    Simulation& operator=(const Simulation&)=delete;
    void step();
    Match match;
    Rules rules;
    float time=0;
    bool finished=false;
    int winner=-2; // -2 running, -1 draw, 0/1 winner
    std::vector<Projectile> projectiles;
    std::vector<SummonedMouse> mice;
    std::vector<Minion> minions;
    std::vector<Hazard> hazards;
    std::vector<Tape> tapes;
    std::array<DamageField,2> damageFields;
    std::vector<FieldShape> fieldShapes;
    std::vector<Burst> bursts;
    std::vector<Event> events;
private:
    b2WorldId world{};
    std::array<b2BodyId,2> bodies{};
    float contactCooldown=0;
    uint64_t eventSerial=0;
    uint64_t criticalSerial=0;
    bool criticalHit(int source);
    void damage(int victim,float amount,int source,const std::string& label,Cue cue=Cue::Hit,bool canCrit=true);
    void log(int owner,const std::string& message,Cue cue=Cue::None,Vec position={},float amount=0,bool critical=false);
    void fire(int owner,const Item& item,size_t slot);
    void checkEnd();
};
struct Wallet {
    int coins=1000,wins=0,losses=0,draws=0,rounds=0;
    bool active=false;
    int stake=0,selection=-1;
    int collectionSlots=0;
    std::vector<uint32_t> ownedBallSeeds;
    std::array<uint32_t,3> marketBallSeeds{};
    bool marketInitialized=false;
    bool place(int side,int amount);
    int settle(int winner,float multiplier);
    int nextCollectionSlotCost() const;
    bool unlockCollectionSlot();
    bool buyMarketBall(size_t index,int price);
    bool refreshMarket(const std::array<uint32_t,3>& seeds);
    bool removeOwnedBall(size_t index);
    void resetSeason(int initialCoins);
    void save(const std::filesystem::path& path) const;
    static Wallet load(const std::filesystem::path& path,int initial);
};
} // namespace orbital
