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
static set<name>  whitelist   = {"testuser1"_n, "merchantx"_n, "frank12345o"_n, "aplobrtfltwg"_n, "111111111112"_n, "amaxtestt111"_n, "aplio1lv1vit"_n, "user1"_n, "12345abcde12"_n};

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

class [[eosio::contract("nftonesave12")]] nftone_save : public contract {
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
  * @param ntoken_contract  nft issued by contract that can be used to participate in campaign.
  * @param profit_token_contract  tokens issued by contract that can be used as a benefit.
  */
  ACTION init( const set<name> &ntoken_contract, 
                const set<name> &profit_token_contract, 
                const uint8_t &nft_size_limit, 
                const uint8_t &plan_size_limit);
  
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
  ACTION setcampaign(const name &sponsor,
                    const uint64_t &campaign_id, 
                    const vector<uint64_t> &nftids, 
                    const vector<uint16_t> &plan_days_list, 
                    const vector<asset> &plan_profits_list,
                    const name &ntoken_contract,
                    const uint32_t &total_quotas,
                    const string &campaign_name_cn,
                    const string &campaign_name_en,
                    const string &campaign_pic_url,
                    const uint32_t &begin_at,
                    const uint32_t &end_at);
  
  /**
  * @brief user claim interest
  *
  * @param issuer  users participating in the campaign.
  * @param owner  users participating in the campaign.
  * @param save_id  save account id.
  */
  ACTION collectint(const name& issuer, const name& owner, const uint64_t& save_id);
  
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
  ACTION cancelcamp(const name& issuer, const name& owner, const uint64_t& campaign_id);
  
  ACTION refundint(const name& issuer, const name& owner, const uint64_t& campaign_id);
  
  ACTION delcampaign(const set<uint64_t>& campaign_ids);
  
  ACTION intcolllog(const name& account, const uint64_t& account_id, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at);
  using interest_collect_log_action = eosio::action_wrapper<"intcolllog"_n, &nftone_save::intcolllog>; 
  
  private:
      global_singleton     _global;
      global_t             _gstate;
      dbc                  _db;
      
      void _on_token_transfer( const name &from,
                                  const name &to,
                                  const asset &quantity,
                                  const string &memo );

      void _on_ntoken_transfer( const name& from,
                                  const name& to,
                                  const std::vector<nasset>& assets,
                                  const string& memo );
                                  
      void _create_campaign( const name& from );
      
      bool _is_whitelist( const name& account );     
                            
      void _set_campaign( save_campaign_t &campaign, 
                          const vector<uint64_t> &nftids,  
                          const name &ntoken_contract,
                          const vector<uint16_t> &plan_days_list, 
                          const vector<asset> &plan_profits_list,
                          const uint32_t &total_quotas,
                          const string_view &campaign_name_cn,
                          const string_view &campaign_name_en,
                          const string_view &campaign_pic_url );
                                                                                                  
      void _int_coll_log(const name& account, const uint64_t& account_id, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at);
      
};
} //namespace amax
