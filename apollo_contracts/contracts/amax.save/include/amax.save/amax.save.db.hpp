#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <utils.hpp>

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>



namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

#define TBL struct [[eosio::table, eosio::contract("amax.save")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.save")]]

namespace deposit_type {
    static constexpr eosio::name TERM     = "term"_n;
    static constexpr eosio::name DEMAND   = "demand"_n;
}

NTBL("global") global_t {
    name admin;
    
    EOSLIB_SERIALIZE( global_t, (admin) )

};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

TBL save_plan_t {
    uint64_t            id;                         //PK
    name                type = deposit_type::TERM;  // term | demand
    extended_symbol     principal_token;            //E.g. 8,AMAX@amax.token
    extended_symbol     interest_token;             //E.g. 8,AMAX@amax.token
    uint64_t            deposit_term_days;          //E.g. 365
    bool                allow_advance_redeem;
    uint64_t            advance_redeem_fine_rate;   //E.g. 50% * 10000 = 5000
    map<uint64_t, uint64_t> interest_rates;
    extended_asset      deposit_available;          //deposited by users
    extended_asset      deposit_redeemed;
    extended_asset      interest_available;         //refuel by admin
    extended_asset      interest_redeemed;
    extended_asset      penalty_available;          //contributed by advanced deposit redeemers 
    extended_asset      penalty_redeemed; 
    time_point_sec      effective_from;             //before which deposits are not allowed
    time_point_sec      effective_to;               //after which deposits are not allowed but penalty split are allowed
    time_point_sec      created_at;
    time_point_sec      updated_at;
};

//Scope: account
TBL save_account_t {
    uint64_t        id;                 //PK
    uint64_t        plan_id;
    asset           deposit_quant;
    asset           total_interest_collected;
    time_point_sec  created_at;
    time_point_sec  updated_at;

    save_account_t() {}
    save_account_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    EOSLIB_SERIALIZE( save_account_t,   (id)(plan_id)(deposit_quant)(total_interest_collected)
                                        (created_at)(updated_at) )

};

} //namespace amax