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
    CHECK(digit >= 0 && digit <= 18,err::SYMBOL_MISMATCH, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
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
    uint128_t sec_index =get_union_id( owner, token_id, sub_token_id );
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
    CHECK( ( type == sttle_type::ADMIN  &&  has_auth(_self) ) || ( type == sttle_type::OWNER  &&  has_auth(owner) ), err::OPERATION_FAILURE, "operation failure");

    //get settlement RECORD
    sttle_t sttle( id );
    CHECK( _db.get( sttle ), err::RECORD_NOT_FOUND, "record not found: " + to_string(id));

    uint32_t cur_days = current_time_point().sec_since_epoch() / seconds_per_day;
    uint32_t last_settled_days = sttle.last_settled_at.sec_since_epoch() / seconds_per_day;
    CHECK( sttle.status != sttle_status::PAUSE, err::PAUSED, "record is already paused");
    CHECK( sttle.status != sttle_status::DEL, err::RECORD_DEL, "record is already delete");
    CHECK( last_settled_days < cur_days, err::RECORD_SETTLED, "record is settled: " + to_string(id));

    // get amount
    auto symbol = asset_symbol( sttle.raw() );
    auto tokenasset = token_asset( symbol );
    account_t account( tokenasset );
    CHECK( _apollo_db.get( owner.value, account ), err::RECORD_NOT_FOUND, "the account not found: " + to_string( tokenasset.symbol.raw() ) );

    //get nft info
    tokenstats_t tokenstats( sttle.token_id );
    CHECK( _apollo_db.get( tokenstats ), err::RECORD_NOT_FOUND, "the token not found:  " + to_string(sttle.token_id) );

    //get variables
    power_asset_variables variables = std::get<power_asset_variables>(tokenstats.vars);
    //get invariables
    pow_asset_invariables invariables = std::get<pow_asset_invariables>(tokenstats.invars);

    //deduction of electricity
    BURN( APOLLO_EV, sttle.owner, variables.daily_electricity_charge, string("settlement consume") );

    // revenue calculate
    int64_t total_rarning = multiply_revenue( variables.actual_hash_rate.value, multiply_decimal64( account.balance.amount, variables.daily_earning_est.amount, get_precision(variables.daily_earning_est) ) );
    CHECK( total_rarning>0, err::NOT_POSITIVE, "settlement transfer must positive quantity " + to_string(id));

    int64_t user_earning = 0;

    //deduction of service charge
    if( sttle.sttle_times <= invariables.service_life_days ){
        user_earning =  multiply_decimal64( 10000 - variables.daily_svcfee_rate, total_rarning, 10000 );

        //increase revenue
        asset quantity = asset(user_earning,AM_SYMBOL);
        AM_TRANSFER( AM_TOKEN, sttle.owner, quantity, string("earning transfer") );

    }else{
        user_earning =  multiply_decimal64( 10000 - variables.daily_svcfee_rate, total_rarning, 10000 );
        //increase revenue
        asset quantity = asset(user_earning,AM_SYMBOL);
        AM_TRANSFER(AM_TOKEN, sttle.owner, quantity, string("earning transfer") );

    }

    //update  settlement times and final settlement time
    if( type == sttle_type::ADMIN ){
       sttle.last_settled_at += eosio::days(1);;

    }

    if( type == sttle_type::OWNER ){
       sttle.last_settled_at = current_time_point();

    }

    sttle.sttle_times ++;
    //sttle.earning.amount += user_revenue;
    _db.set( sttle );
}

ACTION sttlement::pause( const name& owner, const uint64_t& id ) {
    CHECK( has_auth(_self) || has_auth(owner), err::NO_AUTH, "no permistion for destory");

    sttle_t sttle(id);
    CHECK( _db.get(sttle), err::RECORD_NOT_FOUND, "record record not found ");
    CHECK( sttle.status != sttle_status::DEL, err::RECORD_DEL, "record is already delete");
    CHECK( sttle.status == sttle_status::START, err::PAUSED, "record is pause");
    sttle.status = sttle_status::PAUSE;

    _db.set( sttle );
}

ACTION sttlement::destory( const name& owner, const uint64_t& id ) {
    require_auth( _self );

    sttle_t sttle(id);
    CHECK( _db.get(sttle), err::RECORD_NOT_FOUND, "record record not found ");
    CHECK( sttle.status != sttle_status::DEL, err::RECORD_DEL, "record is delete");
    sttle.status = sttle_status::DEL;

    _db.set( sttle );
}

ACTION sttlement::start( const name& owner, const uint64_t& id ) {
    CHECK( has_auth(_self) || has_auth(owner), err::NO_AUTH, "no permistion for destory");

    sttle_t sttle(id);
    CHECK( _db.get(sttle), err::RECORD_NOT_FOUND, "record record not found ");
    CHECK( sttle.status != sttle_status::DEL, err::RECORD_DEL, "record is already delete");
    CHECK( sttle.status == sttle_status::PAUSE, err::STRATED, "record is start");
    sttle.status = sttle_status::START;

    _db.set( sttle );
}

} /// namespace apollo
