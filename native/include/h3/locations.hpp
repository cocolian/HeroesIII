#pragma once
#include "h3/game.hpp"

namespace h3::locations {
const nlohmann::json& facts();
int originalUnit(int);int originalSpell(int);int artifactPrice(int);int artifactExperience(int);int creatureExperience(int);
bool merge(Army&,int,int);bool learnable(const Hero&,int);bool spellLearnable(const Game&,int);
double estimatedGuards(const Object&);void consolidate(Army&);
std::string name(int,int subtype=0);std::string resourcesText(const Resources&);
void giveArtifact(Game&,int);void removeArtifact(Hero&,int);void bonus(Game&,const Object&,int morale,int luck);
std::string pickup(Game&,int);std::string scout(Game&,int);std::string training(Game&,int);std::string dwelling(Game&,int);std::string bank(Game&,int);std::string service(Game&,int);
void initializeDwelling(Game&,Object&);void initializeTraining(Game&,Object&);void initializeBank(Game&,Object&);void initializeService(Game&,Object&);
std::vector<LocationOffer> dwellingOffers(const Game&,const Object&);std::vector<LocationOffer> trainingOffers(const Game&,const Object&);std::vector<LocationOffer> serviceOffers(const Game&,const Object&);
std::string performTraining(Game&,Object&,const LocationOffer&,int);std::string performService(Game&,Object&,const LocationOffer&,int);std::string hire(Game&,Object&,const LocationOffer&,int);
Hero prisoner(const Game&,const Object&);Point spawn(const Game&,const Object&);
}
