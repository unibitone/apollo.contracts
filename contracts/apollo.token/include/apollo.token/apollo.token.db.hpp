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

namespace apollo {

using namespace std;
using namespace eosio;

#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm{"active"_n};
static constexpr eosio::name CNYD_BANK{"cnyd.token"_n};

static constexpr uint64_t percent_boost     = 10000;
static constexpr uint64_t max_memo_size     = 1024;

// static constexpr uint64_t seconds_per_year      = 24 * 3600 * 7 * 52;
// static constexpr uint64_t seconds_per_month     = 24 * 3600 * 30;
// static constexpr uint64_t seconds_per_week      = 24 * 3600 * 7;
// static constexpr uint64_t seconds_per_day       = 24 * 3600;
// static constexpr uint64_t seconds_per_hour      = 3600;

enum class token_type_t: uint8_t {
    NONE                        = 0,
    POW_ASSET                   = 1,
    POS_ASSET                   = 2,
};

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size());

#define TBL struct [[eosio::table, eosio::contract("apollo.token")]]

struct [[eosio::table("global"), eosio::contract("apollo.token")]] global_t {
    name admin;                 // default is contract self
    name fee_collector;         // mgmt fees to collector
    uint64_t fee_rate = 4;      // boost by 10,000, i.e. 0.04%
    uint16_t curr_nft_cat_id = 0;
    uint16_t curr_nft_subcat_id = 1;
    uint32_t curr_nft_item_id = 2;
    bool active = false;

    EOSLIB_SERIALIZE( global_t, (admin)(fee_collector)(fee_rate)
                                (curr_nft_cat_id)(curr_nft_subcat_id)(curr_nft_item_id)
                                (active) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

enum class nft_type: uint8_t {
    NONE        = 0,
    POW         = 1,
    POS         = 2,
};

enum class mine_coin: uint8_t {
    NONE        = 0,
    BTC         = 1,
    ETH         = 2,
    AMAX        = 3,
    FIL         = 4
}

struct token_asset {
    uint64_t    asset_id;
    int64_t     amount;

    token_asset& operator+=(const token_asset& quantity) { 
        check( quantity.symbol == this->symbol, "symbol mismatch");
        this->amount += quantity.amount; return *this;
    } 
    token_asset& operator-=(const token_asset& value) { 
        check( quantity.symbol == this->symbol, "symbol mismatch");
        this->amount -= quantity.amount; return *this; 
    }
};

struct hashrate {
    float value;
    char unit;  //M, G, T

    string to_string() { return to_string(value) + " " + string(unit); }
};

struct pow_asset_invariables {
    string mining_pool;                             //E.g. 超算大陆
    string manufacturer;                            //manufacture info
    string mine_coin_type;                          //btc, eth
    hashrate hash_rate;                             //hash_rate and hash_rate_unit(M/T) E.g. 21.457 MH/s
    float power_in_watt;                            //E.g. 2100 Watt
    uint16_t service_life_days;                     //service lifespan (E.g. 3*365) 

    checksum256 hash(string prefix) {
        string str =    prefix + "\n" + 
                        mining_pool + "\n" +
                        manufacturer + "\n" +
                        mine_coin_type + "\n" +
                        hash_rate.to_string() + "\n" +
                        to_string(power_in_watt) + "\n" +
                        to_string(service_life_days);

        return HASH256(str);
    }
};

struct power_asset_variables {
    string mining_location;                         //E.g. 加拿大
    asset daily_earning_est;                        //daily earning estimate: E.g. "0.00397002 AMETH"
    asset daily_electricity_charge;                 //每日耗电, E.g.: "0.85 CNYD" for reference
    uint16_t daily_svcfee_rate;                     //boost by 10000, 5% => 500
    hashrate actual_hash_rate;                      //normalized hash rate
    uint8_t onshelf_days;                           //0: T+0, 1:T+1
};

typedef std::variant<pow_asset_invariables /*pos_asset_invariables */> token_invars;
typedef std::variant<power_asset_variables /*pos_asset_variables */> token_vars;

TBL token_stats_t {
    uint64_t        token_id;       //PK
    uint8_t         token_type;     //POW, POS, ...etc
    string          token_uri;      //token_uri for token metadata { image }
    token_invars    invars;
    token_vars      vars;
    int64_t         max_supply;     //when amount is 1, it means NFT-721 type
    int64_t         supply;
    name            issuer;
    time_point_sec  issued_at;
    bool            paused;

    token_stats_t() {};
    token_stats_t(const uint64_t& id): token_id(id) {};

    uint64_t primary_key()const { return token_id; }
    uint8_t by_token_type()const { return token_type; }
    checksum256 by_token_invars()const { return invars.hash(""); }
    typedef eosio::multi_index
    < "tokenstats"_n,  token_stats_t,
        indexed_by<"tokentypes"_n, const_mem_fun<token_stats_t, uint8_t, &token_stats_t::by_token_type> >,
        indexed_by<"tokeninvars"_n, const_mem_fun<token_stats_t, checksum256, &token_stats_t::by_token_invars> >
    > idx_t;

    EOSLIB_SERIALIZE(token_stats_t, (token_id)(token_type)(token_uri)(invars)(vars)(max_supply)(supply)
                                    (issuer)(issued_at)(paused) )
};

TBL asset_t {
    uint64_t        id;        //PK
    uint64_t        token_id;
    time_point_sec  effected_at; //起效日
    asset           last_recd_earning;
    asset           total_recd_earing;
    asset           total_paid_electricity_fees;
    uint64_t        total_settled_times;
    time_point_sec  last_settled_at;
    time_point_sec  fee_discounted_from;
    time_point_sec  fee_discounted_to;
    uint16_t        fee_discount_rate; //boost by 10000

    asset_t() {}
    asset_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint128_t by_unique_effected_at()const { return (uint128_t) token_id << 64 | (uint128_t) effected_at.sec_since_epoch(); }

    typedef eosio::multi_index
    < "assets"_n,  asset_t,
        indexed_by<"ukeffectedat"_n, const_mem_fun<asset_t, uint128_t, &asset_t::by_unique_effected_at> >
    > idx_t;

    EOSLIB_SERIALIZE(asset_t,   (id)(token_id)(effected_at)(last_recd_earning)(total_recd_earing)
                                (total_paid_electricity_fees)(total_settled_times)(last_settled_at)
                                (fee_discounted_from)(fee_discounted_to)(fee_discount_rate) )
};

///Scope: owner's account
TBL account_t {
    token_asset     balance;
    bool paused     = false;   //if true, it can no longer be transferred

    account_t() {}
    account_t(const token_asset& asset): balance(asset) {}

    uint64_t    primary_key()const { return balance.asset_id; }

    typedef eosio::multi_index< "accounts"_n, account_t > idx_t;

    EOSLIB_SERIALIZE(account_t, (balance)(paused) )

};

} // apollo
