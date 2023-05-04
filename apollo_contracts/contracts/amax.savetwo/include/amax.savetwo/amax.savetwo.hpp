#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>
#include <string>
#include <amax.savetwo/amax.savetwo.db.hpp>
#include <wasm_db.hpp>

namespace amax {

using std::string;
using std::vector;

using namespace eosio;
using namespace wasm::db;

static constexpr name      SYS_BANK         = "amax.token"_n;
static constexpr symbol    AMAX_SYMBOL      = symbol(symbol_code("AMAX"), 8);
static constexpr name      APLINK_FARM      = "aplink.farm"_n;
static constexpr symbol    APLINK_SYMBOL    = symbol("APL", 4);
static asset APL_LIMIT                      = asset(0, symbol("APL", 4));

enum class amaxsavetwo_err: uint8_t {
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

class [[eosio::contract("amaxsavetwo1")]] amax_savetwo : public contract {
   public:
      using contract::contract;

   amax_savetwo(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
        _global(get_self(), get_self().value), _db(_self)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~amax_savetwo() { 
      _global.set( _gstate, get_self() ); 
    }

  ACTION init();
  
  
  ACTION createplan(const string &plan_name, 
                                const name &type, 
                                const extended_symbol &stake_symbol,
                                const extended_symbol &interest_symbol,
                                const uint16_t &plan_days, 
                                const asset &plan_profits,
                                const uint32_t &total_quotas,
                                const asset &stake_per_quota,
                                const asset &apl_per_quota,
                                const uint32_t &begin_at,
                                const uint32_t &end_at);
  
  /**
  * @brief set plan
  *
  * @param plan_id  plan id.
  * @param plan_name  plan name.
  * @param end_at  plan end time.
  * @param total_quotas  total quotas.
  * @param apl_per_quota  apl per quota.
  */
  
  ACTION setplan(const uint64_t &plan_id,
                      const string &plan_name, 
                      const uint32_t &total_quotas,
                      const asset &apl_per_quota,
                      const uint32_t &end_at);
                      
  ACTION setstatus(const uint64_t &plan_id, const name &status);
  
  ACTION delplan(const uint64_t& plan_id);
  
  [[eosio::on_notify("*::transfer")]]
  void ontransfer(const name &from,
                  const name &to,
                  const asset &quantity,
                  const string &memo);
  /**
  * @brief user claim interest
  *
  * @param issuer  users participating in the plan.
  * @param owner  users participating in the plan.
  * @param save_id  save account id.
  */
  ACTION collectint(const name& issuer, const name& owner, const uint64_t& save_id);
  
  /**
  * @brief user redeem nft
  *
  * @param issuer  users participating in the plan.
  * @param owner  users participating in the plan.
  * @param save_id  save account id.
  */
  ACTION redeem(const name& issuer, const name& owner, const uint64_t& save_id);
  

  ACTION intcolllog(const name& account, const uint64_t& account_id, const uint64_t& plan_id, const asset &quantity);
  using interest_collect_log_action = eosio::action_wrapper<"intcolllog"_n, &amax_savetwo::intcolllog>; 
  
  private:
      global_singleton     _global;
      global_t             _gstate;
      dbc                  _db;
      
      void _create_plan( const string &plan_name, 
                              const name &type, 
                              const extended_symbol &stake_symbol,
                              const extended_symbol &interest_symbol,
                              const uint16_t &plan_days, 
                              const asset &plan_profits,
                              const uint32_t &total_quotas,
                              const asset &stake_per_quota,
                              const asset &apl_per_quota,
                              const uint32_t &begin_at,
                              const uint32_t &end_at);

      void _create_save_act(save_plan_t &plan,
                            const asset &quantity,
                            const name &from,                                 
                            const uint64_t &days,
                            const uint32_t &quotas,
                            const time_point_sec &now);
                            
      void _allot_apl(asset apl, const name& from, const uint64_t& sid);  
                                                                                                     
      void _int_coll_log(const name& account, const uint64_t& account_id, const uint64_t& plan_id, const asset &quantity);
      
};
} //namespace amax
