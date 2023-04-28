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
static constexpr uint64_t  YEAR_SECONDS = 365 * 24 * 60 * 60;
static constexpr uint64_t  YEAR_DAYS   = 365;

namespace campaign_status {
    static constexpr eosio::name INIT               = "init"_n;
    static constexpr eosio::name CREATED            = "created"_n;
    static constexpr eosio::name REFUNDED           = "refunded"_n;
};

GLOBAL_TBL("global") global_t {
    name admin                              = "nftone.admin"_n;
    uint64_t last_save_id                   = 0;
    uint64_t last_campaign_id               = 0;
    uint8_t nft_size_limit                  = 5;
    uint8_t plan_size_limit                 = 5;
    asset campaign_create_fee               = asset(1'0000'0000, symbol("AMAX", 8));                                      
    set<name> nft_contracts                 = {"amax.ntoken"_n};
    set<name> interest_token_contracts      = {"amax.token"_n, "amax.ntt"_n, "amax.mtoken"_n};

    EOSLIB_SERIALIZE( global_t, (admin)(last_save_id)(last_campaign_id)
                                (nft_size_limit)(plan_size_limit)
                                (campaign_create_fee)
                                (nft_contracts)
                                (interest_token_contracts) )

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
    string                            campaign_name_cn;                
    string                            campaign_name_en;            //campaign english name
    string                            campaign_pic_url;            //campaign picture
    map<extended_nsymbol, quotas>     pledge_ntokens;              //pledge ntokens
    extended_symbol                   interest_symbol;
    map<uint16_t, asset>              plans;                       //key:days, value:interest
    uint32_t                          total_quotas;
    uint32_t                          quotas_purchased = 0;
    asset                             interest_total;              //prestore total interest
    asset                             interest_alloted;            //account total interest
    asset                             interest_collected;
    name                              status;                     //campaign status (1)init : fee paid； (2)created : interest transferred
    time_point_sec                    begin_at;             
    time_point_sec                    end_at;               
    time_point_sec                    created_at;    
    save_campaign_t() {}
    save_campaign_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t scope()const { return 0; }
    
    uint32_t calc_available_quotas()  const { return total_quotas   - quotas_purchased; }
    asset    calc_available_interest()const { return interest_total - interest_collected; }
    asset    calc_refund_interest()   const { return interest_total - interest_alloted; }

    typedef multi_index<"savecampaign"_n, save_campaign_t > tbl_t;

    EOSLIB_SERIALIZE( save_campaign_t,  (id)(sponsor)(campaign_name_cn)
                                    (campaign_name_en)(campaign_pic_url)
                                    (pledge_ntokens)(interest_symbol)(plans)
                                    (total_quotas)(quotas_purchased)(interest_total)
                                    (interest_alloted)(interest_collected)(status) 
                                    (begin_at)(end_at)(created_at) )

};

//Scope: account
//Note: record will be deleted upon withdrawal/redemption
SAVE_TBL save_account_t {
    uint64_t            id;                   //PK
    uint64_t            campaign_id;
    extended_nasset     pledged;              //amount == quotas
    uint16_t            plan_term_days;       //pledge days
    asset               interest_alloted;     //alloted interest
    asset               interest_collected;
    time_point_sec      term_ended_at;
    time_point_sec      last_collected_at;
    time_point_sec      created_at;

    save_account_t() {}
    save_account_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    // (current - created_at)/(term_ended_at - created_at) * total_interest - interest_collected
    asset calc_due_interest()const { 
      uint32_t now                  = time_point_sec(current_time_point()).sec_since_epoch();
      uint32_t created_timestamp    = created_at.sec_since_epoch();
      uint32_t term_ended_timestamp = term_ended_at.sec_since_epoch();
      uint32_t collect_timestamp = now > term_ended_timestamp ? term_ended_timestamp : now;
      
      double ratio = double(collect_timestamp - created_timestamp) / double(term_ended_timestamp - created_timestamp);
      int64_t interest = ratio * interest_alloted.amount - interest_collected.amount;
      return asset(interest, interest_alloted.symbol); 
    }

    typedef multi_index<"saveaccounts"_n, save_account_t> tbl_t;

    EOSLIB_SERIALIZE( save_account_t,   (id)(campaign_id)(pledged)(plan_term_days)(interest_alloted)
                                        (interest_collected)(term_ended_at)(last_collected_at)(created_at) )

};

} //namespace amax
