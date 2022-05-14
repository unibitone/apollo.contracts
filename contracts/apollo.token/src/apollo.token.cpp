#include <apollo.token/apollo.token.hpp>

namespace apollo {

ACTION token::init() {
   // _db.del( tokenstats );

   // _gstate.initialized = true;

}

ACTION token::create( const name& issuer, const uint8_t& token_type, const string& token_uri, 
                     const token_invars& invars, const token_vars& vars, const int64_t& maximum_supply )
{
   require_auth( get_self() );

   check( is_account(issuer), "issuer account does not exist" );
   check( issuer == _gstate.admin, "issuer is not an amdin user" );
   check( maximum_supply > 0, "maximum_supply must be positive" );
   check( token_uri.length() < 1024, "token_uri length > 1024: " + to_string(token_uri.length()) );

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
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   auto stats = tokenstats_t(quantity.token_id);
   check( _db.get(stats), "token not found: " + to_string(quantity.token_id) );
   check( to == stats.creator, "tokens can only be issued to token creator" );
   require_auth( stats.creator );
  
   check( quantity.token_id == stats.token_id, "token id not found: " + to_string(stats.token_id) );
   check( quantity.amount > 0, "must issue positive quantity" );
   check( quantity.amount <= stats.max_supply - stats.supply, "quantity exceeds available supply");

   stats.supply += quantity.amount;
   _db.set( stats );

   add_balance( stats.creator, quantity );
}

ACTION token::retire( const token_asset& quantity, const string& memo )
{
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   auto token = tokenstats_t(quantity.token_id);
   check( _db.get(token), "token asset not found: " + to_string(quantity.token_id) );

   require_auth( token.creator );
   check( quantity.amount > 0, "must retire positive quantity" );
   check( quantity.token_id == token.token_id, "symbol mismatch" );
   token.supply -= quantity.amount;
   _db.set( token );

   sub_balance( token.creator, quantity );
}

ACTION token::transfer( const name& from, const name& to, const token_asset& quantity, const string& memo ) {
   check( from != to, "cannot transfer to self" );
   require_auth( from );
   check( is_account( to ), "to account does not exist");
   auto tokenid = quantity.token_id;
   auto token = tokenstats_t(tokenid);
   check( _db.get(token), "token asset not found: " + to_string(tokenid) );

   require_recipient( from );
   require_recipient( to );

   check( quantity.amount > 0, "must transfer positive quantity" );
   check( quantity.token_id == token.token_id, "token_id mismatch" );
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   // DeCommerce Contract ontransfer:
   // if (to == _self && quantity.symbol.token_subid != 0) //put onsales 
   //    quantity.symbol.token_subid = 0;

   sub_balance( from, quantity );
   add_balance( to, quantity );
}

void token::add_balance( const name& owner, const token_asset& value ) {
   auto to_acnt = account_t(value);
   if (_db.get(owner.value, to_acnt)) {
      to_acnt.balance += value;
   } else {
      to_acnt.balance = value;
   }

   _db.set( owner.value, to_acnt );
}

void token::sub_balance( const name& owner, const token_asset& value ) {
   auto from_acnt = account_t(value);
   check( _db.get(owner.value, from_acnt), "account balance not found" );
   check( from_acnt.balance.amount >= value.amount, "overdrawn balance" );

   from_acnt.balance -= value;
   _db.set( owner.value, from_acnt );
}

// ACTION token::setasset( const name& issuer, const uint64_t token_id, const pow_asset_meta& asset_meta) {
//    require_auth( issuer );
//    check( issuer == _gstate.admin, "non-admin issuer not allowed" );

//    auto stats = tokenstats_t(token_id);
//    check( _db.get(stats), "asset token not found: " + to_string(token_id) );

//    auto pow = pow_asset_t(token_id);
//    pow.asset_meta    = asset_meta;

//    _db.set( pow );

// }

} /// namespace apollo
