/************************************************************************
 * Copyright(c) 2021, One Unified. All rights reserved.                 *
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
 * File:    PriceRow.h
 * Author:  raymond@burkholder.net
 * Project: TFVuTrading/MarketDepth
 * Created: November 11, 2021 09:08
 */

#pragma once

#include "WinRow.hpp"

#include "PriceRowElement.hpp"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace l2 { // market depth

class PriceRow {
public:

  enum class EField: int {
    PL,
    BuyCount, BuyVolume,
    BidSize,
    BidOrder, Price, AskOrder,
    AskSize,
    SellVolume, SellCount,
    Ticks, Volume, Static, Dynamic
    };


  explicit PriceRow( double price );
  PriceRow( const PriceRow& );
  ~PriceRow();

  void SetRowElements( WinRow& );
  void DelRowElements();

  void Refresh();

  // doesn't work - cna't do partial specialization, try fusion?
  //template<typename T>
  //void Set(WinRow::EField field, T t ) {}
  //template <> void Set<double>( WinRow::EField field, double value ) {
  //  m_drePrice.Set( value );
  //}

  void SetPrice( double price, bool bHighLight ) {
    m_drePrice.Set( price, bHighLight );
  }

  void IncTicks() { m_dreTicks.Inc(); }

  void AddVolume( unsigned int nVolume ) { m_dreVolume.Add( nVolume ); }

  void SetAskVolume( unsigned int nVolume ) { m_dreAskSize.Set( nVolume ); }
  void SetBidVolume( unsigned int nVolume ) { m_dreBidSize.Set( nVolume ); }

  void IncBuyCount() { m_dreBuyCount.Inc(); }
  void IncSellCount() { m_dreSellCount.Inc(); }

  void AddToBuyVolume( unsigned int n ) { m_dreBuyVolume.Add( n ); }
  void AddToSellVolume( unsigned int n ) { m_dreSellVolume.Add( n ); }

  void AppendIndicatorStatic( const std::string& sIndicator ) {
    m_dreIndicatorStatic.Append( sIndicator );
  }

  void AddIndicatorDynamic( const std::string& sIndicator ) {
    m_dreIndicatorDynamic.Add( sIndicator );
  }

  void DelIndicatorDynamic( const std::string& sIndicator ) {
    m_dreIndicatorDynamic.Del( sIndicator );
  }

protected:
private:

  //double m_price;

  bool m_bChanged;

  // TODO: boost::fusion?  std::tuple?
  //DataRowElement<double>         m_dreAcctPl;
  PriceRowElement<unsigned int>   m_dreBuyCount;
  PriceRowElement<unsigned int>   m_dreBuyVolume;
  PriceRowElement<unsigned int>   m_dreBidSize;
  PriceRowElement<double>         m_drePrice;
  PriceRowElement<unsigned int>   m_dreAskSize;
  PriceRowElement<unsigned int>   m_dreSellVolume;
  PriceRowElement<unsigned int>   m_dreSellCount;
  PriceRowElement<unsigned int>   m_dreTicks;
  PriceRowElement<unsigned int>   m_dreVolume;
  PriceRowElementIndicatorStatic  m_dreIndicatorStatic;
  PriceRowElementIndicatorDynamic m_dreIndicatorDynamic;

  //RowElements* m_pRowElements;  // shared_ptr ?

};

} // market depth
} // namespace tf
} // namespace ou