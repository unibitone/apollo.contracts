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
  
  void nftone_save::setglobal(const set<name> &account, const set<name> &ntoken_contract, const set<name> &profit_token_contract ) {
      require_auth( _self );
      for (auto &a : account) {
          CHECKC( is_account(a), err::ACCOUNT_INVALID, "account not found: " + a.to_string() )
      }
      for (auto &ncontract : ntoken_contract) {
          CHECKC( is_account(ncontract), err::ACCOUNT_INVALID, "ntoken_contract not found: " + ncontract.to_string() )
      }
      for (auto &token_contract : profit_token_contract) {
          CHECKC( is_account(token_contract), err::ACCOUNT_INVALID, "profit_token_contract not found: " + token_contract.to_string() )
      }
      _gstate.whitelist.insert( account.begin(), account.end() );
      _gstate.ntoken_contract_required.insert(ntoken_contract.begin(), ntoken_contract.end());
      _gstate.profit_token_contract_required.insert(profit_token_contract.begin(), profit_token_contract.end());
  }
  
  void nftone_save::ontransfer()
  {
      auto contract = get_first_receiver();
      if (_gstate.profit_token_contract_required.count(contract) > 0) {
          execute_function(&nftone_save::_on_token_transfer);
      } else if (_gstate.ntoken_contract_required.count(contract) > 0) {
          execute_function(&nftone_save::_on_ntoken_transfer);
      }
  }

  void nftone_save::setcampaign(const name &sponsor, const uint64_t &campaign_id, 
                                vector<uint64_t> &nftids, const time_point_sec &end_at, 
                                vector<uint16_t> &plan_days_list, 
                                vector<asset> &plan_profits_list,
                                const name &ntoken_contract,
                                const uint32_t &total_quotas)
  {
      require_auth(sponsor);
      
      save_campaign_t campaign(campaign_id);
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
      CHECKC( campaign.sponsor == sponsor, err::NO_AUTH, "permission denied" )
      
      if(campaign.status == campaign_status::CREATED){
          CHECKC( end_at > campaign.begin_at, err::PARAM_ERROR, "begin time should be less than end time");
          CHECKC( end_at.sec_since_epoch() - campaign.begin_at.sec_since_epoch() <= YEAR_SECONDS, err::PARAM_ERROR, "the duration of the campaign cannot exceed 365 days");
      }
          
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
      CHECKC( total_quotas >= campaign.total_quotas, err::PARAM_ERROR, "quotas cannot be less than the original" )
      asset need_interest = asset( total_quotas * max_days * max_profit_token.amount, max_profit_token.symbol);
      CHECKC( need_interest <= campaign.interest_total, save_err::INTEREST_INSUFFICIENT, "interest insufficient" )
      
      campaign.total_quotas = total_quotas;
      if(campaign.status == campaign_status::CREATED)
        campaign.end_at = end_at;
      _db.set(campaign);
  }

  
  void nftone_save::claim(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );
      
      if ( issuer != owner ) {
          CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to collect others saving interest" )
      }

      save_account_t save_acct( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )

      auto now = current_time_point();
      CHECKC( save_acct.term_ended_at > save_acct.last_claimed_at, save_err::INTEREST_COLLECTED, "interest already collected" )

      auto elapsed_sec = now.sec_since_epoch() - save_acct.last_claimed_at.sec_since_epoch();
      CHECKC( elapsed_sec > DAY_SECONDS, save_err::TIME_PREMATURE, "less than 24 hours since last interest collection time" )
      
      save_campaign_t campaign( save_acct.campaign_id );
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( save_acct.campaign_id ) )

      auto interest_due = save_acct.get_due_interest();
      CHECKC( interest_due.amount > 0, err::NOT_POSITIVE, "interest due amount is zero" )
      CHECKC( campaign.get_available_interest() > interest_due, err::NOT_POSITIVE, "insufficient available interest to collect" )
      TRANSFER( campaign.interest_symbol.get_contract(), owner, interest_due, "interest: " + to_string(save_id) )
      
      save_acct.interest_claimed    += interest_due;
      save_acct.last_claimed_at     = now;
      _db.set( owner.value, save_acct );

      campaign.interest_claimed     += interest_due;
      _db.set( campaign );

      _int_coll_log(owner, save_acct.id, campaign.id, interest_due,  time_point_sec( current_time_point() ));
  }
  
  void nftone_save::redeem(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );
      if ( issuer != owner ) {
         CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to withdraw others saving account" )
      }

      auto save_acct = save_account_t( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )

      auto campaign = save_campaign_t( save_acct.campaign_id );
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "plan not found: " + to_string(save_acct.campaign_id) )

      CHECKC( save_acct.term_ended_at < current_time_point(), save_err::TERM_NOT_ENDED, "term not ended" )
      CHECKC( save_acct.term_ended_at <= save_acct.last_claimed_at, save_err::INTEREST_NOT_COLLECTED, "interest not collected" )
      auto pledged_quant = save_acct.pledged;
      campaign.pledge_ntokens[pledged_quant.get_extended_nsymbol()].redeemed_quotas   += pledged_quant.quantity.amount;
      _db.set( campaign );
      _db.del( owner.value, save_acct );
      
      vector<nasset> redeem_quant = {pledged_quant.quantity};
      NTOKEN_TRANSFER( pledged_quant.contract, owner, redeem_quant, "redeem: " + to_string(save_id) )
  }

  void nftone_save::cnlcampaign(const name& issuer, const name& owner, const uint64_t& campaign_id) {
      require_auth( issuer );
      if ( issuer != owner ) {
         CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to withdraw others saving account" )
      }
      
      save_campaign_t campaign(campaign_id);
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
      CHECKC( campaign.sponsor == owner, err::NO_AUTH, "permission denied" )
      CHECKC( campaign.begin_at > current_time_point(), save_err::STARTED, "campaign already started" )
      if(campaign.status == campaign_status::CREATED){
          TRANSFER( campaign.interest_symbol.get_contract(), owner, campaign.interest_total, "cancel campaign: " + to_string(campaign_id) )
      }
      _db.del( campaign );
  }
  
  void nftone_save::refundint(const name& issuer, const name& owner, const uint64_t& campaign_id) {
      require_auth( issuer );
      
      if ( issuer != owner ) {
         CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to withdraw others saving account" )
      }
      save_campaign_t campaign(campaign_id);
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
      CHECKC( campaign.sponsor == owner, err::NO_AUTH, "permission denied" )
      CHECKC( campaign.end_at < current_time_point(), save_err::NOT_ENDED, "campaign not ended" )
      
      asset refund_interest = campaign.get_refund_interest();
      if(campaign.status == campaign_status::CREATED && refund_interest.amount > 0){
          TRANSFER( campaign.interest_symbol.get_contract(), owner, refund_interest, "refund interest, campaign: " + to_string(campaign_id) )
      }
      _db.del( campaign );
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
  *       1) create_campaign : $campaign_name : $campaign_en_name : $campaign_pic : $begin_at : $end_at                             -- pre-creation campaign by transfer fee
  *       2) refuel : $campaign_id                                                                                                  -- increment interest
  */
 
  void nftone_save::_on_token_transfer( const name &from,
                                          const name &to,
                                          const asset &quantity,
                                          const string &memo)
  {

      if (from == _self || to != _self) return;

      auto parts = split( memo, ":" );

      if ( parts.size() == 6 && parts[0] == "create_campaign" ) {
        
          CHECKC( get_first_receiver() == SYS_BANK, err::PARAM_ERROR, "token contract invalid" )
          CHECKC( quantity == _gstate.crt_campaign_fee, err::FEE_INSUFFICIENT, "fee insufficient");
          CHECKC( _gstate.whitelist.count(from), err::NO_AUTH, "account is not on the whitelist" )

          string_view campaign_name = parts[1];
          CHECKC( campaign_name.size() <= 64 && campaign_name.size() > 0, err::MEMO_FORMAT_ERROR, "campaign_name length is not more than 108 bytes and not empty");

          string_view campaign_en_name = parts[2];
          CHECKC( campaign_en_name.size() <= 64, err::MEMO_FORMAT_ERROR, "campaign_en_name length is not more than 64 bytes");

          string_view campaign_pic = parts[3];
          CHECKC( campaign_pic.size() <= 64 && campaign_pic.size() > 0, err::MEMO_FORMAT_ERROR, "campaign_pic length is not more than 64 bytes and not empty");

          auto begin = to_uint64(parts[4], "begin time parse int error");
          auto end   = to_uint64(parts[5], "end time parse int error");

          CHECKC( end > begin, err::PARAM_ERROR, "begin time should be less than end time");
          CHECKC( end - begin <= YEAR_SECONDS, err::PARAM_ERROR, "the duration of the campaign cannot exceed 365 days");
          CHECKC( end > current_time_point().sec_since_epoch(), err::PARAM_ERROR, "begin time should be less than end time");

          _pre_create_campaign(from, campaign_name, campaign_en_name, campaign_pic, begin, end);
      } else if ( parts.size() == 2 && parts[0] == "refuel" ) {
        
          CHECKC( _gstate.profit_token_contract_required.count(get_first_receiver()), err::PARAM_ERROR, "token contract invalid" )

          uint64_t campaign_id = to_uint64(parts[1], "campaign_id parse int error");
          save_campaign_t campaign(campaign_id);
          CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
          CHECKC( campaign.sponsor == from, err::NO_AUTH, "permission denied" )
          
          if(campaign.status == campaign_status::INIT){
              campaign.interest_claimed     = asset(0, quantity.symbol);
              campaign.interest_frozen      = asset(0, quantity.symbol);
              campaign.interest_symbol      = extended_symbol(quantity.symbol, get_first_receiver());
              campaign.interest_total       = quantity;
              campaign.status               = campaign_status::CREATED;
          } else {
              campaign.interest_total       += quantity;
          }
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
          CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
          CHECKC( campaign.status == campaign_status::CREATED, err::STATE_MISMATCH, "state mismatch" )
          CHECKC( campaign.end_at >= now, save_err::ENDED, "the campaign already ended" )
          CHECKC( campaign.begin_at <= now, save_err::NOT_START, "the campaign not start" )
          CHECKC( campaign.plans.count(days), err::PARAM_ERROR, "this plan does not exist" )
          CHECKC( quantity.amount <= campaign.get_available_quotas(), save_err::QUOTAS_INSUFFICIENT, "quotas insufficient" )
          CHECKC( campaign.pledge_ntokens.count(extended_quantity.get_extended_nsymbol()), err::PARAM_ERROR, "this ntoken does not exist" )
          
          auto sid = _gstate.last_save_id++;
          save_account_t save_acct(sid);
          save_acct.campaign_id                 = campaign_id;
          save_acct.pledged                     = extended_quantity;
          save_acct.interest_per_quota          = campaign.plans[days] * days;
          save_acct.plan_days                   = days;
          save_acct.total_interest              = campaign.plans[days] * days * quantity.amount;
          save_acct.interest_claimed            = asset(0, campaign.interest_symbol.get_symbol());
          save_acct.term_ended_at               = now + days * DAY_SECONDS;
          save_acct.created_at                  = now;
          save_acct.last_claimed_at             = now;
          _db.set( from.value, save_acct, false );
          
          campaign.interest_frozen  += asset(quantity.amount * days * campaign.plans[days].amount, campaign.interest_symbol.get_symbol());
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

  void nftone_save::_int_coll_log(const name& account, const uint64_t& account_id, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at) {
      nftone_save::interest_withdraw_log_action act{ _self, { {_self, active_permission} } };
      act.send( account, account_id, campaign_id, quantity, created_at );
  }
   
  void nftone_save::_int_refuel_log(const name& refueler, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at) {
      nftone_save::intrefuellog_action act{ _self, { {_self, active_permission} } };
      act.send( refueler, campaign_id, quantity, created_at );
  }
} //namespace amax