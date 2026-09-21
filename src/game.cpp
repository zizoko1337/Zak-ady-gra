#include "game.hpp"
#include <algorithm>
#include <fstream>
#include <functional>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>

namespace orbital {
using nlohmann::json;
float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y; }
float length(Vec a) { return std::sqrt(dot(a,a)); }
int payoutFor(int stake,float multiplier) {
    const auto hundredths=std::llround(static_cast<double>(multiplier)*100);
    return static_cast<int>(std::min(1000000000LL,static_cast<long long>(stake)*hundredths/100));
}
Vec normalized(Vec a) { float l=length(a); return l>0.0001f?a/l:Vec{1,0}; }
static Vec turnToward(Vec direction,Vec target,float maximumDegrees) {
    float speed=length(direction); if(speed<0.0001f || length(target)<0.0001f) return direction;
    Vec from=direction/speed,to=normalized(target);
    float angle=std::atan2(from.x*to.y-from.y*to.x,dot(from,to));
    angle=std::clamp(angle,-maximumDegrees*Pi/180.0f,maximumDegrees*Pi/180.0f);
    float c=std::cos(angle),s=std::sin(angle);
    return {speed*(from.x*c-from.y*s),speed*(from.x*s+from.y*c)};
}
float segmentDistance(Vec p,Vec a,Vec b) {
    Vec d=b-a; float t=dot(d,d)>0?std::clamp(dot(p-a,d)/dot(d,d),0.0f,1.0f):0;
    return length(p-(a+d*t));
}
static float cross(Vec a,Vec b) { return a.x*b.y-a.y*b.x; }
static bool onSegment(Vec p,Vec a,Vec b) {
    return std::abs(cross(b-a,p-a))<0.01f && dot(p-a,p-b)<=0;
}
static bool segmentsIntersect(Vec a,Vec b,Vec c,Vec d) {
    float ab1=cross(b-a,c-a),ab2=cross(b-a,d-a);
    float cd1=cross(d-c,a-c),cd2=cross(d-c,b-c);
    if(((ab1>0 && ab2<0) || (ab1<0 && ab2>0)) && ((cd1>0 && cd2<0) || (cd1<0 && cd2>0))) return true;
    return onSegment(c,a,b) || onSegment(d,a,b) || onSegment(a,c,d) || onSegment(b,c,d);
}
static bool properSegmentCrossing(Vec a,Vec b,Vec c,Vec d,Vec& intersection) {
    Vec r=b-a,s=d-c; float denominator=cross(r,s);
    if(std::abs(denominator)<0.0001f) return false;
    float t=cross(c-a,s)/denominator,u=cross(c-a,r)/denominator;
    if(t<=0.001f || t>=0.999f || u<=0.001f || u>=0.999f) return false;
    intersection=a+r*t; return true;
}
static bool pointInPolygon(Vec point,const std::vector<Vec>& vertices) {
    bool inside=false;
    for(size_t a=0,b=vertices.size()-1;a<vertices.size();b=a++) {
        const Vec& first=vertices[a]; const Vec& second=vertices[b];
        bool crosses=(first.y>point.y)!=(second.y>point.y);
        if(crosses && point.x<(second.x-first.x)*(point.y-first.y)/(second.y-first.y)+first.x) inside=!inside;
    }
    return inside;
}
static float segmentGap(Vec a,Vec b,Vec c,Vec d) {
    if(segmentsIntersect(a,b,c,d)) return 0;
    return std::min({segmentDistance(a,c,d),segmentDistance(b,c,d),segmentDistance(c,a,b),segmentDistance(d,a,b)});
}
static int meleeWeight(const Item& item) {
    if(item.meleeClass=="light") return 0;
    if(item.meleeClass=="medium") return 1;
    return 2;
}
struct MeleeSegment { Vec base,tip; };
static MeleeSegment meleeSegment(const Fighter& fighter,const Item& item,float angle) {
    Vec dir{std::cos(angle),std::sin(angle)},perp{-dir.y,dir.x};
    if(item.id=="weapon.orbit_shield") {
        // A broad chord hugs the fighter instead of extending out like a weapon handle.
        Vec center=fighter.position+dir*(fighter.radius+4);
        return {center-perp*(fighter.radius*1.5f),center+perp*(fighter.radius*1.5f)};
    }
    return {fighter.position+dir*(fighter.radius+5),fighter.position+dir*(fighter.radius+item.get("reach"))};
}
static uint32_t mix32(uint32_t value) {
    value^=value>>16; value*=0x7feb352du; value^=value>>15; value*=0x846ca68bu; return value^(value>>16);
}
static bool hasItem(const Fighter& fighter,const std::string& id) {
    return std::any_of(fighter.items.begin(),fighter.items.end(),[&](const Item& item){ return item.id==id; });
}
static std::string titleForItem(const Item& item) {
    static const std::map<std::string,std::string> titles={
        {"weapon.sword","the Blade"},{"weapon.bow","the Archer"},{"weapon.pistol","the Gunslinger"},{"weapon.mines","the Demolitionist"},{"weapon.scatter","the Scattershot"},{"weapon.spear","the Lancer"},{"weapon.hammer","Hammerhead"},{"weapon.dagger","the Quick"},{"weapon.orbit_shield","the Bulwark"},{"weapon.poison_dart","the Toxic"},{"weapon.shuriken","the Whirlwind"},{"weapon.katana","the Ronin"},{"weapon.scythe","the Reaper"},{"weapon.flute","the Piper"},{"weapon.bouncy_ball","the Ricochet"},{"weapon.boomerang","the Returning"},
        {"ability.spikes","the Hedgehog"},{"ability.spiked_skin","the Porcupine"},{"ability.dash","the Dasher"},{"ability.dodge","the Untouchable"},{"ability.regen","the Restored"},{"ability.shield","the Guarded"},{"ability.vampire","the Drainer"},{"ability.careful_steps","the Lucky"},{"ability.slowing_tape","the Snarer"},{"ability.damage_field","the Cartographer"},{"ability.minion","the Commander"},
        {"stat.hp_double","the Giant"},{"stat.hp_plus","the Stalwart"},{"stat.glass","the Fragile"},{"stat.fast","the Speedster"},{"stat.slow","the Patient"},{"stat.small","the Little"},{"stat.large","the Colossus"},{"stat.armor","the Armored"},{"stat.power","the Mighty"},{"stat.berserk","the Furious"},{"stat.crit_striker","the Deadeye"},{"stat.slow_swings","the Sluggish"},{"stat.fast_swings","the Swift"},{"stat.slow_reload","the Patient"},{"stat.fast_reload","the Rapid"},{"stat.bad_aim","the Wayward"},{"stat.combo_master","the Combo"}
    };
    if(auto found=titles.find(item.id); found!=titles.end()) return found->second;
    return "the Unknown";
}
float Item::get(const std::string& key,float fallback) const {
    auto it=params.find(key); return it==params.end()?fallback:it->second;
}
static void require(bool condition,const std::string& message) {
    if(!condition) throw std::runtime_error(message);
}
Catalog Catalog::load(const std::filesystem::path& path) {
    std::ifstream stream(path);
    require(stream.good(),"Cannot open data/catalog.json.");
    json j; stream>>j; Catalog c;
    require(j.at("schema_version")==1,"Unsupported catalog version.");
    const auto& r=j.at("rules"); auto& v=c.rules;
    v.startingCoins=r.at("starting_coins"); v.payout=r.at("payout_multiplier");
    v.limit=r.at("round_limit_seconds"); v.sudden=r.at("sudden_death_seconds");
    v.suddenDps=r.at("sudden_death_dps"); v.hp=r.at("base_hp"); v.speed=r.at("base_speed");
    v.radius=r.at("base_radius"); v.contactDamage=r.at("contact_damage");
    v.weaponSlots=r.at("weapon_slots"); v.abilitySlots=r.at("ability_slots"); v.modifierSlots=r.at("modifier_slots");
    v.budgetMin=r.at("budget_min"); v.budgetMax=r.at("budget_max");
    v.tolerance=r.at("balance_tolerance"); v.candidates=r.at("candidate_count");
    require(v.startingCoins>=100 && v.startingCoins<=1000000,"starting_coins: range 100–1000000.");
    require(v.payout>=1 && v.payout<=5,"payout_multiplier: range 1–5.");
    require(v.hp>=10 && v.hp<=1000 && v.speed>=40 && v.speed<=400 && v.radius>=8 && v.radius<=28,"Invalid base stats.");
    require(v.limit>=10 && v.limit<=300 && v.sudden>=0 && v.sudden<v.limit && v.suddenDps>=0 && v.suddenDps<=100,"Invalid match duration or overtime settings.");
    require(v.contactDamage>=0 && v.contactDamage<=100,"Invalid contact damage.");
    require(v.weaponSlots>=1 && v.weaponSlots<=2 && v.abilitySlots>=0 && v.abilitySlots<=1 && v.modifierSlots>=0 && v.modifierSlots<=2,"The interface supports 1–2 weapons, 0–1 abilities and 0–2 modifiers.");
    require(v.budgetMin<=v.budgetMax && v.budgetMin>=-1000 && v.budgetMax<=10000 && v.tolerance>=0 && v.tolerance<=1000 && v.candidates>=10 && v.candidates<=5000,"Invalid budget settings.");
    const std::map<std::string,std::string> effects={
        {"sword","weapon"},{"bow","weapon"},{"pistol","weapon"},{"mine","weapon"},{"shotgun","weapon"},{"poison_dart","weapon"},{"shuriken","weapon"},{"flute","weapon"},{"bouncy_ball","weapon"},{"boomerang","weapon"},
        {"spikes","ability"},{"spiked_skin","ability"},{"dash","ability"},{"dodge","ability"},{"regen","ability"},{"shield","ability"},{"vampire","ability"},{"careful_steps","ability"},{"tape","ability"},{"damage_field","ability"},{"minion","ability"},{"stats","modifier"},{"crit","modifier"},{"slow_swings","modifier"},{"fast_swings","modifier"},{"slow_reload","modifier"},{"fast_reload","modifier"},{"bad_aim","modifier"},{"combo_master","modifier"}};
    std::set<std::string> ids;
    for(const auto& entry:j.at("items")) {
        Item i; i.id=entry.at("id"); i.name=entry.at("name"); i.category=entry.at("category");
        i.effect=entry.at("effect"); i.points=entry.at("points"); i.description=entry.at("description");
        i.group=entry.value("group",i.id); i.meleeClass=entry.value("melee_class",""); i.params=entry.at("params").get<std::map<std::string,float>>();
        require(!i.id.empty() && ids.insert(i.id).second,"Duplicate or empty ID: "+i.id);
        require(!i.name.empty() && i.name.size()<=100 && i.description.size()<=300,"Empty or overlong name/description: "+i.id);
        require(effects.contains(i.effect) && effects.at(i.effect)==i.category,"Unknown effect or incorrect category: "+i.id);
        require(i.points>=-1000 && i.points<=1000,"Points out of range: "+i.id);
        for(auto& [key,value]:i.params) require(std::isfinite(value) && value>=0 && value<=2000,"Invalid parameter "+key+": "+i.id);
        auto positive=[&](const std::string& key,float low,float high) { require(i.params.contains(key) && i.get(key)>=low && i.get(key)<=high,"Parameter "+key+" out of range: "+i.id); };
        if(i.category=="weapon" || i.effect=="spikes") positive("damage",0.1f,500);
        if(i.effect=="bow" || i.effect=="pistol" || i.effect=="shotgun" || i.effect=="poison_dart" || i.effect=="shuriken" || i.effect=="bouncy_ball" || i.effect=="boomerang") {
            positive("cooldown",0.1f,60); positive("projectile_speed",30,1500); positive("projectile_radius",1,15);
        }
        if(i.effect=="boomerang") positive("return_pull",5,1000);
        if(i.effect=="sword") {
            positive("reach",10,110); positive("rotation_speed",0.1f,15); positive("hit_cooldown",0.1f,10);
            if(i.params.contains("lifesteal")) positive("lifesteal",0.1f,100);
            require(i.meleeClass=="light" || i.meleeClass=="medium" || i.meleeClass=="heavy","Melee weapons need melee_class light, medium, or heavy: "+i.id);
        } else require(i.meleeClass.empty(),"Only melee weapons may define melee_class: "+i.id);
        if(i.effect=="mine") { positive("cooldown",0.5f,60); positive("trigger_radius",5,100); positive("blast_radius",5,160); positive("arm_time",0.1f,5); positive("lifetime",1,60); }
        if(i.effect=="flute") { positive("cooldown",1,60); positive("count",1,12); positive("mouse_hp",0.1f,20); positive("mouse_radius",3,15); positive("mouse_speed",40,400); positive("bite_cooldown",0.1f,10); }
        if(i.effect=="spikes") { positive("radius",3,35); positive("lifetime",1,60); positive("hit_cooldown",0.1f,10); }
        if(i.effect=="dash" || i.effect=="dodge") { positive("cooldown",0.5f,60); positive("duration",0.05f,2); positive("speed_multiplier",1,4); }
        if(i.effect=="dodge") positive("trigger_distance",10,200);
        if(i.effect=="regen") { positive("cooldown",0.5f,60); positive("heal",0.1f,100); }
        if(i.effect=="shield") { positive("cooldown",0.5f,60); positive("duration",0.1f,10); positive("absorb",0.1f,200); }
        if(i.effect=="vampire") positive("lifesteal",0.01f,0.8f);
        if(i.effect=="tape") { positive("chance",0.01f,1); positive("duration",0.1f,10); positive("slow_multiplier",0.05f,1); }
        if(i.effect=="minion") { positive("hp",1,500); positive("radius",5,25); positive("speed",40,400); }
        if(i.effect=="crit") positive("chance",0.01f,1);
        if(i.effect=="stats") {
            for(const auto& [key,value]:i.params) {
                if(key=="armor") require(value<=0.7f,"Armor must not exceed 0.7.");
                else { require(key=="hp_multiplier" || key=="speed_multiplier" || key=="radius_multiplier" || key=="damage_multiplier","Unknown modifier: "+key); require(value>=0.5f && value<=2.5f,"Multiplier outside range 0.5–2.5: "+i.id); }
            }
        }
        c.items.push_back(i);
    }
    for(auto [category,slots]:std::vector<std::pair<std::string,int>>{{"weapon",v.weaponSlots},{"ability",v.abilitySlots},{"modifier",v.modifierSlots}}) {
        std::set<std::string> groups;
        for(const auto& i:c.items) if(i.category==category) groups.insert(i.group);
        require(static_cast<int>(groups.size())>=slots,"Not enough distinct groups in category: "+category);
    }
    ids.clear();
    for(const auto& entry:j.at("arenas")) {
        Arena a; a.id=entry.at("id"); a.name=entry.at("name"); a.weight=entry.at("weight");
        float w=entry.at("width"),h=entry.at("height"); std::string shape=entry.at("shape");
        require(ids.insert(a.id).second,"Duplicate arena ID.");
        require(a.weight>0 && a.weight<=100 && w>=380 && w<=600 && h>=330 && h<=440,"Arena size or weight is out of range.");
        if(shape=="rectangle") a.vertices={{-w/2,-h/2},{w/2,-h/2},{w/2,h/2},{-w/2,h/2}};
        else {
            require(shape=="polygon","Unknown arena shape."); int sides=entry.at("sides");
            require(sides>=4 && sides<=64,"Arena side count: 4–64.");
            for(int k=0;k<sides;++k) { float angle=2*Pi*k/sides; a.vertices.push_back({std::cos(angle)*w/2,std::sin(angle)*h/2}); }
        }
        c.arenas.push_back(a);
    }
    require(!c.arenas.empty(),"The catalog must contain an arena.");
    return c;
}
ChallengeCatalog ChallengeCatalog::load(const std::filesystem::path& path,const Catalog& catalog) {
    std::ifstream stream(path); require(stream.good(),"Cannot open data/challenges.json.");
    json root; stream>>root; require(root.at("schema_version")==1,"Unsupported challenge schema version.");
    require(root.contains("challenges") && root.at("challenges").is_array(),"Challenges must be an array.");
    std::set<std::string> arenaIds,itemIds,ids;
    for(const auto& arena:catalog.arenas) arenaIds.insert(arena.id);
    for(const auto& item:catalog.items) itemIds.insert(item.id);
    ChallengeCatalog result;
    auto position=[](const json& value,const std::string& label) {
        require(value.is_array() && value.size()==2,label+" must contain x and y.");
        Vec p{value.at(0).get<float>(),value.at(1).get<float>()};
        require(std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x)<=1000 && std::abs(p.y)<=1000,label+" is outside supported bounds.");
        return p;
    };
    const std::set<std::string> colors={"red","green","blue","purple","yellow","cyan","orange"};
    const std::set<std::string> encounters={"duel","survival","damage_race","boss","teamfight","beast_hunt"};
    const std::set<std::string> objectives={"defeat_all","survive","deal_damage","protect","reach_zone"};
    const std::set<std::string> actorKinds={"generated_fighter","boss","beast","custom","dummy"};
    const std::set<std::string> teams={"enemy","ally","neutral"};
    const std::set<std::string> objectKinds={"mine","spike","healing_zone","damage_zone","wall","spawn_zone","mouse_spawner"};
    for(const auto& entry:root.at("challenges")) {
        ChallengeDefinition challenge;
        challenge.id=entry.at("id"); challenge.name=entry.at("name"); challenge.description=entry.at("description"); challenge.color=entry.at("color"); challenge.branch=entry.value("branch","Unsorted");
        challenge.prerequisites=entry.value("prerequisites",std::vector<std::string>{}); challenge.unlockCost=entry.at("unlock_cost"); challenge.requiredBalls=entry.at("required_balls");
        const auto& map=entry.at("map"); challenge.mapX=map.at("x"); challenge.mapY=map.at("y");
        const auto& encounter=entry.at("encounter"); challenge.encounterType=encounter.at("type"); challenge.arenaId=encounter.at("arena_id"); challenge.arenaEffect=encounter.value("arena_effect",""); challenge.timeLimit=encounter.at("time_limit");
        const auto& objective=encounter.at("objective"); challenge.objectiveType=objective.at("type"); challenge.objectiveTarget=objective.value("target",0.0f);
        require(!challenge.id.empty() && challenge.id.size()<=80 && ids.insert(challenge.id).second,"Duplicate or invalid challenge ID.");
        require(!challenge.name.empty() && challenge.name.size()<=80 && !challenge.description.empty() && challenge.description.size()<=300 && !challenge.branch.empty() && challenge.branch.size()<=40,"Invalid challenge text: "+challenge.id);
        require(colors.contains(challenge.color) && encounters.contains(challenge.encounterType) && objectives.contains(challenge.objectiveType),"Unknown challenge presentation, encounter, or objective type: "+challenge.id);
        require(challenge.unlockCost>=0 && challenge.unlockCost<=100000000 && challenge.requiredBalls>=1 && challenge.requiredBalls<=5,"Invalid challenge cost or team size: "+challenge.id);
        require(std::isfinite(challenge.mapX) && std::isfinite(challenge.mapY) && std::abs(challenge.mapX)<=10000 && std::abs(challenge.mapY)<=10000,"Challenge map coordinates are outside supported bounds: "+challenge.id);
        require(arenaIds.contains(challenge.arenaId) && (challenge.arenaEffect.empty() || challenge.arenaEffect=="spiked_arena" || challenge.arenaEffect=="center_gravity" || challenge.arenaEffect=="healing_arena"),"Unknown challenge arena setup: "+challenge.id);
        require(std::isfinite(challenge.timeLimit) && challenge.timeLimit>=5 && challenge.timeLimit<=600 && std::isfinite(challenge.objectiveTarget) && challenge.objectiveTarget>=0 && challenge.objectiveTarget<=100000,"Invalid challenge objective values: "+challenge.id);
        for(const auto& actorJson:encounter.value("actors",json::array())) {
            ChallengeActor actor; actor.id=actorJson.at("id"); actor.displayName=actorJson.value("display_name",""); actor.kind=actorJson.at("kind"); actor.team=actorJson.at("team"); actor.count=actorJson.value("count",1); actor.maxPoints=actorJson.value("max_points",0); actor.seed=actorJson.value("seed",0u);
            actor.position=position(actorJson.value("position",json::array({0,0})),"Actor position"); actor.hpMultiplier=actorJson.value("hp_multiplier",1.0f); actor.damageMultiplier=actorJson.value("damage_multiplier",1.0f); actor.sizeMultiplier=actorJson.value("size_multiplier",1.0f); actor.loadout=actorJson.value("loadout",std::vector<std::string>{});
            require(!actor.id.empty() && actor.displayName.size()<=80 && actorKinds.contains(actor.kind) && teams.contains(actor.team) && actor.count>=1 && actor.count<=50 && actor.maxPoints>=0 && actor.maxPoints<=10000,"Invalid actor in challenge: "+challenge.id);
            require(actor.hpMultiplier>0 && actor.hpMultiplier<=100 && actor.damageMultiplier>0 && actor.damageMultiplier<=100 && actor.sizeMultiplier>0.1f && actor.sizeMultiplier<=10,"Invalid actor multipliers: "+actor.id);
            for(const auto& item:actor.loadout) require(itemIds.contains(item),"Unknown actor loadout item: "+item);
            challenge.actors.push_back(std::move(actor));
        }
        for(const auto& objectJson:encounter.value("arena_objects",json::array())) {
            ChallengeArenaObject object; object.type=objectJson.at("type"); object.position=position(objectJson.at("position"),"Arena object position"); object.params=objectJson.value("params",std::map<std::string,float>{});
            require(objectKinds.contains(object.type),"Unknown arena object type: "+object.type);
            for(const auto& [key,value]:object.params) require(!key.empty() && std::isfinite(value) && value>=0 && value<=100000,"Invalid arena object parameter.");
            challenge.arenaObjects.push_back(std::move(object));
        }
        result.challenges.push_back(std::move(challenge));
    }
    require(!result.challenges.empty(),"Challenge map cannot be empty.");
    int roots=0;
    for(const auto& challenge:result.challenges) {
        if(challenge.prerequisites.empty()) { ++roots; require(challenge.unlockCost==0,"Starting challenges must be free."); }
        for(const auto& prerequisite:challenge.prerequisites) require(ids.contains(prerequisite) && prerequisite!=challenge.id,"Unknown or self-referencing prerequisite: "+prerequisite);
    }
    require(roots>=1,"Challenge map needs a starting node.");
    std::map<std::string,int> state;
    std::function<void(const ChallengeDefinition&)> visit=[&](const ChallengeDefinition& challenge) {
        require(state[challenge.id]!=1,"Challenge prerequisite graph contains a cycle."); if(state[challenge.id]==2) return;
        state[challenge.id]=1; for(const auto& id:challenge.prerequisites) visit(*result.find(id)); state[challenge.id]=2;
    };
    for(const auto& challenge:result.challenges) visit(challenge);
    return result;
}
const ChallengeDefinition* ChallengeCatalog::find(const std::string& id) const {
    auto found=std::find_if(challenges.begin(),challenges.end(),[&](const ChallengeDefinition& challenge){return challenge.id==id;});
    return found==challenges.end()?nullptr:&*found;
}
const Item* Fighter::effect(const std::string& value) const {
    for(const auto& i:items) if(i.effect==value) return &i;
    return nullptr;
}
static Fighter rollFighter(const Catalog& c,std::mt19937& rng) {
    Fighter f; f.maxHp=c.rules.hp; f.speed=c.rules.speed; f.radius=c.rules.radius;
    for(auto [category,slots]:std::vector<std::pair<std::string,int>>{{"weapon",c.rules.weaponSlots},{"ability",c.rules.abilitySlots},{"modifier",c.rules.modifierSlots}}) {
        std::vector<Item> pool; for(const auto& i:c.items) if(i.category==category) pool.push_back(i);
        // Weapons roll with replacement, so a fighter can receive two copies of the same weapon.
        if(category=="weapon") {
            std::uniform_int_distribution<size_t> choose(0,pool.size()-1);
            for(int n=0;n<slots;++n) { const auto& i=pool[choose(rng)]; f.items.push_back(i); f.points+=i.points; }
            continue;
        }
        std::shuffle(pool.begin(),pool.end(),rng); std::set<std::string> groups;
        for(const auto& i:pool) {
            if(slots==0) break;
            if(!groups.insert(i.group).second) continue;
            f.items.push_back(i); f.points+=i.points; --slots;
            if(i.effect=="stats") {
                f.maxHp*=i.get("hp_multiplier",1); f.speed*=i.get("speed_multiplier",1);
                f.radius*=i.get("radius_multiplier",1); f.damageMultiplier*=i.get("damage_multiplier",1);
                f.armor=1-(1-f.armor)*(1-i.get("armor"));
            }
        }
    }
    // Bounds protect the solver even when multiple custom modifier groups stack.
    f.maxHp=std::clamp(f.maxHp,5.0f,4000.0f); f.speed=std::clamp(f.speed,30.0f,650.0f);
    f.radius=std::clamp(f.radius,6.0f,40.0f); f.armor=std::clamp(f.armor,0.0f,0.8f);
    f.damageMultiplier=std::clamp(f.damageMultiplier,0.1f,6.0f); f.hp=f.maxHp;
    f.cooldowns.resize(f.items.size()); f.hitCooldowns.resize(f.items.size());
    f.meleeAngles.resize(f.items.size(),f.angle); f.meleeDirections.resize(f.items.size(),1); f.meleeClashCooldowns.resize(f.items.size());
    if(f.effect("minion")) {
        std::vector<Item> weapons;
        for(const auto& item:c.items) if(item.category=="weapon" && item.effect!="mine" && item.effect!="flute" && item.effect!="bouncy_ball" && item.effect!="boomerang" && item.id!="weapon.orbit_shield") weapons.push_back(item);
        if(!weapons.empty()) f.minionWeapon=weapons[std::uniform_int_distribution<size_t>(0,weapons.size()-1)(rng)];
    }
    return f;
}
static void prepareFighter(Fighter& f,const Catalog& c) {
    f.maxHp=c.rules.hp; f.speed=c.rules.speed; f.radius=c.rules.radius; f.damageMultiplier=1; f.armor=0; f.points=0;
    for(const auto& item:f.items) {
        f.points+=item.points;
        if(item.effect=="stats") {
            f.maxHp*=item.get("hp_multiplier",1); f.speed*=item.get("speed_multiplier",1);
            f.radius*=item.get("radius_multiplier",1); f.damageMultiplier*=item.get("damage_multiplier",1);
            f.armor=1-(1-f.armor)*(1-item.get("armor"));
        }
    }
    f.maxHp=std::clamp(f.maxHp,5.0f,4000.0f); f.speed=std::clamp(f.speed,30.0f,650.0f);
    f.radius=std::clamp(f.radius,6.0f,40.0f); f.armor=std::clamp(f.armor,0.0f,0.8f);
    f.damageMultiplier=std::clamp(f.damageMultiplier,0.1f,6.0f); f.hp=f.maxHp;
    f.cooldowns.assign(f.items.size(),0); f.hitCooldowns.assign(f.items.size(),0);
    f.meleeAngles.assign(f.items.size(),f.angle); f.meleeDirections.assign(f.items.size(),1); f.meleeClashCooldowns.assign(f.items.size(),0);
}
Match generateMatch(const Catalog& c,uint32_t seed) {
    std::mt19937 rng(seed); Match m; m.seed=seed;
    std::vector<int> weights; for(const auto& a:c.arenas) weights.push_back(a.weight);
    m.arena=c.arenas[std::discrete_distribution<int>(weights.begin(),weights.end())(rng)];
    // One in four arenas gains exactly one readable, match-wide modifier.
    if(std::uniform_int_distribution<int>(0,99)(rng)<25) {
        static const std::array<std::string,3> effects={"spiked_arena","center_gravity","healing_arena"};
        m.arena.effect=effects[std::uniform_int_distribution<int>(0,static_cast<int>(effects.size()-1))(rng)];
    }
    m.targetBudget=std::uniform_int_distribution<int>(c.rules.budgetMin,c.rules.budgetMax)(rng);
    std::vector<Fighter> candidates; candidates.reserve(c.rules.candidates);
    for(int n=0;n<c.rules.candidates;++n) candidates.push_back(rollFighter(c,rng));
    auto first=std::min_element(candidates.begin(),candidates.end(),[&](const Fighter& a,const Fighter& b){return std::abs(a.points-m.targetBudget)<std::abs(b.points-m.targetBudget);});
    m.fighters[0]=*first;
    int best=std::numeric_limits<int>::max(); Fighter second=*first;
    for(const auto& f:candidates) {
        if(std::abs(f.points-first->points)>c.rules.tolerance) continue;
        int common=0; for(const auto& i:f.items) for(const auto& j:first->items) if(i.id==j.id) ++common;
        int score=std::abs(f.points-first->points)*3+common*4;
        if(score<best) { best=score; second=f; }
    }
    m.fighters[1]=second;
    if(rng()%2) std::swap(m.fighters[0],m.fighters[1]);
    const std::array<std::string,30> names={"ARIA","BLAZE","CINDER","DRIFT","EMBER","FABLE","GLOW","HAVOC","IRIS","JOLT","KITE","LUMEN","MICA","NIMBUS","ONYX","PIXEL","QUARTZ","RIPPLE","SOL","TEMPO","UMBER","VEX","WISP","XYLO","YARA","ZEPHYR","NOVA","ORBIT","PULSE","RUNE"};
    int a=rng()%names.size(),b=(a+1+rng()%(names.size()-1))%names.size();
    m.fighters[0].name=names[a]; m.fighters[1].name=names[b];
    for(int k=0;k<2;++k) {
        auto& f=m.fighters[k]; f.position={k==0?-95.0f:95.0f,0}; f.previous=f.position;
        f.team=k; f.appearanceSeed=mix32(seed^static_cast<uint32_t>((k+1)*0x9e3779b9u));
        f.title=titleForItem(f.items[rng()%f.items.size()]);
        float angle=std::uniform_real_distribution<float>(0,2*Pi)(rng);
        f.velocity=Vec{std::cos(angle),std::sin(angle)}*f.speed; f.angle=angle;
        for(size_t i=0;i<f.items.size();++i) f.cooldowns[i]=f.items[i].get("cooldown",1)*std::uniform_real_distribution<float>(0.35f,0.75f)(rng);
    }
    return m;
}
Fighter generateBall(const Catalog& c,uint32_t seed) {
    Match match=generateMatch(c,seed);
    Fighter fighter=match.fighters[(seed>>31)&1u]; fighter.appearanceSeed=seed; fighter.team=0; return fighter;
}
Match buildChallengeMatch(const Catalog& c,const ChallengeDefinition& challenge,const std::vector<uint32_t>& playerSeeds,uint32_t encounterSeed) {
    require(playerSeeds.size()==static_cast<size_t>(challenge.requiredBalls),"Challenge squad size does not match its requirement.");
    Match match; match.fighters.clear(); match.seed=encounterSeed; match.challenge=true; match.challengeId=challenge.id;
    match.objectiveType=challenge.objectiveType; match.objectiveTarget=challenge.objectiveTarget;
    auto arena=std::find_if(c.arenas.begin(),c.arenas.end(),[&](const Arena& value){ return value.id==challenge.arenaId; });
    require(arena!=c.arenas.end(),"Challenge arena does not exist: "+challenge.arenaId); match.arena=*arena; match.arena.effect=challenge.arenaEffect;
    for(const auto& object:challenge.arenaObjects) if(object.type=="mouse_spawner" || (object.type=="spawn_zone" && object.params.contains("mouse_invasion") && object.params.at("mouse_invasion")>0)) {
        auto value=[&](const std::string& key,float fallback) { auto found=object.params.find(key); return found==object.params.end()?fallback:found->second; };
        match.mouseInvasion.enabled=true; match.mouseInvasion.interval=value("interval",1); match.mouseInvasion.maxCount=static_cast<int>(std::lround(value("max_count",20)));
        match.mouseInvasion.hp=value("hp",1); match.mouseInvasion.radius=value("radius",7); match.mouseInvasion.speed=value("speed",165);
        match.mouseInvasion.damage=value("damage",5); match.mouseInvasion.biteCooldown=value("bite_cooldown",2);
        require(match.mouseInvasion.interval>=0.1f && match.mouseInvasion.interval<=60 && match.mouseInvasion.maxCount>=1 && match.mouseInvasion.maxCount<=120,"Invalid Mouse Invasion spawn rate or limit.");
        require(match.mouseInvasion.hp>0 && match.mouseInvasion.hp<=10000 && match.mouseInvasion.radius>=2 && match.mouseInvasion.radius<=40 && match.mouseInvasion.speed>0 && match.mouseInvasion.speed<=1000 && match.mouseInvasion.damage>=0 && match.mouseInvasion.damage<=1000 && match.mouseInvasion.biteCooldown>=0.1f && match.mouseInvasion.biteCooldown<=60,"Invalid Mouse Invasion mouse stats.");
    }
    std::mt19937 rng(encounterSeed);
    const std::array<std::string,12> rivalNames={"ASH","BOLT","CLAW","DUSK","ECHO","FANG","HEX","MOSS","ROOK","SCAR","TALON","VOID"};
    for(size_t index=0;index<playerSeeds.size();++index) {
        Fighter fighter=generateBall(c,playerSeeds[index]); fighter.team=0; fighter.actorKind="fighter"; fighter.appearanceSeed=playerSeeds[index];
        fighter.position={-130.0f,(static_cast<float>(index)-(playerSeeds.size()-1)*0.5f)*90.0f}; fighter.previous=fighter.position;
        float angle=std::uniform_real_distribution<float>(-0.6f,0.6f)(rng); fighter.velocity={std::cos(angle)*fighter.speed,std::sin(angle)*fighter.speed};
        match.fighters.push_back(std::move(fighter));
    }
    int enemyIndex=0;
    for(const auto& actor:challenge.actors) for(int copy=0;copy<actor.count;++copy) {
        if(actor.team=="ally") continue;
        Fighter fighter;
        uint32_t seed=actor.seed?actor.seed+static_cast<uint32_t>(copy):rng();
        if(actor.kind=="generated_fighter") {
            int attempts=0;
            do { fighter=generateBall(c,seed); if(actor.maxPoints<=0 || fighter.points<=actor.maxPoints) break; seed=rng(); } while(++attempts<10000);
            require(actor.maxPoints<=0 || fighter.points<=actor.maxPoints,"Could not generate a challenge opponent inside its point limit.");
        } else if(actor.kind=="dummy") {
            fighter.name="DAMAGE DUMMY"; fighter.title="the Unmoving"; fighter.actorKind="dummy"; fighter.stationary=true; fighter.harmless=true;
            fighter.maxHp=fighter.hp=100000; fighter.speed=0; fighter.radius=28; fighter.contactDamage=0;
        } else if(actor.kind=="beast" && actor.id=="wild_dog") {
            fighter.name="WILD DOG"; fighter.title="the Small Beast"; fighter.actorKind="wild_dog";
            fighter.maxHp=fighter.hp=120; fighter.speed=165; fighter.radius=20; fighter.contactDamage=10; fighter.specialCooldown=2;
        } else {
            fighter.actorKind=actor.kind=="custom"?"fighter":actor.kind;
            for(const auto& id:actor.loadout) {
                auto item=std::find_if(c.items.begin(),c.items.end(),[&](const Item& value){ return value.id==id; });
                require(item!=c.items.end(),"Challenge loadout item does not exist: "+id); fighter.items.push_back(*item);
            }
            prepareFighter(fighter,c);
        }
        if(actor.id=="mouse_invasion_controller") {
            fighter.name="MOUSE INVASION"; fighter.title="the Swarm"; fighter.actorKind="mouse_spawner";
            fighter.stationary=false; fighter.harmless=true; fighter.hidden=true; fighter.untargetable=true; fighter.speed=900; fighter.radius=1;
        }
        if(!actor.displayName.empty()) fighter.name=actor.displayName;
        if(fighter.name.empty()) fighter.name=rivalNames[enemyIndex%rivalNames.size()];
        if(fighter.title.empty()) fighter.title=fighter.items.empty()?"the Challenger":titleForItem(fighter.items[rng()%fighter.items.size()]);
        fighter.team=1; fighter.appearanceSeed=seed; fighter.maxHp*=actor.hpMultiplier; fighter.hp=fighter.maxHp;
        fighter.damageMultiplier*=actor.damageMultiplier; fighter.radius*=actor.sizeMultiplier;
        float fallbackY=(enemyIndex-(static_cast<int>(challenge.actors.size())-1)*0.5f)*90.0f;
        fighter.position=actor.id=="mouse_invasion_controller"?Vec{}:actor.position;
        if(length(fighter.position)<0.01f && !fighter.stationary) fighter.position={130.0f,fallbackY};
        if(actor.count>1) fighter.position.y+=(copy-(actor.count-1)*0.5f)*80.0f;
        fighter.previous=fighter.position;
        if(!fighter.stationary) { float angle=Pi+std::uniform_real_distribution<float>(-0.6f,0.6f)(rng); fighter.velocity={std::cos(angle)*fighter.speed,std::sin(angle)*fighter.speed}; fighter.angle=angle; }
        for(size_t slot=0;slot<fighter.items.size();++slot) fighter.cooldowns[slot]=fighter.items[slot].get("cooldown",1)*std::uniform_real_distribution<float>(0.25f,0.65f)(rng);
        match.fighters.push_back(std::move(fighter)); ++enemyIndex;
    }
    require(!match.fighters.empty() && (match.mouseInvasion.enabled || std::ranges::any_of(match.fighters,[](const Fighter& fighter){ return fighter.team==1; })),"Challenge requires an enemy actor or arena invasion.");
    return match;
}
int marketPrice(const Fighter& fighter) {
    int raw=1000+std::max(0,fighter.points)*20;
    return ((raw+99)/100)*100;
}
static b2Vec2 physics(Vec v) { return {v.x/40,v.y/40}; }
static Vec pixels(b2Vec2 v) { return {v.x*40,v.y*40}; }
static Item fallbackMinionWeapon(uint32_t seed,int owner) {
    Item item; item.category="weapon";
    switch(mix32(seed^static_cast<uint32_t>(owner*0x9e3779b9u))%3) {
        case 0: item={"weapon.bow","Bow","weapon","bow","", "","",0,{{"damage",12},{"cooldown",1.5f},{"projectile_speed",340},{"projectile_radius",3}}}; break;
        case 1: item={"weapon.dagger","Orbit Dagger","weapon","sword","","","light",0,{{"damage",12},{"reach",36},{"rotation_speed",7.2f},{"hit_cooldown",0.45f}}}; break;
        default: item={"weapon.pistol","Heavy Pistol","weapon","pistol","","","",0,{{"damage",40},{"cooldown",10},{"projectile_speed",620},{"projectile_radius",5}}}; break;
    }
    return item;
}
static Minion* closerEnemyMinion(std::vector<Minion>& minions,const std::vector<Fighter>& fighters,int owner,Vec from,float closerThan) {
    Minion* closest=nullptr;
    for(auto& minion:minions) if(minion.owner>=0 && minion.owner<static_cast<int>(fighters.size()) && fighters[minion.owner].team!=fighters[owner].team && minion.hp>0) {
        float distance=length(minion.position-from);
        if(distance<closerThan) { closerThan=distance; closest=&minion; }
    }
    return closest;
}
static SummonedMouse* closerTargetableMouse(std::vector<SummonedMouse>& mice,const std::vector<Fighter>& fighters,int owner,Vec from,float closerThan) {
    SummonedMouse* closest=nullptr;
    for(auto& mouse:mice) if(mouse.targetable && mouse.owner>=0 && mouse.owner<static_cast<int>(fighters.size()) && fighters[mouse.owner].team!=fighters[owner].team && mouse.hp>0) {
        float distance=length(mouse.position-from);
        if(distance<closerThan) { closerThan=distance; closest=&mouse; }
    }
    return closest;
}
Simulation::Simulation(const Match& m,const Rules& r):match(m),rules(r) {
    if(match.mouseInvasion.enabled) {
        auto existing=std::find_if(match.fighters.begin(),match.fighters.end(),[](const Fighter& fighter){return fighter.actorKind=="mouse_spawner";});
        if(existing!=match.fighters.end()) invasionMouseOwner=static_cast<int>(existing-match.fighters.begin());
        else {
            Fighter controller; controller.name="MOUSE INVASION"; controller.title="the Swarm"; controller.actorKind="mouse_spawner"; controller.team=1;
            controller.maxHp=controller.hp=100000; controller.speed=900; controller.radius=1; controller.position=controller.previous={130,0};
            controller.velocity={-controller.speed,0}; controller.stationary=false; controller.harmless=true; controller.hidden=true; controller.untargetable=true;
            match.fighters.push_back(controller); invasionMouseOwner=static_cast<int>(match.fighters.size())-1;
        }
        invasionMouseTimer=match.mouseInvasion.interval;
    }
    bodies.resize(match.fighters.size()); damageFields.resize(match.fighters.size());
    contactCooldowns.assign(match.fighters.size()*match.fighters.size(),0);
    auto wd=b2DefaultWorldDef(); wd.gravity={0,0}; wd.enableSleep=false; wd.restitutionThreshold=0;
    world=b2CreateWorld(&wd);
    auto sd=b2DefaultShapeDef(); sd.material.friction=0; sd.material.restitution=1;
    auto bd=b2DefaultBodyDef(); auto wall=b2CreateBody(world,&bd);
    const auto& vertices=match.arena.vertices;
    for(size_t i=0;i<vertices.size();++i) {
        Vec a=vertices[i],b=vertices[(i+1)%vertices.size()],d=b-a;
        Vec outward=normalized(Vec{d.y,-d.x});
        b2Polygon box=b2MakeOffsetBox(length(d)/80+0.07f,0.1f,physics((a+b)*0.5f+outward*4),b2MakeRot(std::atan2(d.y,d.x)));
        b2CreatePolygonShape(wall,&sd,&box);
    }
    for(int k=0;k<static_cast<int>(match.fighters.size());++k) {
        auto& f=match.fighters[k];
        f.cooldowns.resize(f.items.size()); f.hitCooldowns.resize(f.items.size());
        f.meleeAngles.resize(f.items.size(),f.angle); f.meleeDirections.resize(f.items.size(),1); f.meleeClashCooldowns.resize(f.items.size());
        int meleeNumber=0; std::map<std::string,std::pair<size_t,int>> rangedCopies;
        for(size_t n=0;n<f.items.size();++n) {
            const auto& item=f.items[n];
            if(item.effect=="sword") {
                // A second melee weapon starts half an orbit away, so the two blades are readable and staggered.
                f.meleeAngles[n]=std::remainder(f.angle+(meleeNumber++?Pi:0),2*Pi);
            } else if(item.category=="weapon") {
                if(auto first=rangedCopies.find(item.id); first!=rangedCopies.end()) {
                    int copyNumber=++first->second.second;
                    f.cooldowns[n]=f.cooldowns[first->second.first]+static_cast<float>(copyNumber);
                } else rangedCopies.emplace(item.id,std::pair<size_t,int>{n,0});
            }
        }
        bd=b2DefaultBodyDef(); bd.type=f.stationary?b2_staticBody:b2_dynamicBody;
        bd.position=physics(f.position); bd.linearVelocity=physics(f.velocity); bd.isBullet=true; bd.fixedRotation=true;
        bodies[k]=b2CreateBody(world,&bd); b2Circle circle{{0,0},f.radius/40}; sd.density=1;
        if(!f.untargetable) b2CreateCircleShape(bodies[k],&sd,&circle);
    }
    for(int k=0;k<static_cast<int>(match.fighters.size());++k) if(const auto* helper=match.fighters[k].effect("minion")) {
        auto& owner=match.fighters[k]; Item weapon=owner.minionWeapon.id.empty()?fallbackMinionWeapon(match.seed,k):owner.minionWeapon;
        int target=closestEnemy(static_cast<int>(k),owner.position); if(target<0) continue;
        Vec forward=normalized(match.fighters[target].position-owner.position),side{-forward.y,forward.x};
        float offset=(mix32(match.seed^static_cast<uint32_t>(k*419))&1u)?1.0f:-1.0f;
        Vec p=owner.position+side*offset*(owner.radius+helper->get("radius")+8);
        Minion minion;
        minion.position=p; minion.previous=p; minion.velocity=forward*helper->get("speed"); minion.owner=k;
        minion.hp=helper->get("hp"); minion.maxHp=helper->get("hp"); minion.radius=helper->get("radius");
        minion.speed=helper->get("speed"); minion.meleeAngle=owner.angle; minion.weapon=weapon;
        minions.push_back(minion);
    }
    log(-1,"Fighters ready");
}
Simulation::~Simulation() { b2DestroyWorld(world); }
bool Simulation::enemies(int first,int second) const {
    return first>=0 && second>=0 && first<static_cast<int>(match.fighters.size()) && second<static_cast<int>(match.fighters.size()) && match.fighters[first].team!=match.fighters[second].team;
}
int Simulation::closestEnemy(int owner,Vec from) const {
    int result=-1; float best=std::numeric_limits<float>::max();
    for(size_t index=0;index<match.fighters.size();++index) if(enemies(owner,static_cast<int>(index)) && match.fighters[index].hp>0 && !match.fighters[index].untargetable) {
        float distance=length(match.fighters[index].position-from); if(distance<best) { best=distance; result=static_cast<int>(index); }
    }
    return result;
}
void Simulation::log(int owner,const std::string& message,Cue cue,Vec position,float amount,bool critical) {
    events.push_back({time,owner,message,cue,position,++eventSerial,amount,critical}); if(events.size()>80) events.erase(events.begin());
}
bool Simulation::criticalHit(int source) {
    if(source<0 || source>=static_cast<int>(match.fighters.size())) return false;
    const auto* crit=match.fighters[source].effect("crit");
    if(!crit) return false;
    uint32_t salt=static_cast<uint32_t>(criticalSerial++*0x9e3779b9ULL);
    uint32_t roll=mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(source*0x85ebca6bu)^salt);
    return roll%10000<static_cast<uint32_t>(crit->get("chance")*10000);
}
void Simulation::damage(int victim,float amount,int source,const std::string& label,Cue cue,bool canCrit) {
    if(victim<0 || victim>=static_cast<int>(match.fighters.size()) || (source>=0 && !enemies(victim,source))) return;
    auto& f=match.fighters[victim];
    if(f.untargetable || f.invulnerable>0 || f.hp<=0) return;
    bool critical=canCrit && criticalHit(source);
    if(source>=0) {
        const auto& attacker=match.fighters[source]; amount*=attacker.damageMultiplier;
        if(attacker.hp<attacker.maxHp*0.2f && hasItem(attacker,"stat.berserk")) amount*=2;
        if(attacker.effect("combo_master") && attacker.comboWindow>0 && label.find("Poison")==std::string::npos) amount*=1.3f;
    }
    if(critical) amount*=2;
    amount*=1-f.armor;
    float blocked=std::min(f.shield,amount); f.shield-=blocked; amount-=blocked;
    if(blocked>0.1f) log(victim,"BLOCKED · "+std::to_string(static_cast<int>(std::ceil(blocked))),Cue::Shield,f.position);
    amount=std::min(f.hp,amount); f.hp-=amount; f.flash=0.15f;
    if(amount>0.1f) {
        bursts.push_back({f.position,0.65f,0.65f,24,victim,"-"+std::to_string(static_cast<int>(std::ceil(amount)))});
        log(source,label+" · "+std::to_string(static_cast<int>(std::ceil(amount)))+" damage",cue,f.position,amount);
        if(critical) events.back().critical=true;
        if(source>=0) {
            auto& attacker=match.fighters[source]; attacker.damageDealt+=amount;
            if(attacker.effect("combo_master") && label.find("Poison")==std::string::npos) attacker.comboWindow=ComboWindow;
            if(const auto* vamp=attacker.effect("vampire")) if(attacker.hp>0) attacker.hp=std::min(attacker.maxHp,attacker.hp+amount*vamp->get("lifesteal"));
        }
    }
}
void Simulation::fire(int owner,const Item& i,size_t slot) {
    auto& f=match.fighters[owner]; int targetIndex=closestEnemy(owner,f.position);
    float targetDistance=targetIndex>=0?length(match.fighters[targetIndex].position-f.position):std::numeric_limits<float>::max();
    auto* minionTarget=closerEnemyMinion(minions,match.fighters,owner,f.position,targetDistance);
    if(minionTarget) targetDistance=length(minionTarget->position-f.position);
    auto* mouseTarget=closerTargetableMouse(mice,match.fighters,owner,f.position,targetDistance);
    if(targetIndex<0 && !minionTarget && !mouseTarget) return;
    Vec targetPosition=mouseTarget?mouseTarget->position:minionTarget?minionTarget->position:match.fighters[targetIndex].position;
    if(i.effect=="mine") {
        if(hazards.size()<300) hazards.push_back({f.position,owner,i.get("damage"),i.get("trigger_radius"),i.get("blast_radius"),i.get("lifetime"),i.get("arm_time"),0,true});
        log(owner,"MINE DEPLOYED",Cue::Mine,f.position); return;
    }
    if(i.effect=="flute") {
        // Summoned flute mice remain non-targetable; in an invasion they emerge toward the nearest hostile mouse.
        Vec forward=normalized(targetPosition-f.position),back=forward*-1,side{-forward.y,forward.x};
        int count=static_cast<int>(i.get("count")); float radius=i.get("mouse_radius"),speed=i.get("mouse_speed");
        for(int n=0;n<count && mice.size()<120;++n) {
            float spread=(n-(count-1)*0.5f)*0.34f;
            Vec heading{std::cos(std::atan2(forward.y,forward.x)+spread),std::sin(std::atan2(forward.y,forward.x)+spread)};
            Vec p=f.position+back*(f.radius+radius+5)+side*((n-(count-1)*0.5f)*radius*1.45f);
            mice.push_back({p,p,heading*speed,owner,i.get("mouse_hp"),radius,0,i.get("damage"),i.get("bite_cooldown"),0});
            mice.back().speed=speed;
        }
        bursts.push_back({f.position+back*(f.radius+10),0.3f,0.3f,30,owner,""});
        log(owner,"MICE SWARM!",Cue::Flute,f.position); return;
    }
    Vec dir=normalized(targetPosition-f.position); float angle=std::atan2(dir.y,dir.x);
    if(f.effect("bad_aim")) {
        uint32_t roll=mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>((owner+1)*0x9e3779b9u+(slot+1)*97));
        float degrees=(static_cast<float>(roll&0xffffu)/65535.0f-0.5f)*10.0f;
        angle+=degrees*Pi/180.0f; dir={std::cos(angle),std::sin(angle)};
    }
    auto projectileLimitReached=[&](const std::string& kind) {
        int copies=static_cast<int>(std::count_if(f.items.begin(),f.items.end(),[&](const Item& item) { return item.effect==kind; }));
        int active=static_cast<int>(std::count_if(projectiles.begin(),projectiles.end(),[&](const Projectile& projectile) { return projectile.kind==kind && projectile.sourceOwner==owner; }));
        return active>=copies;
    };
    if(i.effect=="boomerang") {
        if(projectileLimitReached("boomerang")) return;
        Vec p=f.position+dir*(f.radius+7); float speed=i.get("projectile_speed");
        if(projectiles.size()<400) {
            projectiles.push_back({p,p,dir*speed,owner,i.get("damage"),i.get("projectile_radius"),std::max(1.0f,rules.limit-time),"boomerang",static_cast<int>(slot),owner});
            auto& boomerang=projectiles.back(); boomerang.returnPull=i.get("return_pull"); boomerang.cruiseSpeed=speed;
        }
        bursts.push_back({p,0.18f,0.18f,18,owner,""}); log(owner,"BOOMERANG THROWN",Cue::Arrow,f.position); return;
    }
    if(i.effect=="bouncy_ball") {
        // Each rolled copy has its own active ball, so duplicate weapons can both be in flight.
        if(projectileLimitReached("bouncy_ball")) return;
        Vec p=f.position+dir*(f.radius+7);
        if(projectiles.size()<400) {
            projectiles.push_back({p,p,dir*i.get("projectile_speed"),owner,i.get("damage"),i.get("projectile_radius"),std::max(1.0f,rules.limit-time),"bouncy_ball",static_cast<int>(slot),owner});
            auto& ball=projectiles.back();
            ball.bouncyReturnTarget=owner;
        }
        bursts.push_back({p,0.18f,0.18f,18,owner,""}); log(owner,"BOUNCY BALL THROWN",Cue::Bounce,f.position); return;
    }
    int count=i.effect=="shotgun"?3:1;
    for(int n=0;n<count;++n) {
        float a=angle+(count==3?(n-1)*i.get("spread",0.26f):0);
        Vec heading{std::cos(a),std::sin(a)}; Vec p=f.position+heading*(f.radius+7);
        if(projectiles.size()<400) projectiles.push_back({p,p,heading*i.get("projectile_speed"),owner,i.get("damage"),i.get("projectile_radius"),5,i.effect,static_cast<int>(slot),owner});
    }
    bursts.push_back({f.position+dir*(f.radius+7),0.16f,0.16f,15,owner,""});
    log(owner,i.name+" fired",(i.effect=="bow" || i.effect=="poison_dart" || i.effect=="shuriken")?Cue::Arrow:Cue::Gun,f.position);
}
void Simulation::step() {
    if(finished) return;
    time+=Step; for(auto& cooldown:contactCooldowns) cooldown-=Step;
    for(auto& b:bursts) b.life-=Step;
    std::erase_if(bursts,[](const Burst& b){return b.life<=0;});
    for(auto& shape:fieldShapes) shape.life-=Step;
    std::erase_if(fieldShapes,[](const FieldShape& shape){return shape.life<=0;});
    if(match.mouseInvasion.enabled && invasionMouseOwner>=0) {
        invasionMouseTimer-=Step;
        if(invasionMouseTimer<=0) {
            invasionMouseTimer+=match.mouseInvasion.interval;
            int active=static_cast<int>(std::ranges::count_if(mice,[](const SummonedMouse& mouse){return mouse.targetable && mouse.hp>0;}));
            if(active<match.mouseInvasion.maxCount) {
                float minX=match.arena.vertices.front().x,maxX=minX,minY=match.arena.vertices.front().y,maxY=minY;
                for(Vec vertex:match.arena.vertices) { minX=std::min(minX,vertex.x); maxX=std::max(maxX,vertex.x); minY=std::min(minY,vertex.y); maxY=std::max(maxY,vertex.y); }
                Vec spawn{}; uint32_t salt=static_cast<uint32_t>(++invasionMouseSerial);
                for(int attempt=0;attempt<64;++attempt) {
                    uint32_t rx=mix32(match.seed^salt*0x9e3779b9u^static_cast<uint32_t>(attempt*193));
                    uint32_t ry=mix32(match.seed^salt*0x85ebca6bu^static_cast<uint32_t>(attempt*389));
                    float ux=static_cast<float>(rx&0xffffu)/65535.0f,uy=static_cast<float>(ry&0xffffu)/65535.0f;
                    Vec candidate{minX+(maxX-minX)*ux,minY+(maxY-minY)*uy};
                    bool clear=pointInPolygon(candidate,match.arena.vertices);
                    for(size_t edge=0;edge<match.arena.vertices.size() && clear;++edge) if(segmentDistance(candidate,match.arena.vertices[edge],match.arena.vertices[(edge+1)%match.arena.vertices.size()])<match.mouseInvasion.radius+2) clear=false;
                    for(const auto& fighter:match.fighters) if(clear && !fighter.hidden && fighter.hp>0 && length(candidate-fighter.position)<match.mouseInvasion.radius+fighter.radius+8) clear=false;
                    if(clear) { spawn=candidate; break; }
                }
                uint32_t directionRoll=mix32(match.seed^salt*0xc2b2ae35u); float angle=(static_cast<float>(directionRoll&0xffffu)/65535.0f)*2*Pi;
                SummonedMouse mouse; mouse.position=mouse.previous=spawn; mouse.velocity={std::cos(angle)*match.mouseInvasion.speed,std::sin(angle)*match.mouseInvasion.speed};
                mouse.owner=invasionMouseOwner; mouse.hp=match.mouseInvasion.hp; mouse.radius=match.mouseInvasion.radius; mouse.damage=match.mouseInvasion.damage;
                mouse.biteCooldown=match.mouseInvasion.biteCooldown; mouse.speed=match.mouseInvasion.speed; mouse.targetable=true; mice.push_back(mouse);
                bursts.push_back({spawn,0.3f,0.3f,18,invasionMouseOwner,""}); log(invasionMouseOwner,"INVASION MOUSE!",Cue::None,spawn);
            }
        }
    }
    for(int k=0;k<static_cast<int>(match.fighters.size());++k) {
        auto& f=match.fighters[k]; f.previous=f.position; f.flash=std::max(0.0f,f.flash-Step);
        f.wallCooldown-=Step; f.boostTime-=Step; f.invulnerable=std::max(0.0f,f.invulnerable-Step);
        f.comboWindow=std::max(0.0f,f.comboWindow-Step);
        f.shieldTime-=Step; if(f.shieldTime<=0) f.shield=0;
        f.slowTime=std::max(0.0f,f.slowTime-Step);
        f.wallStunWindow=std::max(0.0f,f.wallStunWindow-Step);
        f.stunTime=std::max(0.0f,f.stunTime-Step);
        if(f.hp<=0 && !f.physicsDisabled) { b2Body_Disable(bodies[k]); f.physicsDisabled=true; }
        if(f.stationary || f.hp<=0) {
            f.velocity={}; b2Body_SetLinearVelocity(bodies[k],{}); continue;
        }
        if(f.stunTime>0) {
            f.velocity=normalized(f.velocity)*(f.speed*0.1f);
            b2Body_SetLinearVelocity(bodies[k],physics(f.velocity));
            continue;
        }
        f.knockbackTime=std::max(0.0f,f.knockbackTime-Step);
        for(size_t n=0;n<f.items.size();++n) {
            const auto& i=f.items[n]; f.cooldowns[n]-=Step; f.hitCooldowns[n]-=Step; f.meleeClashCooldowns[n]-=Step;
            if(i.effect=="sword") f.meleeAngles[n]=std::remainder(f.meleeAngles[n]+i.get("rotation_speed")*(f.effect("fast_swings")?2:1)*(f.effect("slow_swings")?0.7f:1)*f.meleeDirections[n]*Step,2*Pi);
            if(i.category=="weapon" && i.effect!="sword" && f.cooldowns[n]<=0) {
                fire(k,i,n);
                float reload=i.get("cooldown")+(f.effect("slow_reload")?1.0f:0.0f);
                if(f.effect("fast_reload")) reload*=0.65f;
                f.cooldowns[n]+=std::max(0.1f,reload);
            }
            if(i.effect=="regen" && f.cooldowns[n]<=0) {
                f.hp=std::min(f.maxHp,f.hp+i.get("heal")); f.cooldowns[n]=i.get("cooldown");
                bursts.push_back({f.position,0.6f,0.6f,28,k,"+"+std::to_string(static_cast<int>(i.get("heal")))});
                log(k,"HEAL +"+std::to_string(static_cast<int>(i.get("heal"))),Cue::Heal,f.position,i.get("heal"));
            }
            if(i.effect=="shield" && f.cooldowns[n]<=0) { f.shield=i.get("absorb"); f.shieldTime=i.get("duration"); f.cooldowns[n]=i.get("cooldown"); log(k,"SHIELD UP",Cue::Shield,f.position); }
            if(i.effect=="dash" && f.cooldowns[n]<=0) {
                int target=closestEnemy(static_cast<int>(k),f.position);
                if(target>=0) { f.velocity=normalized(match.fighters[target].position-f.position)*f.speed;
                    f.boostTime=i.get("duration"); f.boostMultiplier=i.get("speed_multiplier"); f.cooldowns[n]=i.get("cooldown"); log(static_cast<int>(k),"DASH!",Cue::Dash,f.position); }
            }
            if(i.effect=="dodge" && f.cooldowns[n]<=0) {
                for(const auto& p:projectiles) if(enemies(k,p.owner) && length(p.position-f.position)<i.get("trigger_distance") && dot(p.velocity,f.position-p.position)>0) {
                    Vec perpendicular=normalized(Vec{-p.velocity.y,p.velocity.x});
                    if(dot(perpendicular,f.position)>0) perpendicular=perpendicular*-1;
                    f.velocity=perpendicular*f.speed; f.boostTime=i.get("duration"); f.invulnerable=i.get("duration");
                    f.boostMultiplier=i.get("speed_multiplier"); f.cooldowns[n]=i.get("cooldown"); log(k,"DODGE!",Cue::Dodge,f.position); break;
                }
            }
        }
        if(f.actorKind=="wild_dog") {
            f.specialCooldown-=Step;
            if(f.specialCooldown<=0) { int target=closestEnemy(static_cast<int>(k),f.position); if(target>=0) { f.velocity=normalized(match.fighters[target].position-f.position)*f.speed; f.boostTime=0.32f; f.boostMultiplier=2.7f; f.specialCooldown+=2; log(static_cast<int>(k),"WILD DOG DASH!",Cue::Dash,f.position); } }
        }
        float targetSpeed=f.speed*(f.boostTime>0?f.boostMultiplier:1)*(f.slowTime>0?f.slowMultiplier:1);
        f.velocity=f.knockbackTime>0?f.knockbackDirection*(f.speed*2.15f):normalized(f.velocity)*targetSpeed;
        // A weak, continuous pull curves travelling balls over several seconds.  It is far
        // too small to replace normal wall bounces, which still use Box2D restitution.
        if(match.arena.effect=="center_gravity") f.velocity=turnToward(f.velocity,Vec{}-f.position,4.0f*Step);
        b2Body_SetLinearVelocity(bodies[k],physics(f.velocity));
    }
    b2World_Step(world,Step,4);
    for(int k=0;k<static_cast<int>(match.fighters.size());++k) {
        auto& f=match.fighters[k]; f.position=pixels(b2Body_GetPosition(bodies[k])); f.velocity=pixels(b2Body_GetLinearVelocity(bodies[k]));
        if(f.stationary || f.hp<=0) continue;
        const auto& vertices=match.arena.vertices;
        for(size_t n=0;n<vertices.size();++n) {
            Vec a=vertices[n],b=vertices[(n+1)%vertices.size()],d=b-a;
            Vec normal=normalized(Vec{-d.y,d.x}); float distance=dot(f.position-a,normal);
            // Containment guard for high speeds / heavily edited catalogs.
            if(distance<f.radius-1) { f.position=f.position+normal*(f.radius-distance); if(dot(f.velocity,normal)<0) f.velocity=f.velocity-normal*(2*dot(f.velocity,normal)); b2Body_SetTransform(bodies[k],physics(f.position),b2Rot_identity); }
            if(f.untargetable) continue;
            if(distance<=f.radius+1.2f && f.wallCooldown<=0) {
                f.wallCooldown=0.18f;
                if(f.wallStunWindow>0) {
                    f.wallStunWindow=0; f.knockbackTime=0; f.stunTime=1;
                    f.velocity=normal*(f.speed*0.1f); b2Body_SetLinearVelocity(bodies[k],physics(f.velocity));
                    bursts.push_back({f.position,0.35f,0.35f,30,k,""});
                    log(k,"STUNNED!",Cue::Stun,f.position);
                    continue;
                }
                log(k,"Wall bounce",Cue::Bounce,f.position);
                if(match.arena.effect=="spiked_arena") {
                    uint32_t roll=mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(k*0x9e3779b9u));
                    bool lucky=f.effect("careful_steps") && roll%10000<6000;
                    if(lucky) {
                        bursts.push_back({f.position,0.35f,0.35f,21,k,"LUCKY"});
                        log(k,"LUCKY ARENA SPIKE DODGE",Cue::Dodge,f.position);
                    } else damage(k,1,-1,"ARENA SPIKE",Cue::Hit,false);
                }
                if(const auto* spikes=f.effect("spikes")) {
                    // Keep the hit area on the wall and remember its inward direction for rendering.
                    Vec point=f.position-normal*std::max(0.0f,distance);
                    if(hazards.size()<300) hazards.push_back({point,k,spikes->get("damage"),spikes->get("radius"),0,spikes->get("lifetime"),0,0,false,normal});
                }
                if(const auto* tape=f.effect("tape")) {
                    auto existing=std::find_if(tapes.begin(),tapes.end(),[&](const Tape& value){ return value.owner==k; });
                    Vec wallPoint=f.position-normal*std::max(0.0f,distance);
                    if(existing==tapes.end()) {
                        float baseChance=tape->get("chance");
                        float chance=f.tapeChance>0?f.tapeChance:baseChance;
                        uint32_t roll=mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(k*0x9e3779b9u));
                        if(roll%10000<static_cast<uint32_t>(chance*10000)) {
                            tapes.push_back({wallPoint,f.position,k,0,true});
                            f.tapeChance=baseChance;
                            log(k,"TAPE DEPLOYED",Cue::Dodge,wallPoint);
                        } else {
                            f.tapeChance=std::min(1.0f,chance+0.01f);
                        }
                    } else if(existing->deploying) {
                        existing->end=wallPoint; existing->deploying=false; existing->life=1;
                        log(k,"TAPE SET",Cue::Shield,wallPoint);
                    }
                }
                if(f.effect("damage_field")) {
                    Vec wallPoint=f.position-normal*std::max(0.0f,distance);
                    auto& field=damageFields[k];
                    if(!field.drawing) {
                        field.points={wallPoint}; field.drawing=true;
                        log(k,"DAMAGE FIELD START",Cue::Shield,wallPoint);
                    } else if(field.points.size()<128) {
                        field.points.push_back(wallPoint);
                        log(k,"DAMAGE FIELD LINE",Cue::Shield,wallPoint);
                    }
                }
            }
        }
        if(match.arena.effect=="healing_arena") {
            float zoneRadius=rules.radius*1.4f;
            bool inside=length(f.position)<=zoneRadius;
            if(inside && !f.centerHealingInside && f.hp>0) {
                float healed=std::min(15.0f,f.maxHp-f.hp);
                if(healed>0) {
                    f.hp+=healed;
                    bursts.push_back({f.position,0.7f,0.7f,28,k,"+"+std::to_string(static_cast<int>(healed))});
                    log(k,"CENTER HEAL +"+std::to_string(static_cast<int>(healed)),Cue::Heal,f.position,healed);
                }
            }
            f.centerHealingInside=inside;
        }
        auto& field=damageFields[k];
        if(f.effect("damage_field") && field.drawing && field.points.size()>=3) {
            Vec crossing{}; size_t crossedSegment=field.points.size();
            for(size_t segment=0;segment+2<field.points.size();++segment) {
                if(properSegmentCrossing(field.points.back(),f.position,field.points[segment],field.points[segment+1],crossing)) {
                    crossedSegment=segment; break;
                }
            }
            if(crossedSegment<field.points.size()) {
                std::vector<Vec> polygon{crossing};
                for(size_t point=crossedSegment+1;point<field.points.size();++point) polygon.push_back(field.points[point]);
                if(polygon.size()>=3) {
                    if(fieldShapes.size()<24) fieldShapes.push_back({polygon,k,0.55f,0.55f});
                    Vec centre{}; for(Vec point:polygon) centre=centre+point; centre=centre/static_cast<float>(polygon.size());
                    auto dodgeField=[&](int victim) {
                        const auto& target=match.fighters[victim];
                        if(!target.effect("careful_steps")) return false;
                        uint32_t roll=mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(k*0x9e3779b9u+victim*193));
                        return roll%100<60;
                    };
                    for(size_t victim=0;victim<match.fighters.size();++victim) if(enemies(static_cast<int>(k),static_cast<int>(victim)) && match.fighters[victim].hp>0 && !match.fighters[victim].untargetable && pointInPolygon(match.fighters[victim].position,polygon)) {
                        if(dodgeField(static_cast<int>(victim))) log(static_cast<int>(victim),"LUCKY FIELD DODGE",Cue::Dodge,match.fighters[victim].position);
                        else damage(static_cast<int>(victim),25,static_cast<int>(k),"DAMAGE FIELD",Cue::Hit);
                    }
                    for(auto& minion:minions) if(enemies(static_cast<int>(k),minion.owner) && minion.hp>0 && pointInPolygon(minion.position,polygon)) {
                        minion.hp-=25; bursts.push_back({minion.position,0.35f,0.35f,18,k,"-25"}); log(k,"FIELD HIT MINION",Cue::Hit,minion.position);
                    }
                    for(auto& mouse:mice) if(enemies(static_cast<int>(k),mouse.owner) && mouse.hp>0 && pointInPolygon(mouse.position,polygon)) {
                        mouse.hp-=25; bursts.push_back({mouse.position,0.3f,0.3f,14,k,"-25"});
                    }
                    bursts.push_back({centre,0.55f,0.55f,42,k,"FIELD!"});
                    log(k,"DAMAGE FIELD CLOSED",Cue::Hit,centre,25);
                }
                field.points.clear(); field.drawing=false;
            }
        }
    }
    // A tape follows its owner until the next wall touch, then remains as a one-use snare.
    // Enemy helpers count as a crossing too: a mouse or pocket minion can spend the tape
    // and takes the same movement penalty as its owner would.
    for(auto& tape:tapes) {
        if(tape.deploying) tape.end=match.fighters[tape.owner].position;
        auto crossed=[&](Vec previous,Vec position,float radius) {
            return segmentsIntersect(previous,position,tape.start,tape.end)
                || std::min(segmentDistance(previous,tape.start,tape.end),segmentDistance(position,tape.start,tape.end))<=radius+2;
        };
        if(length(tape.end-tape.start)>8) {
            const auto* source=match.fighters[tape.owner].effect("tape");
            float duration=source?source->get("duration"):3.0f;
            float multiplier=source?source->get("slow_multiplier"):0.2f;
            Vec snaredAt{}; bool snared=false,dodged=false;
            for(size_t targetIndex=0;targetIndex<match.fighters.size() && !snared;++targetIndex) if(enemies(tape.owner,static_cast<int>(targetIndex))) {
                auto& target=match.fighters[targetIndex]; if(target.hp<=0 || target.untargetable || !crossed(target.previous,target.position,target.radius)) continue;
                bool careful=target.effect("careful_steps") && (mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(tape.owner*0x85ebca6bu+targetIndex*193))%100)<60;
                snaredAt=target.position; snared=true; dodged=careful;
                if(careful) log(static_cast<int>(targetIndex),"CAREFUL STEPS",Cue::Dodge,target.position);
                else { target.slowTime=std::max(target.slowTime,duration); target.slowMultiplier=std::min(target.slowMultiplier,multiplier); }
            }
            for(auto& minion:minions) if(!snared && enemies(tape.owner,minion.owner) && minion.hp>0 && crossed(minion.previous,minion.position,minion.radius)) {
                minion.slowTime=std::max(minion.slowTime,duration); minion.slowMultiplier=std::min(minion.slowMultiplier,multiplier);
                snaredAt=minion.position; snared=true;
            }
            for(auto& mouse:mice) if(!snared && enemies(tape.owner,mouse.owner) && mouse.hp>0 && crossed(mouse.previous,mouse.position,mouse.radius)) {
                mouse.slowTime=std::max(mouse.slowTime,duration); mouse.slowMultiplier=std::min(mouse.slowMultiplier,multiplier);
                snaredAt=mouse.position; snared=true;
            }
            if(snared) {
            tape.deploying=false; tape.life=0;
            bursts.push_back({snaredAt,0.3f,0.3f,24,tape.owner,""});
            if(!dodged) log(tape.owner,"TAPE SNARE!",Cue::Dodge,snaredAt);
            }
        }
    }
    std::erase_if(tapes,[](const Tape& tape){ return !tape.deploying && tape.life<=0; });
    // Pocket minions are independent small fighters with a pre-rolled weapon.
    for(auto& minion:minions) {
        if(minion.hp<=0) continue;
        const auto& weapon=minion.weapon;
        int fighterTargetIndex=closestEnemy(minion.owner,minion.position);
        Fighter* fighterTarget=fighterTargetIndex>=0?&match.fighters[fighterTargetIndex]:nullptr;
        float targetDistance=fighterTarget?length(fighterTarget->position-minion.position):std::numeric_limits<float>::max();
        auto* minionTarget=closerEnemyMinion(minions,match.fighters,minion.owner,minion.position,targetDistance);
        if(minionTarget) targetDistance=length(minionTarget->position-minion.position);
        auto* mouseTarget=closerTargetableMouse(mice,match.fighters,minion.owner,minion.position,targetDistance);
        bool hasTarget=fighterTarget || minionTarget || mouseTarget;
        Vec targetPosition=mouseTarget?mouseTarget->position:minionTarget?minionTarget->position:fighterTarget?fighterTarget->position:minion.position+normalized(minion.velocity)*100;
        float targetRadius=mouseTarget?mouseTarget->radius:minionTarget?minionTarget->radius:fighterTarget?fighterTarget->radius:0;
        minion.previous=minion.position;
        minion.wallStunWindow=std::max(0.0f,minion.wallStunWindow-Step);
        minion.stunTime=std::max(0.0f,minion.stunTime-Step);
        minion.slowTime=std::max(0.0f,minion.slowTime-Step);
        float moveMultiplier=minion.slowTime>0?minion.slowMultiplier:1;
        if(minion.stunTime>0) minion.velocity=normalized(minion.velocity)*(minion.speed*0.1f*moveMultiplier);
        else {
            minion.knockbackTime=std::max(0.0f,minion.knockbackTime-Step);
            if(minion.knockbackTime>0) minion.velocity=minion.knockbackDirection*(minion.speed*2.15f*moveMultiplier);
            else minion.velocity=normalized(minion.velocity)*(minion.speed*moveMultiplier);
            minion.cooldown-=Step; minion.hitCooldown-=Step;
        }
        if(match.arena.effect=="center_gravity") minion.velocity=turnToward(minion.velocity,Vec{}-minion.position,4.0f*Step);
        minion.position=minion.position+minion.velocity*Step;
        for(size_t n=0;n<match.arena.vertices.size();++n) {
            Vec a=match.arena.vertices[n],b=match.arena.vertices[(n+1)%match.arena.vertices.size()];
            Vec normal=normalized(Vec{-(b-a).y,(b-a).x}); float distance=dot(minion.position-a,normal);
            if(distance<minion.radius) {
                minion.position=minion.position+normal*(minion.radius-distance);
                if(minion.wallStunWindow>0) {
                    minion.wallStunWindow=0; minion.knockbackTime=0; minion.stunTime=1;
                    minion.velocity=normal*(minion.speed*0.1f); bursts.push_back({minion.position,0.35f,0.35f,26,minion.owner,""}); log(minion.owner,"MINION STUNNED!",Cue::Stun,minion.position);
                } else if(dot(minion.velocity,normal)<0) {
                    minion.velocity=minion.velocity-normal*(2*dot(minion.velocity,normal));
                }
            }
        }
        for(const auto& ally:match.fighters) if(ally.team==match.fighters[minion.owner].team && ally.hp>0 && length(minion.position-ally.position)<minion.radius+ally.radius) {
            Vec normal=normalized(minion.position-ally.position); minion.position=ally.position+normal*(minion.radius+ally.radius+1);
            if(dot(minion.velocity,normal)<0) minion.velocity=minion.velocity-normal*(2*dot(minion.velocity,normal));
            break;
        }
        if(minion.stunTime>0) continue;
        if(hasTarget && length(minion.position-targetPosition)<minion.radius+targetRadius) {
            Vec away=normalized(minion.position-targetPosition); minion.position=targetPosition+away*(minion.radius+targetRadius+1); minion.velocity=away*minion.speed;
        }
        if(!hasTarget) continue;
        if(weapon.effect=="sword") {
            auto leech=[&]() {
                float healing=weapon.get("lifesteal"); if(healing<=0 || minion.hp<=0) return;
                float gained=std::min(healing,minion.maxHp-minion.hp); if(gained<=0) return;
                minion.hp+=gained; bursts.push_back({minion.position,0.35f,0.35f,18,minion.owner,"+"+std::to_string(static_cast<int>(gained))});
                log(minion.owner,"MINION SCYTHE LEECH",Cue::Heal,minion.position,gained);
            };
            minion.meleeAngle=std::remainder(minion.meleeAngle+weapon.get("rotation_speed")*Step,2*Pi);
            Vec direction{std::cos(minion.meleeAngle),std::sin(minion.meleeAngle)};
            Vec start=minion.position+direction*(minion.radius+3),tip=minion.position+direction*(minion.radius+weapon.get("reach"));
            if(minion.hitCooldown<=0 && segmentDistance(targetPosition,start,tip)<targetRadius+3) {
                if(mouseTarget) {
                    float amount=weapon.get("damage")*match.fighters[minion.owner].damageMultiplier;
                    if(match.fighters[minion.owner].hp<match.fighters[minion.owner].maxHp*0.2f && hasItem(match.fighters[minion.owner],"stat.berserk")) amount*=2;
                    mouseTarget->hp-=amount; bursts.push_back({mouseTarget->position,0.3f,0.3f,14,minion.owner,""}); log(minion.owner,"MINION HIT MOUSE",Cue::Sword,mouseTarget->position);
                    leech();
                } else if(minionTarget) {
                    float amount=weapon.get("damage")*match.fighters[minion.owner].damageMultiplier;
                    if(match.fighters[minion.owner].hp<match.fighters[minion.owner].maxHp*0.2f && hasItem(match.fighters[minion.owner],"stat.berserk")) amount*=2;
                    minionTarget->hp-=amount; bursts.push_back({minionTarget->position,0.35f,0.35f,18,minion.owner,""}); log(minion.owner,"MINION HIT",Cue::Sword,minionTarget->position);
                    leech();
                    if(weapon.id=="weapon.hammer" && minionTarget->hp>0) {
                        minionTarget->knockbackDirection=normalized(minionTarget->position-minion.position);
                        minionTarget->knockbackTime=0.3f; minionTarget->wallStunWindow=0.3f;
                    }
                } else {
                    bool hit=fighterTarget->hp>0 && fighterTarget->invulnerable<=0;
                    damage(fighterTargetIndex,weapon.get("damage"),minion.owner,"Minion "+weapon.name,Cue::Sword);
                    if(hit) leech();
                }
                minion.hitCooldown=weapon.get("hit_cooldown");
            }
        } else if(minion.cooldown<=0) {
            Vec direction=normalized(targetPosition-minion.position); float baseAngle=std::atan2(direction.y,direction.x);
            int count=weapon.effect=="shotgun"?3:1;
            for(int n=0;n<count && projectiles.size()<400;++n) {
                float angle=baseAngle+(count==3?(n-1)*weapon.get("spread",0.26f):0);
                Vec heading{std::cos(angle),std::sin(angle)},p=minion.position+heading*(minion.radius+4);
                projectiles.push_back({p,p,heading*weapon.get("projectile_speed"),minion.owner,weapon.get("damage"),weapon.get("projectile_radius"),5,weapon.effect,-1,minion.owner});
            }
            minion.cooldown=weapon.get("cooldown");
            log(minion.owner,"MINION "+weapon.name,(weapon.effect=="bow" || weapon.effect=="poison_dart" || weapon.effect=="shuriken")?Cue::Arrow:Cue::Gun,minion.position);
        }
    }
    for(size_t a=0;a<minions.size();++a) for(size_t b=a+1;b<minions.size();++b) {
        auto& first=minions[a]; auto& second=minions[b]; if(first.hp<=0 || second.hp<=0) continue;
        Vec delta=second.position-first.position; float distance=length(delta),minimum=first.radius+second.radius;
        if(distance>=minimum) continue;
        Vec normal=distance>0.001f?delta/distance:Vec{1,0};
        Vec correction=normal*((minimum-distance)*0.5f); first.position=first.position-correction; second.position=second.position+correction;
        float firstAlong=dot(first.velocity,normal),secondAlong=dot(second.velocity,normal);
        if(secondAlong-firstAlong<0) { first.velocity=first.velocity+normal*(secondAlong-firstAlong); second.velocity=second.velocity+normal*(firstAlong-secondAlong); }
    }
    // Mice are lightweight simulated minions: they travel at their own speed and bounce inside the same arena.
    for(auto& mouse:mice) {
        mouse.previous=mouse.position; mouse.slowTime=std::max(0.0f,mouse.slowTime-Step);
        mouse.velocity=normalized(mouse.velocity)*(mouse.speed*(mouse.slowTime>0?mouse.slowMultiplier:1));
        if(match.arena.effect=="center_gravity") mouse.velocity=turnToward(mouse.velocity,Vec{}-mouse.position,4.0f*Step);
        mouse.position=mouse.position+mouse.velocity*Step;
        mouse.attackCooldown-=Step;
        for(size_t n=0;n<match.arena.vertices.size();++n) {
            Vec a=match.arena.vertices[n],b=match.arena.vertices[(n+1)%match.arena.vertices.size()];
            Vec normal=normalized(Vec{-(b-a).y,(b-a).x}); float distance=dot(mouse.position-a,normal);
            if(distance<mouse.radius) {
                mouse.position=mouse.position+normal*(mouse.radius-distance);
                if(dot(mouse.velocity,normal)<0) {
                    mouse.velocity=mouse.velocity-normal*(2*dot(mouse.velocity,normal));
                }
            }
        }
    }
    for(size_t a=0;a<mice.size();++a) for(size_t b=a+1;b<mice.size();++b) {
        auto& first=mice[a]; auto& second=mice[b]; if(first.hp<=0 || second.hp<=0) continue;
        Vec delta=second.position-first.position; float distance=length(delta),minimum=first.radius+second.radius;
        if(distance>=minimum) continue;
        Vec normal=distance>0.001f?delta/distance:Vec{1,0};
        Vec correction=normal*((minimum-distance)*0.5f); first.position=first.position-correction; second.position=second.position+correction;
        float firstAlong=dot(first.velocity,normal),secondAlong=dot(second.velocity,normal);
        if(secondAlong-firstAlong<0) { first.velocity=first.velocity+normal*(secondAlong-firstAlong); second.velocity=second.velocity+normal*(firstAlong-secondAlong); }
    }
    for(auto& mouse:mice) {
        if(mouse.hp<=0 || mouse.attackCooldown>0) continue;
        Minion* touchedMinion=nullptr;
        for(auto& minion:minions) if(enemies(mouse.owner,minion.owner) && minion.hp>0 && segmentDistance(mouse.position,minion.previous,minion.position)<=mouse.radius+minion.radius) { touchedMinion=&minion; break; }
        if(touchedMinion) {
            float amount=mouse.damage*match.fighters[mouse.owner].damageMultiplier;
            if(match.fighters[mouse.owner].hp<match.fighters[mouse.owner].maxHp*0.2f && hasItem(match.fighters[mouse.owner],"stat.berserk")) amount*=2;
            touchedMinion->hp-=amount; mouse.hp=0;
            bursts.push_back({touchedMinion->position,0.35f,0.35f,18,mouse.owner,""}); log(mouse.owner,"MOUSE BITES MINION",Cue::Hit,touchedMinion->position);
            continue;
        }
        int targetIndex=closestEnemy(mouse.owner,mouse.position); if(targetIndex<0) continue;
        auto& target=match.fighters[targetIndex];
        if(segmentDistance(mouse.position,target.previous,target.position)>mouse.radius+target.radius) continue;
        damage(targetIndex,mouse.damage,mouse.owner,"Mouse bite",Cue::Hit);
        mouse.attackCooldown=mouse.biteCooldown;
        mouse.velocity=normalized(mouse.position-target.position)*length(mouse.velocity);
    }
    // Both body positions must be current before evaluating either fighter's melee hits.
    // A clash chooses an orbit that moves the affected blade tip away from its opponent.
    // Equal weights affect both blades; a heavier blade only deflects the lighter one.
    for(size_t leftIndex=0;leftIndex<match.fighters.size();++leftIndex) for(size_t rightIndex=leftIndex+1;rightIndex<match.fighters.size();++rightIndex) {
        if(!enemies(static_cast<int>(leftIndex),static_cast<int>(rightIndex))) continue;
        auto& left=match.fighters[leftIndex]; auto& right=match.fighters[rightIndex];
        if(left.hp<=0 || right.hp<=0) continue;
        for(size_t a=0;a<left.items.size();++a) {
            const auto& leftItem=left.items[a];
            if(leftItem.effect!="sword" || left.stunTime>0 || left.meleeClashCooldowns[a]>0) continue;
            float leftAngle=left.meleeAngles[a]; auto leftSegment=meleeSegment(left,leftItem,leftAngle);
            for(size_t b=0;b<right.items.size();++b) {
            const auto& rightItem=right.items[b];
            if(rightItem.effect!="sword" || right.stunTime>0 || right.meleeClashCooldowns[b]>0) continue;
            float rightAngle=right.meleeAngles[b]; auto rightSegment=meleeSegment(right,rightItem,rightAngle);
            bool leftShield=leftItem.id=="weapon.orbit_shield",rightShield=rightItem.id=="weapon.orbit_shield";
            // The shield is a broad plate and the hammer has a large head, so edge contact
            // must count as a clash instead of allowing the hammer to slip through its artwork.
            float contactPadding=(leftShield?5.0f:0.0f)+(rightShield?5.0f:0.0f)
                +(leftItem.id=="weapon.hammer"?5.0f:0.0f)+(rightItem.id=="weapon.hammer"?5.0f:0.0f);
            if(segmentGap(leftSegment.base,leftSegment.tip,rightSegment.base,rightSegment.tip)>contactPadding) continue;
            int leftWeight=meleeWeight(leftItem),rightWeight=meleeWeight(rightItem);
            auto deflectAway=[](Fighter& owner,size_t itemIndex,const Fighter& target) {
                float angle=owner.meleeAngles[itemIndex]; Vec dir{std::cos(angle),std::sin(angle)};
                Vec tip=owner.position+dir*(owner.radius+owner.items[itemIndex].get("reach"));
                Vec tangent{-dir.y,dir.x};
                owner.meleeDirections[itemIndex]=dot(tangent,target.position-tip)>0?-1.0f:1.0f;
            };
            if(leftShield && !rightShield) deflectAway(right,b,left);
            else if(rightShield && !leftShield) deflectAway(left,a,right);
            else {
                if(leftWeight<=rightWeight) deflectAway(left,a,right);
                if(rightWeight<=leftWeight) deflectAway(right,b,left);
            }
            // A deflected blade cannot score a grazing hit while it is being pushed away.
            if(!leftShield && (rightShield || leftWeight<=rightWeight)) left.hitCooldowns[a]=std::max(left.hitCooldowns[a],0.5f);
            if(!rightShield && (leftShield || rightWeight<=leftWeight)) right.hitCooldowns[b]=std::max(right.hitCooldowns[b],0.5f);
            left.meleeClashCooldowns[a]=0.16f; right.meleeClashCooldowns[b]=0.16f;
            Vec midpoint=(leftSegment.tip+rightSegment.tip)*0.5f;
            bursts.push_back({midpoint,0.18f,0.18f,16,static_cast<int>(leftIndex),""});
            log(-1,"MELEE CLASH",Cue::Clash,midpoint);
        }
        }
    }
    for(int k=0;k<static_cast<int>(match.fighters.size());++k) {
        auto& f=match.fighters[k];
        for(size_t n=0;n<f.items.size();++n) {
            const auto& i=f.items[n]; if(i.effect!="sword" || i.id=="weapon.orbit_shield" || f.stunTime>0 || f.hitCooldowns[n]>0) continue;
            auto leech=[&]() {
                float healing=i.get("lifesteal"); if(healing<=0 || f.hp<=0) return;
                float gained=std::min(healing,f.maxHp-f.hp); if(gained<=0) return;
                f.hp+=gained; bursts.push_back({f.position,0.35f,0.35f,18,k,"+"+std::to_string(static_cast<int>(gained))});
                log(k,"SCYTHE LEECH",Cue::Heal,f.position,gained);
            };
            float angle=f.meleeAngles[n]; Vec dir{std::cos(angle),std::sin(angle)};
            Vec a=f.position+dir*(f.radius+5),b=f.position+dir*(f.radius+i.get("reach"));
            int fighterTargetIndex=closestEnemy(static_cast<int>(k),f.position);
            Fighter* fighterTarget=fighterTargetIndex>=0?&match.fighters[fighterTargetIndex]:nullptr;
            float targetDistance=fighterTarget?length(fighterTarget->position-f.position):std::numeric_limits<float>::max();
            auto* minionTarget=closerEnemyMinion(minions,match.fighters,static_cast<int>(k),f.position,targetDistance);
            if(minionTarget) {
                if(segmentDistance(minionTarget->position,a,b)<minionTarget->radius+4) {
                    float amount=i.get("damage")*f.damageMultiplier;
                    if(f.hp<f.maxHp*0.2f && hasItem(f,"stat.berserk")) amount*=2;
                    minionTarget->hp-=amount; f.hitCooldowns[n]=i.get("hit_cooldown");
                    bursts.push_back({minionTarget->position,0.35f,0.35f,18,k,""}); log(k,"MINION HIT",i.id=="weapon.hammer"?Cue::Hammer:Cue::Sword,minionTarget->position);
                    leech();
                    if(i.id=="weapon.hammer" && minionTarget->hp>0) {
                        minionTarget->knockbackDirection=normalized(minionTarget->position-f.position);
                        minionTarget->knockbackTime=0.3f; minionTarget->wallStunWindow=0.3f;
                        log(k,"HAMMER PUSH",Cue::Hammer,minionTarget->position);
                    }
                }
            } else if(fighterTarget && segmentDistance(fighterTarget->position,a,b)<fighterTarget->radius+4) {
                bool canKnock=fighterTarget->hp>0 && fighterTarget->invulnerable<=0;
                damage(fighterTargetIndex,i.get("damage"),static_cast<int>(k),i.name,i.id=="weapon.hammer"?Cue::Hammer:Cue::Sword);
                if(canKnock) leech();
                f.hitCooldowns[n]=i.get("hit_cooldown");
                if(i.id=="weapon.hammer" && canKnock && fighterTarget->hp>0) {
                    fighterTarget->knockbackDirection=normalized(fighterTarget->position-f.position);
                    fighterTarget->knockbackTime=0.3f; fighterTarget->wallStunWindow=0.3f;
                    log(k,"HAMMER PUSH",Cue::Hammer,fighterTarget->position);
                }
            }
            for(auto& mouse:mice) {
                if(!enemies(static_cast<int>(k),mouse.owner) || mouse.hp<=0 || segmentDistance(mouse.position,a,b)>=mouse.radius+4) continue;
                float amount=i.get("damage")*f.damageMultiplier;
                if(f.hp<f.maxHp*0.2f && hasItem(f,"stat.berserk")) amount*=2;
                mouse.hp-=amount; f.hitCooldowns[n]=i.get("hit_cooldown");
                bursts.push_back({mouse.position,0.3f,0.3f,14,k,""}); log(k,"MOUSE DOWN",Cue::Hit,mouse.position);
                leech();
                break;
            }
        }
    }
    for(size_t first=0;first<match.fighters.size();++first) for(size_t second=first+1;second<match.fighters.size();++second) {
        if(!enemies(static_cast<int>(first),static_cast<int>(second))) continue;
        auto& a=match.fighters[first]; auto& b=match.fighters[second]; size_t contact=first*match.fighters.size()+second;
        if(a.hp<=0 || b.hp<=0 || a.untargetable || b.untargetable || length(a.position-b.position)>a.radius+b.radius+1.5f || contactCooldowns[contact]>0) continue;
        float toFirst=b.harmless?0:b.contactDamage>=0?b.contactDamage:b.effect("spiked_skin")?10.0f:rules.contactDamage;
        float toSecond=a.harmless?0:a.contactDamage>=0?a.contactDamage:a.effect("spiked_skin")?10.0f:rules.contactDamage;
        if(toFirst>0) damage(static_cast<int>(first),toFirst,static_cast<int>(second),b.actorKind=="wild_dog"?"Wild Dog collision":b.effect("spiked_skin")?"Spiked Skin":"Collision",Cue::Hit,false);
        if(toSecond>0) damage(static_cast<int>(second),toSecond,static_cast<int>(first),a.actorKind=="wild_dog"?"Wild Dog collision":a.effect("spiked_skin")?"Spiked Skin":"Collision",Cue::Hit,false);
        contactCooldowns[contact]=0.5f;
    }
    for(size_t projectileIndex=0;projectileIndex<projectiles.size();++projectileIndex) {
        auto& p=projectiles[projectileIndex];
        const bool bouncy=p.kind=="bouncy_ball";
        const bool boomerang=p.kind=="boomerang";
        p.previous=p.position;
        if(boomerang) {
            Vec pull=normalized(match.fighters[p.owner].position-p.position);
            p.velocity=normalized(p.velocity+pull*(p.returnPull*Step))*std::max(1.0f,p.cruiseSpeed);
        }
        p.position=p.position+p.velocity*Step; p.life-=Step;
        bool deflected=false;
        for(size_t defender=0;defender<match.fighters.size() && !deflected;++defender) {
            if(!enemies(p.owner,static_cast<int>(defender)) || match.fighters[defender].hp<=0 || match.fighters[defender].untargetable) continue;
            auto& fighter=match.fighters[defender];
            for(size_t weapon=0;weapon<fighter.items.size();++weapon) {
                const auto& item=fighter.items[weapon];
                if(item.effect!="sword" || fighter.stunTime>0) continue;
                bool shield=item.id=="weapon.orbit_shield",katana=item.id=="weapon.katana";
                if(!shield && !katana) continue;
                if(katana && (mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(projectileIndex*131+defender*17))&1u)) continue;
                auto segment=meleeSegment(fighter,item,fighter.meleeAngles[weapon]);
                if(!segmentsIntersect(p.previous,p.position,segment.base,segment.tip) && std::min(segmentDistance(p.previous,segment.base,segment.tip),segmentDistance(p.position,segment.base,segment.tip))>p.radius+4) continue;
                float speed=length(p.velocity); int previousOwner=p.owner;
                if(shield) {
                    Vec front=normalized(((segment.base+segment.tip)*0.5f)-fighter.position);
                    p.velocity=p.velocity-front*(2*dot(p.velocity,front));
                } else { int target=closestEnemy(static_cast<int>(defender),fighter.position); if(target>=0) p.velocity=normalized(match.fighters[target].position-fighter.position)*speed; }
                p.owner=static_cast<int>(defender);
                if(bouncy) {
                    // It keeps its physically reflected heading; only its team colour and one-hit return target change.
                    p.bouncyDeflected=true; p.bouncyReturnTarget=previousOwner;
                }
                p.position=p.position+normalized(p.velocity)*(p.radius+5); p.previous=p.position;
                bursts.push_back({p.position,0.18f,0.18f,14,static_cast<int>(defender),""});
                log(defender,shield?"SHIELD DEFLECT":"KATANA DEFLECT",Cue::Clash,p.position);
                deflected=true; break;
            }
        }
        if(deflected) continue;
        if(boomerang) {
            auto& owner=match.fighters[p.owner];
            if(segmentDistance(owner.position,p.previous,p.position)<=owner.radius+p.radius) {
                bursts.push_back({owner.position,0.22f,0.22f,18,p.owner,""}); log(p.owner,"BOOMERANG CAUGHT",Cue::Bounce,owner.position); p.life=0;
                continue;
            }
        }
        // A newly thrown ball must make one real ricochet before its owner can catch it.
        // This prevents an owner moving into the launch point from instantly reclaiming it.
        if(bouncy && !p.bouncyDeflected && p.bouncyBounces>0) {
            auto& owner=match.fighters[p.owner];
            if(segmentDistance(owner.position,p.previous,p.position)<=owner.radius+p.radius) {
                bursts.push_back({owner.position,0.22f,0.22f,18,p.owner,""}); log(p.owner,"BOUNCY BALL CAUGHT",Cue::Bounce,owner.position); p.life=0;
                continue;
            }
        }
        auto bounceBouncy=[&](Vec hitPosition,float hitRadius) {
            float speed=std::max(1.0f,length(p.velocity));
            Vec outward=normalized(p.position-hitPosition);
            if(length(p.position-hitPosition)<0.01f) outward=normalized(p.previous-hitPosition);
            p.position=hitPosition+outward*(hitRadius+p.radius+1); p.previous=p.position;
            ++p.bouncyBounces;
            Vec reflected=p.velocity-outward*(2*dot(p.velocity,outward));
            p.velocity=normalized(reflected)*speed;
            log(p.owner,"BOUNCY BALL BOUNCE",Cue::Bounce,p.position);
            bursts.push_back({p.position,0.16f,0.16f,12,p.owner,""});
        };
        auto bounceBoomerang=[&](Vec hitPosition,float hitRadius) {
            Vec outward=normalized(p.position-hitPosition);
            if(length(p.position-hitPosition)<0.01f) outward=normalized(p.previous-hitPosition);
            p.position=hitPosition+outward*(hitRadius+p.radius+1); p.previous=p.position;
            p.velocity=normalized(p.velocity-outward*(2*dot(p.velocity,outward)))*std::max(1.0f,p.cruiseSpeed);
            p.boomerangHit=true; bursts.push_back({p.position,0.16f,0.16f,12,p.owner,""}); log(p.owner,"BOOMERANG TURN",Cue::Bounce,p.position);
        };
        int hitMouse=-1;
        for(size_t mouseIndex=0;mouseIndex<mice.size();++mouseIndex) {
            const auto& mouse=mice[mouseIndex];
            if(!enemies(p.owner,mouse.owner) || mouse.hp<=0) continue;
            if(segmentDistance(mouse.position,p.previous,p.position)<=mouse.radius+p.radius) { hitMouse=static_cast<int>(mouseIndex); break; }
        }
        if(hitMouse>=0) {
            if(boomerang && p.boomerangHit) { p.life=0; continue; }
            auto& mouse=mice[hitMouse]; float amount=p.damage*match.fighters[p.owner].damageMultiplier;
            if(match.fighters[p.owner].hp<match.fighters[p.owner].maxHp*0.2f && hasItem(match.fighters[p.owner],"stat.berserk")) amount*=2;
            mouse.hp-=amount; bursts.push_back({mouse.position,0.3f,0.3f,14,p.owner,""}); log(p.owner,"MOUSE DOWN",Cue::Hit,mouse.position);
            if(bouncy) bounceBouncy(mouse.position,mouse.radius); else if(boomerang && !p.boomerangHit) bounceBoomerang(mouse.position,mouse.radius); else p.life=0;
            continue;
        }
        int hitMinion=-1;
        for(size_t minionIndex=0;minionIndex<minions.size();++minionIndex) {
            const auto& minion=minions[minionIndex];
            if(!enemies(p.owner,minion.owner) || minion.hp<=0) continue;
            if(segmentDistance(minion.position,p.previous,p.position)<=minion.radius+p.radius) { hitMinion=static_cast<int>(minionIndex); break; }
        }
        if(hitMinion>=0) {
            if(boomerang && p.boomerangHit) { p.life=0; continue; }
            auto& minion=minions[hitMinion]; float amount=p.damage*match.fighters[p.owner].damageMultiplier;
            if(match.fighters[p.owner].hp<match.fighters[p.owner].maxHp*0.2f && hasItem(match.fighters[p.owner],"stat.berserk")) amount*=2;
            minion.hp-=amount; bursts.push_back({minion.position,0.35f,0.35f,18,p.owner,""}); log(p.owner,"MINION HIT",Cue::Hit,minion.position);
            if(bouncy) bounceBouncy(minion.position,minion.radius); else if(boomerang && !p.boomerangHit) bounceBoomerang(minion.position,minion.radius); else p.life=0;
            continue;
        }
        int targetIndex=-1; float hitFraction=2;
        for(size_t candidate=0;candidate<match.fighters.size();++candidate) {
            auto& candidateTarget=match.fighters[candidate];
            if(!enemies(p.owner,static_cast<int>(candidate)) || candidateTarget.hp<=0 || candidateTarget.untargetable || (bouncy && p.bouncyDeflected && p.bouncyReturnTarget!=static_cast<int>(candidate))) continue;
            // Sweep in the moving target's frame, so even fast bullets cannot skip a ball.
            Vec a=p.previous-candidateTarget.previous,b=p.position-candidateTarget.position,delta=b-a;
            float radius=candidateTarget.radius+p.radius,fraction=2;
            float aa=dot(delta,delta),bb=2*dot(a,delta),cc=dot(a,a)-radius*radius;
            if(cc<=0) fraction=0;
            else if(aa>0.00001f) { float disc=bb*bb-4*aa*cc; if(disc>=0) { float t=(-bb-std::sqrt(disc))/(2*aa); if(t>=0 && t<=1) fraction=t; } }
            if(fraction<hitFraction) { hitFraction=fraction; targetIndex=static_cast<int>(candidate); }
        }
        float wallFraction=2; Vec wallNormal{};
        const auto& vertices=match.arena.vertices;
        for(size_t n=0;n<vertices.size();++n) {
            Vec edge=vertices[(n+1)%vertices.size()]-vertices[n],normal=normalized(Vec{-edge.y,edge.x});
            float oldDistance=dot(p.previous-vertices[n],normal)-p.radius,newDistance=dot(p.position-vertices[n],normal)-p.radius;
            if(oldDistance<=0 && wallFraction>0) { wallFraction=0; wallNormal=normal; }
            else if(newDistance<=0) { float fraction=oldDistance/(oldDistance-newDistance); if(fraction<wallFraction) { wallFraction=fraction; wallNormal=normal; } }
        }
        if(targetIndex>=0 && hitFraction<=1 && hitFraction<=wallFraction) {
            auto& target=match.fighters[targetIndex];
            std::string label=p.kind=="bow"?"Arrow":p.kind=="pistol"?"Heavy round":p.kind=="poison_dart"?"Poison dart":p.kind=="shuriken"?"Shuriken":bouncy?"Bouncy Ball":boomerang?"Boomerang":"Pellet";
            int dodgeChance=bouncy?60:10;
            bool lucky=target.effect("careful_steps") && (mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(p.owner*193+projectileIndex*31))%100)<static_cast<uint32_t>(dodgeChance);
            if(lucky) {
                log(targetIndex,"LUCKY DODGE!",Cue::Dodge,target.position);
            } else if(p.kind=="poison_dart" && target.hp>0 && target.invulnerable<=0) {
                // Poison darts deliberately ignore weapon damage multipliers: their hit is always exactly 1 HP.
                target.hp=std::max(0.0f,target.hp-1); target.flash=0.15f;
                match.fighters[p.owner].damageDealt+=1;
                bursts.push_back({target.position,0.65f,0.65f,24,targetIndex,"-1"});
                log(p.owner,label+" · 1 damage",Cue::Hit,target.position,1);
                if(target.poisonDps<=0) target.poisonTick=1;
                target.poisonDps=std::min(12.0f,target.poisonDps+1);
                target.poisonOwner=p.owner;
                log(p.owner,"POISONED",Cue::Hit,target.position);
            } else if(!boomerang || !p.boomerangHit) {
                damage(targetIndex,p.damage,p.owner,label);
                if(p.kind=="shuriken" && p.sourceOwner>=0 && p.sourceOwner<static_cast<int>(match.fighters.size()) && p.weaponSlot>=0 && p.weaponSlot<static_cast<int>(match.fighters[p.sourceOwner].cooldowns.size())) {
                    match.fighters[p.sourceOwner].cooldowns[p.weaponSlot]=std::max(0.0f,match.fighters[p.sourceOwner].cooldowns[p.weaponSlot]-1);
                }
            }
            if(bouncy && p.bouncyDeflected && p.bouncyReturnTarget==targetIndex) {
                // A deflected ball may only hurt its former holder once, when it naturally reaches them.
                p.life=0;
            } else if(bouncy) bounceBouncy(target.position,target.radius);
            else if(boomerang) { if(p.boomerangHit) p.life=0; else bounceBoomerang(target.position,target.radius); }
            else p.life=0;
        }
        else if(wallFraction<=1) {
            Vec impact=p.previous+(p.position-p.previous)*wallFraction;
            if(bouncy) {
                float speed=std::max(1.0f,length(p.velocity));
                p.position=impact+wallNormal*(p.radius+1); p.previous=p.position; ++p.bouncyBounces;
                Vec reflected=p.velocity-wallNormal*(2*dot(p.velocity,wallNormal));
                p.velocity=normalized(reflected)*speed;
                log(p.owner,"BOUNCY BALL BOUNCE",Cue::Bounce,p.position);
                bursts.push_back({impact,0.2f,0.2f,10,p.owner,""});
            } else if(boomerang) {
                p.position=impact+wallNormal*(p.radius+1); p.previous=p.position;
                p.velocity=normalized(match.fighters[p.owner].position-p.position)*std::max(1.0f,p.cruiseSpeed);
                bursts.push_back({impact,0.2f,0.2f,10,p.owner,""}); log(p.owner,"BOOMERANG WALL BOUNCE",Cue::Bounce,p.position);
            } else { p.life=0; bursts.push_back({impact,0.2f,0.2f,8,p.owner,""}); }
        }
    }
    std::erase_if(projectiles,[](const Projectile& p){return p.life<=0;});
    for(auto& h:hazards) {
        h.life-=Step; h.arm-=Step; h.hitCooldown-=Step;
        if(h.life<=0 || h.arm>0 || h.hitCooldown>0) continue;
        if(h.mine) {
            auto touchesTrigger=[&](Vec previous,Vec position,float radius) { return segmentDistance(h.position,previous,position)<=h.radius+radius; };
            bool triggered=false;
            for(size_t target=0;target<match.fighters.size();++target) if(enemies(h.owner,static_cast<int>(target)) && match.fighters[target].hp>0 && !match.fighters[target].untargetable && touchesTrigger(match.fighters[target].previous,match.fighters[target].position,match.fighters[target].radius)) triggered=true;
            for(const auto& minion:minions) if(enemies(h.owner,minion.owner) && minion.hp>0 && touchesTrigger(minion.previous,minion.position,minion.radius)) triggered=true;
            for(const auto& mouse:mice) if(enemies(h.owner,mouse.owner) && mouse.hp>0 && touchesTrigger(mouse.previous,mouse.position,mouse.radius)) triggered=true;
            if(!triggered) continue;
            for(size_t targetIndex=0;targetIndex<match.fighters.size();++targetIndex) if(enemies(h.owner,static_cast<int>(targetIndex))) {
                auto& target=match.fighters[targetIndex]; if(target.hp<=0 || target.untargetable || length(target.position-h.position)>h.blastRadius+target.radius) continue;
                bool careful=target.effect("careful_steps") && (mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(h.owner*97+targetIndex*193))%100)<60;
                if(careful) log(static_cast<int>(targetIndex),"CAREFUL STEPS",Cue::Dodge,target.position);
                else damage(static_cast<int>(targetIndex),h.damage,h.owner,"Mine explosion");
            }
            for(auto& minion:minions) if(enemies(h.owner,minion.owner) && minion.hp>0 && length(minion.position-h.position)<=h.blastRadius+minion.radius) {
                minion.hp-=h.damage; bursts.push_back({minion.position,0.35f,0.35f,18,h.owner,""});
            }
            for(auto& mouse:mice) if(enemies(h.owner,mouse.owner) && mouse.hp>0 && length(mouse.position-h.position)<=h.blastRadius+mouse.radius) mouse.hp-=h.damage;
            log(h.owner,"BOOM!",Cue::Explosion,h.position);
            bursts.push_back({h.position,0.45f,0.45f,h.blastRadius,h.owner,""}); h.life=0;
        } else for(size_t targetIndex=0;targetIndex<match.fighters.size();++targetIndex) if(enemies(h.owner,static_cast<int>(targetIndex))) {
                auto& target=match.fighters[targetIndex]; if(target.hp<=0 || target.untargetable || segmentDistance(h.position,target.previous,target.position)>h.radius+target.radius) continue;
                bool careful=target.effect("careful_steps") && (mix32(match.seed^static_cast<uint32_t>(time/Step)^static_cast<uint32_t>(h.owner*97))%100)<60;
                if(careful) log(static_cast<int>(targetIndex),"CAREFUL STEPS",Cue::Dodge,target.position);
                else damage(static_cast<int>(targetIndex),h.damage,h.owner,"Wall spike");
                const auto* item=match.fighters[h.owner].effect("spikes"); h.hitCooldown=item?item->get("hit_cooldown"):0.8f;
        }
    }
    std::erase_if(hazards,[](const Hazard& h){return h.life<=0;});
    std::erase_if(mice,[](const SummonedMouse& mouse){ return mouse.hp<=0; });
    std::erase_if(minions,[](const Minion& minion){ return minion.hp<=0; });
    // Poison lasts for the round and damages in visible one-second ticks.
    for(int k=0;k<static_cast<int>(match.fighters.size());++k) {
        auto& f=match.fighters[k]; if(f.poisonDps<=0 || f.hp<=0) continue;
        f.poisonTick-=Step;
        if(f.poisonTick<=0) {
            int source=f.poisonOwner; bool critical=criticalHit(source);
            float amount=std::min(f.hp,f.poisonDps*(critical?2:1)); f.hp-=amount; f.flash=0.1f; f.poisonTick+=1;
            if(source>=0 && source<static_cast<int>(match.fighters.size())) match.fighters[source].damageDealt+=amount;
            bursts.push_back({f.position,0.45f,0.45f,20,static_cast<int>(k),""});
            log(source,critical?"POISON CRIT":"POISON",Cue::Hit,f.position,amount,critical);
        }
    }
    if(rules.suddenDps>0 && time>=rules.sudden) {
        if(time-Step<rules.sudden) log(-1,"OVERTIME · both fighters are losing health",Cue::Overtime);
        float loss=rules.suddenDps*Step;
        for(auto& f:match.fighters) if(!f.untargetable) f.hp=std::max(0.0f,f.hp-loss);
    }
    checkEnd();
}
void Simulation::checkEnd() {
    if(match.challenge && match.objectiveType=="survive") {
        bool alive=std::ranges::any_of(match.fighters,[](const Fighter& fighter){ return fighter.team==0 && fighter.hp>0; });
        if(!alive) winner=1;
        else if(time>=rules.limit) winner=0;
        else return;
    } else if(match.challenge && match.objectiveType=="deal_damage") {
        float dealt=0; for(const auto& fighter:match.fighters) if(fighter.team==0) dealt+=fighter.damageDealt;
        if(dealt>=match.objectiveTarget) winner=0;
        else if(time>=rules.limit || std::ranges::none_of(match.fighters,[](const Fighter& fighter){ return fighter.team==0 && fighter.hp>0; })) winner=1;
        else return;
    } else {
        bool alive0=std::ranges::any_of(match.fighters,[](const Fighter& fighter){ return fighter.team==0 && fighter.hp>0; });
        bool alive1=std::ranges::any_of(match.fighters,[](const Fighter& fighter){ return fighter.team==1 && fighter.hp>0; });
        if(!alive0 || !alive1) winner=!alive0&&!alive1?-1:alive0?0:1;
        else if(time>=rules.limit) winner=match.challenge?1:-1;
        else return;
    }
    finished=true; log(winner,winner<0?"DRAW":winner==0&&match.challenge?"CHALLENGE COMPLETE":"TEAM "+std::to_string(winner+1)+" WINS");
}
bool Wallet::place(int side,int amount) {
    if(active || side<0 || side>1 || amount<1 || amount>coins) return false;
    selection=side; stake=amount; coins-=amount; active=true; return true;
}
int Wallet::settle(int result,float multiplier) {
    if(!active || result<-1 || result>1) return 0;
    int returned=0;
    if(result==-1) { returned=stake; ++draws; }
    else if(result==selection) { returned=payoutFor(stake,multiplier); ++wins; }
    else ++losses;
    coins=static_cast<int>(std::min(1000000000LL,static_cast<long long>(coins)+returned));
    ++rounds; active=false; return returned;
}
int Wallet::nextCollectionSlotCost() const {
    if(collectionSlots<0 || collectionSlots>=12) return 0;
    return 1000*(1<<collectionSlots);
}
bool Wallet::unlockCollectionSlot() {
    int cost=nextCollectionSlotCost();
    if(cost<=0 || coins<cost) return false;
    coins-=cost; ++collectionSlots; return true;
}
bool Wallet::buyMarketBall(size_t index,int price) {
    if(index>=marketBallSeeds.size() || marketBallSeeds[index]==0 || price<1 || coins<price || ownedBallSeeds.size()>=static_cast<size_t>(collectionSlots)) return false;
    coins-=price; ownedBallSeeds.push_back(marketBallSeeds[index]); marketBallSeeds[index]=0; return true;
}
bool Wallet::refreshMarket(const std::array<uint32_t,3>& seeds) {
    if(coins<1000 || std::ranges::any_of(seeds,[](uint32_t seed){return seed==0;})) return false;
    coins-=1000; marketBallSeeds=seeds; marketInitialized=true; return true;
}
bool Wallet::removeOwnedBall(size_t index) {
    if(index>=ownedBallSeeds.size()) return false;
    ownedBallSeeds.erase(ownedBallSeeds.begin()+static_cast<std::ptrdiff_t>(index)); return true;
}
bool Wallet::challengeUnlocked(const std::string& id) const { return std::find(unlockedChallenges.begin(),unlockedChallenges.end(),id)!=unlockedChallenges.end(); }
bool Wallet::challengeCompleted(const std::string& id) const { return std::find(completedChallenges.begin(),completedChallenges.end(),id)!=completedChallenges.end(); }
bool Wallet::unlockChallenge(const ChallengeDefinition& challenge) {
    if(challengeUnlocked(challenge.id) || coins<challenge.unlockCost) return false;
    if(std::ranges::any_of(challenge.prerequisites,[&](const std::string& id){return !challengeCompleted(id);})) return false;
    coins-=challenge.unlockCost; unlockedChallenges.push_back(challenge.id); return true;
}
bool Wallet::completeChallenge(const std::string& id) {
    if(!challengeUnlocked(id) || challengeCompleted(id)) return false;
    completedChallenges.push_back(id); return true;
}
void Wallet::resetSeason(int initialCoins) {
    wins=losses=draws=rounds=0; active=false; stake=0; selection=-1; coins=initialCoins;
}
void Wallet::save(const std::filesystem::path& path) const {
    json j={{"version",1},{"coins",coins},{"wins",wins},{"losses",losses},{"draws",draws},{"rounds",rounds},{"active",active},{"stake",stake},{"selection",selection},
        {"collection_slots",collectionSlots},{"owned_ball_seeds",ownedBallSeeds},{"market_ball_seeds",marketBallSeeds},{"market_initialized",marketInitialized},
        {"unlocked_challenges",unlockedChallenges},{"completed_challenges",completedChallenges}};
    auto temporary=path; temporary+=".tmp";
    { std::ofstream s(temporary); require(s.good(),"Cannot save the wallet."); s<<j.dump(2); s.flush(); require(s.good(),"Wallet save failed."); }
    auto backup=path; backup+=".bak";
    std::error_code ec; bool existed=std::filesystem::exists(path);
    if(existed) { std::filesystem::remove(backup,ec); std::filesystem::rename(path,backup); }
    try { std::filesystem::rename(temporary,path); }
    catch(...) { if(existed) std::filesystem::rename(backup,path,ec); throw; }
    if(existed) std::filesystem::remove(backup,ec);
}
Wallet Wallet::load(const std::filesystem::path& path,int initial) {
    Wallet w; w.coins=initial; auto source=path; auto backup=path; backup+=".bak";
    if(!std::filesystem::exists(source)) { if(std::filesystem::exists(backup)) source=backup; else return w; }
    std::ifstream s(source); json j; s>>j;
    require(j.at("version")==1,"Unsupported save version.");
    w.coins=j.at("coins"); w.wins=j.at("wins"); w.losses=j.at("losses"); w.draws=j.at("draws"); w.rounds=j.at("rounds");
    require(w.coins>=0 && w.coins<=1000000000 && w.wins>=0 && w.losses>=0 && w.draws>=0 && w.rounds>=0,"Invalid wallet save.");
    w.collectionSlots=j.value("collection_slots",0); w.ownedBallSeeds=j.value("owned_ball_seeds",std::vector<uint32_t>{});
    if(j.contains("market_ball_seeds")) {
        auto seeds=j.at("market_ball_seeds").get<std::vector<uint32_t>>(); require(seeds.size()==3,"Invalid market save.");
        std::copy(seeds.begin(),seeds.end(),w.marketBallSeeds.begin());
    }
    w.marketInitialized=j.value("market_initialized",j.contains("market_ball_seeds"));
    w.unlockedChallenges=j.value("unlocked_challenges",std::vector<std::string>{}); w.completedChallenges=j.value("completed_challenges",std::vector<std::string>{});
    require(w.collectionSlots>=0 && w.collectionSlots<=12 && w.ownedBallSeeds.size()<=static_cast<size_t>(w.collectionSlots),"Invalid collection save.");
    require(std::ranges::none_of(w.ownedBallSeeds,[](uint32_t seed){return seed==0;}),"Invalid owned ball seed.");
    auto validProgress=[](const std::vector<std::string>& values) { std::set<std::string> unique; for(const auto& value:values) if(value.empty() || value.size()>80 || !unique.insert(value).second) return false; return values.size()<=512; };
    require(validProgress(w.unlockedChallenges) && validProgress(w.completedChallenges),"Invalid challenge progress save.");
    require(std::ranges::all_of(w.completedChallenges,[&](const std::string& id){return w.challengeUnlocked(id);}),"Completed challenge is not unlocked.");
    // A committed bet stays spent if the application is closed during a fight.
    if(j.at("active").get<bool>()) { ++w.losses; ++w.rounds; }
    return w;
}
} // namespace orbital
