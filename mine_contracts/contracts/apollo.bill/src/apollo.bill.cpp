#include <apollo.bill/apollo.bill.hpp>

namespace apollo {

ACTION bill::init() {
   auto billstats = billstats_t(0);
   _db.del( billstats );

   // _gstate.initialized = true;

}

ACTION bill::create( const name& issuer, const uint16_t& asset_type, const string& uri, const int64_t& maximum_supply )
{
   require_auth( get_self() );

   check( is_account(issuer), "issuer account does not exist" );
   check( issuer == _gstate.admin, "issuer is not an amdin user" );
   check( maximum_supply > 0, "maximum_supply must be positive" );
   check( uri.length() < 1024, "uri length > 1024: " + to_string(uri.length()) );

   billstats_t::idx_t billstats(_self, _self.value);
   billstats.emplace( _self, [&]( auto& item ) {
      item.symbid = billstats.available_primary_key();
      item.type = asset_type;
      item.uri = uri;
      item.max_supply = maximum_supply;
      item.issuer = issuer;
      item.created_at = current_time_point();
   });
}

ACTION bill::issue( const name& to, const bill_asset& quantity, const string& memo )
{
   auto symid = quantity.symbid;
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   auto stats = billstats_t(quantity.symbid);
   check( _db.get(stats), "asset bill not found: " + to_string(quantity.symbid) );
   check( to == stats.issuer, "bills can only be issued to issuer account" );
   require_auth( stats.issuer );
  
   check( quantity.symbid == stats.symbid, "symbol ID mismatch" );
   check( quantity.amount > 0, "must issue positive quantity" );
   check( quantity.amount <= stats.max_supply - stats.supply, "quantity exceeds available supply");

   stats.supply += quantity.amount;
   _db.set( stats );

   add_balance( stats.issuer, quantity );
}

ACTION bill::retire( const bill_asset& quantity, const string& memo )
{
   auto symbid = quantity.symbid;
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   auto bill = billstats_t(symbid);
   check( _db.get(bill), "bill asset not found: " + to_string(symbid) );

   require_auth( bill.issuer );
   check( quantity.amount > 0, "must retire positive quantity" );
   check( quantity.symbid == bill.symbid, "symbol mismatch" );
   bill.supply -= quantity.amount;
   _db.set( bill );

   sub_balance( bill.issuer, quantity );
}

ACTION bill::transfer( const name& from, const name& to, const bill_asset& quantity, const string& memo ) {
   check( from != to, "cannot transfer to self" );
   require_auth( from );
   check( is_account( to ), "to account does not exist");
   auto symid = quantity.symbid;
   auto bill = billstats_t(symid);
   check( _db.get(bill), "bill asset not found: " + to_string(symid) );

   require_recipient( from );
   require_recipient( to );

   check( quantity.amount > 0, "must transfer positive quantity" );
   check( quantity.symbid == bill.symbid, "symbol mismatch" );
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   sub_balance( from, quantity );
   add_balance( to, quantity );
}

void bill::add_balance( const name& owner, const bill_asset& value ) {
   auto to_acnt = account_t(value.symbid);
   if (_db.get(owner.value, to_acnt)) {
      to_acnt.balance += value;
   } else {
      to_acnt.balance = value;
   }

   _db.set( owner.value, to_acnt );
}

void bill::sub_balance( const name& owner, const bill_asset& value ) {
   auto from_acnt = account_t(value.symbid);
   check( _db.get(owner.value, from_acnt), "account balance not found" );
   check( from_acnt.balance.amount >= value.amount, "overdrawn balance" );

   from_acnt.balance -= value;
   _db.set( owner.value, from_acnt );
}

ACTION bill::setpowasset( const name& issuer, const uint64_t symbid, const pow_asset_meta& asset_meta) {
   require_auth( issuer );
   check( issuer == _gstate.admin, "non-admin issuer not allowed" );

   auto stats = billstats_t(symbid);
   check( _db.get(stats), "asset bill not found: " + to_string(symbid) );

   auto pow = pow_asset_t(symbid);
   pow.asset_meta    = asset_meta;

   _db.set( pow );

}

} /// namespace apollo
