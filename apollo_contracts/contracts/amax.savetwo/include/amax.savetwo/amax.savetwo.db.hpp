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

#define SAVE_TBL struct [[eosio::table, eosio::contract("amaxsavetwo1")]]
#define GLOBAL_TBL(name) struct [[eosio::table(name), eosio::contract("amaxsavetwo1")]]
// static constexpr uint64_t  DAY_SECONDS = 24 * 60 * 60;
static constexpr uint64_t  DAY_SECONDS = 60;
static constexpr uint64_t  YEAR_SECONDS = 365 * 24 * 60 * 60;
static constexpr uint64_t  YEAR_DAYS   = 365;

namespace plan_type {
    static constexpr eosio::name TERM       = "term"_n;
    static constexpr eosio::name DEMAND     = "demand"_n;
}

namespace plan_status {
    static constexpr eosio::name RUNNING    = "running"_n;
    static constexpr eosio::name SUSPENDED  = "suspended"_n;
    static constexpr eosio::name BLOCKED    = "blocked"_n;
}

GLOBAL_TBL("global") global_t {
    name admin                              = "armoniaadmin"_n;
    uint64_t last_save_id                   = 0;
    uint64_t last_plan_id                   = 0;
    uint64_t farm_lease_id                  = 0;

    EOSLIB_SERIALIZE( global_t, (admin)(last_save_id)(last_plan_id)(farm_lease_id) )

};

typedef eosio::singleton< "global"_n, global_t > global_singleton;


//scope: self
SAVE_TBL save_plan_t {
    uint64_t                          id;                             //PK
    string                            plan_name;                
    extended_symbol                   interest_symbol;                //interest token symbol
    extended_symbol                   stake_symbol;                   //stake token symbol
    extended_symbol                   lquidity_extsym;      
    uint16_t                          plan_days;                      //plan days
    asset                             plan_profit;                    //plan profit per quota
    uint32_t                          total_quotas;                   //total quotas
    uint32_t                          quotas_purchased = 0;
    asset                             stake_per_quota;                //stake amount per quota
    asset                             apl_per_quota;                  //apl reward per quota
    asset                             interest_total;                 //prestore total interest
    asset                             interest_collected;
    name                              type    = plan_type::TERM;      //plan type
    name                              status  = plan_status::RUNNING; //plan status
    time_point_sec                    begin_at;             
    time_point_sec                    end_at;               
    time_point_sec                    created_at;    
    save_plan_t() {}
    save_plan_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t scope()const { return 0; }
    
    uint32_t calc_available_quotas()  const { return total_quotas   - quotas_purchased; }
    asset    calc_available_interest()const { return interest_total - interest_collected; }

    typedef multi_index<"saveplan"_n, save_plan_t > tbl_t;

    EOSLIB_SERIALIZE( save_plan_t,  (id)(plan_name)                    
                                    (interest_symbol)(stake_symbol)
                                    (lquidity_extsym)(plan_days)
                                    (plan_profit)(total_quotas)
                                    (quotas_purchased)(stake_per_quota)
                                    (apl_per_quota)(interest_total)
                                    (interest_collected)(type)(status)
                                    (begin_at)(end_at)(created_at) )

};

//Scope: account
//Note: record will be deleted upon withdrawal/redemption
SAVE_TBL save_account_t {
    uint64_t            id;                   //PK
    uint64_t            plan_id;
    asset               pledged;              //amount
    extended_symbol     lquidity_extsym;      
    uint32_t            quotas;
    uint16_t            plan_term_days;       //pledge days
    asset               interest_alloted;     //alloted interest
    asset               interest_collected;
    time_point_sec      term_ended_at;
    time_point_sec      last_collected_at;
    time_point_sec      created_at;

    save_account_t() {}
    save_account_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    // Interest Calculate Formula: (current - created_at)/(term_ended_at - created_at) * total_interest - interest_collected
    asset calc_due_interest()const { 
      uint32_t now                  = time_point_sec(current_time_point()).sec_since_epoch();
      uint32_t created_timestamp    = created_at.sec_since_epoch();
      uint32_t term_ended_timestamp = term_ended_at.sec_since_epoch();
      uint32_t collect_timestamp = now > term_ended_timestamp ? term_ended_timestamp : now;
      ASSERT( (collect_timestamp - created_timestamp) >= 0 )
      ASSERT( (term_ended_timestamp - created_timestamp) > 0 )

      double ratio = double(collect_timestamp - created_timestamp) / double(term_ended_timestamp - created_timestamp);
      int64_t interest = ratio * interest_alloted.amount - interest_collected.amount;
      return asset(interest, interest_alloted.symbol); 
    }

    typedef multi_index<"saveaccounts"_n, save_account_t> tbl_t;

    EOSLIB_SERIALIZE( save_account_t,   (id)(plan_id)(pledged)
                                        (lquidity_extsym)(quotas)
                                        (plan_term_days)(interest_alloted)
                                        (interest_collected)(term_ended_at)
                                        (last_collected_at)(created_at) )

};

} //namespace amax
