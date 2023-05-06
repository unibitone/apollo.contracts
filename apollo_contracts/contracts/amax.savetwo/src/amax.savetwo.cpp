#include <amax.savetwo/amax.savetwo.hpp>
#include "safemath.hpp"
#include <utils.hpp>
#include <aplink.farm/aplink.farm.hpp>
#include <amax.token.hpp>

static constexpr eosio::name active_permission{"active"_n};


namespace amax {

using namespace std;
using namespace wasm::safemath;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }

#define NTOKEN_TRANSFER(bank, to, quantity, memo) \
{ action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }

#define ALLOT(bank, land_id, customer, quantity, memo) \
    {	aplink::farm::allot_action act{ bank, { {_self, active_perm} } };\
			act.send( land_id, customer, quantity , memo );}

  void amax_savetwo::init(const uint64_t &farm_id) {
      require_auth( _self );
      _gstate.farm_lease_id = farm_id;
  }

  void amax_savetwo::createplan(const string &plan_name, 
                                const name &type, 
                                const extended_symbol &stake_symbol,
                                const extended_symbol &interest_symbol,
                                const int32_t &plan_days, 
                                const asset &plan_profit,
                                const int64_t &total_quotas,
                                const asset &stake_per_quota,
                                const asset &apl_per_quota,
                                const uint32_t &begin_at,
                                const uint32_t &end_at) {
      require_auth(_gstate.admin);
      CHECKC( end_at > begin_at, err::PARAM_ERROR, "begin time should be less than end time");
      CHECKC( end_at - begin_at <= (YEAR_SECONDS * 3), err::PARAM_ERROR, "the duration of the plan cannot exceed 3 years");
      CHECKC( end_at > current_time_point().sec_since_epoch(), err::PARAM_ERROR, "begin time should be less than end time");
      
      CHECKC( total_quotas > 0 , err::PARAM_ERROR, "quotas should be more than zero" );
      CHECKC( stake_per_quota.amount > 0 , err::PARAM_ERROR, "stake asset amount should be more than zero" );
      CHECKC( stake_symbol.get_symbol() == stake_per_quota.symbol, err::PARAM_ERROR, "stake asset symbol mismatch" );
      CHECKC( apl_per_quota >= APL_LIMIT , err::PARAM_ERROR, "apl should be more than zero " );
      CHECKC( plan_days > 0, err::PARAM_ERROR, "plan days must be greater than 0" )
      CHECKC( plan_profit.amount > 0, err::PARAM_ERROR, "plan profit must be greater than 0" )
      CHECKC( plan_profit.symbol == interest_symbol.get_symbol(), err::PARAM_ERROR, "profit asset symbol mismatch" )
      CHECKC( plan_name.size() < 128 && plan_name.size() > 0, err::PARAM_ERROR, "plan name greater than 0 bytes and less than 128 bytes" );
   
      asset stake_supply = token::get_supply(stake_symbol.get_contract(), stake_symbol.get_symbol().code());
      CHECKC( stake_supply.amount > 0, err::SYMBOL_MISMATCH, "stake token not exists" );
    
      asset interest_supply = token::get_supply(interest_symbol.get_contract(), interest_symbol.get_symbol().code());
      CHECKC( interest_supply.amount > 0, err::SYMBOL_MISMATCH, "interest token not exists" );
    
      _create_plan(plan_name, type, stake_symbol, interest_symbol, 
                        plan_days, plan_profit, total_quotas, stake_per_quota, 
                        apl_per_quota, begin_at, end_at);
  }

  void amax_savetwo::setplan(const uint64_t &plan_id,
                                const string &plan_name, 
                                const int64_t &total_quotas,
                                const asset &apl_per_quota,
                                const uint32_t &end_at) {
      require_auth(_gstate.admin);
      
      save_plan_t plan(plan_id);
      CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan not found: " + to_string( plan_id ) )
      
      CHECKC( end_at > plan.begin_at.sec_since_epoch(), err::PARAM_ERROR, "begin time should be less than end time");
      CHECKC( end_at - plan.begin_at.sec_since_epoch() <= (YEAR_SECONDS * 3), err::PARAM_ERROR, "the duration of the plan cannot exceed 3 years");
      CHECKC( plan.total_quotas <= total_quotas, err::PARAM_ERROR, "total_quotas must be greater than or equal to before" );
      CHECKC( plan_name.size() < 128 && plan_name.size() > 0, err::PARAM_ERROR, "plan name greater than 0 bytes and less than 128 bytes" );
      CHECKC( apl_per_quota >= APL_LIMIT , err::PARAM_ERROR, "apl should be more than zero " );
      
      plan.plan_name        = plan_name;
      plan.total_quotas     = total_quotas;
      plan.apl_per_quota    = apl_per_quota;
      plan.end_at           = time_point_sec(end_at);
      _db.set(plan);
  }

  void amax_savetwo::setstatus(const uint64_t &plan_id, const name &status) {
      require_auth( _gstate.admin );
      save_plan_t plan(plan_id);
      CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan not found: " + to_string( plan_id ) )
      CHECKC( status == plan_status::BLOCKED || status == plan_status::RUNNING || status == plan_status::SUSPENDED, err::STATE_MISMATCH, "state mismatch");
      plan.status = status;
      _db.set( plan );
  }
  
  void amax_savetwo::delplan(const uint64_t& plan_id) {
      require_auth( _gstate.admin );
      
      save_plan_t plan(plan_id);
      CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan not found: " + to_string( plan_id ) )
      CHECKC( plan.begin_at > current_time_point(), amaxsavetwo_err::STARTED, "plan already started" )
      
      _db.del( plan );
  }
  
  /**
  * @brief transfer token
  *
  * @param from
  * @param to
  * @param quantity
  * @param memo: three formats:
  *       1) refuel : $plan_id                    -- increment interest
  *       2) pledge : $plan_id : quotas   -- gain interest by pledge token 
  */
  void amax_savetwo::ontransfer(const name &from,
                                const name &to,
                                const asset &quantity,
                                const string &memo) {
      if (from == _self || to != _self) return;

      auto parts = split( memo, ":" );

      if ( parts.size() == 2 && parts[0] == "refuelint" ) {
          uint64_t plan_id = to_uint64(parts[1], "plan_id parse int error");
          save_plan_t plan(plan_id);
          CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan not found: " + to_string( plan_id ) )   
           
          CHECKC( quantity.amount > 0, err::PARAM_ERROR, "token amount invalid" )      
          CHECKC( quantity.symbol == plan.interest_symbol.get_symbol(), err::PARAM_ERROR, "token symbol invalid" )
          CHECKC( get_first_receiver() == plan.interest_symbol.get_contract(), err::PARAM_ERROR, "token contract invalid" )

          plan.interest_total       += quantity;
          _db.set(plan);
          
      } else if ( parts.size() == 3 && parts[0] == "pledge" )  {
          auto now  = time_point_sec(current_time_point());
          
          auto plan_id  = to_uint64(parts[1], "plan_id parse int error");
          auto quotas   = to_uint64(parts[2], "quotas parse uint error");

          save_plan_t plan(plan_id);
          CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan not found: " + to_string( plan_id ) ) 
          CHECKC( plan.status == plan_status::RUNNING, err::PAUSED, "temporarily suspended" )
   
          CHECKC( quantity.amount > 0, err::PARAM_ERROR, "token amount invalid" )      
          CHECKC( quotas > 0 && quantity / quotas >= plan.stake_per_quota, err::PARAM_ERROR, "token amount invalid" )      
          CHECKC( quantity.symbol == plan.stake_symbol.get_symbol(), err::PARAM_ERROR, "token symbol invalid" )
          CHECKC( get_first_receiver() == plan.stake_symbol.get_contract(), err::PARAM_ERROR, "token contract invalid" )
          CHECKC( plan.calc_available_quotas() > 0 && quotas < plan.calc_available_quotas(), amaxsavetwo_err::QUOTAS_INSUFFICIENT, "quotas insufficient" )
          CHECKC( plan.end_at >= now, amaxsavetwo_err::ENDED, "the plan already ended" )
          CHECKC( plan.begin_at <= now, amaxsavetwo_err::NOT_START, "the plan not start" )

          _create_save_act(plan, quantity, from, quotas, now);
          
      } else {
          CHECKC( false, err::PARAM_ERROR, "param error" );
      }
  }
  
  void amax_savetwo::collectint(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );

      save_account_t save_acct( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )

      auto now = current_time_point();
      CHECKC( save_acct.last_collected_at < save_acct.term_ended_at, amaxsavetwo_err::INTEREST_COLLECTED, "interest already collected" )

      auto elapsed_sec = now.sec_since_epoch() - save_acct.last_collected_at.sec_since_epoch();
 
      CHECKC( elapsed_sec > DAY_SECONDS, amaxsavetwo_err::TIME_PREMATURE, "less than 24 hours since last interest collection time" )
      
      save_plan_t plan( save_acct.plan_id );
      CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan not found: " + to_string( save_acct.plan_id ) )

      auto interest_due = save_acct.calc_due_interest();
      CHECKC( interest_due.amount > 0, err::NOT_POSITIVE, "interest due amount is zero" )
      CHECKC( plan.calc_available_interest() >= interest_due, err::NOT_POSITIVE, "insufficient available interest to collect" )
      
      TRANSFER( plan.interest_symbol.get_contract(), owner, interest_due, "interest: " + to_string(save_id) )
      
      save_acct.interest_collected    += interest_due;
      save_acct.last_collected_at     = now;
      _db.set( owner.value, save_acct );

      plan.interest_collected     += interest_due;
      _db.set( plan );

      _int_coll_log(owner, save_acct.id, plan.id, interest_due);
  }
  
  void amax_savetwo::redeem(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );
      
      if ( issuer != owner ) {
          CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to redeem others saving account" )
      }

      auto save_acct = save_account_t( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )

      auto plan = save_plan_t( save_acct.plan_id );
      CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan not found: " + to_string(save_acct.plan_id) )
      CHECKC( plan.status == plan_status::RUNNING || plan.status == plan_status::SUSPENDED, err::PAUSED, "temporarily suspended" )

      CHECKC( save_acct.term_ended_at < current_time_point(), amaxsavetwo_err::TERM_NOT_ENDED, "term not ended" )
      CHECKC( save_acct.term_ended_at <= save_acct.last_collected_at, amaxsavetwo_err::INTEREST_NOT_COLLECTED, "interest not collected" )
      
      auto pledged_quant = save_acct.pledged;
      _db.del( owner.value, save_acct );
      
      TRANSFER( plan.stake_symbol.get_contract(), owner, pledged_quant, "redeem: " + to_string(save_id) )
  }
  
  void amax_savetwo::intcolllog(const name& account, const uint64_t& account_id, const uint64_t& plan_id, const asset &quantity) {
      require_auth(get_self());
      require_recipient(account);
  }

  void amax_savetwo::_create_plan( const string &plan_name, 
                                        const name &type, 
                                        const extended_symbol &stake_symbol,
                                        const extended_symbol &interest_symbol,
                                        const uint16_t &plan_days, 
                                        const asset &plan_profit,
                                        const uint32_t &total_quotas,
                                        const asset &stake_per_quota,
                                        const asset &apl_per_quota,
                                        const uint32_t &begin_at,
                                        const uint32_t &end_at) {
      auto cid = _gstate.last_plan_id++;
      save_plan_t plan(cid);

      plan.plan_name          = plan_name;
      plan.plan_days          = plan_days;
      plan.plan_profit        = plan_profit;
      plan.stake_symbol       = stake_symbol;
      plan.interest_symbol    = interest_symbol;
      plan.total_quotas       = total_quotas;
      plan.stake_per_quota    = stake_per_quota;
      plan.apl_per_quota      = apl_per_quota;
      plan.begin_at           = time_point_sec(begin_at);
      plan.end_at             = time_point_sec(end_at);
      plan.interest_total     = asset(0, interest_symbol.get_symbol());
      plan.interest_collected = asset(0, interest_symbol.get_symbol());
      plan.created_at         = current_time_point();
      _db.set(plan);
  }
  
  void amax_savetwo::_create_save_act(save_plan_t &plan,
                                      const asset &quantity,
                                      const name &from,                                 
                                      const uint32_t &quotas,
                                      const time_point_sec &now) {
      auto sid = _gstate.last_save_id++;
      save_account_t save_acct(sid);
      save_acct.plan_id                     = plan.id;
      save_acct.pledged                     = quantity;
      save_acct.quotas                      = quotas;
      save_acct.plan_term_days              = plan.plan_days;
      save_acct.interest_alloted            = plan.plan_profit * quotas;
      save_acct.interest_collected          = asset(0, plan.interest_symbol.get_symbol());
      save_acct.term_ended_at               = now + plan.plan_days * DAY_SECONDS;
      save_acct.created_at                  = now;
      save_acct.last_collected_at           = now;
      _db.set( from.value, save_acct, false );
      
      plan.quotas_purchased += quotas;
      _db.set( plan );
      
      if(_gstate.farm_lease_id > 0 && plan.apl_per_quota.amount > 0){
         asset apl = plan.apl_per_quota * quotas;
         _allot_apl(apl, from, sid);

      }
  }
    
  void amax_savetwo::_allot_apl(asset apl, const name& from, const uint64_t& sid) {
      
      asset apples = asset(0, APLINK_SYMBOL);
      name  parent = get_account_creator(from);
      aplink::farm::available_apples(APLINK_FARM, _gstate.farm_lease_id, apples);
      
      if (apples >= apl && apl.amount > 0) {
          ALLOT(  APLINK_FARM, _gstate.farm_lease_id,
                  parent, apl,
                  "amax save #2 allot: " + to_string( sid ));
      }
  }
  
  void amax_savetwo::_int_coll_log(const name& account, const uint64_t& account_id, const uint64_t& plan_id, const asset &quantity) {
      amax_savetwo::interest_collect_log_action act{ _self, { {_self, active_permission} } };
      act.send( account, account_id, plan_id, quantity );
  }

} //namespace amax