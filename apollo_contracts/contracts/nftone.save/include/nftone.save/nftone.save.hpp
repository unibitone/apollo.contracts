#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>

#include <string>

#include <nftone.save/nftone.save.db.hpp>
#include <wasm_db.hpp>
namespace amax {

using std::string;
using std::vector;

using namespace eosio;
using namespace wasm::db;

static constexpr name      SYS_BANK    = "amax.token"_n;
static constexpr symbol    AMAX_SYMBOL = symbol(symbol_code("AMAX"), 8);

enum class save_err: uint8_t {
   INTEREST_INSUFFICIENT    = 0,
   QUOTAS_INSUFFICIENT      = 1,
   INTEREST_COLLECTED       = 2,
   TERM_NOT_ENDED           = 3,
   INTEREST_NOT_COLLECTED   = 4,
   TIME_PREMATURE           = 5,
   ENDED                    = 6,
   NOT_START                = 7,
   STARTED                  = 8,
   NOT_ENDED                = 9,
   NOT_EMPTY                = 10
};

class [[eosio::contract("nftonesave11")]] nftone_save : public contract {
   public:
      using contract::contract;

   nftone_save(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
        _global(get_self(), get_self().value), _db(_self)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~nftone_save() { 
      _global.set( _gstate, get_self() ); 
    }
  /**
  * @brief set global
  *
  * @param account  the account that can create campaign
  * @param ntoken_contract  nft issued by contract that can be used to participate in campaign.
  * @param profit_token_contract  tokens issued by contract that can be used as a benefit.
  */
  ACTION setglobal(const set<name> &account, const set<name> &ntoken_contract, const set<name> &profit_token_contract ); 
  
  [[eosio::on_notify("*::transfer")]]
  void ontransfer();
  
  /**
  * @brief set campaign
  *
  * @param sponsor  campaign sponsor.
  * @param campaign_id  campaign id.
  * @param nftids  used to pledge the nft.
  * @param end_at  campaign end time.
  * @param plan_days_list  set of planned days.
  * @param plan_profits_list  set of planned profits.
  * @param ntoken_contract   nft issued by contract that can be used to participate in campaign.
  */
  ACTION setcampaign(const name &sponsor, const uint64_t &campaign_id, 
                                vector<uint64_t> &nftids, const time_point_sec &end_at, 
                                vector<uint16_t> &plan_days_list, 
                                vector<asset> &plan_profits_list,
                                const name &ntoken_contract,
                                const uint32_t &total_quotas);
  
  /**
  * @brief user claim interest
  *
  * @param issuer  users participating in the campaign.
  * @param owner  users participating in the campaign.
  * @param save_id  save account id.
  */
  ACTION claim(const name& issuer, const name& owner, const uint64_t& save_id);
  
  /**
  * @brief user redeem nft
  *
  * @param issuer  users participating in the campaign.
  * @param owner  users participating in the campaign.
  * @param save_id  save account id.
  */
  ACTION redeem(const name& issuer, const name& owner, const uint64_t& save_id);
  
  /**
  * @brief sponsor cancel campaign
  *
  * @param issuer  users participating in the campaign.
  * @param owner  users participating in the campaign.
  * @param campaign_id  campaign id.
  */
  ACTION cnlcampaign(const name& issuer, const name& owner, const uint64_t& campaign_id);
  
  ACTION refundint(const name& issuer, const name& owner, const uint64_t& campaign_id);
  
  ACTION intrefuellog(const name& refueller,const uint64_t& campaign_id, const asset &quantity, const time_point& created_at);
  using intrefuellog_action = eosio::action_wrapper<"intrefuellog"_n, &nftone_save::intrefuellog>; 

  ACTION intcolllog(const name& account, const uint64_t& account_id, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at);
  using interest_withdraw_log_action = eosio::action_wrapper<"intcolllog"_n, &nftone_save::intcolllog>; 

  private:
      global_singleton     _global;
      global_t             _gstate;
      dbc                  _db;
      
      void _on_token_transfer( const name &from,
                                  const name &to,
                                  const asset &quantity,
                                  const string &memo);

      void _on_ntoken_transfer( const name& from,
                                  const name& to,
                                  const std::vector<nasset>& assets,
                                  const string& memo );
                                  
      void _pre_create_campaign( const name& from,
                                  const string_view& campaign_name,
                                  const string_view& campaign_en_name,
                                  const string_view& campaign_pic,
                                  const uint64_t& begin,
                                  const uint64_t& end);
                                          
      // void _build_plan( map<uint16_t, asset>& plans_tmp,
      //                   const symbol& interest_symbol,
      //                   vector<string_view>& plan_days_list,
      //                   vector<string_view>& plan_profits_list,
      //                   asset& max_profit_token,
      //                   uint64_t& max_days);
                                       
      // void _build_pledge_ntokens( map<extended_nsymbol, quotas>& pledge_ntokens_tmp,
      //                             const name& ntoken_contract,
      //                             vector<string_view>& nftids);   
                                                                    
      void _int_refuel_log(const name& refueller, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at);

      void _int_coll_log(const name& account, const uint64_t& account_id, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at);

};
} //namespace amax
