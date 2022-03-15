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
 * File:    Strategy.cpp
 * Author:  raymond@burkholder.net
 * Project: rdaf/at
 * Created: March 7, 2022 14:35
 */

#include <rdaf/TH3.h>
#include <rdaf/TRint.h>
#include <rdaf/TROOT.h>
#include <rdaf/TFile.h>
#include <rdaf/TTree.h>
#include <rdaf//TMacro.h>
//#include <rdaf/TCanvas.h>
#include <rdaf/TFitResult.h>
#include <rdaf/TDirectory.h>

#include <OUCharting/ChartDataView.h>

#include <TFTrading/Watch.h>

#include "Config.h"
#include "Strategy.h"

using pWatch_t = ou::tf::Watch::pWatch_t;

Strategy::Strategy(
  const std::string& sFilePrefix
, const config::Options& options
, ou::ChartDataView& cdv
)
: ou::tf::DailyTradeTimeFrame<Strategy>()
, m_options( options )
, m_sFilePrefix( sFilePrefix )
, m_cdv( cdv )
, m_ceShortEntry( ou::ChartEntryShape::EShort, ou::Colour::Red )
, m_ceLongEntry( ou::ChartEntryShape::ELong, ou::Colour::Blue )
, m_ceShortFill( ou::ChartEntryShape::EFillShort, ou::Colour::Red )
, m_ceLongFill( ou::ChartEntryShape::EFillLong, ou::Colour::Blue )
, m_ceShortExit( ou::ChartEntryShape::EShortStop, ou::Colour::Red )
, m_ceLongExit( ou::ChartEntryShape::ELongStop, ou::Colour::Blue )
, m_bfQuotes01Sec( 1 )
, m_stateTrade( ETradeState::Init )
{

  m_ceQuoteAsk.SetColour( ou::Colour::Red );
  m_ceQuoteBid.SetColour( ou::Colour::Blue );
  m_ceTrade.SetColour( ou::Colour::DarkGreen );

  m_ceQuoteAsk.SetName( "Ask" );
  m_ceTrade.SetName( "Tick" );
  m_ceQuoteBid.SetName( "Bid" );

  m_ceVolume.SetName( "Volume" );

  m_ceProfitLoss.SetName( "P/L" );

  m_bfQuotes01Sec.SetOnBarComplete( MakeDelegate( this, &Strategy::HandleBarQuotes01Sec ) );

}

Strategy::~Strategy() {
  if ( m_threadRdaf.joinable() ) {
    m_prdafApp->SetReturnFromRun( true );
    m_threadRdaf.join(); // returns after .quit at command line
  }
  Clear();
}

void Strategy::SetupChart() {

  m_cdv.Add( EChartSlot::Price, &m_ceQuoteAsk );
  m_cdv.Add( EChartSlot::Price, &m_ceTrade );
  m_cdv.Add( EChartSlot::Price, &m_ceQuoteBid );

  m_cdv.Add( EChartSlot::Price, &m_ceLongEntry );
  m_cdv.Add( EChartSlot::Price, &m_ceLongFill );
  m_cdv.Add( EChartSlot::Price, &m_ceLongExit );
  m_cdv.Add( EChartSlot::Price, &m_ceShortEntry );
  m_cdv.Add( EChartSlot::Price, &m_ceShortFill );
  m_cdv.Add( EChartSlot::Price, &m_ceShortExit );

  m_cdv.Add( EChartSlot::Volume, &m_ceVolume );

  m_cdv.Add( EChartSlot::PL, &m_ceProfitLoss );

}

void Strategy::SetPosition( pPosition_t pPosition ) {

  assert( pPosition );

  Clear();

  m_pPosition = pPosition;
  pWatch_t pWatch = m_pPosition->GetWatch();

  m_cdv.SetNames( "rdaf TMacro test", pWatch->GetInstrument()->GetInstrumentName() );

  SetupChart();

  StartRdaf( m_sFilePrefix );

  //time_duration td = time_duration( 0, 0, m_nPeriodWidth );

  pWatch->OnQuote.Add( MakeDelegate( this, &Strategy::HandleQuote ) );
  pWatch->OnTrade.Add( MakeDelegate( this, &Strategy::HandleTrade ) );

}

void Strategy::Clear() {
  if  ( m_pPosition ) {
    pWatch_t pWatch = m_pPosition->GetWatch();
    pWatch->OnQuote.Remove( MakeDelegate( this, &Strategy::HandleQuote ) );
    pWatch->OnTrade.Remove( MakeDelegate( this, &Strategy::HandleTrade ) );
    m_cdv.Clear();
    //m_pPosition.reset(); // need to fix relative to thread
  }
}

void Strategy::ThreadRdaf( Strategy* self, const std::string& sFilePrefix ) {

  //const config::Options& options( self->m_options );

  double dblDateTimeUpper;
  double dblDateTimeLower;

  std::time_t nTime;
  nTime = boost::posix_time::to_time_t( self->m_options.dtTimeUpper );
  dblDateTimeUpper = (double) nTime / 1000.0;
  nTime = boost::posix_time::to_time_t( self->m_options.dtTimeLower );
  dblDateTimeLower = (double) nTime / 1000.0;

  //std::cout << "date range: " << dblDateTimeLower << " ... " << dblDateTimeUpper << std::endl;

  using pWatch_t = ou::tf::Watch::pWatch_t;
  pWatch_t pWatch = self->m_pPosition->GetWatch();

  self->m_pFile = std::make_unique<TFile>(
    ( sFilePrefix + ".root" ).c_str(), "RECREATE", "tradeframe rdaf/at based data, quotes & trades"
  );

  self->m_pTreeQuote = std::make_shared<TTree>(
    "quotes", ( pWatch->GetInstrumentName() + " quotes" ).c_str()
  );
  self->m_pTreeQuote->Branch( "quote", &self->m_branchQuote, "time/D:ask/D:askvol/l:bid/D:bidvol/l" );
  if ( !self->m_pTreeQuote ) {
    std::cout << "problems m_pTreeQuote" << std::endl;
  }

  self->m_pTreeTrade = std::make_shared<TTree>(
    "trades", ( pWatch->GetInstrumentName() + " trades" ).c_str()
  );
  self->m_pTreeTrade->Branch( "trade", &self->m_branchTrade, "time/D:price/D:vol/l:direction/L" );
  if ( !self->m_pTreeTrade ) {
    std::cout << "problems m_pTreeTrade" << std::endl;
  }

  self->m_pHistVolume = std::make_shared<TH3D>(
    "h1", ( self->m_options.sSymbol + "Volume" ).c_str(),
    self->m_options.nTimeBins, dblDateTimeLower, dblDateTimeUpper,
    self->m_options.nPriceBins, self->m_options.dblPriceLower, self->m_options.dblPriceUpper,
    self->m_options.nVolumeBins, self->m_options.dblVolumeLower, self->m_options.dblVolumeUpper
  );
  if ( !self->m_pHistVolume ) {
    std::cout << "problems history" << std::endl;
  }

  // example charting code in live analysis mode
  //TCanvas* c = new TCanvas("c", "Something", 0, 0, 800, 600);
  //TF1 *f1 = new TF1("f1","sin(x)", -5, 5);
  //f1->SetLineColor(kBlue+1);
  //f1->SetTitle("My graph;x; sin(x)");
  //f1->Draw();
  //c->Modified(); c->Update();

  // un-comment if you want the prompt for live analysis
  //self->m_prdafApp->Run();
}

void Strategy::StartRdaf( const std::string& sFileName ) {

  int argc {};
  char** argv = nullptr;

  m_prdafApp = std::make_unique<TRint>( "rdaf_at", &argc, argv );
  ROOT::EnableThreadSafety();
  ROOT::EnableImplicitMT();

  m_threadRdaf = std::move( std::thread( ThreadRdaf, this, sFileName ) );
}

void Strategy::HandleQuote( const ou::tf::Quote& quote ) {
  // position has the quotes via the embedded watch
  // indicators are also attached to the embedded watch

  if ( !quote.IsValid() ) {
    return;
  }

  ptime dt( quote.DateTime() );

  m_ceQuoteAsk.Append( dt, quote.Ask() );
  m_ceQuoteBid.Append( dt, quote.Bid() );

  m_quote = quote;

  if ( m_pTreeQuote ) { // wait for initialization in thread to start
    std::time_t nTime = boost::posix_time::to_time_t( quote.DateTime() );

    m_branchQuote.time = (double)nTime / 1000.0;
    m_branchQuote.ask = quote.Ask();
    m_branchQuote.askvol = quote.AskSize();
    m_branchQuote.bid = quote.Bid();
    m_branchQuote.bidvol = quote.BidSize();

    m_pTreeQuote->Fill();
}

  m_bfQuotes01Sec.Add( dt, m_quote.Midpoint(), 1 ); // provides a 1 sec pulse for checking the alogorithm

}

void Strategy::HandleTrade( const ou::tf::Trade& trade ) {

  ptime dt( trade.DateTime() );

  m_ceTrade.Append( dt, trade.Price() );
  m_ceVolume.Append( dt, trade.Volume() );

  const double mid = m_quote.Midpoint();
  const double price = trade.Price();
  const uint64_t volume = trade.Volume();

  if ( m_pTreeTrade ) { // wait for initialization in thread to start
    std::time_t nTime = boost::posix_time::to_time_t( trade.DateTime() );
    m_branchTrade.time = (double)nTime / 1000.0;
    m_branchTrade.price = price;
    m_branchTrade.vol = volume;
    if ( mid == price ) {
      m_branchTrade.direction = 0;
    }
    else {
      m_branchTrade.direction = ( mid < price ) ? volume : -volume;
    }

    m_pTreeTrade->Fill();
  }

  if ( m_pHistVolume ) {
    m_pHistVolume->Fill( m_branchTrade.time, trade.Price(), trade.Volume() );
  }

}

void Strategy::HandleBarQuotes01Sec( const ou::tf::Bar& bar ) {

  double dblUnRealized, dblRealized, dblCommissionsPaid, dblTotal;

  m_pPosition->QueryStats( dblUnRealized, dblRealized, dblCommissionsPaid, dblTotal );
  m_ceProfitLoss.Append( bar.DateTime(), dblTotal );

  TimeTick( bar );
}

void Strategy::EnterLong( const ou::tf::Bar& bar ) {
  m_pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, 100 );
  m_pOrder->OnOrderCancelled.Add( MakeDelegate( this, &Strategy::HandleOrderCancelled ) );
  m_pOrder->OnOrderFilled.Add( MakeDelegate( this, &Strategy::HandleOrderFilled ) );
  m_ceLongEntry.AddLabel( bar.DateTime(), bar.Close(), "Long Submit" );
  m_stateTrade = ETradeState::LongSubmitted;
  m_pPosition->PlaceOrder( m_pOrder );
}

void Strategy::EnterShort( const ou::tf::Bar& bar ) {
  m_pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, 100 );
  m_pOrder->OnOrderCancelled.Add( MakeDelegate( this, &Strategy::HandleOrderCancelled ) );
  m_pOrder->OnOrderFilled.Add( MakeDelegate( this, &Strategy::HandleOrderFilled ) );
  m_ceShortEntry.AddLabel( bar.DateTime(), bar.Close(), "Short Submit" );
  m_stateTrade = ETradeState::ShortSubmitted;
  m_pPosition->PlaceOrder( m_pOrder );
}

void Strategy::HandleRHTrading( const ou::tf::Bar& bar ) { // once a second

  // DailyTradeTimeFrame: Trading during regular active equity market hours
  // https://learnpriceaction.com/3-moving-average-crossover-strategy/
  // TODO: include the marketRule price difference here?

  switch ( m_stateTrade ) {
    case ETradeState::Search:
      if ( m_pHistVolume ) {
        //input of this function is the time, which is the current time
        //therefore we need to find a suitable range of bins along Y-axis (remember time is stored in y-axis)
        // and find the projection of h2. the result is a 1-D hist with x-axis being price
        // and value being volume

        std::time_t nTime = boost::posix_time::to_time_t( bar.DateTime() );
        nTime = (double)nTime / 1000.0;

        //find the bin that the time given belongs to:
        Int_t bin_y = m_pHistVolume->GetYaxis()->FindBin( nTime );

        //if bin_y is valid and larger than 1 then proceed, else abort (since there is not enought data)
        if ( bin_y < 1 ) {
        }
        else {
          //now find projection of h2 from the beginning till now:
          auto h2_x = m_pHistVolume->ProjectionX( "_x", 1, bin_y );

          // now that h2_x is calculated, fit a gaussian to the it (i.e volume distribution)
          auto b = h2_x->Fit( "gaus", "S" );

          //if fit is valid proceed, else abort
//          if ( !b->IsValid() ) {
//          }
//          else {
            //finally, if price is within 1 sigma from the mean, it's a good signal and go long:
//            if ( abs( bar.Close() - b->Parameter(1)) < b->Parameter(2) ) {
//              EnterLong( bar );
//            }
//          }
        }
      }
      break;
    case ETradeState::LongSubmitted:
      // wait for order to execute
      break;
    case ETradeState::LongExit:
/*      if ( bar.Close() < ma2 ) {
        // exit long
        m_pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, 100 );
        m_pOrder->OnOrderCancelled.Add( MakeDelegate( this, &Strategy::HandleOrderCancelled ) );
        m_pOrder->OnOrderFilled.Add( MakeDelegate( this, &Strategy::HandleOrderFilled ) );
        m_ceLongExit.AddLabel( bar.DateTime(), m_dblMid, "Long Exit" );
        m_stateTrade = ETradeState::ExitSubmitted;
        m_pPosition->PlaceOrder( m_pOrder );
      }*/
      break;
    case ETradeState::ShortSubmitted:
      // wait for order to execute
      break;
    case ETradeState::ShortExit:
/*      if ( bar.Close() > ma2 ) {
        // exit short
        m_pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, 100 );
        m_pOrder->OnOrderCancelled.Add( MakeDelegate( this, &Strategy::HandleOrderCancelled ) );
        m_pOrder->OnOrderFilled.Add( MakeDelegate( this, &Strategy::HandleOrderFilled ) );
        m_ceShortExit.AddLabel( bar.DateTime(), m_dblMid, "Short Exit" );
        m_stateTrade = ETradeState::ExitSubmitted;
        m_pPosition->PlaceOrder( m_pOrder );
      } */
      break;
    case ETradeState::ExitSubmitted:
      // wait for order to execute
      break;
    case ETradeState::Done:
      // quiescent
      break;
    case ETradeState::Init:
      // market open statistics management here
      // will need to wait for ma to load & diverge (based upon width & period)
      m_stateTrade = ETradeState::Search;
      break;
  }
}

void Strategy::HandleOrderCancelled( const ou::tf::Order& ) {
  m_pOrder->OnOrderCancelled.Remove( MakeDelegate( this, &Strategy::HandleOrderCancelled ) );
  m_pOrder->OnOrderFilled.Remove( MakeDelegate( this, &Strategy::HandleOrderFilled ) );
  switch ( m_stateTrade ) {
    case ETradeState::ExitSubmitted:
      assert( false );  // TODO: need to figure out a plan to retry exit
      break;
    default:
      m_stateTrade = ETradeState::Search;
  }
  m_pOrder.reset();
}

void Strategy::HandleOrderFilled( const ou::tf::Order& order ) {
  m_pOrder->OnOrderCancelled.Remove( MakeDelegate( this, &Strategy::HandleOrderCancelled ) );
  m_pOrder->OnOrderFilled.Remove( MakeDelegate( this, &Strategy::HandleOrderFilled ) );
  switch ( m_stateTrade ) {
    case ETradeState::LongSubmitted:
      m_ceLongFill.AddLabel( order.GetDateTimeOrderFilled(), m_quote.Midpoint(), "Long Fill" );
      m_stateTrade = ETradeState::LongExit;
      break;
    case ETradeState::ShortSubmitted:
      m_ceShortFill.AddLabel( order.GetDateTimeOrderFilled(), m_quote.Midpoint(), "Short Fill" );
      m_stateTrade = ETradeState::ShortExit;
      break;
    case ETradeState::ExitSubmitted:
      m_stateTrade = ETradeState::Search;
      break;
    case ETradeState::Done:
      break;
    default:
      assert( false ); // TODO: unravel the state mess if we get here
  }
  m_pOrder.reset();
}

void Strategy::HandleCancel( boost::gregorian::date, boost::posix_time::time_duration ) { // one shot
  m_pPosition->CancelOrders();
}

void Strategy::HandleGoNeutral( boost::gregorian::date, boost::posix_time::time_duration ) { // one shot
  m_pPosition->ClosePosition();
}

void Strategy::SaveWatch( const std::string& sPrefix ) {
  m_pPosition->GetWatch()->SaveSeries( sPrefix );
}

void Strategy::CloseAndDone() {
  std::cout << "Sending Close & Done" << std::endl;
  m_pPosition->ClosePosition();
  m_stateTrade = ETradeState::Done;
}