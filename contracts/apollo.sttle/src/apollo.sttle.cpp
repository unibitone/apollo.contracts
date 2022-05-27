#include <apollo.sttle/apollo.sttle.hpp>
#include <utils.hpp>

namespace apollo {

#define BURN(bank, to, quantity, memo) \
    { action(permission_level{get_self(), "active"_n }, bank, "burn"_n, std::make_tuple( to, quantity, memo )).send(); }

#define EV_TRANSFER(bank, to, quantity, memo) \
    { action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }    

#define AM_TRANSFER(bank, to, quantity, memo) \
    { action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }  

inline int64_t get_precision(const symbol &s) {
    int64_t digit = s.precision();
    CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
    return calc_precision(digit);
}

inline int64_t get_precision(const asset &a) {
    return get_precision(a.symbol);
}

ACTION sttlement::startmine( const uint32_t& token_id, const name& owner, const name& beneficiary ) {
    require_auth( owner );

    uint32_t sub_token_id = (current_time_point().sec_since_epoch() - start_time_since_epoch) / seconds_per_day;

    sttle_t::idx_t sttles(_self, _self.value);
    auto sttle_index = sttles.get_index<"unionid"_n>();
    uint128_t sec_index = ( (uint128_t)owner.value ) << 64 | ((uint64_t)token_id << 32 | sub_token_id);
    // check( sttle_index.find(sec_index) == sttle_index.end() , "sttlement already existing!" ); 
    if( sttle_index.find(sec_index) == sttle_index.end() ){
        sttles.emplace( _self, [&]( auto& item ) {
                item.pk_id              = sttles.available_primary_key(); if (item.pk_id == 0) item.pk_id = 1; 
                item.owner              = owner;
                item.beneficiary        = beneficiary;
                item.status             = sttle_status::START;
                item.last_settled_at    = time_point();
                item.token_id           = token_id;
                item.sub_token_id       = sub_token_id; 

                asset_symbol symbol     = asset_symbol( item.raw() );
                token_asset tokenasset  = token_asset( symbol );

                item.earning            = tokenasset;
            });
    }; 

   
}

ACTION sttlement::settlement( const name& owner, const uint64_t& id, const name& type  ) {
    CHECK( ( type == sttle_type::ADMIN  &&  has_auth(_self) ) || ( type == sttle_type::OWNER  &&  has_auth(owner) ), "operation failure");

    //get settlement RECORD
    sttle_t sttle( id );
    CHECK( _db.get( sttle ), "record not found: " + to_string(id));

    uint32_t days = current_time_point().sec_since_epoch() / seconds_per_day;

    CHECK( sttle.status != sttle_status::PAUSE, "record is already paused");
    CHECK( sttle.status != sttle_status::DEL, "record is already delete");
    CHECK( sttle.last_settled_at.sec_since_epoch() < time_point(eosio::days(days)).sec_since_epoch(), "record is settled: " + to_string(id));

    // get amount 
    auto symbol = asset_symbol( sttle.raw() );
    auto tokenasset = token_asset( symbol );
    account_t account( tokenasset );
    CHECK( _apollo_db.get( owner.value, account ), "the account not exists: " + to_string( tokenasset.symbol.raw() ) );

    //get nft info
    tokenstats_t tokenstats( sttle.token_id );
    CHECK( _apollo_db.get( tokenstats ), "the token not exists:  " + to_string(sttle.token_id) );

    //get variables
    power_asset_variables variables = std::get<power_asset_variables>(tokenstats.vars);
    //get invariables
    pow_asset_invariables invariables = std::get<pow_asset_invariables>(tokenstats.invars);

    //deduction of electricity
    BURN( APOLLO_EV, sttle.owner, variables.daily_electricity_charge, string("settlement consume") );
    
    // revenue calculate
    auto total_revenue = multiply_revenue( variables.actual_hash_rate.value, multiply_decimal64( account.balance.amount, variables.daily_earning_est.amount, get_precision(variables.daily_earning_est) ) );

    int64_t user_revenue = 0;

    //deduction of service charge
    if( sttle.sttle_times <= invariables.service_life_days ){
        user_revenue =  multiply_revenue( (10000.00 - variables.daily_svcfee_rate)/10000.00, total_revenue );
        //increase revenue
        asset quantity = asset(user_revenue,AM_SYMBOL);
        AM_TRANSFER( AM_TOKEN, sttle.owner, quantity, string("revenue transfer") );

    }else{
        user_revenue = multiply_revenue( (10000.00 - variables.daily_svcfee_rate)/10000.00 , total_revenue);
        //increase revenue
        asset quantity = asset(user_revenue,AM_SYMBOL);
        AM_TRANSFER(AM_TOKEN, sttle.owner, quantity, string("revenue transfer") );

    }

    //update  settlement times and final settlement time
    if( type == sttle_type::ADMIN ){
       sttle.last_settled_at += eosio::days(1);;

    } 

    if( type == sttle_type::OWNER ){
       sttle.last_settled_at = current_time_point();
       
    } 

    sttle.sttle_times += 1;
    //sttle.earning.amount += user_revenue;
    _db.set( sttle );
}

ACTION sttlement::pause( const name& owner, const uint64_t& id ) {
    CHECK( has_auth(_self) || has_auth(owner), "no permistion for destory");

    sttle_t sttle(id);
    CHECK( _db.get(sttle), "the record record not exists ");
    CHECK( sttle.status != sttle_status::DEL, "record is already delete");
    sttle.status = sttle_status::PAUSE;

    _db.set( sttle );
}

ACTION sttlement::destory( const name& owner, const uint64_t& id ) {
    require_auth( _self );

    sttle_t sttle(id);
    CHECK( _db.get(sttle), "the record record not exists ");
    sttle.status = sttle_status::DEL;

    _db.set( sttle );
}

} /// namespace apollo
