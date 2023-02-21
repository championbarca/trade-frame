/************************************************************************
 * Copyright(c) 2020, One Unified. All rights reserved.                 *
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
 * File:    Tracker.cpp
 * Author:  raymond@burkholder.net
 * Project: TFOptionCombos
 * Created: Novemeber 8, 2020, 11:41 AM
 */

#include "Tracker.h"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace option { // options

namespace {
  bool lt( double a, double b ) { return a < b; }
  bool gt( double a, double b ) { return a > b; }
  bool eq( double a, double b ) { return a == b; }
  bool ge( double a, double b ) { return a >= b; }
  bool le( double a, double b ) { return a <= b; }
}

Tracker::Tracker()
: m_transition( ETransition::Initial ),
  m_pChain( nullptr ),
  m_compare( nullptr ),
  m_luStrike( nullptr ),
  m_dblUnderlyingSlope {}, m_dblUnderlyingPrice {}
{}

Tracker::Tracker( Tracker&& rhs )
: m_compare( std::move( rhs.m_compare ) ),
  m_luStrike( std::move( rhs.m_luStrike ) ),
  m_dblStrikePosition( rhs.m_dblStrikePosition ),
  m_sidePosition( rhs.m_sidePosition ),
  m_dblUnderlyingPrice( rhs.m_dblUnderlyingPrice ),
  m_dblUnderlyingSlope( rhs.m_dblUnderlyingSlope ),
  m_transition( rhs.m_transition ),
  m_pChain( std::move( rhs.m_pChain ) ),
  m_pPosition( std::move( rhs.m_pPosition ) ),
  m_pOptionCandidate( std::move( rhs.m_pOptionCandidate ) ),
  m_fConstructOption( std::move( rhs.m_fConstructOption ) ),
  m_fOpenLeg( std::move( rhs.m_fOpenLeg ) ),
  m_fCloseLeg( std::move( rhs.m_fCloseLeg ) )
{
  assert( !m_pOptionCandidate );  // can't be watching
}

Tracker::~Tracker() {
  Quiesce();
  m_transition = ETransition::Done;
  m_pPosition.reset();
  m_fConstructOption = nullptr;
  m_fOpenLeg = nullptr;
  m_fCloseLeg = nullptr;
  m_compare = nullptr;
  m_luStrike = nullptr;
}

void Tracker::Initialize(
  pPosition_t pPosition,
  const chain_t* pChain,
  fConstructOption_t&& fConstructOption,
  fCloseLeg_t&& fCloseLeg,
  fOpenLeg_t&& fOpenLeg
) {

  assert( pPosition );
  assert( fConstructOption );
  assert( fCloseLeg );
  assert( fOpenLeg );
  assert( ETransition::Initial == m_transition );
  // this is for a premature roll, need better state management for this
  //assert( ( ETransition::Initial == m_transition ) || ( ETransition::Roll == m_transition ) );

  m_pChain = pChain;

  m_fConstructOption = std::move( fConstructOption );
  m_fCloseLeg = std::move( fCloseLeg );
  m_fOpenLeg = std::move( fOpenLeg );

  Initialize( pPosition );

  m_transition = ETransition::Track;
}

void Tracker::Initialize( pPosition_t pPosition ) {

  m_pPosition = std::move( pPosition );

  using pInstrument_t = ou::tf::Instrument::pInstrument_t;

  pInstrument_t pInstrument = m_pPosition->GetInstrument();
  assert( pInstrument->IsOption() || pInstrument->IsFuturesOption() );

  m_dblStrikePosition = pInstrument->GetStrike();
  m_sidePosition = pInstrument->GetOptionSide();

  switch ( m_sidePosition ) {
    case ou::tf::OptionSide::Call:
      m_compare = &gt;
      m_luStrike = [pChain=m_pChain](double dblUnderlying){ return pChain->Call_Itm( dblUnderlying ); };
      break;
    case ou::tf::OptionSide::Put:
      m_compare = &lt;
      m_luStrike = [pChain=m_pChain](double dblUnderlying){ return pChain->Put_Itm( dblUnderlying ); };
      break;
  }
  assert( m_compare );

}

void Tracker::TestLong( boost::posix_time::ptime dt, double dblUnderlyingSlope, double dblUnderlyingPrice ) {

  switch ( m_transition ) {
    case ETransition::Track:
      {
        m_dblUnderlyingPrice = dblUnderlyingPrice;
        m_dblUnderlyingSlope = dblUnderlyingSlope;

        double strikeItm = m_luStrike( dblUnderlyingPrice );

        if ( m_compare( strikeItm, m_dblStrikePosition ) ) { // is new strike further itm?
          // TODO: refactor to remove the if/else and put Vacant/Construct together?
          if ( m_pOptionCandidate ) { // if already tracking the option
            if ( m_compare( strikeItm, m_pOptionCandidate->GetStrike() ) ) { // move further itm?
              m_transition = ETransition::Vacant;
              OptionCandidate_StopWatch();
              m_pOptionCandidate.reset();
              Construct( dt, strikeItm );
            }
            else {
              // TODO: if retreating, stay pat, retreat, or try the roll?
              // nothing to do, track in existing option as quotes are updated
            }
          }
          else {
            // need to obtain option, but track via state machine to request only once
            m_transition = ETransition::Vacant;
            Construct( dt, strikeItm );
          }
        }
        else {
          // nothing to do, hasn't moved enough itm
        }
      }
      break;
  }

}

void Tracker::TestShort( boost::posix_time::ptime dt, double dblUnderlyingSlope, double dblUnderlyingPrice ) {

  switch ( m_transition ) {
    case ETransition::Track:
      {
        m_dblUnderlyingPrice = dblUnderlyingPrice;
        m_dblUnderlyingSlope = dblUnderlyingSlope;

        double diff = m_pPosition->GetUnRealizedPL();

        // close out when value close to zero
        const ou::tf::Quote& quote( m_pPosition->GetWatch()->LastQuote() );
        if ( quote.IsNonZero() && ( quote.Ask() > quote.Bid() ) ) {
          if ( 0.101 >= quote.Ask() ) {
            m_transition = ETransition::Fill;
            m_fCloseLeg( m_pPosition );
            m_transition = ETransition::Initial;
          }
        }
      }
      break;
    default:
      break;
  }
}

void Tracker::Construct( boost::posix_time::ptime dt, double strikeItm ) {
  m_transition = ETransition::Acquire;
  std::string sName;
  switch ( m_sidePosition ) {
    case ou::tf::OptionSide::Call:
      sName = m_pChain->GetIQFeedNameCall( strikeItm );
      break;
    case ou::tf::OptionSide::Put:
      sName = m_pChain->GetIQFeedNamePut( strikeItm );
      break;
  }
  std::cout
    << dt.time_of_day() << " "
    << "Tracker::Construct: "
    << sName
    << std::endl;
  m_fConstructOption(
    sName,
    [this]( pOption_t pOption ){
      m_pOptionCandidate = pOption;
      OptionCandidate_StartWatch();
      m_transition = ETransition::Track;
    } );
}

void Tracker::HandleLongOptionQuote( const ou::tf::Quote& quote ) {
  switch ( m_transition ) {
    case ETransition::Track:
      {
        if ( m_compare( m_dblUnderlyingSlope, 0.0 ) ) {
          // nothing if the slope is going in the right direction
          // positive for long call, negative for long put
        }
        else {
          // test if roll will be profitable for long option
          //double diff = m_pPosition->GetUnRealizedPL() - quote.Ask();  // buy new at the ask
          double diff = m_pPosition->GetUnRealizedPL() /  // calc per share
            ( m_pPosition->GetActiveSize() * m_pPosition->GetInstrument()->GetMultiplier() );
          diff -= ( 2.0 * quote.Spread() );  // unrealized p/l incorporates entry spread, this calculates exit spread
          diff -= 0.10;  // subtract estimated commissions plus some spare change
          if ( 0.10 < diff ) { // desire at least 10 cents on the roll
            if ( ( 0 == quote.BidSize() ) || ( 0.0 == quote.Bid() ) ) {
              // no one will buy our stuff
            }
            else {
              auto pOldWatch = m_pPosition->GetWatch();
              std::cout
                << quote.DateTime().time_of_day()
                << ",old=" << pOldWatch->GetInstrumentName()
                << ",b=" << pOldWatch->LastQuote().Bid()
                << ",a=" << pOldWatch->LastQuote().Ask()
                << ",new=" << m_pOptionCandidate->GetInstrument()->GetInstrumentName()
                << ",b=" << m_pOptionCandidate->LastQuote().Bid()
                << ",a=" << m_pOptionCandidate->LastQuote().Ask()
                << ",roll-per-share-diff=" << diff
                << ",underlying=" << m_dblUnderlyingPrice
                << ",slope=" << m_dblUnderlyingSlope
                << std::endl;
              m_transition = ETransition::Roll;
              OptionCandidate_StopWatch();
              m_compare = nullptr;
              m_luStrike = nullptr;
              pOption_t pOption( std::move( m_pOptionCandidate ) );
              std::string sNotes( m_pPosition->Notes() ); // notes are needed for new position creation
              m_fCloseLeg( m_pPosition );  // TODO: closer needs to use EnableStatsAdd
              // TODO: on opening a position, will need to extend states to handle order with errors
              m_transition = ETransition::Initial;  // this is going to recurse back in here (TODO: think about kill & rebuild?)
              Initialize( m_fOpenLeg( std::move( pOption ), sNotes ) ); // with new position, NOTE: opener needs to use EnableStatsAdd
              //m_transition = ETransition::Track;  // start all over again - this will get set in Initialize [TODO: how or why did this work before?]
            }
          }
        }
      }
      break;
    default:
      break;
  }
}

void Tracker::TestItmRoll( boost::gregorian::date date, boost::posix_time::time_duration time ) {
  switch ( m_transition ) {
    case ETransition::Quiesce:
      {
        if ( m_pPosition ) {
          if ( m_pPosition->IsActive() ) {
            if ( m_pPosition->GetInstrument()->GetExpiry() == date ) {

              m_transition = ETransition::Roll;

              double strike( m_dblStrikePosition );
              ou::tf::OptionSide::EOptionSide sidePosition( m_sidePosition );

              m_compare = nullptr;
              m_luStrike = nullptr;
              std::string sNotes( m_pPosition->Notes() ); // notes are needed for new position creation

              m_fCloseLeg( m_pPosition );

              std::string sName;
              switch ( sidePosition ) {
                case ou::tf::OptionSide::Call:
                  sName = m_pChain->GetIQFeedNameCall( strike );
                  break;
                case ou::tf::OptionSide::Put:
                  sName = m_pChain->GetIQFeedNamePut( strike );
                  break;
                default:
                  break;
              }
              std::cout << "Tracker::TestItmRoll: " << sName << std::endl;

              m_fConstructOption(
                sName,
                [this,sNotes_=std::move(sNotes)]( pOption_t pOption ){
                  Initialize( m_fOpenLeg( std::move( pOption ), sNotes_ ) ); // with new position
                  m_transition = ETransition::Quiesce;
                } );
            }
          }
        }
      }
      break;
    default:
      std::cout
        << "Tracker::TestItmRoll: bad state: " << int( m_transition )
        << std::endl;
      break;
  }
}

void Tracker::OptionCandidate_StartWatch() {
  assert( m_pOptionCandidate );
  m_pOptionCandidate->OnQuote.Add( MakeDelegate( this, &Tracker::HandleLongOptionQuote ) );
  m_pOptionCandidate->StartWatch();
}

void Tracker::OptionCandidate_StopWatch() {
  assert( m_pOptionCandidate );
  m_pOptionCandidate->StopWatch();
  m_pOptionCandidate->OnQuote.Remove( MakeDelegate( this, &Tracker::HandleLongOptionQuote ) );
}

void Tracker::Quiesce() { // called from destructor, Collar
  m_transition = ETransition::Quiesce;
  if ( m_pOptionCandidate ) {
    OptionCandidate_StopWatch();
    m_pOptionCandidate.reset();
  }
}

} // namespace option
} // namespace tf
} // namespace ou
