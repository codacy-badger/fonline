#ifndef __ENTITY_MANAGER__
#define __ENTITY_MANAGER__

#include "Common.h"
#include "Entity.h"
#include "Item.h"
#include "Critter.h"
#include "Map.h"

class EntityManager
{
private:
    EntityMap allEntities;
    uint      entitiesCount[ (int) EntityType::Max ];

    bool LinkMaps();
    bool LinkNpc();
    bool LinkItems();
    void InitAfterLoad();

public:
    EntityManager();

    void    RegisterEntity( Entity* entity );
    void    UnregisterEntity( Entity* entity );
    Entity* GetEntity( uint id, EntityType type );
    void    GetEntities( EntityType type, EntityVec& entities );
    uint    GetEntitiesCount( EntityType type );

    void      GetItems( ItemVec& items );
    void      GetCritterItems( uint crid, ItemVec& items );
    Critter*  GetCritter( uint crid );
    void      GetCritters( CritterVec& critters );
    Map*      GetMapByPid( hash pid, uint skip_count );
    void      GetMaps( MapVec& maps );
    Location* GetLocationByPid( hash pid, uint skip_count );
    void      GetLocations( LocationVec& locations );

    bool LoadEntities();
    void ClearEntities();
};

extern EntityManager EntityMngr;

#endif // __ENTITY_MANAGER__
