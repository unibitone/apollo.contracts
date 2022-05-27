 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

using namespace eosio;


static constexpr eosio::name APOLLO_TOKEN{"apollo.token"_n};
static constexpr eosio::name AM_TOKEN{"cnyd.token"_n};
static constexpr eosio::name APOLLO_EV{"apollo.ev"_n};

namespace apollo {
using namespace std;
using namespace eosio;
using namespace wasm;

enum class token_type: uint8_t {
    NONE                  = 0,
    POW                   = 1,
    POS                   = 2,
};

struct asset_symbol {
    uint32_t token_id;      // 1:1 with NFT invars hash
    uint32_t sub_token_id;   // can be token receiving date
    //uint8_t precision = 0;

    asset_symbol() {}
    asset_symbol(const uint64_t raw) {
        token_id = raw >> 32;
        sub_token_id = (raw << 32) >> 32;
        // sub_token_id = raw | 0x00000000FFFFFFFF;

    }

    uint64_t raw()const { return (uint64_t) token_id << 32 | sub_token_id; }

    friend bool operator==(const asset_symbol&, const asset_symbol&);

    EOSLIB_SERIALIZE(asset_symbol, (token_id)(sub_token_id) )
};

bool operator==(const asset_symbol& symb1, const asset_symbol& symb2) { 
    return( symb1.token_id == symb2.token_id && symb1.sub_token_id == symb2.sub_token_id ); 
}

struct token_asset {
    int64_t         amount;
    asset_symbol    symbol;

    token_asset() {}
    token_asset(const asset_symbol& symb): amount(0), symbol(symb) {}

    token_asset& operator+=(const token_asset& quantity) { 
        check( quantity.symbol == this->symbol, "symbol mismatch");
        this->amount += quantity.amount; return *this;
    } 
    token_asset& operator-=(const token_asset& quantity) { 
        check( quantity.symbol == this->symbol, "symbol mismatch");
        this->amount -= quantity.amount; return *this; 
    }

    EOSLIB_SERIALIZE(token_asset, (amount)(symbol) )
};

struct hashrate {
    float value;
    string unit;  //M, G, T

    string to_str()const { return to_string(value) + " " + unit; }
};

struct pow_asset_invariables {
    string manufacturer;                            //manufacture info
    string mine_coin_type;                          //btc, eth
    hashrate hash_rate;                             //hash_rate and hash_rate_unit(M/T) E.g. 21.457 MH/s
    float power_in_watt;                            //E.g. 2100 Watt
    uint16_t service_life_days;                     //service lifespan (E.g. 3*365) 

    checksum256 hash(const string& prefix)const {
        string str =    prefix + "\n" + 
                        manufacturer + "\n" +
                        mine_coin_type + "\n" +
                        hash_rate.to_str() + "\n" +
                        to_string(power_in_watt) + "\n" +
                        to_string(service_life_days);

        return HASH256(str);
    }

    // EOSLIB_SERIALIZE( pow_asset_invariables, (manufacturer) )
};

struct power_asset_variables {
    string mining_pool;                             //E.g. 超算大陆
    string mining_location;                         //E.g. 加拿大
    asset daily_earning_est;                        //daily earning estimate: E.g. "0.00397002 AMETH"
    asset daily_electricity_charge;                 //每日耗电, E.g.: "0.85 CNYD" for reference
    uint16_t daily_svcfee_rate;                     //boost by 10000, 5% => 500
    hashrate actual_hash_rate;                      //normalized hash rate
    uint8_t onshelf_days;                           //0: T+0, 1:T+1

    // EOSLIB_SERIALIZE( power_asset_variables, (mining_pool) )
};

typedef std::variant<pow_asset_invariables /*pos_asset_invariables */> token_invars;
typedef std::variant<power_asset_variables /*pos_asset_variables */> token_vars;

struct tokenstats_t {
    uint64_t        token_id;       //PK
    uint8_t         token_type;     //POW, POS, ...etc
    string          token_uri;      //token_uri for token metadata { image }
    token_invars    invars;         //used to derive first part of token asset ID
    token_vars      vars;
    int64_t         max_supply;     //when amount is 1, it means NFT-721 type
    int64_t         supply;
    name            creator;
    time_point_sec  created_at;
    bool            paused;

    tokenstats_t() {};
    tokenstats_t(const uint64_t& id): token_id(id) {};

    uint64_t primary_key()const { return token_id; }
    uint64_t by_token_type()const { return (uint64_t) token_type; }
    checksum256 by_token_invars()const { 
        if (token_type == (uint8_t) token_type::POW)
            return std::get<pow_asset_invariables>(invars).hash(to_string(token_type)); 
        else 
            return checksum256();
    }
    typedef eosio::multi_index
    < "tokenstats"_n,  tokenstats_t,
        indexed_by<"tokentypes"_n, const_mem_fun<tokenstats_t, uint64_t, &tokenstats_t::by_token_type> >,
        indexed_by<"tokeninvars"_n, const_mem_fun<tokenstats_t, checksum256, &tokenstats_t::by_token_invars> >
    > idx_t;

    EOSLIB_SERIALIZE(tokenstats_t,  (token_id)(token_type)(token_uri)(invars)(vars)(max_supply)(supply)
                                    (creator)(created_at)(paused) )
};


///Scope: owner's account
struct account_t {
    token_asset     balance;
    name            beneficiary;    //usually it is the same account owner
    bool paused     = false;   //if true, it can no longer be transferred

    account_t() {}
    account_t(const token_asset& asset): balance(asset) {}

    uint64_t    primary_key()const { return balance.symbol.raw(); }

    typedef eosio::multi_index< "accounts"_n, account_t > idx_t;

    EOSLIB_SERIALIZE(account_t, (balance)(beneficiary)(paused) )

};

} // apollo
