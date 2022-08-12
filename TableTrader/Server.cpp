/************************************************************************
 * Copyright(c) 2022, One Unified. All rights reserved.                 *
 * email: info@oneunified.net                                           *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

/*
 * File:      Server.cpp
 * Author:    raymond@burkholder.net
 * Project:   TableTrader
 * Created:   2022/08/02 09:58:23
 */

#include <sstream>

#include <boost/log/trivial.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/format.hpp>

#include <OUCommon/TimeSource.h>

#include "Server.hpp"
#include "Server_impl.hpp"

namespace {
  //const std::string sFormatFloat( "%0.*f" ); // boost::format does not process variable precision
  const std::string sFormatUSD( "%0.2f" );
}

Server::Server(
  int argc, char *argv[],
  const config::Choices& choices,
  const std::string &wtConfigurationFile
)
: Wt::WServer( argc, argv, wtConfigurationFile )
, m_choices( choices )
{
  m_implServer = std::make_unique<Server_impl>();
}

Server::~Server() {
}

void Server::SessionAttach( const std::string& sSessionId ) {
  m_implServer->SessionAttach( sSessionId );
}

void Server::SessionDetach( const std::string& sSessionId ) {
  m_implServer->SessionDetach( sSessionId );
}

bool Server::ValidateLogin( const std::string& sUserName, const std::string& sPassWord ) {
  return ( ( sUserName == m_choices.m_sUIUserName ) && ( sPassWord == m_choices.m_sUIPassWord ) );
}

void Server::AddCandidateFutures( fAddCandidateFutures_t&& f ) {
  //TODO: need to query Server_impl to see if something in progress, if so, skip to the point of sync
  for ( const std::string& s: m_choices.m_vCandidateFutures ) {
    f( s );
  }
}

void Server::Start(
    const std::string& sSessionId, const std::string& sUnderlyingFuture,
    fUpdateUnderlyingInfo_t&& fUpdateUnderlyingInfo,
    fUpdateUnderlyingPrice_t&& fUpdateUnderlyingPrice,
    fUpdateOptionExpiries_t&& fUpdateOptionExpiries,
    fUpdateOptionExpiriesDone_t&& fUpdateOptionExpiriesDone
) {
  assert( fUpdateUnderlyingInfo );
  m_fUpdateUnderlyingInfo = std::move( fUpdateUnderlyingInfo );

  assert( fUpdateUnderlyingPrice );
  m_fUpdateUnderlyingPrice = std::move( fUpdateUnderlyingPrice );

  assert( fUpdateOptionExpiries );
  m_fUpdateOptionExpiries = std::move( fUpdateOptionExpiries );

  assert( fUpdateOptionExpiriesDone );
  m_fUpdateOptionExpiriesDone = std::move( fUpdateOptionExpiriesDone );

  //boost::format format( sFormatFloat );

  m_implServer->Start(
    sUnderlyingFuture,
    [this,sSessionId](const std::string& sName, int multiplier ) { // fUpdateUnderlyingInfo_t
      post(
        sSessionId,
        [this, sName_=std::move(sName), multiplier ]() {
          std::string sMultiplier = boost::lexical_cast<std::string>( multiplier );
          m_fUpdateUnderlyingInfo( sName_, sMultiplier );
        }
      );
    },
    [this,sSessionId]( double price, int precision) mutable { // fUpdateUnderlyingPrice_t
      //format_ % precision % price;
      boost::format format( "%0." + boost::lexical_cast<std::string>( precision ) + "f" );
      format % price;
      std::string sPrice( format.str() );
      //post(
      //  sSessionId,
      //  [this,sPrice_=std::move(sPrice)](){
          m_fUpdateUnderlyingPrice( sPrice );
      //  }
      //);
    },
    [this,sSessionId]( boost::gregorian::date date ){ // fAddExpiry_t
      post(
        sSessionId,
        [this,date](){
          //std::string sDate = boost::lexical_cast<std::string>( date );
          const std::string sDate = boost::gregorian::to_iso_string( date );
          m_fUpdateOptionExpiries( sDate );
        }
      );
    },
    [this,sSessionId](){ // fAddExpiryDone_t
      post(
        sSessionId,
        [this](){
          m_fUpdateOptionExpiriesDone();
        }
      );
    }
  );
}

void Server::PrepareStrikeSelection(
  const std::string& sDate,
  fPopulateStrike_t&& fPopulateStrike,
  fPopulateStrikeDone_t&& fPopulateStrikeDone
) {
  boost::gregorian::date date( boost::gregorian::date_from_iso_string( sDate ) );
  m_implServer->PopulateStrikes(
    date,
    [fPopulateStrike_=std::move(fPopulateStrike)]( double strike, int precision){ // fPopulateStrike_t
      boost::format format( "%0." + boost::lexical_cast<std::string>( precision ) + "f" );
      format % strike;
      fPopulateStrike_( format.str() );

      if ( false ) {
        try {
          format % precision % strike;
          fPopulateStrike_( format.str() );
        }
        catch( boost::io::too_many_args& e ) {
          BOOST_LOG_TRIVIAL(error) << "boost::format too many args " << strike << "," << precision ;
        }
        catch (boost::io::too_few_args& e ) {
          BOOST_LOG_TRIVIAL(error) << "boost::format too few args " << strike << "," << precision;
        }
      }
    },
    [fPopulateStrikeDone_=std::move(fPopulateStrikeDone)](){ // fPopulateStrikeDone_t
      fPopulateStrikeDone_();
    }
  );
}

void Server::ChangeInvestment( const std::string& sInvestmentAmount ) {
  try {
    double amount = boost::lexical_cast<double>( sInvestmentAmount );
    m_implServer->ChangeInvestment( amount );
  }
  catch ( boost::bad_lexical_cast& e ) {
  }
}

void Server::TriggerUpdates( const std::string& sSessionId ) {
  m_implServer->TriggerUpdates( sSessionId );
}

void Server::AddStrike(
  const std::string& sSessionId,
  EOptionType type, EOrderSide side,
  const std::string& sStrike,
  fPopulateOption_t&& fPopulateOption,
  fUpdateAllocated_t&& fUpdateAllocated,
  fRealTime_t&& fRealTime,
  fFill_t&& fFillEntry, fFill_t&& fFillExit
) {

  assert( fUpdateAllocated );
  assert( fPopulateOption );
  assert( fRealTime );
  assert( fFillEntry );
  assert( fFillExit );

  Server_impl::fRealTime_t fRealTime_impl =
    [fRealTime_=std::move(fRealTime)]( uint32_t oi, double bid, double ask, uint32_t precision, uint32_t volume, uint32_t contracts, double pnl ){

      const std::string sOI = boost::lexical_cast<std::string>( oi );

      boost::format formatPrice( "%0." + boost::lexical_cast<std::string>( precision ) + "f" );

      formatPrice % bid;
      const std::string sBid = boost::lexical_cast<std::string>( formatPrice.str() );

      formatPrice % ask;
      const std::string sAsk = boost::lexical_cast<std::string>( formatPrice.str() );

      const std::string sVol = boost::lexical_cast<std::string>( volume );
      const std::string sCon = boost::lexical_cast<std::string>( contracts );

      boost::format formatPnL( sFormatUSD );
      formatPnL % pnl;
      fRealTime_( sOI, sBid, sAsk, sVol, sCon, formatPnL.str() );
    };

  Server_impl::fAllocated_t fAllocated_impl =
    [fUpdateAllocated_=std::move(fUpdateAllocated)](double allocatedTotal, double allocatedOption ){
      const std::string sTotal = boost::lexical_cast<std::string>( allocatedTotal );
      const std::string sOption = boost::lexical_cast<std::string>( allocatedOption );
      fUpdateAllocated_( sTotal, sOption );
    };

  Server_impl::fFill_t fFillEntry_impl =
    [this,sSessionId,fFill=std::move(fFillEntry)]( uint32_t quan, double fill ){
      post(
        sSessionId,
        [this, quan, fill, fFill = std::move( fFill ) ]() {
          boost::format format( "%d@" + sFormatUSD );
          format % quan % fill;
          fFill( format.str() );
        }
      );
    };

  Server_impl::fFill_t fFillExit_impl =
    [this,sSessionId,fFill=std::move(fFillExit)]( uint32_t quan, double fill ){
      post(
        sSessionId,
        [this, quan, fill, fFill = std::move( fFill ) ]() {
          boost::format format( "%d@" + sFormatUSD );
          format % quan % fill;
          fFill( format.str() );
        }
      );
    };

  ou::tf::OrderSide::EOrderSide side_;
  switch ( side ) {
    case EOrderSide::buy:
      side_ = ou::tf::OrderSide::Buy;
      break;
    case EOrderSide::sell:
      side_ = ou::tf::OrderSide::Sell;
      break;
    default:
      assert( false );
  }

  double strike = boost::lexical_cast<double>( sStrike );
  // open interest will have to come during watch startup, and populate ticker again with real open interest, will need to use 'post' with session id for that
  switch ( type ) {
    case EOptionType::call:
      fPopulateOption( m_implServer->Ticker( strike, ou::tf::OptionSide::Call ) );
      m_implServer->AddStrike(
        strike, ou::tf::OptionSide::Call, side_,
        std::move( fRealTime_impl ),
        std::move( fAllocated_impl ),
        std::move( fFillEntry_impl ),
        std::move( fFillExit_impl )
        );
      break;
    case EOptionType::put:
      fPopulateOption( m_implServer->Ticker( strike, ou::tf::OptionSide::Put ) );
      m_implServer->AddStrike(
        strike, ou::tf::OptionSide::Put, side_,
        std::move( fRealTime_impl ),
        std::move( fAllocated_impl ),
        std::move( fFillEntry_impl ),
        std::move( fFillExit_impl )
        );
      break;
  }
}

void Server::DelStrike( const std::string& sStrike ) {
  double strike = boost::lexical_cast<double>( sStrike );
  m_implServer->DelStrike( strike );
}

void Server::ChangeAllocation( const std::string& sStrike, const std::string& sPercent ) {
  try {
    double strike = boost::lexical_cast<double>( sStrike );
    double percent = boost::lexical_cast<double>( sPercent );
    m_implServer->ChangeAllocation( strike, percent / 100.0 );
  }
  catch ( boost::bad_lexical_cast& e ) {
    assert( false );
  }
}

std::string Server::SetOrderType(
  EOrderType eOrderType,
  const std::string& sStrike,
  const std::string& sLimitPrice,
  const std::string& sInitialQuantity,
  const std::string& sIncrementQuantity,
  const std::string& sIncrementPrice
) {
  std::string sMessage;

  double dblStrike {};
  double dblLimitPrice {};
  uint32_t nInitialQuantity {};
  uint32_t nIncrementQuantity {};
  double dblIncrementPrice {};

  bool bOk( true );

  try {

    dblStrike = boost::lexical_cast<double>( sStrike );

    switch ( eOrderType ) {
      case EOrderType::market:
        sMessage += m_implServer->SetAsMarket( dblStrike );
        break;
      case EOrderType::limit_manual:
      case EOrderType::limit_ask:
      case EOrderType::limit_bid:
        if ( sLimitPrice.empty() ) {}
        else {
          dblLimitPrice = boost::lexical_cast<double>( sLimitPrice );
          if ( 0.0 < dblLimitPrice ) {
            sMessage += m_implServer->SetAsLimit( dblStrike, dblLimitPrice );
          }
          else {
            if ( !sMessage.empty() ) sMessage+= ", ";
            sMessage += "limit price not gt 0.0";
          }
        }
        break;
      case EOrderType::scale:

        if ( sLimitPrice.empty() ) {
          if ( !sMessage.empty() ) sMessage+= ", ";
          sMessage += "limit price is a required field";
        }
        else {
          dblLimitPrice = boost::lexical_cast<double>( sLimitPrice );
        }
        bOk &= ( 0.0 < dblLimitPrice );

        if ( sInitialQuantity.empty() ) {
          if ( !sMessage.empty() ) sMessage+= ", ";
          sMessage += "initial quantity is a required field";
        }
        else {
          nInitialQuantity = boost::lexical_cast<uint32_t>( sInitialQuantity );
        }
        bOk &= ( 0 < nInitialQuantity );

        if ( sIncrementQuantity.empty() ) {
          if ( !sMessage.empty() ) sMessage+= ", ";
          sMessage += "incremental quantity is a required field";
        }
        else {
          nIncrementQuantity = boost::lexical_cast<uint32_t>( sIncrementQuantity );
        }
        bOk &= ( 0 < nIncrementQuantity );

        if ( sIncrementPrice.empty() ) {
          if ( !sMessage.empty() ) sMessage+= ", ";
          sMessage += "increment price is a required field";
        }
        else {
          dblIncrementPrice = boost::lexical_cast<double>( sIncrementPrice );
        }
        bOk &= ( 0.0 < dblIncrementPrice );

        if ( bOk ) {
          sMessage += m_implServer->SetAsScale( dblStrike, dblLimitPrice, nInitialQuantity, nIncrementQuantity, dblIncrementPrice );
        }
        else {
          if ( !sMessage.empty() ) sMessage+= ", ";
          sMessage += "all fields require positive non-zero value";
        }

        break;
    }

  }
  catch ( const boost::bad_lexical_cast& e ) {
    if ( !sMessage.empty() ) sMessage+= ", ";
    sMessage += "field has illegal content";
  }

  return sMessage;
}

bool Server::PlaceOrders() {

  std::string sDateTimeStamp;

  {
    std::stringstream ss;
    auto dt = ou::TimeSource::GlobalInstance().External();
    ss
      << ou::tf::Instrument::BuildDate( dt.date() )
      << "-"
      << dt.time_of_day()
      ;
    sDateTimeStamp = ss.str();
  }

  return m_implServer->PlaceOrders( sDateTimeStamp );
}

void Server::CancelAll() {
  m_implServer->CancelAll();
}

void Server::CloseAll() {
  m_implServer->CloseAll();
}