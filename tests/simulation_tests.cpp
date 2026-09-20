#include "game.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace orbital;
static int checks=0;
static void check(bool ok,const std::string& message) { ++checks; if(!ok) throw std::runtime_error(message); }
static void run(Simulation& sim,int batch=1) {
    for(int frame=0;frame<50000 && !sim.finished;++frame) for(int k=0;k<batch && !sim.finished;++k) sim.step();
    check(sim.finished,"Simulation did not terminate");
}
static const Item& findItem(const Catalog& c,const std::string& id) {
    auto it=std::find_if(c.items.begin(),c.items.end(),[&](const Item& i){return i.id==id;});
    if(it==c.items.end()) throw std::runtime_error("Missing fixture item");
    return *it;
}
static Match fixture(const Catalog& c) {
    Match m=generateMatch(c,123);
    m.arena=c.arenas[0];
    for(int k=0;k<2;++k) {
        Fighter f; f.name=k==0?"A":"B"; f.position={k==0?-100.0f:100.0f,0}; f.previous=f.position;
        f.speed=0; f.velocity={0,0}; f.hp=f.maxHp=100; m.fighters[k]=f;
    }
    return m;
}
static void equip(Fighter& f,const Item& i) { f.items.push_back(i); f.cooldowns.push_back(0); f.hitCooldowns.push_back(0); }
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected catalog path"); Catalog c=Catalog::load(argv[1]);
        check(c.rules.limit==180 && c.rules.suddenDps==0,"Fights should draw after three minutes with no overtime damage");
        check(findItem(c,"weapon.pistol").get("damage")==40,"Heavy Pistol damage should be 40");
        check(findItem(c,"weapon.mines").get("damage")==20,"Minelayer damage should be nerfed to 20");
        check(findItem(c,"weapon.mines").get("trigger_radius")==16 && findItem(c,"weapon.mines").get("blast_radius")==38,"Mine size should be half of its original size");
        const auto& flute=findItem(c,"weapon.flute");
        check(flute.points==36 && flute.get("cooldown")==8 && flute.get("count")==5 && flute.get("mouse_hp")==1 && flute.get("damage")==5,"Pied Piper Flute balance values are incorrect");
        const auto& minionAbility=findItem(c,"ability.minion");
        check(minionAbility.points==36 && minionAbility.get("hp")==40 && minionAbility.get("radius")==11,"Pocket Minion balance values are incorrect");
        const auto& tapeAbility=findItem(c,"ability.slowing_tape");
        check(tapeAbility.points==20 && tapeAbility.get("chance")==0.15f && tapeAbility.get("duration")==3 && tapeAbility.get("slow_multiplier")==0.2f,"Slowing Tape balance values are incorrect");
        const auto& damageField=findItem(c,"ability.damage_field");
        check(damageField.category=="ability" && damageField.effect=="damage_field" && damageField.points==15,"Damage Field balance values are incorrect");
        const auto& critStriker=findItem(c,"stat.crit_striker");
        check(critStriker.category=="modifier" && critStriker.effect=="crit" && critStriker.points==20 && critStriker.get("chance")==0.2f,"Crit Striker balance values are incorrect");
        check(findItem(c,"stat.fast_swings").category=="modifier" && findItem(c,"stat.fast_swings").group=="swing_speed" && findItem(c,"ability.careful_steps").name=="Lucky" && findItem(c,"ability.careful_steps").points==15 && findItem(c,"stat.berserk").category=="modifier","Lucky, Faster Swings, or Berserk are missing");
        check(findItem(c,"weapon.sword").get("damage")==17 && findItem(c,"weapon.sword").meleeClass=="medium","Orbital Sword should be medium and deal 17 damage");
        check(findItem(c,"weapon.spear").meleeClass=="light","Spear should be a light melee weapon");
        const auto& hammer=findItem(c,"weapon.hammer");
        check(hammer.points==37 && hammer.meleeClass=="heavy" && hammer.get("damage")==25 && hammer.get("reach")==50 && hammer.get("rotation_speed")==1.5f && hammer.get("hit_cooldown")==1,"Hammer balance values are incorrect");
        check(findItem(c,"weapon.dagger").meleeClass=="light" && findItem(c,"weapon.dagger").get("rotation_speed")>7,"Dagger is missing its fast light-melee setup");
        check(findItem(c,"weapon.orbit_shield").meleeClass=="heavy" && findItem(c,"weapon.orbit_shield").points==26 && findItem(c,"weapon.katana").get("damage")==findItem(c,"weapon.spear").get("damage"),"Shield or Katana balance setup is incorrect");
        const auto& scythe=findItem(c,"weapon.scythe");
        check(scythe.meleeClass=="medium" && scythe.points==40 && scythe.get("damage")==15 && scythe.get("reach")==findItem(c,"weapon.spear").get("reach") && scythe.get("rotation_speed")==findItem(c,"weapon.katana").get("rotation_speed") && scythe.get("lifesteal")==10,"Orbit Scythe balance values are incorrect");
        const auto& bouncyBall=findItem(c,"weapon.bouncy_ball");
        check(bouncyBall.points==18 && bouncyBall.get("damage")==7 && bouncyBall.get("cooldown")==3 && bouncyBall.get("projectile_speed")==650 && bouncyBall.get("projectile_radius")==6,"Bouncy Ball balance values are incorrect");
        const auto& boomerang=findItem(c,"weapon.boomerang");
        check(boomerang.points==22 && boomerang.get("damage")==14 && boomerang.get("cooldown")==4 && boomerang.get("projectile_speed")==360 && boomerang.get("return_pull")==260,"Boomerang balance values are incorrect");
        const auto& spikedSkin=findItem(c,"ability.spiked_skin");
        check(spikedSkin.category=="ability" && spikedSkin.effect=="spiked_skin" && spikedSkin.points==10 && spikedSkin.description.find("10 damage")!=std::string::npos,"Spiked Skin balance values are incorrect");
        const auto& slowSwings=findItem(c,"stat.slow_swings"),&slowReload=findItem(c,"stat.slow_reload"),&fastReload=findItem(c,"stat.fast_reload"),&badAim=findItem(c,"stat.bad_aim"),&comboMaster=findItem(c,"stat.combo_master");
        check(slowSwings.points==-10 && slowReload.points==-9 && fastReload.points==14 && badAim.points==-8 && comboMaster.points==23,"New modifier balance values are incorrect");
        bool rolledDuplicateWeapons=false;
        for(uint32_t seed=0;seed<300 && !rolledDuplicateWeapons;++seed) for(const auto& fighter:generateMatch(c,seed).fighters) {
            rolledDuplicateWeapons=fighter.items[0].id==fighter.items[1].id;
        }
        check(rolledDuplicateWeapons,"Weapon rolls should allow two copies of the same weapon");
        {
            for(uint32_t seed=0;seed<1000;++seed) for(const auto& fighter:generateMatch(c,seed).fighters) {
                auto has=[&](const std::string& id) { return std::any_of(fighter.items.begin(),fighter.items.end(),[&](const Item& item) { return item.id==id; }); };
                check(!(has("stat.fast_swings") && has("stat.slow_swings")),"Faster Swings and Slow Swings were rolled together");
                check(!(has("stat.fast_reload") && has("stat.slow_reload")),"Fast Reload and Slow Reload were rolled together");
            }
        }
        {
            std::set<std::string> names;
            for(uint32_t seed=0;seed<350;++seed) for(const auto& fighter:generateMatch(c,seed).fighters) {
                names.insert(fighter.name);
                check(!fighter.title.empty() && fighter.title!="the Unknown","A fighter did not receive a title from its loadout");
            }
            check(names.size()==30,"The complete 30-name fighter pool was not used");
        }
        {
            int specialArenas=0; std::set<std::string> effects;
            for(uint32_t seed=0;seed<1000;++seed) {
                auto match=generateMatch(c,seed);
                if(!match.arena.effect.empty()) { ++specialArenas; effects.insert(match.arena.effect); }
            }
            check(specialArenas>200 && specialArenas<300 && effects.size()==3,"Arena effects should roll near 25% and include all three variants");
        }
        {
            std::set<int> eyes,mouths,hats,beards; int bareHeads=0;
            for(uint32_t seed=0;seed<1000;++seed) {
                auto a=appearanceFor(seed,0),b=appearanceFor(seed,1);
                check(a==appearanceFor(seed,0) && b==appearanceFor(seed,1),"Cosmetics changed between preview and fight");
                check(a.eyes>=0 && a.eyes<6 && a.mouth>=0 && a.mouth<6 && a.hat>=0 && a.hat<11 && a.beard>=0 && a.beard<6,"Invalid cosmetic variant");
                eyes.insert(a.eyes); mouths.insert(a.mouth); hats.insert(a.hat); beards.insert(a.beard); if(a.hat==0) ++bareHeads;
            }
            check(eyes.size()==6 && mouths.size()==6 && hats.size()==11 && beards.size()==6,"Missing appearance variants");
            check(bareHeads>330 && bareHeads<470,"No-hat option should be common");
            Countdown countdown; countdown.start();
            check(countdown.number()==3 && countdown.active(),"Missing countdown 3");
            countdown.advance(1); check(countdown.number()==2,"Missing countdown 2");
            countdown.advance(1); check(countdown.number()==1,"Missing countdown 1");
            countdown.advance(1); check(countdown.number()==0 && countdown.active(),"Missing FIGHT phase");
            countdown.advance(0.7f); check(!countdown.active(),"Countdown did not finish");
            countdown.start(); check(countdown.elapsed==0 && countdown.number()==3,"Countdown did not reset for next fight");
            countdown.reset(); check(!countdown.active(),"Reroll left countdown active");
            for(int n=1;n<static_cast<int>(Cue::Count);++n) {
                auto samples=synthesize(static_cast<Cue>(n)); double energy=0;
                check(samples.size()>1000 && samples.size()<44100,"Invalid effect duration");
                check(samples.front()==0 && samples.back()==0,"Sound has abrupt endpoints");
                for(float sample:samples) { check(std::isfinite(sample) && std::abs(sample)<=0.6f,"Invalid/clipping audio sample"); energy+=sample*sample; }
                check(energy/samples.size()>0.0001,"Silent audio effect");
            }
        }
        Wallet wallet; check(!wallet.place(-1,20),"Invalid side accepted"); check(!wallet.place(0,0),"Zero accepted");
        check(!wallet.place(0,1001),"Overdraft accepted"); check(wallet.place(1,100),"Valid bet rejected");
        check(wallet.coins==900,"Debit incorrect"); check(!wallet.place(0,20),"Double bet accepted");
        check(wallet.settle(1,1.9f)==190 && wallet.coins==1090,"Payout failed");
        // Decimal multipliers must pay their displayed amount, not float roundoff.
        Wallet payout; payout.place(0,100); check(payout.settle(0,1.9f)==190,"1.90x must return 190 for stake 100");
        int coins=payout.coins; payout.settle(0,1.9f); check(payout.coins==coins,"Double settlement");
        payout.place(1,50); check(payout.settle(-1,1.9f)==50,"Draw did not refund");
        payout.place(1,50); check(payout.settle(0,1.9f)==0,"Loss paid out");
        Wallet collection; collection.coins=10000000;
        for(int slot=0;slot<12;++slot) {
            check(collection.nextCollectionSlotCost()==1000*(1<<slot),"Collection slot price does not double");
            check(collection.unlockCollectionSlot(),"Collection slot could not be unlocked");
        }
        check(collection.nextCollectionSlotCost()==0 && !collection.unlockCollectionSlot(),"Collection exceeded 12 slots");
        collection.marketBallSeeds={101,202,303}; collection.marketInitialized=true;
        int ballCost=marketPrice(generateBall(c,101)); int beforePurchase=collection.coins;
        check(collection.buyMarketBall(0,ballCost) && collection.coins==beforePurchase-ballCost && collection.ownedBallSeeds==std::vector<uint32_t>{101} && collection.marketBallSeeds[0]==0,"Market purchase failed");
        check(collection.removeOwnedBall(0) && collection.ownedBallSeeds.empty(),"Owned ball removal failed");
        int beforeRefresh=collection.coins; check(collection.refreshMarket({404,505,606}) && collection.coins==beforeRefresh-1000 && collection.marketBallSeeds[1]==505,"Market refresh failed");
        auto ownedBeforeSeason=collection.ownedBallSeeds; auto marketBeforeSeason=collection.marketBallSeeds; int slotsBeforeSeason=collection.collectionSlots;
        collection.resetSeason(1000); check(collection.coins==1000 && collection.wins==0 && collection.collectionSlots==slotsBeforeSeason && collection.ownedBallSeeds==ownedBeforeSeason && collection.marketBallSeeds==marketBeforeSeason,"New season erased collection progress");
        auto temp=std::filesystem::temp_directory_path()/"orbital-odds-test-save.json";
        collection.save(temp); auto restored=Wallet::load(temp,1000); check(restored.coins==collection.coins && restored.collectionSlots==12 && restored.marketBallSeeds[2]==606,"Collection save roundtrip failed");
        payout.save(temp); restored=Wallet::load(temp,1000); check(restored.coins==payout.coins && restored.rounds==3,"Save roundtrip failed");
        payout.place(0,25); payout.save(temp); restored=Wallet::load(temp,1000);
        check(restored.coins==payout.coins && !restored.active && restored.losses==payout.losses+1,"Interrupted bet refunded");
        std::filesystem::remove(temp);
        // Relative swept collision: a very fast bullet hits even with both endpoints outside the target.
        {
            Match m=fixture(c); Simulation s(m,c.rules);
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,35,2,1,"pistol"}); s.step();
            check(s.match.fighters[1].hp==65,"Fast projectile tunneled through target");
            check(s.projectiles.empty(),"Hit projectile not removed");
        }
        {
            Match m=fixture(c); m.fighters[1].shield=20; m.fighters[1].shieldTime=5; Simulation s(m,c.rules);
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,35,2,1,"pistol"}); s.step();
            check(s.match.fighters[1].hp==85 && s.match.fighters[1].shield==0,"Shield absorption incorrect");
        }
        {
            Match m=fixture(c); m.fighters[0].position=m.fighters[0].previous={0,0}; m.fighters[1].position=m.fighters[1].previous={0,0};
            equip(m.fighters[0],spikedSkin); Simulation s(m,c.rules); s.step();
            check(s.match.fighters[0].hp==98 && s.match.fighters[1].hp==90,"Spiked Skin did not replace direct contact damage with 10 damage");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],findItem(c,"weapon.mines")); m.fighters[1].position={-68,0};
            Rules rules=c.rules; rules.contactDamage=0; Simulation s(m,rules); for(int n=0;n<150;++n) s.step();
            check(s.match.fighters[1].hp<100,"Mine did not explode"); check(s.match.fighters[0].hp==100,"Mine damaged owner");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],flute); Rules rules=c.rules; rules.contactDamage=0;
            Simulation s(m,rules); s.step();
            check(s.mice.size()==5,"Flute did not summon five mice");
            for(const auto& mouse:s.mice) check(mouse.owner==0 && mouse.hp==1 && std::abs(length(mouse.velocity)-165)<0.01f && mouse.position.x<m.fighters[0].position.x,"Mouse spawn values or behind-summoner position are incorrect");
            for(size_t n=1;n<s.mice.size();++n) s.mice[n].hp=0;
            s.mice[0].position=s.match.fighters[0].position; s.mice[0].previous=s.mice[0].position; s.mice[0].velocity={0,0}; s.step();
            check(s.match.fighters[0].hp==100,"Mouse damaged its summoner");
            s.mice[0].position=s.match.fighters[1].position; s.mice[0].previous=s.mice[0].position; s.step();
            check(s.match.fighters[1].hp==95,"Mouse did not bite the opposing fighter for 5 damage");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],minionAbility); m.fighters[0].minionWeapon=findItem(c,"weapon.bow");
            Simulation s(m,c.rules); check(s.minions.size()==1,"Pocket Minion was not created");
            check(s.minions[0].owner==0 && s.minions[0].hp==40 && s.minions[0].radius==11 && s.minions[0].weapon.id=="weapon.bow","Pocket Minion did not keep its configured health, size, or rolled weapon");
            Vec before=s.minions[0].position; s.step();
            check(length(s.minions[0].position-before)>1,"Pocket Minion did not move with its independent bouncing velocity");
            Vec p=s.minions[0].position; s.projectiles.push_back({p,p,{0,0},1,50,2,1,"pistol"}); s.step();
            check(s.minions.empty(),"Enemy projectile did not destroy the 40-HP Pocket Minion");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],findItem(c,"weapon.bow")); equip(m.fighters[1],minionAbility);
            m.fighters[1].minionWeapon=findItem(c,"weapon.dagger"); Rules rules=c.rules; rules.contactDamage=0;
            Simulation s(m,rules); s.minions[0].position={-20,0}; s.minions[0].previous=s.minions[0].position; s.minions[0].velocity={0,0};
            for(int n=0;n<60;++n) s.step();
            check(s.minions[0].hp<40 && s.match.fighters[1].hp==100,"A closer enemy minion was not targeted before its owner");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],flute); equip(m.fighters[1],minionAbility); m.fighters[1].minionWeapon=findItem(c,"weapon.dagger");
            Rules rules=c.rules; rules.contactDamage=0; Simulation s(m,rules); s.step();
            for(size_t n=1;n<s.mice.size();++n) s.mice[n].hp=0;
            s.mice[0].position=s.minions[0].position; s.mice[0].previous=s.mice[0].position; s.mice[0].velocity={0,0}; s.step();
            check(s.minions[0].hp==35 && s.mice.empty(),"A mouse and an enemy minion did not damage each other on contact");
        }
        {
            bool avoided=false,hit=false;
            for(uint32_t seed=0;seed<160 && (!avoided || !hit);++seed) {
                Match m=fixture(c); m.seed=seed; equip(m.fighters[1],findItem(c,"ability.careful_steps")); Simulation s(m,c.rules);
                s.projectiles.push_back({{60,0},{60,0},{15000,0},0,20,2,1,"pistol"}); s.step();
                avoided|=s.match.fighters[1].hp==100; hit|=s.match.fighters[1].hp<100;
            }
            check(avoided && hit,"Lucky did not dodge roughly some, but not all, ranged attacks");
        }
        {
            bool avoided=false,hit=false;
            for(uint32_t seed=0;seed<160 && (!avoided || !hit);++seed) {
                Match m=fixture(c); m.seed=seed; equip(m.fighters[1],findItem(c,"ability.careful_steps")); Simulation s(m,c.rules);
                s.projectiles.push_back({{60,0},{60,0},{15000,0},0,7,6,1,"bouncy_ball"}); s.step();
                avoided|=s.match.fighters[1].hp==100; hit|=s.match.fighters[1].hp<100;
            }
            check(avoided && hit,"Lucky did not avoid roughly some, but not all, Bouncy Ball hits");
        }
        {
            Match m=fixture(c); Simulation s(m,c.rules);
            s.projectiles.push_back({m.fighters[0].position,m.fighters[0].position,{0,0},0,7,6,1,"bouncy_ball",-1,0});
            s.step(); check(s.projectiles.size()==1,"A Bouncy Ball was caught before making its first ricochet");
        }
        {
            Match m=fixture(c); m.fighters[0].position={200,0}; m.fighters[0].previous=m.fighters[0].position; m.fighters[0].angle=0;
            m.fighters[1].position={100,0}; m.fighters[1].previous=m.fighters[1].position;
            equip(m.fighters[0],findItem(c,"weapon.hammer")); equip(m.fighters[1],minionAbility); m.fighters[1].minionWeapon=findItem(c,"weapon.dagger");
            Simulation s(m,c.rules); s.match.fighters[0].meleeAngles[0]=0; s.minions[0].position={250,0}; s.minions[0].previous=s.minions[0].position; s.minions[0].velocity={0,0};
            for(int n=0;n<45 && s.minions[0].stunTime<=0;++n) s.step();
            check(s.minions[0].stunTime>0.9f,"Hammer push into a wall did not stun the Pocket Minion");
        }
        {
            Match m=fixture(c); m.fighters[0].angle=0; equip(m.fighters[0],findItem(c,"weapon.sword")); m.fighters[1].position={-45,0};
            Simulation s(m,c.rules); s.step(); check(s.match.fighters[1].hp==83,"Sword did not hit");
            s.step(); check(s.match.fighters[1].hp==83,"Sword ignored hit cooldown");
        }
        {
            Match m=fixture(c); m.fighters[0].position={0,0}; m.fighters[0].previous=m.fighters[0].position; m.fighters[0].angle=0; m.fighters[0].hp=50;
            m.fighters[1].position={60,0}; m.fighters[1].previous=m.fighters[1].position; equip(m.fighters[0],scythe);
            Simulation s(m,c.rules); s.match.fighters[0].meleeAngles[0]=0; s.step();
            check(s.match.fighters[1].hp==85 && s.match.fighters[0].hp==60,"Orbit Scythe did not deal 15 damage and restore 10 HP");
        }
        {
            Match m=fixture(c); m.fighters[0].angle=0; equip(m.fighters[0],findItem(c,"weapon.sword")); equip(m.fighters[0],findItem(c,"stat.fast_swings"));
            Simulation s(m,c.rules); s.step(); check(s.match.fighters[0].meleeAngles[0]>0.055f,"Faster Swings did not double melee rotation speed");
        }
        {
            Match m=fixture(c); m.fighters[0].angle=0; equip(m.fighters[0],findItem(c,"weapon.sword")); equip(m.fighters[0],slowSwings);
            Simulation s(m,c.rules); s.step(); check(s.match.fighters[0].meleeAngles[0]>0.019f && s.match.fighters[0].meleeAngles[0]<0.022f,"Slow Swings did not reduce melee rotation by 30%");
        }
        {
            Match fast=fixture(c); equip(fast.fighters[0],findItem(c,"weapon.bow")); equip(fast.fighters[0],fastReload); Simulation quick(fast,c.rules); quick.step();
            Match slow=fixture(c); equip(slow.fighters[0],findItem(c,"weapon.bow")); equip(slow.fighters[0],slowReload); Simulation delayed(slow,c.rules); delayed.step();
            check(quick.match.fighters[0].cooldowns[0]>0.9f && quick.match.fighters[0].cooldowns[0]<1.0f && delayed.match.fighters[0].cooldowns[0]>2.4f,"Fast Reload did not reduce reloads by 35%, or Slow Reload did not add one second");
        }
        {
            bool skewed=false;
            for(uint32_t seed=0;seed<32 && !skewed;++seed) {
                Match m=fixture(c); m.seed=seed; equip(m.fighters[0],findItem(c,"weapon.bow")); equip(m.fighters[0],badAim); Simulation s(m,c.rules); s.step();
                skewed=!s.projectiles.empty() && std::abs(s.projectiles.front().velocity.y)>0.1f;
            }
            check(skewed,"Bad Aim did not select a random -5 to +5 degree shot offset");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],comboMaster); Simulation s(m,c.rules);
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,10,2,1,"pistol"}); s.step();
            check(s.match.fighters[1].hp==90 && s.match.fighters[0].comboWindow>2.4f,"Combo Master did not start its 2.5-second timer after a hit");
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,10,2,1,"pistol"}); s.step();
            check(s.match.fighters[1].hp==77 && s.match.fighters[0].comboWindow>2.4f,"Combo Master did not strengthen a follow-up hit by 30%");
        }
        {
            Match m=fixture(c); m.fighters[0].hp=19; equip(m.fighters[0],findItem(c,"stat.berserk")); Simulation s(m,c.rules);
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,10,2,1,"pistol"}); s.step();
            check(s.match.fighters[1].hp==80,"Berserk did not double damage below 20% health");
        }
        {
            Item guaranteedCrit=critStriker; guaranteedCrit.params["chance"]=1;
            Match m=fixture(c); equip(m.fighters[0],guaranteedCrit); Simulation s(m,c.rules);
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,10,2,1,"pistol"}); s.step();
            check(s.match.fighters[1].hp==80 && !s.events.empty() && s.events.back().critical,"Crit Striker did not double a guaranteed critical hit");
        }
        {
            Match m=fixture(c); Simulation s(m,c.rules);
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,1,3,1,"poison_dart"}); s.step();
            check(s.match.fighters[1].poisonDps==1,"Poison dart did not apply its permanent poison stack");
            check(std::abs(s.match.fighters[1].hp-99)<0.001f,"Poison dart should deal exactly 1 initial damage");
            float afterHit=s.match.fighters[1].hp; for(int n=0;n<120;++n) s.step();
            check(s.match.fighters[1].hp<afterHit-0.9f,"Poison dart did not deal 1 damage per second");
        }
        {
            Item guaranteedCrit=critStriker; guaranteedCrit.params["chance"]=1;
            Match m=fixture(c); equip(m.fighters[0],guaranteedCrit); Simulation s(m,c.rules);
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,1,3,1,"poison_dart"}); s.step();
            check(s.match.fighters[1].hp==99,"Poison dart initial damage must remain 1 even with Crit Striker");
            for(int n=0;n<120;++n) s.step();
            check(s.match.fighters[1].hp<=97 && std::any_of(s.events.begin(),s.events.end(),[](const Event& event){ return event.text=="POISON CRIT" && event.critical; }),"Poison tick did not receive a critical hit");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],findItem(c,"weapon.shuriken")); Simulation s(m,c.rules);
            s.match.fighters[0].cooldowns[0]=2;
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,11,4,1,"shuriken",0,0}); s.step();
            check(s.match.fighters[0].cooldowns[0]<1.01f,"Shuriken hit did not shorten its cooldown by one second");
        }
        {
            Match m=fixture(c); m.fighters[1].angle=Pi; equip(m.fighters[1],findItem(c,"weapon.orbit_shield"));
            Simulation s(m,c.rules); s.match.fighters[1].meleeAngles[0]=Pi;
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,12,3,1,"bow"}); s.step();
            check(!s.projectiles.empty() && s.projectiles[0].owner==1 && s.projectiles[0].velocity.x<0,"Orbit Shield did not deflect a projectile");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],bouncyBall); Simulation s(m,c.rules);
            bool hit=false,stillFlying=false;
            for(int n=0;n<90 && !hit;++n) {
                s.step();
                hit=s.match.fighters[1].hp<100;
                stillFlying=std::any_of(s.projectiles.begin(),s.projectiles.end(),[](const Projectile& p){ return p.kind=="bouncy_ball" && p.life>0; });
            }
            check(hit && s.match.fighters[1].hp==93 && stillFlying,"Bouncy Ball did not damage and ricochet instead of disappearing");
        }
        {
            Match m=fixture(c); m.fighters[0].position={-240,0}; m.fighters[0].previous=m.fighters[0].position; m.fighters[1].position={240,0}; m.fighters[1].previous=m.fighters[1].position;
            equip(m.fighters[0],bouncyBall); equip(m.fighters[0],bouncyBall); Simulation s(m,c.rules); for(int n=0;n<125;++n) s.step();
            int active=static_cast<int>(std::count_if(s.projectiles.begin(),s.projectiles.end(),[](const Projectile& projectile) { return projectile.kind=="bouncy_ball"; }));
            check(active==2,"Two Bouncy Ball copies did not launch two balls after the second copy's starting delay");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],boomerang); Simulation s(m,c.rules);
            bool hit=false,stillFlying=false;
            for(int n=0;n<120 && !hit;++n) {
                s.step(); hit=s.match.fighters[1].hp<100;
                stillFlying=std::any_of(s.projectiles.begin(),s.projectiles.end(),[](const Projectile& p){ return p.kind=="boomerang" && p.life>0; });
            }
            check(hit && s.match.fighters[1].hp==86 && stillFlying,"Boomerang did not hit once and continue its curved return");
        }
        {
            Match m=fixture(c); m.fighters[0].position={-240,0}; m.fighters[0].previous=m.fighters[0].position; m.fighters[1].position={240,0}; m.fighters[1].previous=m.fighters[1].position;
            equip(m.fighters[0],boomerang); equip(m.fighters[0],boomerang); Simulation s(m,c.rules); for(int n=0;n<125;++n) s.step();
            int active=static_cast<int>(std::count_if(s.projectiles.begin(),s.projectiles.end(),[](const Projectile& projectile) { return projectile.kind=="boomerang"; }));
            check(active==2,"Two Boomerang copies did not launch two boomerangs after the second copy's starting delay");
        }
        {
            Match m=fixture(c); m.fighters[1].angle=Pi; equip(m.fighters[1],findItem(c,"weapon.orbit_shield"));
            Simulation s(m,c.rules); s.match.fighters[1].meleeAngles[0]=Pi;
            s.projectiles.push_back({{60,0},{60,0},{15000,0},0,7,6,5,"bouncy_ball",-1,0});
            s.step();
            check(!s.projectiles.empty() && s.projectiles[0].owner==1 && s.projectiles[0].bouncyDeflected && s.projectiles[0].bouncyReturnTarget==0,"Shield did not take over the Bouncy Ball");
            for(int n=0;n<12 && !s.projectiles.empty();++n) s.step();
            check(s.projectiles.empty() && s.match.fighters[0].hp==93 && s.match.fighters[1].hp==100,"Deflected Bouncy Ball did not damage its prior holder exactly once on return");
        }
        {
            bool reflected=false;
            for(uint32_t seed=0;seed<24 && !reflected;++seed) {
                Match m=fixture(c); m.seed=seed; m.fighters[1].angle=Pi; equip(m.fighters[1],findItem(c,"weapon.katana"));
                Simulation s(m,c.rules); s.match.fighters[1].meleeAngles[0]=Pi;
                s.projectiles.push_back({{60,0},{60,0},{15000,0},0,12,3,1,"bow"}); s.step();
                reflected=!s.projectiles.empty() && s.projectiles[0].owner==1 && s.projectiles[0].velocity.x<0;
            }
            check(reflected,"Katana never deflected an incoming projectile");
        }
        {
            Match m=fixture(c); m.fighters[0].angle=0; equip(m.fighters[0],findItem(c,"weapon.orbit_shield")); m.fighters[1].position={-45,0};
            Simulation s(m,c.rules); s.match.fighters[0].meleeAngles[0]=0; s.step();
            check(s.match.fighters[1].hp==100,"Orbit Shield should not deal melee damage");
        }
        {
            Match m=fixture(c); m.fighters[0].position={-100,0}; m.fighters[0].previous=m.fighters[0].position;
            m.fighters[1].position={0,0}; m.fighters[1].previous=m.fighters[1].position; m.fighters[1].speed=100;
            equip(m.fighters[0],tapeAbility); Simulation s(m,c.rules);
            s.tapes.push_back({{-50,0},{50,0},0,5,false}); s.step();
            check(s.match.fighters[1].slowTime>2.9f && s.match.fighters[1].slowMultiplier==0.2f,"Slowing Tape did not apply its 3-second 20% slow");
            check(s.tapes.empty(),"Slowing Tape should disappear after an enemy crosses it");
            s.step(); check(length(s.match.fighters[1].velocity)<25,"Slowing Tape did not reduce movement to 20% speed");
        }
        {
            bool avoided=false;
            for(uint32_t seed=0;seed<160 && !avoided;++seed) {
                Match m=fixture(c); m.seed=seed; m.fighters[1].position={0,0}; m.fighters[1].previous=m.fighters[1].position;
                equip(m.fighters[1],findItem(c,"ability.careful_steps")); Simulation s(m,c.rules);
                s.tapes.push_back({{-50,0},{50,0},0,5,false}); s.step();
                avoided=s.match.fighters[1].slowTime==0 && s.tapes.empty();
            }
            check(avoided,"Lucky did not avoid a placed Slowing Tape");
        }
        {
            Match m=fixture(c); m.fighters[1].position={0,100}; m.fighters[1].previous=m.fighters[1].position;
            equip(m.fighters[0],tapeAbility); Simulation s(m,c.rules);
            s.tapes.push_back({{-50,0},{50,0},0,0.001f,false}); s.step();
            check(s.tapes.size()==1,"A placed Slowing Tape should persist until an enemy crosses it");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],tapeAbility); Simulation s(m,c.rules);
            Minion minion; minion.owner=1; minion.position=minion.previous={0,0}; minion.speed=100; minion.velocity={100,0}; s.minions.push_back(minion);
            s.tapes.push_back({{-50,0},{50,0},0,5,false}); s.step();
            check(s.minions[0].slowTime>2.9f && s.minions[0].slowMultiplier==0.2f && s.tapes.empty(),"Pocket Minion did not activate Slowing Tape");
            check(length(s.minions[0].velocity)<25,"Slowing Tape did not slow Pocket Minion movement");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],tapeAbility); Simulation s(m,c.rules);
            SummonedMouse mouse; mouse.owner=1; mouse.position=mouse.previous={0,0}; mouse.speed=100; mouse.velocity={100,0}; s.mice.push_back(mouse);
            s.tapes.push_back({{-50,0},{50,0},0,5,false}); s.step();
            check(s.mice[0].slowTime>2.9f && s.mice[0].slowMultiplier==0.2f && s.tapes.empty(),"Mouse did not activate Slowing Tape");
            check(length(s.mice[0].velocity)<25,"Slowing Tape did not slow mouse movement");
        }
        {
            Match m=fixture(c); Rules rules=c.rules; rules.contactDamage=0; Simulation s(m,rules);
            Minion minion; minion.owner=1; minion.position=minion.previous={0,0}; minion.speed=0; s.minions.push_back(minion);
            s.hazards.push_back({{0,0},0,20,16,38,10,-1,0,true}); s.step();
            check(s.hazards.empty() && s.minions[0].hp<40,"Pocket Minion did not activate or take damage from a mine");
        }
        {
            Match m=fixture(c); Rules rules=c.rules; rules.contactDamage=0; Simulation s(m,rules);
            SummonedMouse mouse; mouse.owner=1; mouse.position=mouse.previous={0,0}; mouse.speed=0; s.mice.push_back(mouse);
            s.hazards.push_back({{0,0},0,20,16,38,10,-1,0,true}); s.step();
            check(s.hazards.empty() && s.mice.empty(),"Mouse did not activate or get removed by a mine");
        }
        {
            // A hammer grazing the broad outer edge of the shield must still be deflected.
            Match m=fixture(c); m.fighters[0].position={0,0}; m.fighters[0].previous=m.fighters[0].position; m.fighters[0].angle=0;
            m.fighters[1].position={85,35}; m.fighters[1].previous=m.fighters[1].position; m.fighters[1].angle=Pi;
            equip(m.fighters[0],findItem(c,"weapon.orbit_shield")); equip(m.fighters[1],findItem(c,"weapon.hammer"));
            Simulation s(m,c.rules); s.match.fighters[0].meleeAngles[0]=0; s.match.fighters[1].meleeAngles[0]=Pi; s.step();
            check(s.match.fighters[1].hitCooldowns[0]>=0.49f,"Orbit Shield did not deflect a grazing Hammer hit");
        }
        {
            Match m=fixture(c); m.fighters[0].angle=0.35f;
            equip(m.fighters[0],findItem(c,"weapon.sword")); equip(m.fighters[0],findItem(c,"weapon.spear"));
            Simulation s(m,c.rules);
            check(std::abs(std::abs(std::remainder(s.match.fighters[0].meleeAngles[1]-s.match.fighters[0].meleeAngles[0],2*Pi))-Pi)<0.001f,"Second melee weapon did not start half an orbit away");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],findItem(c,"weapon.bow")); equip(m.fighters[0],findItem(c,"weapon.bow"));
            Simulation s(m,c.rules);
            check(std::abs(s.match.fighters[0].cooldowns[1]-s.match.fighters[0].cooldowns[0]-1)<0.001f,"Second matching ranged weapon did not receive its one-second start delay");
        }
        {
            Match m=fixture(c); m.fighters[0].position={-35,0}; m.fighters[1].position={35,0};
            m.fighters[0].previous=m.fighters[0].position; m.fighters[1].previous=m.fighters[1].position;
            equip(m.fighters[0],findItem(c,"weapon.sword")); equip(m.fighters[1],findItem(c,"weapon.sword"));
            Simulation s(m,c.rules); s.match.fighters[0].meleeAngles[0]=0.6f; s.match.fighters[1].meleeAngles[0]=Pi-0.6f;
            s.match.fighters[0].meleeDirections[0]=-1; s.match.fighters[1].meleeDirections[0]=-1;
            s.step();
            check(s.match.fighters[0].meleeDirections[0]>0 && s.match.fighters[1].meleeDirections[0]<0,"Equal melee classes did not deflect their tips away from each other");
            check(s.match.fighters[0].hitCooldowns[0]>=0.49f && s.match.fighters[1].hitCooldowns[0]>=0.49f,"Deflected melee weapons should receive a half-second hit cooldown");
        }
        {
            Match m=fixture(c); m.fighters[0].position={-35,0}; m.fighters[1].position={35,0};
            m.fighters[0].previous=m.fighters[0].position; m.fighters[1].previous=m.fighters[1].position;
            equip(m.fighters[0],findItem(c,"weapon.spear")); equip(m.fighters[1],findItem(c,"weapon.hammer"));
            Simulation s(m,c.rules); s.match.fighters[0].meleeAngles[0]=0.6f; s.match.fighters[1].meleeAngles[0]=Pi-0.6f;
            s.match.fighters[0].meleeDirections[0]=-1; s.match.fighters[1].meleeDirections[0]=-1;
            s.step();
            check(s.match.fighters[0].meleeDirections[0]>0 && s.match.fighters[1].meleeDirections[0]<0,"Heavy melee weapon did not deflect the lighter one away from its target");
        }
        {
            Match m=fixture(c); m.fighters[0].position={200,0}; m.fighters[1].position={250,0};
            m.fighters[0].previous=m.fighters[0].position; m.fighters[1].previous=m.fighters[1].position;
            m.fighters[0].angle=0; m.fighters[1].angle=Pi/2; m.fighters[1].speed=100; m.fighters[1].velocity={-100,0};
            equip(m.fighters[0],findItem(c,"weapon.hammer")); equip(m.fighters[1],findItem(c,"weapon.spear")); equip(m.fighters[1],findItem(c,"weapon.bow"));
            Simulation s(m,c.rules); s.match.fighters[0].meleeAngles[0]=0; s.match.fighters[1].meleeAngles[0]=Pi/2; s.match.fighters[1].cooldowns[1]=1;
            for(int n=0;n<45 && s.match.fighters[1].stunTime<=0;++n) s.step();
            check(s.match.fighters[1].stunTime>0.9f,"Hammer push into a wall did not stun");
            float cooldown=s.match.fighters[1].cooldowns[1],angle=s.match.fighters[1].meleeAngles[0]; s.step();
            float stunnedSpeed=length(s.match.fighters[1].velocity);
            check(s.match.fighters[1].cooldowns[1]==cooldown && s.match.fighters[1].meleeAngles[0]==angle && stunnedSpeed>9 && stunnedSpeed<11,"Stun did not slow movement to 10% while freezing cooldowns and melee rotation");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],findItem(c,"ability.spikes"));
            m.fighters[0].speed=200; m.fighters[0].velocity={-200,0};
            Simulation s(m,c.rules); for(int n=0;n<150;++n) s.step();
            check(!s.hazards.empty() && !s.hazards.front().mine,"Wall bounce did not leave a spike");
            check(length(s.hazards.front().normal)>0.99f,"Wall spike did not retain its wall direction");
        }
        {
            Match m=fixture(c); Rules rules=c.rules; rules.contactDamage=0;
            Simulation s(m,rules);
            // This is the same non-mine hazard created after a wall bounce.
            s.hazards.push_back({m.fighters[1].position,0,10,18,0,10,0,0,false,{1,0}});
            s.step();
            check(s.match.fighters[1].hp<100,"Visible wall spike did not damage the opponent");
        }
        {
            Match m=fixture(c); equip(m.fighters[1],findItem(c,"ability.dodge")); m.fighters[1].speed=100;
            Simulation s(m,c.rules); s.projectiles.push_back({{40,0},{40,0},{500,0},0,40,2,1,"pistol"}); s.step();
            check(s.match.fighters[1].invulnerable>0 && std::abs(s.match.fighters[1].velocity.y)>100,"Dodge failed");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],findItem(c,"ability.dash")); m.fighters[0].speed=100;
            Simulation s(m,c.rules); s.step(); check(s.match.fighters[0].velocity.x>200,"Dash failed");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],findItem(c,"ability.regen")); m.fighters[0].hp=50;
            Simulation s(m,c.rules); s.step(); check(s.match.fighters[0].hp==55,"Regeneration failed");
        }
        {
            Match m=fixture(c); m.arena.effect="healing_arena"; m.fighters[0].position={0,0}; m.fighters[0].previous={0,0}; m.fighters[0].hp=50;
            Simulation s(m,c.rules); s.step(); check(s.match.fighters[0].hp==65,"Healing arena did not restore 15 HP on entry");
            s.step(); check(s.match.fighters[0].hp==65,"Healing arena healed repeatedly without leaving its circle");
        }
        {
            Match m=fixture(c); m.arena.effect="spiked_arena"; m.fighters[0].position={-270,0}; m.fighters[0].previous=m.fighters[0].position; m.fighters[0].speed=120; m.fighters[0].velocity={-120,0};
            Simulation s(m,c.rules); for(int n=0;n<8;++n) s.step(); check(s.match.fighters[0].hp==99,"Spiked arena did not deal 1 damage on a wall bounce");
        }
        {
            Match m=fixture(c); m.arena.effect="center_gravity"; m.fighters[0].position={100,100}; m.fighters[0].previous=m.fighters[0].position; m.fighters[0].speed=120; m.fighters[0].velocity={0,120};
            Simulation s(m,c.rules); for(int n=0;n<30;++n) s.step(); check(s.match.fighters[0].velocity.x<-1,"Center gravity did not gradually curve a travelling ball toward the arena centre");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],damageField); m.fighters[0].position={0,-60}; m.fighters[0].previous=m.fighters[0].position; m.fighters[1].position={40,0}; m.fighters[1].previous=m.fighters[1].position;
            Simulation s(m,c.rules); s.damageFields[0].points={{-50,-50},{50,-50},{50,50}}; s.damageFields[0].drawing=true;
            s.step(); check(s.match.fighters[1].hp==75 && !s.fieldShapes.empty() && !s.damageFields[0].drawing,"Damage Field did not close a three-line shape for 25 damage");
        }
        {
            Match m=fixture(c); Simulation s(m,c.rules); SummonedMouse mouse; mouse.position={0,80}; mouse.previous=mouse.position; mouse.owner=0; mouse.hp=1; mouse.speed=0; mouse.life=0;
            s.mice.push_back(mouse); for(int n=0;n<200;++n) s.step(); check(s.mice.size()==1,"A mouse disappeared without being defeated");
        }
        {
            Match m=fixture(c); equip(m.fighters[0],findItem(c,"ability.vampire")); m.fighters[0].hp=50;
            Simulation s(m,c.rules); s.projectiles.push_back({{60,0},{60,0},{15000,0},0,20,2,1,"pistol"}); s.step();
            check(std::abs(s.match.fighters[0].hp-54.4f)<0.01f,"Lifesteal failed");
        }
        {
            Match m=fixture(c); Rules rules=c.rules; rules.sudden=0; rules.suddenDps=1200;
            Simulation s(m,rules); run(s); check(s.winner==-1,"Simultaneous death should draw");
            rules.suddenDps=0; rules.limit=0.1f; Simulation timeout(m,rules); run(timeout); check(timeout.winner==-1,"Timeout should draw");
        }
        // Match outcomes are identical when the renderer requests 1x or 16x batches.
        for(uint32_t seed=0;seed<12;++seed) {
            Match m=generateMatch(c,seed); Simulation a(m,c.rules),b(m,c.rules); run(a,1); run(b,16);
            check(a.winner==b.winner && a.time==b.time && a.match.fighters[0].hp==b.match.fighters[0].hp && a.match.fighters[1].hp==b.match.fighters[1].hp,"Playback speed changed result");
        }
        int outcomes[3]={0,0,0}; std::set<std::string> seenArenas,seenItems;
        for(uint32_t seed=100;seed<250;++seed) {
            Match m=generateMatch(c,seed); check(std::abs(m.fighters[0].points-m.fighters[1].points)<=c.rules.tolerance,"Budget tolerance exceeded");
            seenArenas.insert(m.arena.id);
            for(const auto& f:m.fighters) { check(f.items.size()==5,"Missing items"); for(const auto& i:f.items) seenItems.insert(i.id); }
            Simulation s(m,c.rules);
            uint64_t latestEvent=0;
            while(!s.finished) {
                s.step();
                for(const auto& event:s.events) if(event.serial>latestEvent) {
                    check(event.serial==latestEvent+1,"Feedback event sequence skipped or repeated");
                    latestEvent=event.serial;
                }
                for(const auto& f:s.match.fighters) {
                    check(std::isfinite(f.position.x) && std::isfinite(f.position.y) && std::isfinite(f.hp),"Non-finite physics");
                    for(size_t n=0;n<m.arena.vertices.size();++n) {
                        Vec a=m.arena.vertices[n],edge=m.arena.vertices[(n+1)%m.arena.vertices.size()]-a;
                        check(dot(f.position-a,normalized(Vec{-edge.y,edge.x}))>=f.radius-2,"Ball escaped arena");
                    }
                }
                check(s.time<c.rules.limit+Step*2,"Exceeded round time");
            }
            ++outcomes[s.winner+1];
        }
        check(seenArenas.size()==c.arenas.size(),"Not all arenas covered"); check(seenItems.size()==c.items.size(),"Not all items covered");
        // Invalid user edits are rejected before creating a physics world.
        nlohmann::json original; std::ifstream input(argv[1]); input>>original;
        auto broken=std::filesystem::temp_directory_path()/"orbital-odds-invalid-catalog.json";
        for(int scenario=0;scenario<3;++scenario) {
            auto j=original;
            if(scenario==0) j["items"][1]["params"]["cooldown"]=0;
            if(scenario==1) j["items"][1]["id"]=j["items"][0]["id"];
            if(scenario==2) j["items"][0]["effect"]="not_implemented";
            { std::ofstream out(broken); out<<j; }
            bool rejected=false; try { (void)Catalog::load(broken); } catch(...) { rejected=true; }
            check(rejected,"Invalid catalog accepted");
        }
        std::filesystem::remove(broken);
        std::cout<<"PASS "<<checks<<" checks; 150 random fights; draws="<<outcomes[0]<<", A="<<outcomes[1]<<", B="<<outcomes[2]<<"; all items and arenas covered\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
