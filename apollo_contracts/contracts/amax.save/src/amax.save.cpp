#include <amax.save/amax.save.hpp>
#include "safemath.hpp"
#include <utils.hpp>

#include <amax.token.hpp>

static constexpr eosio::name active_permission{"active"_n};


namespace amax {

using namespace std;
using namespace wasm::safemath;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }

   inline int64_t get_precision(const symbol &s) {
      int64_t digit = s.precision();
      CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
      return calc_precision(digit);
   }

   inline int64_t get_precision(const asset &a) {
      return get_precision(a.symbol);
   }

   inline uint64_t get_interest_rate( const uint64_t& deposit_amount ) {
         if( deposit_amount <= 1000 )  return 800;    //0.08 * 10000
         if( deposit_amount <= 2000 )  return 1000;   // 0.1 * 10000
                                       return 1200;   //0.12 * 10000
   }

   void amax_save::init(eosio::symbol pay_symbol, name bank_contract) {
      CHECK(false, "not allowed" )
      require_auth( _self );
      
      // _gstate.admin                 = "armoniaadmin"_n;
   }

   void amax_save::withdraw(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );
      if ( issuer != owner ) {
         CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to withdraw others saving account" )
      }

      auto save_acct = save_account_t( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )

      auto plan = save_plan_t( save_acct.plan_id );
      CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan not found: " + to_string(save_acct.plan_id) )

      if (plan.plan_conf.type == deposit_type::TERM) {
         auto save_termed_at = save_acct.created_at + plan.plan_conf.deposit_term_days;
         auto now = current_time_point();
         auto premature_withdraw = (now.sec_since_epoch() < save_termed_at.sec_since_epoch());
         if (!plan.plan_conf.allow_advance_redeem)
            CHECKC( !premature_withdraw, err::NO_AUTH, "premature withdraw not allowed" )

         if (premature_withdraw) {
            auto token_precision = get_precision(save_acct.deposit_quant);
            auto unfinish_rate   = div( save_termed_at.sec_since_epoch() - now.sec_since_epoch(), plan.plan_conf.deposit_term_days * DAY_SECONDS, PCT_BOOST );
            auto penalty_amount  = div( mul( mul( save_acct.deposit_quant.amount, unfinish_rate, token_precision ),
                                        plan.plan_conf.advance_redeem_fine_rate, PCT_BOOST ), PCT_BOOST, token_precision );
            auto penalty         = asset( penalty_amount, plan.plan_conf.principal_token.get_symbol() );
            
            plan.penalty_available += penalty;
            _db.set( plan );
            _db.del( save_acct );

            auto quant           = save_acct.deposit_quant - penalty;
            TRANSFER( plan.plan_conf.principal_token.get_contract(), owner, quant, "withdraw: " + to_string(save_id) )
         }
      } else {
         auto quant           = save_acct.deposit_quant;
         TRANSFER( plan.plan_conf.principal_token.get_contract(), owner, quant, "withdraw: " + to_string(save_id) )
      }
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
    *       2) deposit:$plan_id     -- by saver to deposit saving quantity to his or her own account
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
         auto plan = save_plan_t( plan_id );
         CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan id not found: " + to_string( plan_id ) )
         CHECKC( plan.plan_conf.interest_token.get_contract() == token_bank, err::CONTRACT_MISMATCH, "interest token contract mismatches" )

         plan.interest_available += quant;
         _db.set( plan );

      } else {
         uint64_t plan_id = 1;   //default 1st-plan 
         if (memo_params[0] == "deposit")
            plan_id = to_uint64(memo_params[1], "deposit plan");

         auto plan = save_plan_t( plan_id );
         CHECKC( _db.get( plan ), err::RECORD_NOT_FOUND, "plan id not found: " + to_string( plan_id ) )
         CHECKC( plan.plan_conf.principal_token.get_contract() == token_bank, err::CONTRACT_MISMATCH, "deposit token contract mismatches" )

         plan.deposit_available     += quant;
         _db.set( plan );

         auto accts                 = save_account_t::tbl_t(_self, from.value);
         auto save_acct             = save_account_t( accts.available_primary_key() );
         save_acct.interest_rate    = get_interest_rate( quant.amount / get_precision(quant) ); 
         save_acct.deposit_quant    = quant;
         save_acct.total_interest_collected = asset( 0, plan.plan_conf.interest_token.get_symbol() );
         save_acct.created_at       = current_time_point();

      }
   }

   void amax_save::setplan(const uint64_t& pid, const plan_conf_s& pc) {
      require_auth( _gstate.admin );

      auto plan = save_plan_t(pid);
      bool plan_existing = _db.get( plan );

      plan.plan_conf.principal_token        = pc.principal_token;
      plan.plan_conf.interest_token         = pc.interest_token;
      plan.plan_conf.deposit_term_days      = pc.deposit_term_days;
      plan.plan_conf.allow_advance_redeem   = pc.allow_advance_redeem;
      plan.plan_conf.effective_from         = pc.effective_from;
      plan.plan_conf.effective_to           = pc.effective_to;

      if (!plan_existing) plan.created_at   = current_time_point();

      _db.set( plan );
   }

   void amax_save::delplan(const uint64_t& pid) {
      require_auth( _gstate.admin );

      auto plan = save_plan_t(pid);
      CHECKC(_db.get( plan ), err::RECORD_NOT_FOUND, "plan not exist: " + to_string(pid) )

      _db.del( plan );
   }

} //namespace amax