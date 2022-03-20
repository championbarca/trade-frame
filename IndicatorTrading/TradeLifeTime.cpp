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
 * File:    TradeLifeTime.cpp
 * Author:  raymond@burkholder.net
 * Project: IndicatorTrading
 * Created: March 9, 2022 16:38
 */

#include <boost/lexical_cast.hpp>

#include <TFInteractiveBrokers/IBTWS.h>

#include <TFVuTrading/PanelOrderButtons_structs.h>

#include "TradeLifeTime.h"

using EPositionEntryMethod = ou::tf::PanelOrderButtons_Order::EPositionEntryMethod;
using EPositionExitProfitMethod = ou::tf::PanelOrderButtons_Order::EPositionExitProfitMethod;
using EPositionExitStopMethod = ou::tf::PanelOrderButtons_Order::EPositionExitStopMethod;

TradeLifeTime::TradeLifeTime( pPosition_t pPosition, const ou::tf::PanelOrderButtons_Order& selectors, Indicators& indicators )
: m_statePosition( EPositionState::InitializeEntry )
, m_pPosition( pPosition )
, m_bWatching( false )
, m_bWatchStop( false )
, m_dblStopTrailDelta( 0.0 )
, m_ceBuySubmit(  indicators.ceBuySubmit )
, m_ceBuyFill(    indicators.ceBuyFill )
, m_ceSellSubmit( indicators.ceSellSubmit )
, m_ceSellFill(   indicators.ceSellFill )
, m_ceCancelled(  indicators.ceCancelled )
{}

TradeLifeTime::~TradeLifeTime() {
  StopWatch();
  m_pPosition.reset();
}

void TradeLifeTime::HandleOrderCancelled( const ou::tf::Order& order ) {
  std::cout << "LifeTime order " << order.GetOrderId() << " cancelled" << std::endl;
}

void TradeLifeTime::HandleOrderFilled( const ou::tf::Order& order ) {
  std::cout << "Lifetime order " << order.GetOrderId() << " filled" << std::endl;
  m_statePosition = EPositionState::EnteredPosition;
}

void TradeLifeTime::StartWatch() {
  if ( !m_bWatching ) {
    m_bWatching = true;
    ou::tf::Watch::pWatch_t pWatch = m_pPosition->GetWatch();
    pWatch->OnQuote.Add( MakeDelegate( this, &TradeLifeTime::HandleQuote ) );
    pWatch->StartWatch();
  }
}

void TradeLifeTime::StopWatch() {
  if ( m_bWatching ) {
    m_bWatching = false;
    ou::tf::Watch::pWatch_t pWatch = m_pPosition->GetWatch();
    pWatch->StopWatch();
    pWatch->OnQuote.Remove( MakeDelegate( this, &TradeLifeTime::HandleQuote ) );
  }
}

void TradeLifeTime::ClearOrders() {
}

// see TFTrading/MonitorOrder.cpp
double TradeLifeTime::PriceInterval( double price ) const {
  auto idRule = m_pPosition->GetInstrument()->GetExchangeRule();
  double interval = boost::dynamic_pointer_cast<ou::tf::ib::TWS>( m_pPosition->GetExecutionProvider() )->GetInterval( price, idRule );
  return interval;
}

// see TFTrading/MonitorOrder.cpp
double TradeLifeTime::NormalizePrice( double price ) const {
  double interval = PriceInterval( price );
  return m_pPosition->GetInstrument()->NormalizeOrderPrice( price, interval );
}

void TradeLifeTime::HandleQuote( const ou::tf::Quote& quote ) {
  m_quote = quote;
}

size_t TradeLifeTime::Quantity( pPosition_t pPosition, const ou::tf::PanelOrderButtons_Order& selectors ) const {
  size_t quantity {};
  switch ( pPosition->GetInstrument()->GetInstrumentType() ) {
    case ou::tf::InstrumentType::Future:
      quantity = selectors.QuanFuture();
      break;
    case ou::tf::InstrumentType::Stock:
      quantity = selectors.QuanStock();
      break;
    case ou::tf::InstrumentType::Option:
      quantity = selectors.QuanOption();
      break;
    default:
      assert( false );
      break;
  }
  assert( 0 < quantity );
  return quantity;
}

void TradeLifeTime::Cancel() {

  bool bCancelled( false );
  std::string sCancelled( "Cancelled:" );

  auto fCancel =
    [this,&bCancelled,&sCancelled]( pOrder_t pOrder ){
      if ( ou::tf::OrderStatus::Created == pOrder->OrderStatus() ) {}
      else {
        if ( 0 < pOrder->GetQuanRemaining() ) {
          m_pPosition->CancelOrder( pOrder->GetOrderId() );
          bCancelled = true;
          sCancelled += " " + boost::lexical_cast<std::string>( pOrder->GetOrderId() );
        }
      }
    };

  if ( m_pOrderProfit ) {
    fCancel( m_pOrderProfit );
  }
  if ( m_pOrderEntry ) {
    fCancel( m_pOrderEntry );
  }
  if ( m_pOrderStop ) {
    fCancel( m_pOrderStop );
  }

  if ( bCancelled ) {
    m_ceCancelled.AddLabel( m_quote.DateTime(), m_quote.Midpoint(), sCancelled );
  }
  else {
    std::cout << "TradeLifeTime::Cancel - nothing cancelled - " << m_pOrderEntry->GetOrderId() << std::endl;
  }
}

void TradeLifeTime::EmitStatus() {
  std::string sStatus( "TradeLifeTime::Status:" );
  if ( m_pOrderEntry ) {
    sStatus
      += " entry " + boost::lexical_cast<std::string>( m_pOrderEntry->GetOrderId() )
      +  " has "
      +  boost::lexical_cast<std::string>( m_pOrderEntry->GetQuanRemaining() )
      + " remaining of "
      +  boost::lexical_cast<std::string>( m_pOrderEntry->GetQuanOrdered() )
      + " ordered;"
      ;
  }
  if ( m_pOrderProfit ) {
    sStatus
      += " profit order " + boost::lexical_cast<std::string>( m_pOrderProfit->GetOrderId() )
      +  " has "
      ;
    if ( ou::tf::OrderStatus::Created == m_pOrderProfit->OrderStatus() ) {
      sStatus += "not be submitted";
    }
    else {
      sStatus
      += boost::lexical_cast<std::string>( m_pOrderProfit->GetQuanRemaining() )
      + " remaining of "
      +  boost::lexical_cast<std::string>( m_pOrderProfit->GetQuanOrdered() )
      + " ordered;"
      ;
    }
  }
  if ( m_pOrderStop ) {
    sStatus
      += " stop order " + boost::lexical_cast<std::string>( m_pOrderProfit->GetOrderId() )
      +  " has "
      ;
    if ( ou::tf::OrderStatus::Created == m_pOrderProfit->OrderStatus() ) {
      sStatus += "not be submitted";
    }
    else {
    sStatus
      += boost::lexical_cast<std::string>( m_pOrderStop->GetQuanRemaining() )
      + " remaining of "
      +  boost::lexical_cast<std::string>( m_pOrderStop->GetQuanOrdered() )
      + " ordered;"
      ;
    }
  }
  std::cout << sStatus << std::endl;
}

//void TradeLifeTime::Close() {
// determine direction, then do:
// OrderBuy( ou::tf::PanelOrderButtons_Order() );
// OrderSell( ou::tf::PanelOrderButtons_Order() );
// and delete menu item
// need to check the other orders, cancel if exists
// no close if entry has't been established
// too complicated, simply cancel stuff, then in caller create a new LifeTime to exit position
//}

// ===== TradeWithABuy =====

TradeWithABuy::TradeWithABuy( pPosition_t pPosition, const ou::tf::PanelOrderButtons_Order& selectors, Indicators& indicators )
: TradeLifeTime( pPosition, selectors, indicators )
{
  std::cout << pPosition->GetInstrument()->GetInstrumentName() << " buying" << std::endl;

  ou::tf::Quote quote( m_pPosition->GetWatch()->LastQuote() ); // probably no quotes yet

  size_t quantity = Quantity( pPosition, selectors );

  assert( selectors.m_bPositionEntryEnable );
  {
    switch ( selectors.m_ePositionEntryMethod ) {
      case EPositionEntryMethod::Market:
        m_pOrderEntry = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, quantity );
        m_ceBuySubmit.AddLabel( quote.DateTime(), quote.Midpoint(), "Buy Submit " + boost::lexical_cast<std::string>( m_pOrderEntry->GetOrderId() ) );
        break;
      case EPositionEntryMethod::Limit:
        {
          double price( NormalizePrice( selectors.PositionEntryValue() ) );
          m_pOrderEntry = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Buy, quantity, price );
          m_ceBuySubmit.AddLabel( quote.DateTime(), price, "Buy Submit " + boost::lexical_cast<std::string>( m_pOrderEntry->GetOrderId() ) );
        }
        break;
      case EPositionEntryMethod::Stoch:
        assert( false ); // need code for this
        break;
    }

    m_pOrderEntry->OnOrderCancelled.Add( MakeDelegate( this, &TradeWithABuy::HandleEntryOrderCancelled ) );
    m_pOrderEntry->OnOrderFilled.Add( MakeDelegate( this, &TradeWithABuy::HandleEntryOrderFilled ) );

  }

  if ( selectors.m_bPositionExitProfitEnable ) {
    switch ( selectors.m_ePositionExitProfitMethod ) {
      case EPositionExitProfitMethod::Absolute:
        {
          double value = NormalizePrice( selectors.PositionExitProfitValue() );
          m_pOrderProfit = m_pPosition->ConstructOrder(
            ou::tf::OrderType::Limit, ou::tf::OrderSide::Sell, quantity, value );
        }
        break;
      case EPositionExitProfitMethod::Relative:
        {
          double value = NormalizePrice( quote.Midpoint() + selectors.PositionExitProfitValue() );
          m_pOrderProfit = m_pPosition->ConstructOrder(
            ou::tf::OrderType::Limit, ou::tf::OrderSide::Sell, quantity, value );
        }
        break;
      case EPositionExitProfitMethod::Stoch:
        assert( false ); // need code for this
        break;
    }

    m_pOrderProfit->OnOrderCancelled.Add( MakeDelegate( this, &TradeWithABuy::HandleProfitOrderCancelled ) );
    m_pOrderProfit->OnOrderFilled.Add( MakeDelegate( this, &TradeWithABuy::HandleProfitOrderFilled ) );

  }

  if ( selectors.m_bPositionExitStopEnable ) {
    switch ( selectors.m_ePositionExitStopMethod ) {
      case EPositionExitStopMethod::TrailingAbsolute:
        {
          m_dblStopTrailDelta = selectors.PositionExitStopValue();
          m_dblStopCurrent = NormalizePrice( quote.Midpoint() - selectors.PositionExitStopValue() );
          m_pOrderStop = m_pPosition->ConstructOrder(
            ou::tf::OrderType::Stop, ou::tf::OrderSide::Sell, quantity, m_dblStopCurrent );
        }
        break;
      case EPositionExitStopMethod::TrailingPercent:
        {
          assert( false ); // need code for this
        }
        break;
      case EPositionExitStopMethod::Stop:
        {
          m_dblStopCurrent = NormalizePrice( selectors.PositionExitStopValue() );
          m_pOrderStop = m_pPosition->ConstructOrder(
            ou::tf::OrderType::Stop, ou::tf::OrderSide::Sell, quantity, m_dblStopCurrent );
        }
        break;
    }

    m_pOrderStop->OnOrderCancelled.Add( MakeDelegate( this, &TradeWithABuy::HandleStopOrderCancelled ) );
    m_pOrderStop->OnOrderFilled.Add( MakeDelegate( this, &TradeWithABuy::HandleStopOrderFilled ) );

  }

  m_statePosition = EPositionState::EnteringPosition;
  m_pPosition->PlaceOrder( m_pOrderEntry );
  std::cout << "TradeWithABuy entry order " << m_pOrderEntry->GetOrderId() << " placed" << std::endl;
  if ( m_bWatchStop ) {
    StartWatch();
  }
}

TradeWithABuy::~TradeWithABuy() {
  if ( m_pOrderEntry ) {
    m_pOrderEntry->OnOrderCancelled.Remove( MakeDelegate( this, &TradeWithABuy::HandleEntryOrderCancelled ) );
    m_pOrderEntry->OnOrderFilled.Remove( MakeDelegate( this, &TradeWithABuy::HandleEntryOrderFilled ) );
    m_pOrderEntry.reset();
  }
  if ( m_pOrderStop ) {
    m_pOrderStop->OnOrderCancelled.Remove( MakeDelegate( this, &TradeWithABuy::HandleStopOrderCancelled ) );
    m_pOrderStop->OnOrderFilled.Remove( MakeDelegate( this, &TradeWithABuy::HandleStopOrderFilled ) );
    m_pOrderStop.reset();
  }
  if ( m_pOrderProfit ) {
    m_pOrderProfit->OnOrderCancelled.Add( MakeDelegate( this, &TradeWithABuy::HandleProfitOrderCancelled ) );
    m_pOrderProfit->OnOrderFilled.Add( MakeDelegate( this, &TradeWithABuy::HandleProfitOrderFilled ) );
    m_pOrderProfit.reset();
  }
}

void TradeWithABuy::HandleQuote( const ou::tf::Quote& quote ) {
  TradeLifeTime::HandleQuote( quote );
  if ( m_bWatchStop ) {
    double stop = NormalizePrice( quote.Midpoint() - m_dblStopTrailDelta );
    if ( stop > m_dblStopCurrent ) {
      m_pOrderStop->SetPrice1( stop );
      m_dblStopCurrent = stop;
      m_pPosition->UpdateOrder( m_pOrderStop );
      m_ceSellSubmit.AddLabel( quote.DateTime(), stop,  "Stop Update " + boost::lexical_cast<std::string>( m_pOrderStop->GetOrderId() ) );
    }
  }
}

void TradeWithABuy::HandleEntryOrderCancelled( const ou::tf::Order& order ) {
  StopWatch();
  m_statePosition = EPositionState::Done;
  TradeLifeTime::HandleOrderCancelled( order );
}

void TradeWithABuy::HandleEntryOrderFilled( const ou::tf::Order& order ) {
  m_ceBuyFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Buy Fill" );
  if ( m_pOrderStop ) {
    m_pPosition->PlaceOrder( m_pOrderStop );
    if ( 0.0 < m_dblStopTrailDelta ) m_bWatchStop = true;
    m_ceSellSubmit.AddLabel( m_quote.DateTime(), m_pOrderStop->GetPrice1(), "Stop Submit " + boost::lexical_cast<std::string>( m_pOrderStop->GetOrderId() ) );
    std::cout << "order " << m_pOrderStop->GetOrderId() << " placed (buy stop)" << std::endl;
  }
  if ( m_pOrderProfit ) {
    m_pPosition->PlaceOrder( m_pOrderProfit );
    m_ceSellSubmit.AddLabel( m_quote.DateTime(), m_pOrderProfit->GetPrice1(), "Profit Submit " + boost::lexical_cast<std::string>( m_pOrderProfit->GetOrderId() ) );
    std::cout << "order " << m_pOrderProfit->GetOrderId() << " placed (buy profit)" << std::endl;
  }
  TradeLifeTime::HandleOrderFilled( order );
}

void TradeWithABuy::HandleProfitOrderCancelled( const ou::tf::Order& order ) {
  TradeLifeTime::HandleOrderCancelled( order );
}

void TradeWithABuy::HandleProfitOrderFilled( const ou::tf::Order& order ) {
  if ( m_pOrderStop ) {
    m_pPosition->CancelOrder( m_pOrderStop->GetOrderId() );
  }
  m_ceSellFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Profit Fill" );
  TradeLifeTime::HandleOrderFilled( order );
}

void TradeWithABuy::HandleStopOrderCancelled( const ou::tf::Order& order ) {
  m_bWatchStop = false;
  TradeLifeTime::HandleOrderCancelled( order );
}

void TradeWithABuy::HandleStopOrderFilled( const ou::tf::Order& order ) {
  m_bWatchStop = false;
  if ( m_pOrderProfit ) {
    m_pPosition->CancelOrder( m_pOrderProfit->GetOrderId() );
  }
  m_ceSellFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Stop Fill" );
  TradeLifeTime::HandleOrderFilled( order );
}

void TradeWithABuy::Cancel() {
  TradeLifeTime::Cancel();
}

// ===== TradeWithASell =====

TradeWithASell::TradeWithASell( pPosition_t pPosition, const ou::tf::PanelOrderButtons_Order& selectors, Indicators& indicators )
: TradeLifeTime( pPosition, selectors, indicators )
{
  std::cout << pPosition->GetInstrument()->GetInstrumentName() << " selling" << std::endl;

  ou::tf::Quote quote( m_pPosition->GetWatch()->LastQuote() ); // probably no quotes yet

  size_t quantity = Quantity( pPosition, selectors );

  assert( selectors.m_bPositionEntryEnable );
  {
    switch ( selectors.m_ePositionEntryMethod ) {
      case EPositionEntryMethod::Market:
        m_pOrderEntry = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, quantity );
        m_ceSellSubmit.AddLabel( quote.DateTime(), quote.Midpoint(), "Sell Submit " + boost::lexical_cast<std::string>( m_pOrderEntry->GetOrderId() ) );
        break;
      case EPositionEntryMethod::Limit:
        {
          double price( NormalizePrice( selectors.PositionEntryValue() ) );
          m_pOrderEntry = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Sell, quantity, price );
          m_ceSellSubmit.AddLabel( quote.DateTime(), price, "Sell Submit" + boost::lexical_cast<std::string>( m_pOrderEntry->GetOrderId() ) );
        }
        break;
      case EPositionEntryMethod::Stoch:
        assert( false ); // need code for this
        break;
    }

    m_pOrderEntry->OnOrderCancelled.Add( MakeDelegate( this, &TradeWithASell::HandleEntryOrderCancelled ) );
    m_pOrderEntry->OnOrderFilled.Add( MakeDelegate( this, &TradeWithASell::HandleEntryOrderFilled ) );

  }

  if ( selectors.m_bPositionExitProfitEnable ) {
    switch ( selectors.m_ePositionExitProfitMethod ) {
      case EPositionExitProfitMethod::Absolute:
        {
          double value = NormalizePrice( selectors.PositionExitProfitValue() );
          m_pOrderProfit = m_pPosition->ConstructOrder(
            ou::tf::OrderType::Limit, ou::tf::OrderSide::Buy, quantity, value );
        }
        break;
      case EPositionExitProfitMethod::Relative:
        {
          double value = NormalizePrice( quote.Midpoint() - selectors.PositionExitProfitValue() );
          m_pOrderProfit = m_pPosition->ConstructOrder(
            ou::tf::OrderType::Limit, ou::tf::OrderSide::Buy, quantity, value );
        }
        break;
      case EPositionExitProfitMethod::Stoch:
        assert( false ); // need code for this
        break;
    }

    m_pOrderProfit->OnOrderCancelled.Add( MakeDelegate( this, &TradeWithASell::HandleProfitOrderCancelled ) );
    m_pOrderProfit->OnOrderFilled.Add( MakeDelegate( this, &TradeWithASell::HandleProfitOrderFilled ) );

  }

  if ( selectors.m_bPositionExitStopEnable ) {
    switch ( selectors.m_ePositionExitStopMethod ) {
      case EPositionExitStopMethod::TrailingAbsolute:
        {
          m_dblStopTrailDelta = selectors.PositionExitStopValue();
          m_dblStopCurrent = NormalizePrice( quote.Midpoint() + selectors.PositionExitStopValue() );
          m_pOrderStop = m_pPosition->ConstructOrder(
            ou::tf::OrderType::Stop, ou::tf::OrderSide::Buy, quantity, m_dblStopCurrent );
        }
        break;
      case EPositionExitStopMethod::TrailingPercent:
        {
          assert( false ); // need code for this
        }
        break;
      case EPositionExitStopMethod::Stop:
        {
          m_dblStopCurrent = NormalizePrice( selectors.PositionExitStopValue() );
          m_pOrderStop = m_pPosition->ConstructOrder(
            ou::tf::OrderType::Stop, ou::tf::OrderSide::Buy, quantity, m_dblStopCurrent );
        }
        break;
    }

    m_pOrderStop->OnOrderCancelled.Add( MakeDelegate( this, &TradeWithASell::HandleStopOrderCancelled ) );
    m_pOrderStop->OnOrderFilled.Add( MakeDelegate( this, &TradeWithASell::HandleStopOrderFilled ) );

  }

  m_statePosition = EPositionState::EnteringPosition;
  m_pPosition->PlaceOrder( m_pOrderEntry );
  std::cout << "TradeWithASell entry order " << m_pOrderEntry->GetOrderId() << " placed" << std::endl;

  if ( m_bWatchStop ) {
    StartWatch();
  }
}

TradeWithASell::~TradeWithASell() {
  if ( m_pOrderEntry ) {
    m_pOrderEntry->OnOrderCancelled.Remove( MakeDelegate( this, &TradeWithASell::HandleEntryOrderCancelled ) );
    m_pOrderEntry->OnOrderFilled.Remove( MakeDelegate( this, &TradeWithASell::HandleEntryOrderFilled ) );
    m_pOrderEntry.reset();
  }
  if ( m_pOrderStop ) {
    m_pOrderStop->OnOrderCancelled.Remove( MakeDelegate( this, &TradeWithASell::HandleStopOrderCancelled ) );
    m_pOrderStop->OnOrderFilled.Remove( MakeDelegate( this, &TradeWithASell::HandleStopOrderFilled ) );
    m_pOrderStop.reset();
  }
  if ( m_pOrderProfit ) {
    m_pOrderProfit->OnOrderCancelled.Add( MakeDelegate( this, &TradeWithASell::HandleProfitOrderCancelled ) );
    m_pOrderProfit->OnOrderFilled.Add( MakeDelegate( this, &TradeWithASell::HandleProfitOrderFilled ) );
    m_pOrderProfit.reset();
  }
}

void TradeWithASell::HandleQuote( const ou::tf::Quote& quote ) {
  TradeLifeTime::HandleQuote( quote );
  if ( m_bWatchStop ) {
    double stop = NormalizePrice( quote.Midpoint() + m_dblStopTrailDelta );
    if ( stop < m_dblStopCurrent ) {
      m_pOrderStop->SetPrice1( stop );
      m_dblStopCurrent = stop;
      m_pPosition->UpdateOrder( m_pOrderStop );
      m_ceSellSubmit.AddLabel( quote.DateTime(), stop,  "Stop Update" );
    }
  }
}

void TradeWithASell::HandleEntryOrderCancelled( const ou::tf::Order& order ) {
  StopWatch();
  m_statePosition = EPositionState::Done;
  TradeLifeTime::HandleOrderCancelled( order );
}

void TradeWithASell::HandleEntryOrderFilled( const ou::tf::Order& order ) {
  m_ceBuyFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Sell Fill" );
  if ( m_pOrderStop ) {
    m_pPosition->PlaceOrder( m_pOrderStop );
    if ( 0.0 < m_dblStopTrailDelta ) m_bWatchStop = true;
    m_ceBuySubmit.AddLabel( m_quote.DateTime(), m_pOrderStop->GetPrice1(), "Stop Submit " + boost::lexical_cast<std::string>( m_pOrderStop->GetOrderId() ) );
    std::cout << "order " << m_pOrderStop->GetOrderId() << " placed (sell stop)" << std::endl;
  }
  if ( m_pOrderProfit ) {
    m_pPosition->PlaceOrder( m_pOrderProfit );
    m_ceBuySubmit.AddLabel( m_quote.DateTime(), m_pOrderProfit->GetPrice1(), "Profit Submit " + boost::lexical_cast<std::string>( m_pOrderProfit->GetOrderId() ) );
    std::cout << "order " << m_pOrderProfit->GetOrderId() << " placed (sell profit)" << std::endl;
  }
  TradeLifeTime::HandleOrderFilled( order );
}

void TradeWithASell::HandleProfitOrderCancelled( const ou::tf::Order& order ) {
  TradeLifeTime::HandleOrderCancelled( order );
}

void TradeWithASell::HandleProfitOrderFilled( const ou::tf::Order& order ) {
  if ( m_pOrderStop ) {
    m_pPosition->CancelOrder( m_pOrderStop->GetOrderId() );
  }
  m_ceBuyFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Profit Fill" );
  TradeLifeTime::HandleOrderFilled( order );
}

void TradeWithASell::HandleStopOrderCancelled( const ou::tf::Order& order ) {
  m_bWatchStop = false;
  TradeLifeTime::HandleOrderCancelled( order );
}

void TradeWithASell::HandleStopOrderFilled( const ou::tf::Order& order ) {
  m_bWatchStop = false;
  if ( m_pOrderProfit ) {
    m_pPosition->CancelOrder( m_pOrderProfit->GetOrderId() );
  }
  m_ceBuyFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Stop Fill" );
  TradeLifeTime::HandleOrderFilled( order );
}

void TradeWithASell::Cancel() {
  TradeLifeTime::Cancel();
}
