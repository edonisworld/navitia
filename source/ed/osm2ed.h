/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.
  
This file is part of Navitia,
    the software to build cool stuff with public transport.
 
Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!
  
LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
   
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.
   
You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
  
Stay tuned using
twitter @navitia 
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/

#pragma once
#include "type/type.h"
#include "third_party/osmpbfreader/osmpbfreader.h"
#include "utils/lotus.h"
#include "utils/logger.h"
#include <unordered_map>
#include "ed/types.h"
#include "ed_persistor.h"

namespace ed { namespace connectors {


struct Node {
private:
    // On utilise des int32 pour économiser de la mémoire. C’est les coordonnées * 1e6
    int32_t ilon;
    int32_t ilat;
    static constexpr double factor = 1e6;
public:
    uint32_t uses;
    int32_t idx;

    void set_coord(double lon, double lat){
        this->ilon = lon * factor;
        this->ilat = lat * factor;
    }

    double lon() const {
        return double(this->ilon) / factor;
    }

    double lat() const {
        return double(this->ilat) / factor;
    }

    Node() : uses(0), idx(-1) {}

    /// Incrémente le nombre d'utilisations, on lui passe l'index qu'il devra avoit
    /// Retourne vrai si c'est un nouveau nœud (càd utilisé au moins 2 fois et n'avait pas encore d'idx)
    /// Ca permet d'éviter d'utiliser deux fois le même nœud
    bool increment_use(int idx){
        uses++;
        if(this->idx == -1 && uses > 1){
            this->idx = idx;
            return true;
        } else {
            return false;
        }
    }
};

struct OSMWay {
    const static uint8_t CYCLE_FWD = 0;
    const static uint8_t CYCLE_BWD = 1;
    const static uint8_t CAR_FWD = 2;
    const static uint8_t CAR_BWD = 3;
    const static uint8_t FOOT_FWD = 4;
    const static uint8_t FOOT_BWD = 5;

    /// Propriétés du way : est-ce qu'on peut "circuler" dessus
    std::bitset<8> properties;

    /// Nodes qui composent le way
    std::vector<uint64_t> refs;
};

struct OSMAdminRef{
    std::string level;
    std::string insee;
    std::string name;
    std::string postcode;
    navitia::type::GeographicalCoord coord;
    CanalTP::References refs;
};

/** Structure appelée par parseur OSM PBF */
struct Visitor{
    ed::EdPersistor persistor;
    log4cplus::Logger logger;
    std::unordered_map<uint64_t,CanalTP::References> references;
    std::unordered_map<uint64_t, Node> nodes;
    std::unordered_map<uint64_t, ed::types::HouseNumber> housenumbers;
    std::unordered_map<uint64_t, OSMAdminRef> OSMAdminRefs;

    std::unordered_map<uint64_t, ed::types::Poi> pois;
    std::unordered_map<std::string, ed::types::PoiType> poi_types;
    std::vector<std::string> properties_to_ignore;

    int total_ways;
    int total_house_number;

    int node_idx;

    std::unordered_map<uint64_t, OSMWay> ways;
    //Pour charger les données administratives
    //navitia::georef::Levels levellist;

    Visitor(const std::string & conn_str) : persistor(conn_str),
        logger(log4cplus::Logger::getInstance("log")),
        total_ways(0), total_house_number(0), node_idx(0){}

    void node_callback(uint64_t osmid, double lon, double lat, const CanalTP::Tags & tags);
    void way_callback(uint64_t osmid, const CanalTP::Tags &tags, const std::vector<uint64_t> &refs);
    void relation_callback(uint64_t osmid, const CanalTP::Tags & tags, const CanalTP::References & refs);

    void add_osm_housenumber(uint64_t osmid, const CanalTP::Tags & tags);

    /// Once all the ways and nodes are read, we count how many times a node is used to detect intersections
    void count_nodes_uses();

    /// Calcule la source et cible des edges
    void insert_edges();

    /// Chargement des adresses
    void insert_house_numbers();

    /// récupération des coordonnées du noued "admin_centre"
    navitia::type::GeographicalCoord admin_centre_coord(const CanalTP::References & refs);

    /// à partir des références d'une relation, reconstruit la liste des identifiants OSM ordonnés
    std::vector<uint64_t> nodes_of_relation(const CanalTP::References & refs) const;

    /// gestion des limites des communes : création des polygons
    std::string geometry_of_admin(const CanalTP::References & refs) const;

    /// construction des informations administratives
    void insert_admin();


    /// construit les relation d'inclusions entre les objets géographique
    void build_relation();
 
    void fill_PoiTypes();
    void fill_properties_to_ignore();
    void fill_pois(const uint64_t osmid, const CanalTP::Tags & tags);
    void insert_poitypes();
    void insert_pois();
    void insert_properties();
    void set_coord_admin();
    /// Ajoute le nœud à la base que s’il le mérite :
    /// 1/ être une intersaction
    /// 2/ ne pas avoir déjà été inséré
    void insert_if_needed(uint64_t ref);
    bool is_property_to_ignore(const std::string& tag);
};

}}

