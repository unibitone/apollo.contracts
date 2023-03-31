#include <nftone.save/nftone.save.hpp>
#include "safemath.hpp"
#include <utils.hpp>
#include <contract_function.hpp>
#include <amax.ntoken/amax.ntoken.hpp>
#include <amax.token.hpp>

static constexpr eosio::name active_permission{"active"_n};


namespace amax {

using namespace std;
using namespace wasm::safemath;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }

#define NTOKEN_TRANSFER(bank, to, quantity, memo) \
{ action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }

  inline int64_t get_precision(const symbol &s) {
    int64_t digit = s.precision();
    CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
    return calc_precision(digit);
  }

  inline int64_t get_precision(const asset &a) {
    return get_precision(a.symbol);
  }
  
  void nftone_save::ontransfer()
  {

      auto contract = get_first_receiver();
      if (token_contracts.count(contract) > 0) {
          execute_function(&nftone_save::_on_token_transfer);

      } else if (ntoken_contracts.count(contract)>0) {
          execute_function(&nftone_save::_on_ntoken_transfer);
      }
  }

  void nftone_save::setcampaign(const name &sponsor, const uint64_t &campaign_id, 
                                vector<uint64_t> &nftids, const time_point_sec &end_at, 
                                vector<uint16_t> &plan_days_list, 
                                vector<asset> &plan_profits_list,
                                const name &ntoken_contract)
  {
      require_auth(sponsor);
      
      save_campaign_t campaign(campaign_id);
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaignnot found: " + to_string( campaign_id ) )
      CHECKC( campaign.status == campaign_status::CREATED, err::STATE_MISMATCH, "state mismatch" )
      CHECKC( campaign.sponsor == sponsor, err::NO_AUTH, "permission denied" )
      CHECKC( end_at > campaign.begin_at, err::PARAM_ERROR, "begin time should be less than end time");
      CHECKC( nftids.size() + campaign.pledge_ntokens.size() <= 5, err::PARAM_ERROR, "nft size should be less than or equal to 5");
      CHECKC( plan_days_list.size() + campaign.plans.size() <= 5, err::PARAM_ERROR, "plan size should be less than or equal to 5");
      CHECKC( plan_days_list.size() == plan_profits_list.size(), err::PARAM_ERROR, "days and profit_tokens size mismatch" );
      CHECKC( _gstate.ntoken_contract_required.count(ntoken_contract), err::PARAM_ERROR, "ntoken contract invalid" )
      
      extended_nsymbol extended_nsymbol_tmp;
      for (int i = 0; i < nftids.size(); i++) {
        
          extended_nsymbol_tmp = extended_nsymbol(nsymbol(nftids[i]), ntoken_contract);
          
          int64_t nft_supply = ntoken::get_supply(ntoken_contract, extended_nsymbol_tmp.get_nsymbol());
          CHECKC( nft_supply>0, err::RECORD_NOT_FOUND, "nft not found: " + to_string(nftids[i]) )
          
          if (campaign.pledge_ntokens.count(extended_nsymbol_tmp)) 
              continue;
          campaign.pledge_ntokens.insert({extended_nsymbol_tmp, quotas()});
      }
      
      asset profit_token(0, campaign.interest_symbol.get_symbol());
      uint64_t days = 0, max_days = 0;
      asset max_profit_token = profit_token;
      for (int i = 0; i < plan_days_list.size(); i++) {
        
          days = plan_days_list[i];
          profit_token = plan_profits_list[i];
          CHECKC( days > 0, err::PARAM_ERROR, "plan days must be greater than 0" )
          CHECKC( profit_token.amount > 0, err::PARAM_ERROR, "plan profit must be greater than 0" )
          
          if(campaign.plans.count(days)) 
              continue;
          campaign.plans.insert({days, profit_token});
          
          if(profit_token > max_profit_token) max_profit_token = profit_token;
          if(days > max_days)                 max_days = days;
      }
      
      asset need_interest = asset(campaign.total_quotas * max_days * max_profit_token.amount, max_profit_token.symbol);
      CHECKC( need_interest <= campaign.get_total_interest(), save_err::INTEREST_INSUFFICIENT, "interest insufficient" )
      
      campaign.end_at = end_at;
      _db.set(campaign);
  }

  
  void nftone_save::collectint(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );
      
      if ( issuer != owner ) {
          CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to collect others saving interest" )
      }

      save_account_t save_acct( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )

      auto now = current_time_point();
      CHECKC( save_acct.term_ended_at > save_acct.last_collected_at, save_err::INTEREST_COLLECTED, "interest already collected" )

      auto elapsed_sec = now.sec_since_epoch() - save_acct.last_collected_at.sec_since_epoch();
      CHECKC( elapsed_sec > DAY_SECONDS, save_err::TIME_PREMATURE, "less than 24 hours since last interest collection time" )
      
      save_campaign_t campaign( save_acct.campaign_id );
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( save_acct.campaign_id ) )

      auto interest_due = save_acct.get_due_interest();
      CHECKC( interest_due.amount > 0, err::NOT_POSITIVE, "interest due amount is zero" )
      TRANSFER( campaign.interest_symbol.get_contract(), owner, interest_due, "interest: " + to_string(save_id) )
      
      save_acct.interest_collected  += interest_due;
      save_acct.last_collected_at   = now;
      _db.set( owner.value, save_acct );

      CHECKC( campaign.interest_available > interest_due, err::NOT_POSITIVE, "insufficient available interest to collect" )

      campaign.interest_available       -= interest_due;
      campaign.interest_redeemed        += interest_due;

      _db.set( campaign );

      _int_coll_log(owner, save_acct.id, campaign.id, interest_due,  time_point_sec( current_time_point() ));

  }
  
  void nftone_save::withdraw(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );
      if ( issuer != owner ) {
         CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to withdraw others saving account" )
      }

      auto save_acct = save_account_t( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )

      auto campaign = save_campaign_t( save_acct.campaign_id );
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "plan not found: " + to_string(save_acct.campaign_id) )

      CHECKC( save_acct.term_ended_at < current_time_point(), save_err::TERM_NOT_ENDED, "term not ended" )
      CHECKC( save_acct.term_ended_at <= save_acct.last_collected_at, save_err::INTEREST_NOT_COLLECTED, "interest not collected" )
      auto pledged_quant = save_acct.pledged;
      campaign.pledge_ntokens[pledged_quant.get_extended_nsymbol()].redeemed_quotas   += pledged_quant.quantity.amount;
      _db.set( campaign );
      _db.del( owner.value, save_acct );
      
      vector<nasset> redeem_quant = {pledged_quant.quantity};
      NTOKEN_TRANSFER( pledged_quant.contract, owner, redeem_quant, "redeem: " + to_string(save_id) )
  }

  void nftone_save::intcolllog(const name& account, const uint64_t& account_id, const uint64_t& plan_id, const asset &quantity, const time_point& created_at) {
      require_auth(get_self());
      require_recipient(account);
  }
  
  void nftone_save::intrefuellog(const name& refueler, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at) {
      require_auth(get_self());
      require_recipient(refueler);
  }
  
  /**
  * @brief transfer token
  *
  * @param from
  * @param to
  * @param quantity
  * @param memo: three formats:
  *       1) pre_create_campaign : $campaign_name : $campaign_en_name : $campaign_pic : $begin_at : $end_at                             -- pre-creation campaign by transfer fee
  *       2) create_campaign : $campaign_id : $contract_name : $nftids : $interest_symbol : $plan_days : $plan_profit : $quotas         -- create campaign by transfer interest
  *       3) increment_interest : $campaign_id : $plan_max_days : $plan_max_profit : $quotas                                            -- increment interest
  */
  void nftone_save::_on_token_transfer( const name &from,
                                          const name &to,
                                          const asset &quantity,
                                          const string &memo)
  {

      if (from == _self || to != _self) return;

      auto parts = split( memo, ":" );

      if ( parts.size() == 6 && parts[0] == "pre_create_campaign" ) {
        
          CHECKC( get_first_receiver() == SYS_BANK, err::PARAM_ERROR, "token contract invalid" )
          CHECKC( quantity >= crt_campaign_fee, err::FEE_INSUFFICIENT, "fee insufficient");

          string_view campaign_name = parts[1];
          CHECKC( campaign_name.size() <= 108, err::MEMO_FORMAT_ERROR, "campaign_name length is more than 120 bytes");

          string_view campaign_en_name = parts[2];
          CHECKC( campaign_en_name.size() <= 64, err::MEMO_FORMAT_ERROR, "campaign_en_name length is more than 64 bytes");

          string_view campaign_pic = parts[3];
          CHECKC( campaign_pic.size() <= 64, err::MEMO_FORMAT_ERROR, "campaign_pic length is more than 64 bytes");

          auto begin = to_uint64(parts[4], "begin time parse int error");
          auto end   = to_uint64(parts[5], "end time parse int error");
    
          CHECKC( end > begin, err::PARAM_ERROR, "begin time should be less than end time");
          
          _pre_create_campaign(from, campaign_name, campaign_en_name, campaign_pic, begin, end);
      } else if (parts.size() == 8 && parts[0] == "create_campaign") {
        
          uint64_t campaign_id = to_uint64(parts[1], "campaign_id parse int error");
          save_campaign_t campaign(campaign_id);
          CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaignnot found: " + to_string( campaign_id ) )
          CHECKC( campaign.status == campaign_status::INIT, err::STATE_MISMATCH, "state mismatch" )
          CHECKC( campaign.sponsor == from, err::NO_AUTH, "permission denied" )

          name ntoken_contract = name(parts[2]);
          CHECKC( _gstate.ntoken_contract_required.count(ntoken_contract), err::PARAM_ERROR, "ntoken contract invalid" )

          vector<string_view> nftids = split( parts[3], ":" );
          CHECKC( nftids.size() > 0, err::PARAM_ERROR, "nftids must be greater than 0" )
          map<extended_nsymbol, quotas> pledge_ntokens_tmp; 
          _build_pledge_ntokens(pledge_ntokens_tmp, ntoken_contract, nftids);
          
          symbol interest_symbol = symbol_from_string(parts[4]);
          CHECKC( _gstate.ntt_symbol_required.count(extended_symbol(interest_symbol, get_first_receiver())), err::PARAM_ERROR, "token contract invalid" )

          auto plan_days_list = split( parts[5], ":" );
          auto plan_profits_list = split( parts[6], ":" );
          CHECKC( plan_days_list.size() > 0 && (plan_days_list.size() == plan_profits_list.size()), err::PARAM_ERROR, "The number of days does not match the number of plans" )
          
          map<uint16_t, asset> plans_tmp; 
          asset max_profit_token(0, interest_symbol);
          uint64_t max_days = 0;
          _build_plan(plans_tmp, interest_symbol, plan_days_list, plan_profits_list, max_profit_token, max_days);
          
          uint64_t quotas  = to_uint64(parts[7], "quotas parse int error");
          asset need_interest = asset(quotas * max_days * max_profit_token.amount, max_profit_token.symbol);
          CHECKC( need_interest <= quantity, save_err::INTEREST_INSUFFICIENT, "interest insufficient" )
          
          campaign.plans              = plans_tmp;
          campaign.interest_symbol    = extended_symbol(interest_symbol, get_first_receiver());
          campaign.total_quotas       = quotas;
          campaign.pledge_ntokens     = pledge_ntokens_tmp;
          campaign.interest_available = quantity;
          campaign.interest_redeemed  = asset(0, interest_symbol);
          campaign.status             = campaign_status::CREATED;
          _db.set(campaign);
      } else if ( parts.size() == 5 && parts[0] == "increment_interest" ) {
        
          CHECKC( _gstate.ntt_symbol_required.count(extended_symbol(quantity.symbol, get_first_receiver())), err::PARAM_ERROR, "token contract invalid" )

          uint64_t campaign_id = to_uint64(parts[1], "campaign_id parse int error");
          save_campaign_t campaign(campaign_id);
          CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaignnot found: " + to_string( campaign_id ) )
          CHECKC( campaign.status == campaign_status::CREATED, err::STATE_MISMATCH, "state mismatch" )
          
          uint64_t max_days  = to_uint64(parts[2], "max days parse int error");
          asset max_profit_token  = asset_from_string(parts[3]);
          uint64_t quotas  = to_uint64(parts[4], "quotas parse int error");
          CHECKC( quotas >= campaign.total_quotas, err::PARAM_ERROR, "quotas cannot be less than the original" )
          asset need_interest = asset(quotas * max_days * max_profit_token.amount, max_profit_token.symbol);
          CHECKC( need_interest <= quantity + campaign.get_total_interest(), save_err::INTEREST_INSUFFICIENT, "interest insufficient" )
          
          campaign.interest_available += quantity;
          campaign.total_quotas = quotas;
          _db.set(campaign);
          
          _int_refuel_log(from, campaign_id, quantity, current_time_point());
      } else {
          CHECKC( false, err::PARAM_ERROR, "param error" );
      }
  }


  /**
  * @brief pledge ntoken
  *
  * @param from
  * @param to
  * @param quantity
  * @param memo: one formats:
  *       1) pledge : $campaign_id : $days                             -- gain interest by pledge ntoken 
  */  
  void nftone_save::_on_ntoken_transfer( const name& from,
                                              const name& to,
                                              const std::vector<nasset>& assets,
                                              const string& memo )
  {
      if (from == _self || to != _self) return;

      auto parts = split( memo, ":" );
      
      if (parts.size() == 3 && parts[0] == "pledge") {
        
          nasset quantity = assets[0];
          auto now = time_point_sec(current_time_point());
          extended_nasset extended_quantity = extended_nasset(quantity, get_first_receiver());
          auto campaign_id = to_uint64(parts[1], "campaign_id parse uint error");
          auto days        = to_uint64(parts[2], "days parse uint error");
          
          save_campaign_t campaign(campaign_id);
          CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaignnot found: " + to_string( campaign_id ) )
          CHECKC( campaign.status == campaign_status::CREATED, err::STATE_MISMATCH, "state mismatch" )
          CHECKC( campaign.plans.count(days), err::PARAM_ERROR, "this plan does not exist" )
          CHECKC( quantity.amount <= campaign.get_available_quotas(), save_err::QUOTAS_INSUFFICIENT, "quotas insufficient" )
          CHECKC( campaign.pledge_ntokens.count(extended_quantity.get_extended_nsymbol()), err::PARAM_ERROR, "this ntoken does not exist" )
          
          auto sid = _gstate.last_save_id++;
          save_account_t save_acct(sid);
          save_acct.campaign_id                 = campaign_id;
          save_acct.pledged                     = extended_quantity;
          save_acct.daily_interest_per_quota    = campaign.plans[days];
          save_acct.days                        = days;
          save_acct.interest_collected          = asset(0, campaign.interest_symbol.get_symbol());
          save_acct.term_ended_at               = now + days * DAY_SECONDS;
          save_acct.created_at                  = now;
          save_acct.last_collected_at           = now;
          _db.set( from.value, save_acct );
          
          campaign.quotas_purchased += quantity.amount;
          campaign.pledge_ntokens[extended_quantity.get_extended_nsymbol()].allocated_quotas += quantity.amount;
          _db.set( campaign );
      } else {
          CHECKC( false, err::PARAM_ERROR, "param error" );
      }
  }

  void nftone_save::_pre_create_campaign( const name& from,
                                    const string_view& campaign_name,
                                    const string_view& campaign_en_name,
                                    const string_view& campaign_pic,
                                    const uint64_t& begin,
                                    const uint64_t& end)
  {
      auto cid = _gstate.last_campaign_id++;
      save_campaign_t campaign(cid);
      campaign.sponsor          = from;
      campaign.campaign_name    = campaign_name;
      campaign.campaign_en_name = campaign_en_name;
      campaign.campaign_pic     = campaign_pic;
      campaign.status           = campaign_status::INIT;
      campaign.begin_at         = time_point_sec(begin);
      campaign.end_at           = time_point_sec(end);
      campaign.created_at       = current_time_point();
      _db.set(campaign);
  }
  
  void nftone_save::_build_plan(  map<uint16_t, asset>& plans_tmp,
                                  const symbol& interest_symbol,
                                  vector<string_view>& plan_days_list,
                                  vector<string_view>& plan_profits_list,
                                  asset max_profit_token,
                                  uint64_t max_days)
  {
      asset profit_token(0, interest_symbol);
      uint64_t days = 0;
      for (int i = 0; i < plan_days_list.size(); i++) {
          days = to_uint64(plan_days_list[i], "plan_days parse int error");
          CHECKC( days > 0, err::PARAM_ERROR, "plan days must be greater than 0" )
          profit_token = asset_from_string(plan_profits_list[i]);
          CHECKC( profit_token.amount > 0, err::PARAM_ERROR, "plan profit must be greater than 0" )
          plans_tmp.insert({days, profit_token});
          if(profit_token > max_profit_token) max_profit_token = profit_token;
          if(days > max_days)                 max_days = days;
      }
  }
  
  void nftone_save::_build_pledge_ntokens(  map<extended_nsymbol, quotas>& pledge_ntokens_tmp,
                                            const name& ntoken_contract,
                                            vector<string_view>& nftids)
  {   
      extended_nsymbol extended_nsymbol_tmp;
      for (int i = 0; i < nftids.size(); i++) {
          extended_nsymbol_tmp = extended_nsymbol(nsymbol(to_uint64(nftids[i], "nftid parse int error")), ntoken_contract);
          int64_t nft_supply = ntoken::get_supply(ntoken_contract, extended_nsymbol_tmp.get_nsymbol());
          CHECKC( nft_supply>0, err::RECORD_NOT_FOUND, "nft not found: " + string(nftids[i]) )
          pledge_ntokens_tmp.insert({extended_nsymbol_tmp, quotas()});
      }
  }
  
  void nftone_save::_int_coll_log(const name& account, const uint64_t& account_id, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at) {
      nftone_save::interest_withdraw_log_action act{ _self, { {_self, active_permission} } };
      act.send( account, account_id, campaign_id, quantity, created_at );
  }
   
  void nftone_save::_int_refuel_log(const name& refueler, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at) {
      nftone_save::intrefuellog_action act{ _self, { {_self, active_permission} } };
      act.send( refueler, campaign_id, quantity, created_at );
  }
} //namespace amax