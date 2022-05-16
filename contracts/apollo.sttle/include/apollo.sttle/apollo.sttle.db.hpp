 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

using namespace eosio;

#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

namespace apollo {

using namespace std;
using namespace eosio;
using namespace wasm;

static constexpr eosio::name active_perm{"active"_n};

#define STTLE_TBL [[eosio::table, eosio::contract("apollo.sttle")]]

struct hashrate_t {
    float value;
    char unit;
};

struct time {
    int hour;
    int minute;

    time() {}
    
    time(int h, int m){
        this->hour = h;
        this->minute = m;
    }

    bool operator == ( const time& time1, const int& time2 ){
        if (time1.hour == time2.hour && time1.minute == time2.minute)
            return true;
        return false;
    }
    
    bool operator <= ( const time& time1, const int& time2 ){
        if ( time1.hour < time2.hour ){

            return true;
        }else if (time1.hour > time2.hour){

            return false;
        }
            
        if ( time1.minute < time2.minute ){

            return true;
        }else if ( time1.minute > time2.minute ){

           return false; 
        }

        return true;
    }
    
    bool operator >= ( const time& time1, const int& time2 ){

        if ( time1.hour > time2.hour ){

            return true;
        }else if (time1.hour < time2.hour){

            return false;
        }
            
        if ( time1.minute > time2.minute ){

            return true;
        }else if ( time1.minute < time2.minute ){

           return false; 
        }

        return true;
    }

};

struct [[eosio::table("global"), eosio::contract("apollo.sttle")]] global_t {
    hashrate_t          hashrate;
    asset               asset;
    time                check_begin;
    time                check_end;
    time                sttle_begin;
    time                sttle_end;


    global_t() {}

    EOSLIB_SERIALIZE( global_t, (hashrate)(asset)(check_begin)(check_end)(sttle_begin)(sttle_end) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

struct STTLE_TBL sttle_t {
    name        owner;
    string      nft_id;
    asset       profit;
    time_point  mine_at;
    time_point  start_at;

    uint64_t    primary_key()const { return owner.value; }
    uint64_t    scope() const { return 0; }
    uint64_t    by_mine_coin()const { return name(asset_meta.mine_coin_type).value; }

    sttle_t() {}
    sttle_t(const name& c): claimer(c) {}

    EOSLIB_SERIALIZE( sttle_t, (owner)(nft_id)(profit)(mine_at)(start_at) )

    typedef eosio::multi_index<"sttles"_n, sttle_t,
            indexed_by<"nftid"_n, const_mem_fun<sttle_t, uint64_t, &sttle_t::by_mine_coin> >
     > tbl_t;
};

} // apollo
