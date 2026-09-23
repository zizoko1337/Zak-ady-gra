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
struct ChallengeActor {
    std::string id,displayName,kind,team;
    int count=1,maxPoints=0;
    uint32_t seed=0;
    Vec position{};
    float hpMultiplier=1,damageMultiplier=1,sizeMultiplier=1;
    std::vector<std::string> loadout;
};
struct ChallengeArenaObject {
    std::string type;
    Vec position{};
    std::map<std::string,float> params;
};
struct ChallengeDefinition {
    std::string id,name,description,color,branch,encounterType,arenaId,arenaEffect,objectiveType;
    std::vector<std::string> prerequisites;
    int unlockCost=0,requiredBalls=1;
    float mapX=0.5f,mapY=0.5f,timeLimit=60,objectiveTarget=0;
    std::vector<ChallengeActor> actors;
    std::vector<ChallengeArenaObject> arenaObjects;
};
struct ChallengeCatalog {
    std::vector<ChallengeDefinition> challenges;
    static ChallengeCatalog load(const std::filesystem::path& path,const Catalog& catalog);
    const ChallengeDefinition* find(const std::string& id) const;
};
struct Fighter {
    std::string name,title;
    std::string actorKind="fighter";
    std::vector<Item> items;
    int points=0,team=0;
    uint32_t appearanceSeed=0;
    float maxHp=100,hp=100,speed=165,radius=18,damageMultiplier=1,armor=0;
    Vec position,previous,velocity;
    float angle=0,flash=0,wallCooldown=0,boostTime=0,boostMultiplier=1,invulnerable=0;
    float shield=0,shieldTime=0,damageDealt=0,poisonDps=0,poisonTick=0,slowTime=0,slowMultiplier=1,tapeChance=0,comboWindow=0;
    bool centerHealingInside=false,stationary=false,harmless=false,physicsDisabled=false,hidden=false,untargetable=false;
    float contactDamage=-1,specialCooldown=0;
    int poisonOwner=-1;
    float stunTime=0,wallStunWindow=0,knockbackTime=0;
    Vec knockbackDirection{};
    Item minionWeapon;
    std::vector<float> cooldowns,hitCooldowns,meleeAngles,meleeDirections,meleeClashCooldowns;
    const Item* effect(const std::string& name) const;
};
struct MouseInvasionSetup {
    bool enabled=false;
    float interval=1,hp=1,radius=7,speed=165,damage=5,biteCooldown=2;
    int maxCount=20;
};
struct Match {
    uint32_t seed=0;
    Arena arena;
    std::vector<Fighter> fighters=std::vector<Fighter>(2);
    int targetBudget=0;
    bool challenge=false;
    std::string challengeId,objectiveType;
    float objectiveTarget=0;
    MouseInvasionSetup mouseInvasion;
};
Match generateMatch(const Catalog& catalog,uint32_t seed);
Fighter generateBall(const Catalog& catalog,uint32_t seed);
Fighter generateMarketBall(const Catalog& catalog,uint32_t seed);
Match buildChallengeMatch(const Catalog& catalog,const ChallengeDefinition& challenge,const std::vector<uint32_t>& playerSeeds,uint32_t encounterSeed,const std::vector<bool>& marketGenerated={});
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
    bool targetable=false;
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
    std::vector<DamageField> damageFields;
    std::vector<FieldShape> fieldShapes;
    std::vector<Burst> bursts;
    std::vector<Event> events;
private:
    b2WorldId world{};
    std::vector<b2BodyId> bodies;
    std::vector<float> contactCooldowns;
    uint64_t eventSerial=0;
    uint64_t criticalSerial=0;
    float invasionMouseTimer=0;
    int invasionMouseOwner=-1,invasionMouseSerial=0;
    bool criticalHit(int source);
    bool enemies(int first,int second) const;
    int closestEnemy(int owner,Vec from) const;
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
    // Old collection entries retain their original random generator. New market purchases use the market distribution.
    std::vector<bool> ownedBallMarketGenerated;
    std::array<uint32_t,3> marketBallSeeds{};
    bool marketInitialized=false;
    std::vector<std::string> unlockedChallenges,completedChallenges;
    bool place(int side,int amount);
    int settle(int winner,float multiplier);
    int nextCollectionSlotCost() const;
    bool unlockCollectionSlot();
    bool buyMarketBall(size_t index,int price);
    bool refreshMarket(const std::array<uint32_t,3>& seeds);
    bool removeOwnedBall(size_t index);
    bool challengeUnlocked(const std::string& id) const;
    bool challengeCompleted(const std::string& id) const;
    bool unlockChallenge(const ChallengeDefinition& challenge);
    bool completeChallenge(const std::string& id);
    void resetSeason(int initialCoins);
    void save(const std::filesystem::path& path) const;
    static Wallet load(const std::filesystem::path& path,int initial);
};
} // namespace orbital
