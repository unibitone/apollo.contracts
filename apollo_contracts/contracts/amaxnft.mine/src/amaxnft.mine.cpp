#include <amaxnft.mine/amaxnft.mine.hpp>
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
  
  // admin initializes the project configuration
  void amaxnft_mine::init( const set<name> &ntoken_contract, 
                          const set<name> &profit_token_contract, 
                          const uint8_t &nft_size_limit, 
                          const uint8_t &plan_size_limit) {
      require_auth( _self );

      for (auto &ncontract : ntoken_contract) {
          CHECKC( is_account(ncontract), err::ACCOUNT_INVALID, "ntoken_contract not found: " + ncontract.to_string() )
      }
      for (auto &token_contract : profit_token_contract) {
          CHECKC( is_account(token_contract), err::ACCOUNT_INVALID, "profit_token_contract not found: " + token_contract.to_string() )
      }
      CHECKC( nft_size_limit > 0, err::PARAM_ERROR, "nft_size_limit must be greater than 0" )
      CHECKC( plan_size_limit > 0, err::PARAM_ERROR, "plan_size_limit must be greater than 0" )

      _gstate.nft_contracts             = ntoken_contract;
      _gstate.interest_token_contracts  = profit_token_contract;
      _gstate.nft_size_limit            = nft_size_limit;
      _gstate.plan_size_limit           = plan_size_limit;
  }
  
  // user transfer nft pledge
  // campaign creator transfers interest token
  void amaxnft_mine::ontransfer()
  {
      auto contract = get_first_receiver();
      if (_gstate.interest_token_contracts.count(contract) > 0) {
          execute_function(&amaxnft_mine::_on_token_transfer);
      } else if (_gstate.nft_contracts.count(contract) > 0) {
          execute_function(&amaxnft_mine::_on_ntoken_transfer);
      }
  }

  // campaign creator edit content
  void amaxnft_mine::setcampaign(const name &sponsor, 
                                const uint64_t &campaign_id, 
                                const vector<uint64_t> &nftids, 
                                const uint16_t &plan_day,
                                const asset &plan_interest,
                                const name &ntoken_contract,
                                const uint32_t &total_quotas,
                                const string &campaign_name_cn,
                                const string &campaign_name_en,
                                const string &campaign_pic_url_cn,
                                const string &campaign_pic_url_en,
                                const uint32_t &begin_at,
                                const uint32_t &end_at)
  {
      require_auth(sponsor);
      
      save_campaign_t campaign(campaign_id);
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
      CHECKC( campaign.sponsor == sponsor, err::NO_AUTH, "permission denied" )
      CHECKC( nftids.size() > 0, err::PARAM_ERROR, "nft size is zero");
      
      bool is_created = campaign.status == campaign_status::CREATED or campaign.status == campaign_status::APPEND;
      CHECKC( end_at > begin_at, err::PARAM_ERROR, "begin time should be less than end time");
      CHECKC( end_at - begin_at <= 5 * YEAR_SECONDS, err::PARAM_ERROR, "the duration of the campaign cannot exceed 5 * 365 days");
      CHECKC( end_at > current_time_point().sec_since_epoch(), err::PARAM_ERROR, "begin time should be less than end time");
      
      CHECKC( nftids.size() + campaign.pledge_ntokens.size() <= _gstate.nft_size_limit, err::PARAM_ERROR, "nft size should be less than or equal to " + to_string(_gstate.nft_size_limit));
      CHECKC( plan_day >= _gstate.plan_size_limit, err::PARAM_ERROR, "plan day should be more than or equal to "+to_string(_gstate.plan_size_limit));

      CHECKC( plan_interest.symbol.is_valid(), err::PARAM_ERROR, "invalid plan_interest symbol" )
      CHECKC( plan_interest.is_valid(), err::PARAM_ERROR, "invalid plan_interest" )

      CHECKC( _gstate.nft_contracts.count(ntoken_contract), err::PARAM_ERROR, "ntoken contract invalid" )
      CHECKC( campaign_name_cn.size() <= 64 && campaign_name_cn.size() > 0, err::MEMO_FORMAT_ERROR, "campaign_name_cn length is not more than 64 bytes and not empty");
      CHECKC( campaign_name_en.size() <= 64, err::MEMO_FORMAT_ERROR, "campaign_name_en length is not more than 64 bytes");
      CHECKC( campaign_pic_url_cn.size() <= 64 && campaign_pic_url_cn.size() > 0, err::MEMO_FORMAT_ERROR, "campaign_pic_url chinese length is not more than 64 bytes and not empty");
      CHECKC( campaign_pic_url_en.size() <= 64, err::MEMO_FORMAT_ERROR, "campaign_pic_url english length is not more than 64 bytes");

      _set_campaign(campaign, nftids, ntoken_contract, plan_day, plan_interest, total_quotas, campaign_name_cn, campaign_name_en, campaign_pic_url_cn, campaign_pic_url_en);

      campaign.begin_at               = time_point_sec(begin_at);
      campaign.end_at                 = time_point_sec(end_at);
      if (is_created == false) {
          campaign.pre_interest       = asset(power10(plan_interest.symbol.precision()), plan_interest.symbol); // default pre interest is 1
          campaign.interest_collected = asset(0, plan_interest.symbol);
          campaign.interest_total     = asset(0, plan_interest.symbol);
          campaign.status             = campaign_status::CREATED;
      }
      _db.set(campaign);
  }

  // user earn interest
  void amaxnft_mine::collectint(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );

      save_account_t save_acct( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )

      auto now = current_time_point();
      CHECKC( save_acct.last_collected_at + DAY_SECONDS <= time_point_sec(now), save_err::TERM_NOT_ENDED, "term not ended" )

      save_campaign_t campaign( save_acct.campaign_id );
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( save_acct.campaign_id ) )

      auto interest_due = save_acct.calc_due_interest(campaign.pre_interest);
      CHECKC( interest_due.amount > 0, err::NOT_POSITIVE, "interest due amount is zero" )
      ASSERT( campaign.calc_available_interest() >= interest_due )
      
      TRANSFER( campaign.interest_symbol.get_contract(), owner, interest_due, "interest: " + to_string(save_id) )
      
      save_acct.interest_collected    += interest_due;
      save_acct.last_collected_at     = now;
      _db.set( owner.value, save_acct );

      campaign.interest_collected     += interest_due;
      _db.set( campaign );

      _int_coll_log(owner, save_acct.id, campaign.id, interest_due,  time_point_sec( current_time_point() ));
  }
  
  // user redemption nft
  void amaxnft_mine::redeem(const name& issuer, const name& owner, const uint64_t& save_id) {
      require_auth( issuer );
      
      if ( issuer != owner ) {
          CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to redeem others saving account" )
      }

      auto save_acct = save_account_t( save_id );
      CHECKC( _db.get( owner.value, save_acct ), err::RECORD_NOT_FOUND, "account save not found" )
      CHECKC( save_acct.term_ended_at < current_time_point(), save_err::TERM_NOT_ENDED, "term not ended" )

      auto campaign = save_campaign_t( save_acct.campaign_id );
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "plan not found: " + to_string(save_acct.campaign_id) )
      auto pledged_quant = save_acct.pledged;
      // check interest is zero
      auto interest_due = save_acct.calc_due_interest(campaign.pre_interest);
      CHECKC( interest_due.amount == 0, err::NOT_POSITIVE, "interest due amount is not zero" )

      campaign.pledge_ntokens[pledged_quant.get_extended_nsymbol()].redeemed_quotas   += pledged_quant.quantity.amount;
      campaign.quotas_purchased -= pledged_quant.quantity.amount;
      _db.set( campaign );
      _db.del( owner.value, save_acct );
      
      vector<nasset> redeem_quant = {pledged_quant.quantity};
      NTOKEN_TRANSFER( pledged_quant.contract, owner, redeem_quant, "redeem: " + to_string(save_id) )
  }

  // campaign creator cancel campaign
  void amaxnft_mine::cancelcamp(const name& issuer, const name& owner, const uint64_t& campaign_id) {
      require_auth( issuer );
      if ( issuer != owner ) {
         CHECKC( issuer == _gstate.admin, err::NO_AUTH, "non-admin not allowed to cancel others campaign" )
      }
      
      save_campaign_t campaign(campaign_id);
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
      CHECKC( campaign.status == campaign_status::CREATED, err::STATE_MISMATCH, "status mismatch" )
      CHECKC( campaign.sponsor == owner, err::NO_AUTH, "permission denied" )
      CHECKC( campaign.begin_at > current_time_point(), save_err::STARTED, "campaign already started" )
      
      _db.del( campaign );
  }
  

  void amaxnft_mine::intcolllog(const name& account, const uint64_t& account_id, const uint64_t& plan_id, const asset &quantity, const time_point& created_at) {
      require_auth(get_self());
      require_recipient(account);
  }

  void amaxnft_mine::intrefulog(const name& account, const uint64_t& campaign_id, const asset& quantity, const uint32_t& total_quotas, 
                      const uint32_t& quotas_purchased, const time_point& created_at) {
      require_auth(get_self());
      require_recipient(account);
  }
  

  void amaxnft_mine::setcamptime(const name &sponsor, const uint64_t &campaign_id, const uint32_t &begin_at, const uint32_t &end_at) {
      require_auth(sponsor);
      
      save_campaign_t campaign(campaign_id);
      CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
      CHECKC( campaign.sponsor == sponsor, err::NO_AUTH, "permission denied" )
      
      bool is_created = campaign.status == campaign_status::CREATED or campaign.status == campaign_status::APPEND;
      CHECKC( end_at > begin_at, err::PARAM_ERROR, "begin time should be less than end time");
      CHECKC( end_at - begin_at <= 5 * YEAR_SECONDS, err::PARAM_ERROR, "the duration of the campaign cannot exceed 5 * 365 days");
      CHECKC( end_at > current_time_point().sec_since_epoch(), err::PARAM_ERROR, "begin time should be less than end time");
    
      campaign.begin_at = time_point_sec(begin_at);
      campaign.end_at   = time_point_sec(end_at);

      _db.set(campaign);
  }


  /**
  * @brief transfer token
  *
  * @param from
  * @param to
  * @param quantity
  * @param memo: three formats:
  *       1) create_campaign                  -- pre-creation campaign by transfer fee
  *       2) refuelint : $campaign_id         -- increment interest
  */
 
  void amaxnft_mine::_on_token_transfer( const name &from,
                                          const name &to,
                                          const asset &quantity,
                                          const string &memo)
  {

      if (from == _self || to != _self) return;

      auto parts = split( memo, ":" );

      if ( parts.size() == 1 && parts[0] == "create_campaign" ) {
        
          CHECKC( get_first_receiver() == SYS_BANK, err::PARAM_ERROR, "token contract invalid" )
          CHECKC( quantity == _gstate.campaign_create_fee, err::FEE_INSUFFICIENT, "fee insufficient");
          CHECKC( _is_whitelist(from), err::NO_AUTH, "account is not on the whitelist" )
          _create_campaign(from);
          
      } else if ( parts.size() == 2 && parts[0] == "refuelint" ) {
        
          CHECKC( _gstate.interest_token_contracts.count(get_first_receiver()), err::PARAM_ERROR, "token contract invalid" )

          uint64_t campaign_id = to_uint64(parts[1], "campaign_id parse int error");
          save_campaign_t campaign(campaign_id);
          CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
          CHECKC( campaign.sponsor == from, err::NO_AUTH, "permission denied" )

          // check quotas gt zero
          CHECKC(campaign.quotas_purchased > 0, err::OVERSIZED, "purchase quotas must be greater than 0" )

          // calc and update pre_interest
          auto pre_quantity = quantity / campaign.quotas_purchased;
          CHECKC(pre_quantity.amount > 0, err::OVERSIZED, "quantity must be greater than 0")
          campaign.pre_interest += pre_quantity;
          
          if(campaign.status == campaign_status::CREATED){
              campaign.interest_collected   = asset(0, quantity.symbol);
              campaign.interest_symbol      = extended_symbol(quantity.symbol, get_first_receiver());
              campaign.interest_total       = quantity;
              campaign.status               = campaign_status::APPEND;
          } else {
              campaign.interest_total       += quantity;
          }
          
          _db.set(campaign);

          auto now = current_time_point();
          _int_refu_log( from, campaign_id, quantity, campaign.total_quotas, campaign.quotas_purchased, time_point_sec(current_time_point()) );
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
  *       1) pledge : $campaign_id    -- gain interest by pledge ntoken 
  */  
  void amaxnft_mine::_on_ntoken_transfer( const name& from,
                                              const name& to,
                                              const std::vector<nasset>& assets,
                                              const string& memo )
  {
      if (from == _self || to != _self) return;

      auto parts = split( memo, ":" );
      
      if (parts.size() == 2 && parts[0] == "pledge") {
          nasset quantity = assets[0];
          auto now = time_point_sec(current_time_point());
          extended_nasset extended_quantity = extended_nasset(quantity, get_first_receiver());
          auto campaign_id = to_uint64(parts[1], "campaign_id parse uint error");
          
          save_campaign_t campaign(campaign_id);
          CHECKC( _db.get( campaign ), err::RECORD_NOT_FOUND, "campaign not found: " + to_string( campaign_id ) )
          CHECKC( campaign.status == campaign_status::CREATED or campaign.status == campaign_status::APPEND, err::STATE_MISMATCH, "state mismatch" )
          CHECKC( campaign.end_at >= now, save_err::ENDED, "the campaign already ended" )
          CHECKC( campaign.begin_at <= now, save_err::NOT_START, "the campaign not start" )
          CHECKC( quantity.amount <= campaign.calc_available_quotas(), save_err::QUOTAS_INSUFFICIENT, "quotas insufficient" )
          CHECKC( campaign.pledge_ntokens.count(extended_quantity.get_extended_nsymbol()), err::PARAM_ERROR, "this ntoken does not exist" )
          
          auto sid = _gstate.last_save_id++;
          save_account_t save_acct(sid);
          save_acct.campaign_id                 = campaign_id;
          save_acct.pledged                     = extended_quantity;
          save_acct.save_pre_interest           = campaign.pre_interest;
          save_acct.interest_collected          = asset(0, campaign.plan_interest.symbol);
          save_acct.term_ended_at               = now + campaign.plan_day * DAY_SECONDS;
          save_acct.created_at                  = now;
          _db.set( from.value, save_acct, false );
          
          campaign.quotas_purchased += quantity.amount;
          campaign.pledge_ntokens[extended_quantity.get_extended_nsymbol()].allocated_quotas += quantity.amount;
          _db.set( campaign );
      } else {
          CHECKC( false, err::PARAM_ERROR, "param error" );
      }
  }

  /// create campaign
  void amaxnft_mine::_create_campaign( const name& from)
  {
      auto cid = _gstate.last_campaign_id++;
      save_campaign_t campaign(cid);
      campaign.sponsor          = from;
      campaign.status           = campaign_status::INIT;
      campaign.created_at       = current_time_point();
      _db.set(campaign);
  }
  
  bool amaxnft_mine::_is_whitelist( const name& account )
  {
      return whitelist.count(account) ? true : false;
  }
  
  void amaxnft_mine::_set_campaign( save_campaign_t &campaign, 
                                    const vector<uint64_t> &nftids,  
                                    const name &ntoken_contract,
                                    const uint16_t &plan_day, 
                                    const asset &plan_interest,
                                    const uint32_t &total_quotas,
                                    const string_view &campaign_name_cn,
                                    const string_view &campaign_name_en,
                                    const string_view &campaign_pic_url_cn,
                                    const string_view &campaign_pic_url_en)
  {
      extended_nsymbol extended_nsymbol_tmp;
      for (int i = 0; i < nftids.size(); i++) {
          extended_nsymbol_tmp = extended_nsymbol(nsymbol(nftids[i]), ntoken_contract);
          
          int64_t nft_supply = ntoken::get_supply(ntoken_contract, extended_nsymbol_tmp.get_nsymbol());
          CHECKC( nft_supply>0, err::RECORD_NOT_FOUND, "nft not found: " + to_string(nftids[i]) )
          
          if (campaign.pledge_ntokens.count(extended_nsymbol_tmp)) 
              continue;
          campaign.pledge_ntokens.insert({extended_nsymbol_tmp, quotas()});
      }
              
      CHECKC( plan_day > 0, err::PARAM_ERROR, "plan days must be greater than 0" )      
      CHECKC( total_quotas >= campaign.total_quotas, err::PARAM_ERROR, "quotas cannot be less than the original" )

      campaign.plan_day         = plan_day;
      campaign.plan_interest    = plan_interest;
      campaign.total_quotas     = total_quotas;
      campaign.campaign_name_cn = campaign_name_cn;
      campaign.campaign_name_en = campaign_name_en;
      campaign.campaign_pic_url_cn = campaign_pic_url_cn;
      campaign.campaign_pic_url_en = campaign_pic_url_en;
  }

  void amaxnft_mine::_int_coll_log(const name& account, const uint64_t& account_id, const uint64_t& campaign_id, const asset &quantity, const time_point& created_at) {
      amaxnft_mine::interest_collect_log_action act{ _self, { {_self, active_permission} } };
      act.send( account, account_id, campaign_id, quantity, created_at );
  }

  void amaxnft_mine::_int_refu_log(const name& account, const uint64_t& campaign_id, const asset& quantity, const uint32_t& total_quotas, 
                      const uint32_t& quotas_purchased, const time_point& created_at) {
      amaxnft_mine::interest_refuel_log_action act{ _self, { {_self, active_permission} } };
      act.send( account, campaign_id, quantity, total_quotas, quotas_purchased, created_at );
  }

} //namespace amax