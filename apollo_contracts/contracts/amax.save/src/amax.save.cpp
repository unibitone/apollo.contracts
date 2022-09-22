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
      require_auth( _self );

      _gstate.admin                 = "armoniaadmin"_n;
   
   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: $ask_price      E.g.:  10288    (its currency unit is CNYD)
    *
    */
   void amax_save::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );

      if (from == get_self() || to != get_self()) return;

      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )
    
   }

   // void amax_save::compute_memo_price(const string& memo, asset& price) {
   //    price.amount =  to_int64( memo, "price");
   //    CHECKC( price.amount > 0, err::PARAM_ERROR, " price non-positive quantity not allowed" )
   // }

  

} //namespace amax