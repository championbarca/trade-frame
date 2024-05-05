/************************************************************************
 * Copyright(c) 2024, One Unified. All rights reserved.                 *
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
 * File:    TrackOrder.cpp
 * Author:  raymond@burkholder.net
 * Project: CurrencyTrader
 * Created: April 28, 2024 10:39:16
 */

#include <boost/log/trivial.hpp>

#include <boost/lexical_cast.hpp>

#include <TFInteractiveBrokers/IBTWS.h>

#include "TrackOrder.hpp"

// TODO: may desire events/callbacks/lambdas on order completion

TrackOrder::TrackOrder()
: m_stateTrade( ETradeState::Init )
{}

TrackOrder::~TrackOrder() {}

void TrackOrder::Set( quantity_t quantity, pPosition_t pPosition, ou::ChartDataView& cdv, int slot ) {

  assert( 0 < quantity );
  m_quantityToOrder = quantity;

  assert( pPosition );
  assert( !m_pPosition );
  m_pPosition = pPosition;

  cdv.Add( slot, &m_ceEntrySubmit );
  cdv.Add( slot, &m_ceEntryFill );

  cdv.Add( slot, &m_ceExitSubmit );
  cdv.Add( slot, &m_ceExitFill );

}

void TrackOrder::QueryStats( double& unrealized, double& realized, double& commission, double& total ) {
  m_pPosition->QueryStats( unrealized, realized, commission, total );
}

// see TFTrading/MonitorOrder.cpp
double TrackOrder::PriceInterval( double price ) const {
  double interval {};
  auto pProvider( m_pPosition->GetExecutionProvider() );
  if ( ou::tf::keytypes::EProviderIB == pProvider->ID() ) {
    auto idRule = m_pPosition->GetInstrument()->GetExchangeRule();
    double interval = ou::tf::ib::TWS::Cast( pProvider )->GetInterval( price, idRule );
  }
  else {
    interval = m_pPosition->GetInstrument()->GetMinTick();
  }
  assert( 0.0 < interval );
  return interval;
}

// see TFTrading/MonitorOrder.cpp
double TrackOrder::Normalize( double price ) const {
  double interval = PriceInterval( price );
  return m_pPosition->GetInstrument()->NormalizeOrderPrice( price, interval );
}

void TrackOrder::SetGoodTill( const OrderArgs& args, pOrder_t& pOrder ) {
  // submit GTC limit order (for Interactive Brokers)

  if ( 0 < args.duration ) {
    // strip off fractional seconds
    boost::posix_time::ptime dt
      = args.dt
      - boost::posix_time::time_duration( 0, 0, 0, args.dt.time_of_day().fractional_seconds() );

    pOrder->SetGoodTillDate( dt + boost::posix_time::seconds( args.duration ) );
    pOrder->SetTimeInForce( ou::tf::ETimeInForce::GoodTillDate );
  }
}

void TrackOrder::Common( const OrderArgs& args, pOrder_t& pOrder ) {
  pOrder->SetSignalPrice( args.signal );
  pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  assert( !m_pOrderPending );
  m_pOrderPending = pOrder;
  m_pPosition->PlaceOrder( pOrder );
  //ShowOrder( pOrder );
}

void TrackOrder::EnterCommon( const OrderArgs& args, pOrder_t& pOrder ) {
  m_dblProfitMax = m_dblUnRealized = m_dblProfitMin = 0.0;
  m_stateTrade = ETradeState::EntrySubmitted;
  Common( args, pOrder );
}

void TrackOrder::EnterLongLmt( const OrderArgs& args ) {
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Buy, m_quantityToOrder, Normalize( args.limit ) );
  assert( pOrder );
  SetGoodTill( args, pOrder );
  m_ceEntrySubmit.AddLabel( args.dt, args.signal, "LeS-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  EnterCommon( args, pOrder );
}

void TrackOrder::EnterLongMkt( const OrderArgs& args ) {
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, m_quantityToOrder );
  assert( pOrder );
  m_ceEntrySubmit.AddLabel( args.dt, args.signal, "LeS-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  EnterCommon( args, pOrder );
}

void TrackOrder::EnterShortLmt( const OrderArgs& args ) {
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Sell, m_quantityToOrder, Normalize( args.limit ) );
  assert( pOrder );
  SetGoodTill( args, pOrder );
  m_ceEntrySubmit.AddLabel( args.dt, args.signal, "SeS-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  EnterCommon( args, pOrder );
}

void TrackOrder::EnterShortMkt( const OrderArgs& args ) {
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, m_quantityToOrder );
  assert( pOrder );
  m_ceEntrySubmit.AddLabel( args.dt, args.signal, "SeS-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  EnterCommon( args, pOrder );
}

void TrackOrder::ExitCommon( const OrderArgs& args, pOrder_t& pOrder ) {
  m_stateTrade = ETradeState::ExitSubmitted;
  Common( args, pOrder );
}

void TrackOrder::ExitLongLmt( const OrderArgs& args ) {
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Sell, m_quantityToOrder, Normalize( args.limit ) );
  assert( pOrder );
  SetGoodTill( args, pOrder );
  m_ceExitSubmit.AddLabel( args.dt, args.signal, "LxS1-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  ExitCommon( args, pOrder );
}

void TrackOrder::ExitLongMkt( const OrderArgs& args ) {
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, m_quantityToOrder );
  assert( pOrder );
  m_ceExitSubmit.AddLabel( args.dt, args.signal, "LxS1-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  ExitCommon( args, pOrder );
}

void TrackOrder::ExitShortLmt( const OrderArgs& args ) {
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Buy, m_quantityToOrder, Normalize( args.limit ) );
  assert( pOrder );
  SetGoodTill( args, pOrder );
  m_ceExitSubmit.AddLabel( args.dt, args.signal, "SxS1-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  ExitCommon( args, pOrder );
}

void TrackOrder::ExitShortMkt( const OrderArgs& args ) {
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, m_quantityToOrder );
  assert( pOrder );
  m_ceExitSubmit.AddLabel( args.dt, args.signal, "SxS1-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  ExitCommon( args, pOrder );
}

void TrackOrder::ShowOrder( pOrder_t& pOrder ) {
  //m_pTreeItemOrder = m_pTreeItemSymbol->AppendChild(
  //    "Order "
  //  + boost::lexical_cast<std::string>( m_pOrder->GetOrderId() )
  //  );
}

void TrackOrder::HandleOrderCancelled( const ou::tf::Order& order ) {
  m_pOrderPending->OnOrderCancelled.Remove( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  m_pOrderPending->OnOrderFilled.Remove( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  switch ( m_stateTrade ) {
    case ETradeState::EndOfDayCancel:
    case ETradeState::EndOfDayNeutral:
      BOOST_LOG_TRIVIAL(info)
        << m_pPosition->GetInstrument()->GetInstrumentName()
        << " order " << order.GetOrderId() << " cancelled - end of day";
      break;
    case ETradeState::EntrySubmitted:
      // cancels will happen due to limit time out
      BOOST_LOG_TRIVIAL(info)
        << m_pPosition->GetInstrument()->GetInstrumentName()
        << " order " << order.GetOrderId() << " entry cancelled";
      m_stateTrade = ETradeState::Search;
      break;
    case ETradeState::ExitSubmitted:
      //assert( false );  // TODO: need to figure out a plan to retry exit
      BOOST_LOG_TRIVIAL(error)
        << m_pPosition->GetInstrument()->GetInstrumentName()
        << " order " << order.GetOrderId() << " exit cancelled - state machine needs fixes";
      m_stateTrade = ETradeState::Done;
      break;
    default:
      m_stateTrade = ETradeState::Search;
  }
  m_pOrderPending.reset();
}

void TrackOrder::HandleOrderFilled( const ou::tf::Order& order ) {
  m_pOrderPending->OnOrderCancelled.Remove( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  m_pOrderPending->OnOrderFilled.Remove( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  switch ( m_stateTrade ) {
    case ETradeState::EntrySubmitted:
      m_ceEntryFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Entry Fill" );
      m_stateTrade = ETradeState::ExitSignal;
      break;
    case ETradeState::ExitSubmitted:
      m_ceExitFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Exit Fill" );
      m_stateTrade = ETradeState::Search;
      break;
    case ETradeState::EndOfDayCancel:
    case ETradeState::EndOfDayNeutral:
      // figure out what labels to apply
      break;
    case ETradeState::Done:
      break;
    default:
       assert( false ); // TODO: unravel the state mess if we get here
  }
  m_pOrderPending.reset();
}

void TrackOrder::HandleCancel( boost::gregorian::date, boost::posix_time::time_duration ) { // one shot
  m_stateTrade = TrackOrder::ETradeState::EndOfDayCancel;
  if ( m_pPosition ) {
    m_pPosition->CancelOrders();
  }
}

void TrackOrder::HandleGoNeutral( boost::gregorian::date, boost::posix_time::time_duration ) { // one shot
  switch ( m_stateTrade ) {
    case ETradeState::NoTrade:
      // do nothing
      break;
    default:
      m_stateTrade = TrackOrder::ETradeState::EndOfDayNeutral;
      if ( m_pPosition ) {
        m_pPosition->ClosePosition();
      }
      break;
  }
}

// unused
void TrackOrder::HandleExitOrderCancelled( const ou::tf::Order& order ) {
  ou::tf::Order& order_( const_cast<ou::tf::Order&>( order ) );
  order_.OnOrderCancelled.Remove( MakeDelegate( this, &TrackOrder::HandleExitOrderCancelled ) );
  order_.OnOrderFilled.Remove( MakeDelegate( this, &TrackOrder::HandleExitOrderFilled ) );

  switch ( order.GetOrderSide() ) {
    case ou::tf::OrderSide::EOrderSide::Buy: // is dt filled at 'internal' time?
      //m_ceLongExit.AddLabel( order.GetDateTimeOrderFilled(), order.GetSignalPrice(), "LxC-" + boost::lexical_cast<std::string>( order.GetOrderId() ) );
      m_stateTrade = ETradeState::Cancelled;  // or use cancelled for custom processing
      break;
    case ou::tf::OrderSide::EOrderSide::Sell: // is dt filled at 'internal' time?
      //m_ceShortExit.AddLabel( order.GetDateTimeOrderFilled(), order.GetSignalPrice(), "SxC-" + boost::lexical_cast<std::string>( order.GetOrderId() ) );
      m_stateTrade = ETradeState::Cancelled;  // or use cancelled for custom processing
      break;
    default:
      assert( false );
  }
}

// unused
void TrackOrder::HandleExitOrderFilled( const ou::tf::Order& order ) {
  ou::tf::Order& order_( const_cast<ou::tf::Order&>( order ) );
  order_.OnOrderCancelled.Remove( MakeDelegate( this, &TrackOrder::HandleExitOrderCancelled ) );
  order_.OnOrderFilled.Remove( MakeDelegate( this, &TrackOrder::HandleExitOrderFilled ) );

  switch ( order.GetOrderSide() ) {
    case ou::tf::OrderSide::EOrderSide::Buy:
      //m_ceLongExit.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "LxF-" + boost::lexical_cast<std::string>( order.GetOrderId() ) );
      switch( m_stateTrade ) {
        case ETradeState::ExitSubmitted:
          m_stateTrade = ETradeState::Search;
          break;
      }
      break;
    case ou::tf::OrderSide::EOrderSide::Sell:
      //m_ceShortExit.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "SxF-" + boost::lexical_cast<std::string>( order.GetOrderId() ) );
      switch( m_stateTrade ) {
        case ETradeState::ExitSubmitted:
          m_stateTrade = ETradeState::Search;
          break;
      }
      break;
    default:
      assert( false );
  }
}

// unused
void TrackOrder::ExitPosition( const ou::tf::Quote& quote ) {
  pOrder_t pOrder;
  double dblMidPoint( quote.Midpoint() );

  if ( m_pPosition->IsActive() ) {
    assert( false ); // size of 1 not going to work with currency
    assert( 1 == m_pPosition->GetActiveSize() );
    switch ( m_pPosition->GetRow().eOrderSideActive ) {
      case ou::tf::OrderSide::EOrderSide::Buy:
        pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, 1 );
        pOrder->SetSignalPrice( dblMidPoint );
        pOrder->SetDescription(
            m_sProfitDescription + ","
          + boost::lexical_cast<std::string>( m_dblProfitMin ) + ","
          + boost::lexical_cast<std::string>( m_dblUnRealized ) + ","
          + boost::lexical_cast<std::string>( m_dblProfitMax - m_dblUnRealized ) + ","
          + boost::lexical_cast<std::string>( m_dblProfitMax )
          );
        pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleExitOrderCancelled ) );
        pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleExitOrderFilled ) );
        m_ceExitSubmit.AddLabel( quote.DateTime(), dblMidPoint, "LxS2-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
        m_stateTrade = ETradeState::ExitSubmitted;
        m_pPosition->PlaceOrder( pOrder );
        ShowOrder( pOrder );
        break;
      case ou::tf::OrderSide::EOrderSide::Sell:
        pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, 1 );
        pOrder->SetSignalPrice( dblMidPoint );
        pOrder->SetDescription(
            m_sProfitDescription + ","
          + boost::lexical_cast<std::string>( m_dblProfitMin ) + ","
          + boost::lexical_cast<std::string>( m_dblUnRealized ) + ","
          + boost::lexical_cast<std::string>( m_dblProfitMax - m_dblUnRealized ) + ","
          + boost::lexical_cast<std::string>( m_dblProfitMax )
          );
        pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleExitOrderCancelled ) );
        pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleExitOrderFilled ) );
        m_ceExitSubmit.AddLabel( quote.DateTime(), dblMidPoint, "SxS2-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
        m_stateTrade = ETradeState::ExitSubmitted;
        m_pPosition->PlaceOrder( pOrder );
        ShowOrder( pOrder );
        break;
      default:
        assert( false ); // maybe check for unknown
    }
  }
  else {
    m_stateTrade = ETradeState::Search;
  }
}

