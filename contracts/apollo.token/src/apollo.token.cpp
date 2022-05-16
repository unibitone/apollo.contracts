#include <apollo.token/apollo.token.hpp>
#include <utils.hpp>

namespace apollo {

ACTION token::init() {
   // _db.del( tokenstats );

   // _gstate.initialized = true;

}

ACTION token::create( const name& issuer, const uint8_t& token_type, const string& token_uri, 
                     const token_invars& invars, const token_vars& vars, const int64_t& maximum_supply )
{
   require_auth( get_self() );

   CHECKC(is_account(issuer), err::RECORD_NOT_FOUND, "issuer account does not exist" )
   CHECKC(issuer == _gstate.admin, err::NO_AUTH, "issuer is not an amdin user" )
   CHECKC(maximum_supply > 0, err::NOT_POSITIVE, "maximum_supply must be positive" )
   CHECKC(token_uri.length() < 1024, err::OVERSIZED, "token_uri length > 1024: " + to_string(token_uri.length()) )

   tokenstats_t::idx_t tokenstats(_self, _self.value);
   tokenstats.emplace( _self, [&]( auto& item ) {
      item.token_id     = tokenstats.available_primary_key(); if (item.token_id == 0) item.token_id = 1; 
      item.token_type   = token_type;
      item.token_uri    = token_uri;
      item.invars       = invars;
      item.vars         = vars;
      item.max_supply   = maximum_supply;
      item.creator      = issuer;
      item.created_at   = time_point_sec( current_time_point() );
   });
}

ACTION token::issue( const name& to, const token_asset& quantity, const string& memo )
{
   CHECKC(memo.size() <= 256, err::OVERSIZED, "memo has more than 256 bytes" )

   auto stats = tokenstats_t(quantity.symbol.token_id);
   CHECKC(_db.get(stats), err::RECORD_NOT_FOUND, "token not found: " + to_string(quantity.symbol.token_id) )
   CHECKC(to == stats.creator, err::NO_AUTH, "tokens can only be issued to token creator" )
   require_auth( stats.creator );
  
   CHECKC(quantity.symbol.token_id == stats.token_id, err::SYMBOL_MISMATCH, "token id not found: " + to_string(stats.token_id) )
   CHECKC(quantity.amount > 0, err::NOT_POSITIVE, "must issue positive quantity" )
   CHECKC(quantity.amount <= stats.max_supply - stats.supply, err::OVERSIZED, "quantity exceeds available supply");

   stats.supply += quantity.amount;
   _db.set( stats );

   add_balance( stats.creator, quantity );
}

ACTION token::retire( const token_asset& quantity, const string& memo )
{
   CHECKC(memo.size() <= 256, err::OVERSIZED, "memo has more than 256 bytes" )

   auto token = tokenstats_t(quantity.symbol.token_id);
   CHECKC(_db.get(token), err::RECORD_NOT_FOUND, "token asset not found: " + to_string(quantity.symbol.token_id) )

   require_auth( token.creator );
   CHECKC(quantity.amount > 0, err::NOT_POSITIVE, "must retire positive quantity" )
   CHECKC(quantity.symbol.token_id == token.token_id, err::SYMBOL_MISMATCH, "symbol mismatch" )
   token.supply -= quantity.amount;
   _db.set( token );

   sub_balance( token.creator, quantity );
}

ACTION token::transfer( const name& from, const name& to, token_asset& quantity, const string& memo ) {
   CHECKC(from != to, err::NO_AUTH, "cannot transfer to self" );
   
   auto now = current_time_point();
   CHECKC(now.sec_since_epoch() > start_time_since_epoch, err::NOT_STARTED, "not started yet" )

   require_auth( from );
   CHECKC(is_account( to ), err::RECORD_NOT_FOUND, "to account does not exist");
   auto tokenid = quantity.symbol.token_id;
   auto token = tokenstats_t(tokenid);
   CHECKC(_db.get(token), err::RECORD_NOT_FOUND, "token asset not found: " + to_string(tokenid) )

   require_recipient( from );
   require_recipient( to );

   CHECKC(quantity.amount > 0, err::NOT_POSITIVE, "must transfer positive quantity" );
   CHECKC(quantity.symbol.token_id == token.token_id, err::SYMBOL_MISMATCH, "token_id mismatch" )
   CHECKC(memo.size() <= 256, err::OVERSIZED, "memo has more than 256 bytes" )

   sub_balance( from, quantity );
   if (to == _gstate.decommerce_contract) {           //transfer: to sell or resell
      quantity.symbol.sub_token_id = 0;

   } else if (from == _gstate.decommerce_contract) {  //transfer: to buy
      quantity.symbol.sub_token_id = (now.sec_since_epoch() - start_time_since_epoch) / seconds_per_day;
   } 
   add_balance( to, quantity );

}

void token::add_balance( const name& owner, const token_asset& value ) {
   auto to_acnt = account_t(value);
   if (_db.get(owner.value, to_acnt)) {
      to_acnt.balance += value;
      _db.set( owner.value, to_acnt );

   } else {
      to_acnt.balance = value;
      _db.set( owner.value, to_acnt, false );
   }
}

void token::sub_balance( const name& owner, const token_asset& value ) {
   auto from_acnt = account_t(value);
   CHECKC(_db.get(owner.value, from_acnt), err::RECORD_NOT_FOUND, "account balance not found" )
   CHECKC(from_acnt.balance.amount >= value.amount, err::OVERSIZED, "overdrawn balance" );

   from_acnt.balance -= value;
   _db.set( owner.value, from_acnt );
}

} /// namespace apollo
