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

#define SAVE_TBL struct [[eosio::table, eosio::contract("nftone.save")]]
#define GLOBAL_TBL(name) struct [[eosio::table(name), eosio::contract("nftone.save")]]
static constexpr uint64_t  DAY_SECONDS = 24 * 60 * 60;
static constexpr uint64_t  YEAR_DAYS   = 365;

GLOBAL_TBL("global") global_t {
    name admin                              = "armoniaadmin"_n;
    uint64_t last_save_id                   = 0;
    uint64_t last_campaign_id               = 0;
    set<name> ntoken_contract_required;
    set<extended_symbol> ntt_symbol_required;

    EOSLIB_SERIALIZE( global_t, (admin)(last_save_id)
                                (last_campaign_id)
                                (ntoken_contract_required)
                                (ntt_symbol_required) )

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
    string                            campaign_en_name;
    string                            campaign_pic;
    map<extended_nsymbol, quotas>     pledge_ntokens;              //pledge ntokens
    extended_symbol                   interest_symbol;
    map<uint16_t, asset>              plans;
    uint32_t                          total_quotas;
    uint32_t                          quotas_purchased = 0;
    asset                             interest_available;
    asset                             interest_redeemed;
    name                              status;                     
    time_point_sec                    begin_at;             
    time_point_sec                    end_at;               
    time_point_sec                    created_at;    
    save_campaign_t() {}
    save_campaign_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t scope()const { return 0; }
    
    uint32_t get_available_quotas()const { return total_quotas - quotas_purchased; }
    asset    get_total_interest()const { return interest_available + interest_redeemed; }

    typedef multi_index<"savecampaign"_n, save_campaign_t > tbl_t;

    EOSLIB_SERIALIZE( save_campaign_t,  (id)(sponsor)(campaign_name)
                                    (campaign_en_name)(campaign_pic)
                                    (pledge_ntokens)(interest_symbol)(plans)
                                    (total_quotas)(quotas_purchased)(interest_available)
                                    (interest_redeemed)(begin_at) 
                                    (end_at)(created_at) )

};

//Scope: account
//Note: record will be deleted upon withdrawal/redemption
SAVE_TBL save_account_t {
    uint64_t            id;               //PK
    uint64_t            campaign_id;
    extended_nasset     pledged;          //amount == quotas
    asset               daily_interest_per_quota;   //daily output interest per quota
    uint16_t            days;             //pledge days
    asset               interest_collected;
    time_point_sec      created_at;
    time_point_sec      term_ended_at;
    time_point_sec      last_collected_at;

    save_account_t() {}
    save_account_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t by_campaign()const { return campaign_id; }
    
    inline asset get_daily_interest()const { return daily_interest_per_quota * pledged.quantity.amount; }
    inline uint32_t get_term_sec()const { return current_time_point().sec_since_epoch() - created_at.sec_since_epoch(); }
    asset get_due_interest()const { return ( get_daily_interest() / DAY_SECONDS * get_term_sec() ) - interest_collected; }

    typedef multi_index<"saveaccounts"_n, save_account_t,
        indexed_by<"campaignid"_n, const_mem_fun<save_account_t, uint64_t, &save_account_t::by_campaign> >
    > tbl_t;

    EOSLIB_SERIALIZE( save_account_t,   (id)(campaign_id)(pledged)(daily_interest_per_quota)(days)(interest_collected)(created_at)
                                        (term_ended_at)(last_collected_at) )

};

} //namespace amax