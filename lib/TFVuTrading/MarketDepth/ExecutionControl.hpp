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
 * File:    ExecutionControl.hpp
 * Author:  raymond@burkholder.net
 * Project: lib/TFVuTrading/MarketDepth
 * Created: 2022/11/21 14:59:32
 */

#pragma once

// overall controller for handling interface events and distributing requests to the models
// initiates orders and updates

// TODO: add stop orders

#include <map>

#include <TFTrading/Order.h>
#include <TFTrading/Position.h>

#include "PriceLevelOrder.hpp"

namespace ou {
namespace tf {
namespace l2 {

class PanelTrade;

class ExecutionControl {
public:

  using pOrder_t = ou::tf::Order::pOrder_t;
  using pPosition_t = ou::tf::Position::pPosition_t;

  ExecutionControl( pPosition_t, unsigned int nDefaultOrder );
  ~ExecutionControl();

  void Set( ou::tf::l2::PanelTrade* );

protected:
private:

  pPosition_t m_pPosition;

  ou::tf::l2::PanelTrade* m_pPanelTrade;

  unsigned int m_nDefaultOrder;

  // TODO: allow multiple orders per level
  using mapOrders_t = std::map<double,PriceLevelOrder>;
  // note: the exchange will complain if there are orders on both sides
  mapOrders_t m_mapAskOrders;
  mapOrders_t m_mapBidOrders;

  int m_nActiveOrders;
  double m_dblAveragePrice; // the zero point for the ladder

  PriceLevelOrder m_KillPriceLevelOrder; // temporary for unrolling lambda call

  void AskLimit( double );
  void AskStop( double );
  void AskCancel( double );

  void BidLimit( double );
  void BidStop( double );
  void BidCancel( double );

  void HandleExecution( const ou::tf::Execution& );
  void HandlePositionChanged( const ou::tf::Position& );

};

} // market depth
} // namespace tf
} // namespace ou
