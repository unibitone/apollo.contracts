#pragma once

#include <amax.ntoken/amax.nasset.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <utils.hpp>

// #include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <type_traits>

namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

#define SAVE_TBL struct [[eosio::table, eosio::contract("amaxnft.mine")]]
#define GLOBAL_TBL(name) struct [[eosio::table(name), eosio::contract("amaxnft.mine")]]
static constexpr uint64_t DAY_SECONDS  = 24 * 60 * 60;
static constexpr uint64_t YEAR_SECONDS = 365 * 24 * 60 * 60;
static constexpr uint64_t YEAR_DAYS    = 365;

namespace campaign_status {
   static constexpr eosio::name INIT    = "init"_n;
   static constexpr eosio::name CREATED = "created"_n;
   static constexpr eosio::name APPEND  = "append"_n;
}; // namespace campaign_status

GLOBAL_TBL("global") global_t {
   name      admin                    = "nftone.admin"_n; // admin account
   uint64_t  last_save_id             = 1;   // last save_account_t.id
   uint64_t  last_campaign_id         = 1;   // last save_campaign_t.id
   uint8_t   nft_size_limit           = 50;  // nft id list limit
   uint8_t   plan_size_limit          = 1;   // plan day limit
   asset     campaign_create_fee      = asset(1'0000'0000, symbol("AMAX", 8)); // create campaign fee 
   set<name> nft_contracts            = { "amax.ntoken"_n };                   // supply nft contract
   set<name> interest_token_contracts = { "amax.token"_n, "amax.ntt"_n, "amax.mtoken"_n }; // supply interest token list

   EOSLIB_SERIALIZE(
         global_t,
         (admin)(last_save_id)(last_campaign_id)(nft_size_limit)(plan_size_limit)(campaign_create_fee)(nft_contracts)(interest_token_contracts))
};

typedef eosio::singleton<"global"_n, global_t> global_singleton;

struct quotas {
   uint32_t allocated_quotas; // total pledged quota
   uint32_t redeemed_quotas;  // total redeem quota

   quotas() : allocated_quotas(0), redeemed_quotas(0) {}
   EOSLIB_SERIALIZE(quotas, (allocated_quotas)(redeemed_quotas))
};

// scope: self
SAVE_TBL save_campaign_t {
   uint64_t                      id; // pk
   name                          sponsor;              // campaign creater
   string                        campaign_name_cn;     // campaign chinese name
   string                        campaign_name_en;     // campaign english name
   string                        campaign_pic_url_cn;  // campaign chinese picture
   string                        campaign_pic_url_en;  // campaign english picture
   map<extended_nsymbol, quotas> pledge_ntokens;       // supply pledge ntoken list
   extended_symbol               interest_symbol;      // interest symbol
   uint16_t                      plan_day;             // plan limit days
   asset                         plan_interest;        // plan interest asset
   uint32_t                      total_quotas;         // total quotas
   uint32_t                      quotas_purchased = 0; // purchased or pledged quotas
   asset                         interest_total;       // bonus total interest
   asset                         pre_interest;         // interest per share
   asset                         interest_collected;   // collected interest
   name                          status;     // campaign status (1)init : fee paid； (2)created : campaign content edited; (3)append : add interest token
   time_point_sec                begin_at;   // begin timestamp
   time_point_sec                end_at;     // end timestamp
   time_point_sec                created_at; // create timestamp

   save_campaign_t() {}
   save_campaign_t(const uint64_t& i) : id(i) {}

   uint64_t primary_key() const { return id; }
   uint64_t scope() const { return 0; }

   uint32_t calc_available_quotas() const { return total_quotas - quotas_purchased; }
   asset    calc_available_interest() const { return interest_total - interest_collected; }

   typedef multi_index<"minecampaign"_n, save_campaign_t> tbl_t;

   EOSLIB_SERIALIZE(
         save_campaign_t,
         (id)(sponsor)(campaign_name_cn)(campaign_name_en)(campaign_pic_url_cn)(campaign_pic_url_en)
         (pledge_ntokens)(interest_symbol)(plan_day)(plan_interest)(total_quotas)(quotas_purchased)(interest_total)
         (pre_interest)(interest_collected)(status)(begin_at)(end_at)(created_at))
};

// Scope: account
// Note: record will be deleted upon withdrawal/redemption
SAVE_TBL save_account_t {
   uint64_t        id; // PK
   uint64_t        campaign_id;       // save_campaign_t.id
   extended_nasset pledged;           // amount == quotas
   asset           save_pre_interest; // save interest per share
   asset           interest_collected; // collected interest
   time_point_sec  term_ended_at;      // redeemable timestamp
   time_point_sec  last_collected_at;  // last collected timestamp
   time_point_sec  created_at;         // create timestamp

   save_account_t() {}
   save_account_t(const uint64_t& i) : id(i) {}

   uint64_t primary_key() const { return id; }

   // (save_campaign_t.pre_interest - save_pre_interest) * extended_nasset - interest_collected
   asset calc_due_interest(const asset& camp_pre_interest) const {
      auto interest = (camp_pre_interest - save_pre_interest) * pledged.quantity.amount - interest_collected;
      return interest;
   }

   typedef multi_index<"mineaccounts"_n, save_account_t> tbl_t;

   EOSLIB_SERIALIZE(save_account_t,
                    (id)(campaign_id)(pledged)(save_pre_interest)(interest_collected)(term_ended_at)(last_collected_at)(created_at))
};

} // namespace amax
