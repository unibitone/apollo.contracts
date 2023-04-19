#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <amax.ntoken/amax.nasset.hpp>

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

#define SAVE_TBL struct [[eosio::table, eosio::contract("nftonesave11")]]
#define GLOBAL_TBL(name) struct [[eosio::table(name), eosio::contract("nftonesave11")]]
// static constexpr uint64_t  DAY_SECONDS = 24 * 60 * 60;
static constexpr uint64_t  DAY_SECONDS = 60;
static constexpr uint64_t  YEAR_SECONDS = 365 * 24 * 60 * 60;
static constexpr uint64_t  YEAR_DAYS   = 365;

namespace campaign_status {
    static constexpr eosio::name INIT               = "init"_n;
    static constexpr eosio::name CREATED            = "created"_n;
};

GLOBAL_TBL("global") global_t {
    name admin                              = "armoniaadmin"_n;
    uint64_t last_save_id                   = 0;
    uint64_t last_campaign_id               = 0;
    asset crt_campaign_fee                  = asset(1'0000'0000, symbol("AMAX", 8));                                      
    set<name> whitelist;
    set<name> ntoken_contract_required      = {"amax.ntoken"_n};
    set<name> profit_token_contract_required= {"amax.token"_n, "amax.ntt"_n, "amax.mtoken"_n};

    EOSLIB_SERIALIZE( global_t, (admin)(last_save_id)
                                (last_campaign_id)
                                (crt_campaign_fee)(whitelist)
                                (ntoken_contract_required)
                                (profit_token_contract_required) )

};

typedef eosio::singleton< "global"_n, global_t > global_singleton;


struct quotas {
    uint32_t                           allocated_quotas;
    uint32_t                           redeemed_quotas;          
    quotas(): allocated_quotas(0), redeemed_quotas(0){}
    EOSLIB_SERIALIZE( quotas,  (allocated_quotas)(redeemed_quotas))
};

//scope: self
SAVE_TBL save_campaign_t {
    uint64_t                          id;
    name                              sponsor;                     
    string                            campaign_name;                
    string                            campaign_en_name;            //campaign english name
    string                            campaign_pic;                //campaign picture
    map<extended_nsymbol, quotas>     pledge_ntokens;              //pledge ntokens
    extended_symbol                   interest_symbol;
    map<uint16_t, asset>              plans;
    uint32_t                          total_quotas;
    uint32_t                          quotas_purchased = 0;
    asset                             interest_total;              //prestore total interest
    asset                             interest_frozen;             //account total interest
    asset                             interest_claimed;
    name                              status;                     //campaign status (1)init : fee paid； (2)created : interest transferred
    time_point_sec                    begin_at;             
    time_point_sec                    end_at;               
    time_point_sec                    created_at;    
    save_campaign_t() {}
    save_campaign_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t scope()const { return 0; }
    
    uint32_t get_available_quotas()const { return total_quotas - quotas_purchased; }
    asset    get_available_interest()const { return interest_total - interest_claimed; }
    asset    get_refund_interest()const { return interest_total - interest_frozen; }

    typedef multi_index<"savecampaign"_n, save_campaign_t > tbl_t;

    EOSLIB_SERIALIZE( save_campaign_t,  (id)(sponsor)(campaign_name)
                                    (campaign_en_name)(campaign_pic)
                                    (pledge_ntokens)(interest_symbol)(plans)
                                    (total_quotas)(quotas_purchased)(interest_total)
                                    (interest_frozen)(interest_claimed)(status) 
                                    (begin_at)(end_at)(created_at) )

};

//Scope: account
//Note: record will be deleted upon withdrawal/redemption
SAVE_TBL save_account_t {
    uint64_t            id;                   //PK
    uint64_t            campaign_id;
    extended_nasset     pledged;              //amount == quotas
    asset               interest_per_quota;   //total interest per quota
    uint16_t            plan_days;            //pledge days
    asset               total_interest;       //total interest
    asset               interest_claimed;
    time_point_sec      created_at;
    time_point_sec      term_ended_at;
    time_point_sec      last_claimed_at;
  
    save_account_t() {}
    save_account_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t by_campaign()const { return campaign_id; }
        
    double get_sec_ratio()const { 
      uint32_t now = time_point_sec(current_time_point()).sec_since_epoch();
      return (now - created_at.sec_since_epoch()) / (term_ended_at.sec_since_epoch() - created_at.sec_since_epoch());
    }
    
    // (current - created_at)/(term_ended_at - created_at) * total_interest - interest_claimed
    asset get_due_interest()const { 
      int64_t interest = get_sec_ratio() * total_interest.amount - interest_claimed.amount;
      return asset(interest, total_interest.symbol); 
    }

    typedef multi_index<"saveaccounts"_n, save_account_t,
        indexed_by<"campaignid"_n, const_mem_fun<save_account_t, uint64_t, &save_account_t::by_campaign> >
    > tbl_t;

    EOSLIB_SERIALIZE( save_account_t,   (id)(campaign_id)(pledged)(interest_per_quota)(plan_days)(total_interest)
                                        (interest_claimed)(created_at)(term_ended_at)(last_claimed_at) )

};

} //namespace amax
