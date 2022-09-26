#include <amax.save/amax.save.hpp>
#include <utils.hpp>

static constexpr eosio::name active_permission{"active"_n};


namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }


   inline int64_t get_precision(const symbol &s) {
      int64_t digit = s.precision();
      CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
      return calc_precision(digit);
   }


   void amax_save::init(eosio::symbol pay_symbol, name bank_contract) {
      CHECK(false, "not allowed" )
      require_auth( _self );
      
      // _gstate.admin                 = "armoniaadmin"_n;
   }

   void amax_save::withdraw(const name& issuer, const name& owner) {

   }

   void amax_save::collectint(const name& issuer, const name& owner) {

   }

   void amax_save::splitshare(const name& issuer, const name& owner) {

   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: two formats:
    *       1) refuel:$plan_id      -- by admin to deposit interest quantity
    *       2) deposit:$plan_id     -- by saver to deposit saving quantity
    *
    */
   void amax_save::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );

      if (from == get_self() || to != get_self()) return;

      auto token_bank = get_first_receiver();
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )
      vector<string_view> memo_params = split(memo, ":");

      if( memo_params[0] == "refuel" ) {
         auto plan_id = to_uint64(memo_params[1], "refuel plan");
         auto save_plan = save_plan_t( plan_id );
         CHECKC( _db.get( save_plan ), err::RECORD_NOT_FOUND, "plan id not found: " + to_string( plan_id ) )

         save_plan.plan_info.interest_available += extended_asset(quant.amount, extended_symbol(quant.symbol, token_bank));
         _db.set( save_plan );

      } else if (memo_params[0] == "deposit") {
         auto plan_id = to_uint64(memo_params[1], "deposit plan");
         auto save_plan = save_plan_t( plan_id );
         CHECKC( _db.get( save_plan ), err::RECORD_NOT_FOUND, "plan id not found: " + to_string( plan_id ) )

         save_plan.plan_info.deposit_available += extended_asset(quant.amount, extended_symbol(quant.symbol, token_bank));
         _db.set( save_plan );

      } else {
         CHECKC(false, err::MEMO_FORMAT_ERROR, "memo format err" )
      }
    
   }

   void amax_save::setplan(const uint64_t& pid, const plan_conf_s& pc) {
      require_auth( _gstate.admin );

      auto plan = save_plan_t(pid);
      _db.get( plan );

      plan.plan_info.plan_conf.principal_token       = pc.principal_token;
      plan.plan_info.plan_conf.interest_token        = pc.interest_token;
      plan.plan_info.plan_conf.deposit_term_days     = pc.deposit_term_days;
      plan.plan_info.plan_conf.allow_advance_redeem  = pc.allow_advance_redeem;
      plan.plan_info.plan_conf.interest_rates        = pc.interest_rates;
      plan.plan_info.plan_conf.effective_from        = pc.effective_from;
      plan.plan_info.plan_conf.effective_to          = pc.effective_to;

      _db.set( plan );
   }

   void amax_save::delplan(const uint64_t& pid) {
      require_auth( _gstate.admin );

      auto plan = save_plan_t(pid);
      CHECKC(_db.get( plan ), err::RECORD_NOT_FOUND, "plan not exist: " + to_string(pid) )

      _db.del( plan );
   }

} //namespace amax