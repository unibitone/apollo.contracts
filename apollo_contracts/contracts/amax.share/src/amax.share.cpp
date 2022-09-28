#include <amax.share/amax.share.hpp>
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
   

   void amax_share::init() {
      // CHECK(false, "not allowed" )
      require_auth( _self );

   }
 
   void amax_share::setpool(const uint64_t& pool_id) {

   }

   void amax_share::delpool(const uint64_t& pool_id) {

   }

   void amax_share::addshare(const name& issuer, const name& owner, const uint64_t& pool_id, const asset& quant) {
      require_auth( issuer );

      auto share_pool = share_pool_t( pool_id );
      CHECKC( _db.get( share_pool ), err::RECORD_NOT_FOUND, "share pool not found: " + to_string( pool_id ) )
      CHECKC( share_pool.share_admin == issuer, err::NO_AUTH, "non share-admin not allowed to add share: " + issuer.to_string() )

      auto share_acct = share_account_t( pool_id );
      if (!_db.get( owner.value, share_acct ))
         share_acct.share = quant;
      else
         share_acct.share += quant;
      
      _db.set( owner.value, share_acct );

      share_pool.total_share += quant;
      _db.set( share_pool );
      
   }

   void amax_share::claimshare(const name& issuer, const name& owner, const uint64_t& pool_id) {
      require_auth( issuer );
      if ( issuer != owner ) {
         CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to collect others saving interest" )
      }

      auto now = time_point_sec( current_time_point() );
      auto pool = share_pool_t( pool_id );
      CHECKC( _db.get( pool ), err::RECORD_NOT_FOUND, "share pool not found: " + to_string( pool_id ) )
      CHECKC( pool.opened_at != time_point() && now > pool.opened_at, err::NO_AUTH, "share pool not opened yet" )
      CHECKC( pool.ended_at == time_point() || now < pool.ended_at, err::NO_AUTH, "share pool ended already" )

      auto share_acct = share_account_t( pool_id );
      CHECKC( _db.get( owner.value, share_acct ), err::RECORD_NOT_FOUND, "share account not found for pool: " + to_string( pool_id ) )
      
      // total_reward * share / total_share
      auto reward_amount = mul( pool.total_reward.amount, div( share_acct.share.amount, pool.total_share.amount, PCT_BOOST ), PCT_BOOST );
      auto reward_quant = asset( reward_amount, pool.share_token.get_symbol() );
      pool.total_claimed += reward_quant;
      _db.set( pool );
      _db.del( owner.value, share_acct );

      TRANSFER( pool.share_token.get_contract(), owner, reward_quant, "share: " + to_string(pool_id) )

   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: two formats:
    *       0) <NULL>               -- by saver to deposit to plan_id=1
    *       1) refuel:$plan_id      -- by admin to deposit interest quantity
    *       2) deposit:$plan_id     -- by saver to deposit saving quantity to his or her own account
    *
    */
   void amax_share::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );

      if (from == get_self() || to != get_self()) return;

      auto token_bank = get_first_receiver();
     
   
   }

} //namespace amax