#include <apollo.vcoin/apollo.vcoin.hpp>

static constexpr eosio::name active_permission{"active"_n};

#define TRANSFER(bank, to, quantity, memo) \
    {	token::transfer_action act{ bank, { {_self, active_permission} } };\
			act.send( _self, to, quantity , memo );}

#define ISSUE(to, quantity, memo) \
    {	token::issue_action act{ _self, { {_self, active_permission} } };\
			act.send( to, quantity , memo );}

namespace apollo {

void token::create( const name& burner,
                    const name& collector,
                    const asset& maximum_supply)
{
    require_auth( get_self() );

    check(is_account(burner), "burner account does not exist");
    check(is_account(collector), "collector account does not exist");
    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( maximum_supply.is_valid(), "invalid supply");
    check( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( get_self(), [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.burner        = burner;
       s.collector     = collector;    
    });
}

void token::issue( const name& to, const asset& quantity, const string& memo )
{
   require_auth( get_self() );
   auto sym = quantity.symbol;
   check( sym.is_valid(), "invalid symbol name" );
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   stats statstable( get_self(), sym.code().raw() );
   auto existing = statstable.find( sym.code().raw() );
   check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
   const auto& st = *existing;
   
   check( quantity.is_valid(), "invalid quantity" );
   check( quantity.amount > 0, "must issue positive quantity" );
   check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
   check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

   statstable.modify( st, same_payer, [&]( auto& s ) {
      s.supply += quantity;
   });

   add_balance( to, quantity, get_self());
   // add_balance( to, quantity, to);
}

void token::retire( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.burner );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.burner, quantity );
}

void token::burn(const name& from, const asset& quantity, const string& memo )
{
    check(is_account(from), "account does not exist");
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.burner );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( from, quantity);
}


void token::transfer( const name&    from,
                      const name&    to,
                      const asset&   quantity,
                      const string&  memo )
{
    check( from != to, "cannot transfer to self" );
    require_auth( from );
    check( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void token::sub_balance( const name& owner, const asset& value ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   check( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, same_payer, [&]( auto& a ) {
         a.balance -= value;
      });
}

void token::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::ontransfer(name from, name to, asset quantity, string memo) {
    check( quantity.amount > 0, "must transfer positive quantity" );
   // 矿机3000, 电费900, transfer 900.0000 CNYD -> apollo.vcoin
   // CNYD 1:1, USDT: 1:6.5, DEX x .9
   //memo params format:
   //1. topup:${receiver}
   auto receiver = from;
   vector<string_view> memo_params = split(memo, ":");
   if (memo_params.size() == 2 && memo_params[0] == "topup") {
         receiver = name(memo_params[1]);
         check( is_account( receiver ), "owner account does not exist" );
   }
   auto topup_quantity = asset(quantity.amount, VCOIN_SYMBOL); // TODO: exchange_rate
   ISSUE(receiver, topup_quantity, memo);
}

void token::collect(asset quantity, string memo){
    check( quantity.amount > 0, "quantity must be positive" );
    check( quantity.symbol == CNYD_SYMBOL, "quantity symbol mismatch with cnyd symbol");
    
    auto sym = VCOIN_SYMBOL;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;
    require_auth( st.collector );

    TRANSFER(CNYD_BANK, st.collector, quantity, memo);
}

} /// namespace eosio