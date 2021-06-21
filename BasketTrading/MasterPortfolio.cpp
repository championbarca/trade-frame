/************************************************************************
 * Copyright(c) 2012, One Unified. All rights reserved.                 *
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

// _148423314X 2017 Data Mining Algorithms in C++: Data Patterns and Algorithms for Modern Applications
//    may have some interesting thoughts on data mining inter-day and intra-day data for relationships

#include <set>
#include <algorithm>

#include <OUCommon/TimeSource.h>

#include <TFIQFeed/BuildInstrument.h>

#include <TFTrading/InstrumentManager.h>

#include "MoneyManager.h"
#include "MasterPortfolio.h"

namespace {
  const std::string sUnderlyingPortfolioPrefix( "portfolio-" );
}

MasterPortfolio::MasterPortfolio(
    pPortfolio_t pMasterPortfolio,
    pProvider_t pExec, pProvider_t pData1, pProvider_t pData2,
    fGatherOptionDefinitions_t&& fGatherOptionDefinitions,
    fGetTableRowDef_t&& fGetTableRowDef,
    fChartRoot_t&& fChartRoot, fChartAdd_t&& fChartAdd, fChartDel_t&& fChartDel
    )
  : m_bStarted( false ),
    m_nSharesTrading( 0 ),
    m_fOptionNamesByUnderlying( std::move( fGatherOptionDefinitions ) ),
    m_fGetTableRowDef( std::move( fGetTableRowDef ) ),
    m_fChartRoot( std::move( fChartRoot ) ),
    m_fChartAdd( std::move( fChartAdd ) ),
    m_fChartDel( std::move( fChartDel ) ),
    m_pMasterPortfolio( pMasterPortfolio ),
    m_pExec( pExec ),
    m_pData1( pData1 ),
    m_pData2( pData2 )
    //m_eAllocate( EAllocate::Waiting )
{
  assert( m_fOptionNamesByUnderlying );
  assert( m_fGetTableRowDef );
  assert( m_fChartRoot );
  assert( m_fChartAdd );
  assert( m_fChartDel );

  assert( pMasterPortfolio );
  assert( pExec );
  assert( pData1 );

  switch ( pExec->ID() ) {
    case ou::tf::keytypes::EProviderIB:
      m_pIB = boost::dynamic_pointer_cast<ou::tf::IBTWS>( pExec );  // TODO: convert to boost:: to std::
      break;
    default:
      assert( false ); // need the IB provider, or at least some provider
  }

  switch ( pData1->ID() ) {
    case ou::tf::keytypes::EProviderIQF:
      m_pIQ = boost::dynamic_pointer_cast<ou::tf::IQFeedProvider>( pData1 );  // TODO: convert to boost:: to std::
      break;
    default:
      assert( false ); // need the iqfeed provider
  }

  m_cePLCurrent.SetColour( ou::Colour::Fuchsia );
  m_cePLUnRealized.SetColour( ou::Colour::DarkCyan );
  m_cePLRealized.SetColour( ou::Colour::MediumSlateBlue );
  m_ceCommissionPaid.SetColour( ou::Colour::SteelBlue );

  m_cePLCurrent.SetName( "P/L Current" );
  m_cePLUnRealized.SetName( "P/L UnRealized" );
  m_cePLRealized.SetName( "P/L Realized" );
  m_ceCommissionPaid.SetName( "Commissions Paid" );

  m_pChartDataView.reset( new ou::ChartDataView );
  m_pChartDataView->Add( 0, &m_cePLCurrent );
  m_pChartDataView->Add( 0, &m_cePLUnRealized );
  m_pChartDataView->Add( 0, &m_cePLRealized );
  m_pChartDataView->Add( 2, &m_ceCommissionPaid );
  m_pChartDataView->SetNames( "Portfolio Profit / Loss", "Master P/L" );
  m_idTreeRoot = m_fChartRoot( "Master P/L", m_pChartDataView );

  m_idTreeUnderlying = m_fChartAdd( m_idTreeRoot, "Underlying", nullptr );
  m_idTreeStrategies = m_fChartAdd( m_idTreeRoot, "Strategies", nullptr );

  std::stringstream ss;
  //ss.str( "" );
  ss << ou::TimeSource::Instance().External();
  // will need to make this generic if need some for multiple providers.
  m_sTSDataStreamStarted = ss.str();  // will need to make this generic if need some for multiple providers.

  m_pOptionEngine = std::make_unique<ou::tf::option::Engine>( m_libor );
  m_pOptionEngine->m_fBuildWatch
    = [this](pInstrument_t pInstrument)->pWatch_t {
      // fix: need to look up and retrieve the pre-constructed watch
        //ou::tf::Watch::pWatch_t pWatch( new ou::tf::Watch( pInstrument, m_pData1 ) );
        pWatch_t pWatch;  // will cause a fault, should not need to be used -- hmm?  check this!
        return pWatch;
      };
  m_pOptionEngine->m_fBuildOption
    = [this](pInstrument_t pInstrument)->pOption_t {
      // fix: need to lookup and retrieve the pre-constructed option
        //ou::tf::option::Option::pOption_t pOption( new ou::tf::option::Option( pInstrument, m_pData1 ) );
        pOption_t pOption; // will cause a fault, should not need to be used -- hmm?  check this!
        return pOption;
      };

  m_libor.SetWatchOn( m_pIQ );

}

MasterPortfolio::~MasterPortfolio() {
  if ( m_worker.joinable() ) {
    m_worker.join();
  }
  //TODO: need to wait for m_pOptionEngine to finish
  //m_mapVolatility.clear();
  m_mapStrategyArtifacts.clear();
  m_mapUnderlyingWithStrategies.clear();
  m_pOptionEngine.release();
  m_libor.SetWatchOff();
}

// auto loading portfolio from database into the mapArtifacts cache
void MasterPortfolio::Add( pPortfolio_t pPortfolio ) {

  // TODO: will need to test that strategy portfolios are active??

  // will have a mixture of 'standard' and 'multilegged'
  mapStrategyArtifacts_t::iterator iterArtifacts = m_mapStrategyArtifacts.find( pPortfolio->Id() );
  if ( m_mapStrategyArtifacts.end() != iterArtifacts ) {
    std::cout << "MasterPortfolio::Add already added portfolio " << pPortfolio->Id() << std::endl;
  }
  else {

    std::cout
      << "Add Portfolio: "
      << "T=" << pPortfolio->GetRow().ePortfolioType
      << ",O=" << pPortfolio->GetRow().idOwner
      << ",ID=" << pPortfolio->Id()
      << std::endl;

    std::pair<mapStrategyArtifacts_t::iterator,bool> pair
      = m_mapStrategyArtifacts.insert( mapStrategyArtifacts_t::value_type( pPortfolio->Id(), std::move( StrategyArtifacts( pPortfolio ) ) ) );
    assert( pair.second );
    //m_curStrategyArtifacts = pair.first;

    switch ( pPortfolio->GetRow().ePortfolioType ) {
      case ou::tf::Portfolio::EPortfolioType::Basket:
        // no need to do anything with the basket
        break;
      case ou::tf::Portfolio::EPortfolioType::Standard:
        // this is the strategy level portfolio
        break;
      case ou::tf::Portfolio::EPortfolioType::Aggregate:
      case ou::tf::Portfolio::EPortfolioType::MultiLeggedPosition:
        // this is the combo level portfolio of positions, needs to be associated with owner
        //    which allows it to be submitted to ManageStrategy
        {
          mapStrategyArtifacts_t::iterator iter = m_mapStrategyArtifacts.find( pPortfolio->GetRow().idOwner );
          assert( m_mapStrategyArtifacts.end() != iter );
          StrategyArtifacts& saOwner( iter->second );
          std::pair<mapPortfolio_iter,bool> pair2 // insert child
            = saOwner.m_mapPortfolio.insert( mapPortfolio_t::value_type( pPortfolio->Id(), pPortfolio ) );
          assert( pair2.second );
        }
        break;
    } // switch
  }
}

// auto loading position from database into the mapArtifacts cache (and from runtime creations?)
void MasterPortfolio::Add( pPosition_t pPosition ) {

  std::cout
    << "Add Position: "
    << pPosition->GetRow().sName
    << ",quan=" << pPosition->GetActiveSize()
    << ",id=" << pPosition->GetRow().idPosition << ","
    << pPosition->Notes()
    << std::endl;

  mapStrategyArtifacts_iter iterStrategyArtifacts = m_mapStrategyArtifacts.find( pPosition->IdPortfolio() );  // need to preload the iterator for random adds
  if ( m_mapStrategyArtifacts.end() == iterStrategyArtifacts ) {
    assert( false );
  }

  StrategyArtifacts& artifact( iterStrategyArtifacts->second );
  if ( pPosition->GetRow().idPortfolio != artifact.m_pPortfolio->Id() ) {
    std::string idInstrument( pPosition->GetInstrument()->GetInstrumentName() );
    std::string idPortfolio1( pPosition->IdPortfolio() );
    std::string idPortfolio2( artifact.m_pPortfolio->Id() );
    assert( false );
  }
  std::pair<mapPosition_t::iterator,bool> pair
    = artifact.m_mapPosition.insert( mapPosition_t::value_type( pPosition->GetRow().sName, pPosition ) );
  assert( pair.second );
}

void MasterPortfolio::UpdateChart( double dblPLCurrent, double dblPLUnRealized, double dblPLRealized, double dblCommissionPaid ) {
  // TODO: use local instance of master portfolio
  boost::posix_time::ptime dt( ou::TimeSource::Instance().External() );
  m_cePLCurrent.Append( dt, dblPLCurrent );
  m_cePLUnRealized.Append( dt, dblPLUnRealized );
  m_cePLRealized.Append( dt, dblPLRealized );
  m_ceCommissionPaid.Append( dt, dblCommissionPaid );
}

void MasterPortfolio::Test() {
  std::for_each(
    m_mapUnderlyingWithStrategies.begin(), m_mapUnderlyingWithStrategies.end(),
    [](mapUnderlyingWithStrategies_t::value_type& vt){
      //vt.second.pManageStrategy->Test();
      }
    );
}

// look for potential underlying symbols to support strategy implementation
void MasterPortfolio::Load( ptime dtLatestEod, bool bAddToList ) {

  if ( !m_mapUnderlyingWithStrategies.empty() ) {
    std::cout << "MasterPortfolio: already loaded." << std::endl;
  }
  else {
    if ( m_worker.joinable() ) m_worker.join(); // finish existing processing
    m_worker = std::thread(
      [this,dtLatestEod,bAddToList](){

        std::for_each( // ensure overnight positions are represented in the new day
          m_mapStrategyArtifacts.begin(), m_mapStrategyArtifacts.end(),
          [this](mapStrategyArtifacts_t::value_type& vt){
            StrategyArtifacts& artifact( vt.second );
            if ( !artifact.m_bAccessed ) {
              std::string idPortfolio( vt.first );
              std::string sTemp( idPortfolio.substr( 0, sUnderlyingPortfolioPrefix.size() ) ); // some are strategy-, some are 'strangle-'
              if ( sTemp == sUnderlyingPortfolioPrefix ) {
                std::string sUnderlying( idPortfolio.substr( sUnderlyingPortfolioPrefix.size() ) );
                std::cout << vt.first << " (" << sUnderlying << ") being examined :";
                bool bPositionActive( false );
                std::for_each(
                  artifact.m_mapPosition.begin(), artifact.m_mapPosition.end(),
                  [&bPositionActive](mapPosition_t::value_type& vt){
                    bPositionActive |= ( 0 != vt.second->GetRow().nPositionActive );
                  }
                );
                if ( bPositionActive ) {
                  std::cout << vt.first << " has position";
                  m_setSymbols.insert( sUnderlying );
                }
                // TODO: is a test required for active sub-portfolios?  ie, strategy portfolios?  then will need to recurse the test?
                //    no, should run this test on the baskets as well, then bring in underlying
                //    at this point, this test will come up empty, as no positions in the underlying
                //      just sub-portfolios for the strategies
                std::cout << std::endl;
              }
            }
          }
        );

        std::set<std::string> vDesired = { "SPY", "SLV", "GLD" }; // USB, XLP, XBI

        SymbolSelection selector(
          dtLatestEod, m_setSymbols,
          [this,bAddToList,&vDesired](const IIPivot& iip) {
            if ( bAddToList ) {
              if ( vDesired.end() != vDesired.find( iip.sName ) ) {
                std::cout << "desired: " << iip.sName << std::endl;
//              if (
//                   ( "NEM" != iip.sName ) // NEM has a non-standard strike price: 35.12, etc
//              )
//              {
                // see if we get wider swings with this
//                double dblSum = iip.dblProbabilityAboveAndUp + iip.dblProbabilityBelowAndDown;
//                if ( 1.24 < dblSum ) {
                  AddUnderlyingSymbol( iip );
//                }
              }
            }
            else { // simply emit statisitcs
              std::cout
                << iip.sName
                << ": " << iip.dblPV
                << "," << iip.dblProbabilityAboveAndUp
                << "," << iip.dblProbabilityAboveAndDown
                << "," << iip.dblProbabilityBelowAndUp
                << "," << iip.dblProbabilityBelowAndDown
                << std::endl;
            }
          } );

        std::cout << "Symbol Load finished, " << m_mapUnderlyingWithStrategies.size() << " symbols chosen" << std::endl;
    } );

  }
}

void MasterPortfolio::AddUnderlyingSymbol( const IIPivot& iip ) {

  const std::string sUnderlying( iip.sName );

  if ( m_mapUnderlyingWithStrategies.end() != m_mapUnderlyingWithStrategies.find( sUnderlying ) ) {
    std::cout << "NOTE: underlying " << sUnderlying << " already added" << std::endl;
  }
  else {

    auto result
      = m_mapUnderlyingWithStrategies.emplace( std::make_pair( sUnderlying, UnderlyingWithStrategies( std::move( iip ) ) ) );
    assert( result.second );

    ConstructWatchUnderlying(
      sUnderlying,
      [this,iter=result.first]( pWatch_t pWatchUnderlying ){

        const std::string& sUnderlying( pWatchUnderlying->GetInstrument()->GetInstrumentName() );
        assert( sUnderlying == iter->first );
        //assert( m_mapUnderlyingWithStrategies.end() != m_mapUnderlyingWithStrategies.find( sUnderlying ) );

        const ou::tf::Portfolio::idPortfolio_t idPortfolioUnderlying( sUnderlyingPortfolioPrefix + sUnderlying );

        pPortfolio_t pPortfolioUnderlying;
        mapStrategyArtifacts_iter iterStrategyArtifacts = m_mapStrategyArtifacts.find( idPortfolioUnderlying );
        if ( m_mapStrategyArtifacts.end() != iterStrategyArtifacts ) { // use existing portfolio
          StrategyArtifacts& sa( iterStrategyArtifacts->second );
          pPortfolioUnderlying = sa.m_pPortfolio;
        }
        else { // create new portfolio
          ou::tf::Portfolio::idAccountOwner_t idAccountOwner( "aggregate" );
          pPortfolioUnderlying
            = ou::tf::PortfolioManager::Instance().ConstructPortfolio(
                idPortfolioUnderlying, idAccountOwner, m_pMasterPortfolio->Id(), ou::tf::Portfolio::EPortfolioType::Aggregate, ou::tf::Currency::Name[ ou::tf::Currency::USD ], "Underlying Aggregate"
            );
        }

        pChartDataView_t pChartDataView = std::make_shared<ou::ChartDataView>();

        UnderlyingWithStrategies& uws( iter->second );
        const IIPivot& iip( uws.iip );
        uws.pUnderlying = std::make_unique<Underlying>( pWatchUnderlying, pPortfolioUnderlying );
        uws.pUnderlying->SetPivots( iip.dblR2, iip.dblR1, iip.dblPV, iip.dblS1, iip.dblS2 );
        uws.pUnderlying->SetChartDataView( pChartDataView );

        m_fChartAdd( m_idTreeUnderlying, sUnderlying, pChartDataView );

        //m_mapVolatility.insert( mapVolatility_t::value_type( iip_.dblDailyHistoricalVolatility, sUnderlying ) );

        StartStrategies( sUnderlying, pPortfolioUnderlying );
      }
      );

  }
}

void MasterPortfolio::ConstructWatchUnderlying( const std::string& sIQFeedEquityName, fConstructedWatch_t&& fWatch ) {

  const trd_t& trd( m_fGetTableRowDef( sIQFeedEquityName ) ); // TODO: check for errors

  bool bNeedContract( false );
  pInstrument_t pInstrumentUnderlying;
  ou::tf::InstrumentManager& im( ou::tf::InstrumentManager::GlobalInstance().Instance() );

  if ( im.Exists( sIQFeedEquityName, pInstrumentUnderlying ) ) {
    // pEquityInstrument has been populated
    assert( 0 != pInstrumentUnderlying->GetContract() );
  }
  else {
    pInstrumentUnderlying = ou::tf::iqfeed::BuildInstrument( sIQFeedEquityName, trd );  // builds equity option, may not build futures option
    bNeedContract = true;
  }

  pWatch_t pWatch = std::make_shared<ou::tf::Watch>( pInstrumentUnderlying, m_pData1 );

  // maybe BasketTrading.cpp needs to do the construction, to keep the id's proper?
  if ( !bNeedContract ) {
    fWatch( pWatch );
  }
  else {
    if ( !m_pIB ) {
      throw std::runtime_error( "MasterPortfolio::ConstructWatchUnderlying: IB provider unavailable for contract");
    }
    else {
        m_pIB->RequestContractDetails(
          sIQFeedEquityName, pInstrumentUnderlying,  // TOOD: for futures, may need different name for IB
          [this,pWatch,fWatch](const ou::tf::IBTWS::ContractDetails& details, pInstrument_t& pInstrument){
            // the contract details fill in the contract in the instrument, which can then be passed back to the caller
            //   as a fully defined, registered instrument
            assert( 0 != pInstrument->GetContract() );
            ou::tf::InstrumentManager& im( ou::tf::InstrumentManager::GlobalInstance().Instance() );
            im.Register( pInstrument );  // is a CallAfter required, or can this run in a thread?
            // TODO: test that the instument is the same as in the watch?
            fWatch( pWatch );
          },
          nullptr  // request complete doesn't need a function
          );
    }
  }
}

MasterPortfolio::pManageStrategy_t MasterPortfolio::ConstructStrategy( const std::string& sUnderlying, pPortfolio_t pPortfolioUnderlying ) {

  iterUnderlyingWithStrategies_t iterUnderlyingWithStrategies
    = m_mapUnderlyingWithStrategies.find( sUnderlying );
  UnderlyingWithStrategies& uws( iterUnderlyingWithStrategies->second );
  assert( !uws.pManageStrategyInWaiting );  // need empty location
  const IIPivot& iip_( uws.iip );

  const idPortfolio_t& idPortfolioUnderlying( pPortfolioUnderlying->Id() );

  pChartDataView_t pChartDataView = std::make_shared<ou::ChartDataView>();

  namespace ph = std::placeholders;

  uws.pManageStrategyInWaiting = std::make_shared<ManageStrategy>(
        //iip.sPath, // TODO: supply to Underlying instead
        //iip_.bar,
        1.0, // TODO: defaults to rising for now, use BollingerTransitions::ReadDailyBars for directional selection
        uws.pUnderlying->GetWatch(),
        pPortfolioUnderlying,
        pChartDataView,
    // ManageStrategy::fGatherOptionDefinitions_t
        m_fOptionNamesByUnderlying,  // TODO, need to pass in mapChains from Underlying
    // ManageStrategy::fConstructOption_t
        [this,idPortfolioUnderlying](const std::string& sIQFeedOptionName, const pInstrument_t pUnderlyingInstrument, ManageStrategy::fConstructedOption_t fOption){

              bool bUseExistingOption( true );
              mapPosition_iter iterPosition;
              mapStrategyArtifacts_iter iterArtifact = m_mapStrategyArtifacts.find( idPortfolioUnderlying );
              if ( m_mapStrategyArtifacts.end() == iterArtifact ) {
                bUseExistingOption = false;
              }
              else {
                StrategyArtifacts& artifact( iterArtifact->second );
                iterPosition = artifact.m_mapPosition.find( sIQFeedOptionName );
                if ( artifact.m_mapPosition.end() == iterPosition ) {
                  bUseExistingOption = false;
                }
              }

              if ( bUseExistingOption ) {
                // option should exist given code at line 118 Position.cpp ConstructWatch
                pOption_t pOption = std::dynamic_pointer_cast<ou::tf::option::Option>( iterPosition->second->GetWatch() );
                fOption( pOption );
              }
              else {
                const trd_t& trd( m_fGetTableRowDef( sIQFeedOptionName ) ); // TODO: check for errors

                std::string sGenericOptionName
                  = ou::tf::Instrument::BuildGenericOptionName( pUnderlyingInstrument->GetInstrumentName(), trd.eOptionSide, trd.nYear, trd.nMonth, trd.nDay, trd.dblStrike );
                bool bNeedContract( false );
                pInstrument_t pOptionInstrument;
                ou::tf::InstrumentManager& im( ou::tf::InstrumentManager::GlobalInstance().Instance() );

                if ( im.Exists( sGenericOptionName, pOptionInstrument ) ) {
                  // pOptionInstrument has been populated
                  assert( 0 != pOptionInstrument->GetContract() );
                }
                else {
                  pOptionInstrument = ou::tf::iqfeed::BuildInstrument( sGenericOptionName, trd );  // builds equity option, may not build futures option
                  bNeedContract = true;
                }

                //pOption_t pOption = m_pOptionEngine->m_fBuildOption( pOptionInstrument );
                pOption_t pOption( new ou::tf::option::Option( pOptionInstrument, m_pData1 ) );
                //pOption_t pOption;
                //m_pOptionEngine->Find( pOptionInstrument, pOption );

                if ( bNeedContract ) {
                  if ( nullptr == m_pIB.get() ) {
                    throw std::runtime_error( "MasterPortfolio::AddSymbol fConstructOption_t: IB provider unavailable for contract");
                  }
                  else {
                      m_pIB->RequestContractDetails(
                        pUnderlyingInstrument->GetInstrumentName(), pOptionInstrument,
                        [this,pOption,fOption](const ou::tf::IBTWS::ContractDetails& details, pInstrument_t& pInstrument){
                          // the contract details fill in the contract in the instrument, which can then be passed back to the caller
                          //   as a fully defined, registered instrument
                          assert( 0 != pInstrument->GetContract() );
                          ou::tf::InstrumentManager& im( ou::tf::InstrumentManager::GlobalInstance().Instance() );
                          if ( !im.Exists( pInstrument ) ) {
                            im.Register( pInstrument );  // is a CallAfter required, or can this run in a thread?
                          }
                          fOption( pOption );
                        },
                        nullptr  // request complete doesn't need a function
                        );
                  }
                }
                else {
                  fOption( pOption );
                }
              }
        },
    // ManageStrategy::fConstructPosition_t
        [this]( const idPortfolio_t& idPortfolio, pWatch_t pWatch, const std::string& sNote )->ManageStrategy::pPosition_t{
              pPosition_t pPosition;
              bool bUseExistingPosition( true );
              mapPosition_t::iterator iterPosition;
              mapStrategyArtifacts_iter iter = m_mapStrategyArtifacts.find( idPortfolio );
              if ( m_mapStrategyArtifacts.end() == iter ) {
                // maybe BasketTrading.cpp needs to do the construction, to keep the id's proper?
                bUseExistingPosition = false;
              }
              else {
                StrategyArtifacts& artifacts( iter->second );
                iterPosition = artifacts.m_mapPosition.find( pWatch->GetInstrument()->GetInstrumentName() );
                if ( artifacts.m_mapPosition.end() == iterPosition ) {
                  bUseExistingPosition = false;
                }
              }

              if ( bUseExistingPosition ) {
                pPosition = iterPosition->second;
                if ( pWatch != pPosition->GetWatch() ) {
                  std::cout << "** duplicate watch for " << pWatch->GetInstrument()->GetInstrumentName() << std::endl;
                }
              }
              else {
                auto& instance( ou::tf::PortfolioManager::Instance() );
                const std::string& sInstrumentName( pWatch->GetInstrument()->GetInstrumentName() );
                pPosition = instance.ConstructPosition(
                  idPortfolio, sInstrumentName, "Basket", "ib01", "iq01", m_pExec, pWatch );
                if ( 0 != sNote.size() ) {
                  pPosition->SetNotes( sNote );
                  instance.PositionUpdateNotes( pPosition );
                }
                Add( pPosition );  // update the archive
              }

              return pPosition;
          },
    // ManageStrategy::fConstructPortfolio_t
          [this,sUnderlying]( const idPortfolio_t& idPortfolioNew, const idPortfolio_t& idPortfolioOwner )->pPortfolio_t {
            pPortfolio_t pPortfolio;
            bool bUseExistingPortfolio( true );
            mapStrategyArtifacts_iter iterStrategyArtifacts = m_mapStrategyArtifacts.find( idPortfolioNew );
            if ( m_mapStrategyArtifacts.end() == iterStrategyArtifacts ) {
              bUseExistingPortfolio = false;
            }
            if ( bUseExistingPortfolio ) {
              pPortfolio = iterStrategyArtifacts->second.m_pPortfolio;
            }
            else {
              ou::tf::Portfolio::idAccountOwner_t idAccountOwner( "basket" ); // need to re-factor this with the other instance
              pPortfolio
                = ou::tf::PortfolioManager::Instance().ConstructPortfolio(
                    idPortfolioNew, idAccountOwner, idPortfolioOwner, ou::tf::Portfolio::EPortfolioType::MultiLeggedPosition, ou::tf::Currency::Name[ ou::tf::Currency::USD ], "Combo"
                );
              Add( pPortfolio );  // update the archive
            }

            // move from pManageStrategyInWaiting to mapStrategyActive
            iterUnderlyingWithStrategies_t iterUWS = m_mapUnderlyingWithStrategies.find( sUnderlying );
            assert( m_mapUnderlyingWithStrategies.end() != iterUWS );
            UnderlyingWithStrategies& uws( iterUWS->second );
            assert( uws.pManageStrategyInWaiting );

            pChartDataView_t pChartDataView = std::make_shared<ou::ChartDataView>();

            uws.mapStrategyActive.emplace(
              std::make_pair( idPortfolioNew, std::make_unique<Strategy>( std::move( uws.pManageStrategyInWaiting ), pChartDataView ) )
              );

            return pPortfolio;
          },
    // ManageStrategy::fRegisterWatch_t
          std::bind( &ou::tf::option::Engine::RegisterWatch, m_pOptionEngine.get(), ph::_1 ),
    // ManageStrategy::fRegisterOption_t
          std::bind( &ou::tf::option::Engine::RegisterOption, m_pOptionEngine.get(), ph::_1 ),
    // ManageStrategy::fStartCalc_t
          [this]( pOption_t pOption, pWatch_t pUnderlying ){
            m_pOptionEngine->Add( pOption, pUnderlying );
          },
    // ManageStrategy::m_fStopCalc
          [this]( pOption_t pOption, pWatch_t pUnderlying ){
            m_pOptionEngine->Remove( pOption, pUnderlying );
          },
    // ManageStrategy::m_fFirstTrade
          [this](ManageStrategy& ms, const ou::tf::Trade& trade){  // assumes same thread entry
            // calculate the starting parameters
            // TODO: need to do this differently
            //mapStrategy_t::iterator iter = m_mapStrategy.find( ms.GetPortfolio()->Id() );
            //assert( m_mapStrategy.end() != iter );
            //Strategy& strategy( iter->second );
            //strategy.priceOpen = trade.Price();
            //IIPivot::Pair pair = strategy.iip.Test( trade.Price() );
            //strategy.dblBestProbability = pair.first;
            //std::cout << "FirstTrade " << m_mapPivotProbability.size() << " - " << strategy.iip.sName << ": " << (int)pair.second << ", " << pair.first << std::endl;
            //if ( IIPivot::Direction::Unknown != pair.second ) {
            //  switch ( m_eAllocate ) {
            //    case EAllocate::Waiting:
            //      //mapPivotProbability_t::iterator iterRanking =
            //        m_mapPivotProbability.insert( mapPivotProbability_t::value_type( pair.first, Ranking( strategy.iip.sName, pair.second ) ) );
            //      break;
            //    case EAllocate::Process:
            //      break;
            //    case EAllocate::Done:
            //      break;
            //  }
            //}
          },
    // ManageStrategy::m_fAuthorizeUnderlying
          [this]( pOrder_t& pOrder, pPosition_t& pPosition, pPortfolio_t& pPortfolio )->bool{
            //ou::tf::MoneyManager& mm( ou::tf::MoneyManager::GlobalInstance() );
            //bool bAuthorized = mm.Authorize( pOrder, pPosition, pPortfolio );
            //if ( bAuthorized ) {
            //  if ( !strategy.bChartActivated ) {
            //    strategy.idTree = m_fChartAdd( m_idTreeStrategies, strategy.pManageStrategy->GetPortfolio()->Id(), strategy.pChartDataView );
            //    strategy.bChartActivated = true;
            //  }
            //}
            //ou::tf::DatedDatum::volume_t volume = strategy.pManageStrategy->CalcShareCount( dblAmountToTradePerInstrument );
            //m_nSharesTrading += strategy.pManageStrategy->CalcShareCount( dblAmountToTradePerInstrument );
            //strategy.pManageStrategy->SetFundsToTrade( dblAmountToTradePerInstrument );
            //strategy.pManageStrategy->Start();
            return true;
          },
    // ManageStrategy::m_fAuthorizeOption
          [this/*,&strategy*/]( pOrder_t& pOrder, pPosition_t& pPosition, pPortfolio_t& pPortfolio, pWatch_t& pWatch )->bool{
            ou::tf::MoneyManager& mm( ou::tf::MoneyManager::GlobalInstance() );
            //bool bAuthorized = mm.Authorize( pOrder, pPosition, pPortfolio, pWatch );
            //if ( bAuthorized ) {
            //  if ( !strategy.bChartActivated ) {
            //    strategy.idTree = m_fChartAdd( m_idTreeStrategies, strategy.pManageStrategy->GetPortfolio()->Id(), strategy.pChartDataView );
            //    strategy.bChartActivated = true;
            //  }
            //}
            //ou::tf::DatedDatum::volume_t volume = strategy.pManageStrategy->CalcShareCount( dblAmountToTradePerInstrument );
            //m_nSharesTrading += strategy.pManageStrategy->CalcShareCount( dblAmountToTradePerInstrument );
            //strategy.pManageStrategy->SetFundsToTrade( dblAmountToTradePerInstrument );
            //strategy.pManageStrategy->Start();
            //return bAuthorized;
            return true;
          },
    // ManageStrategy::m_fAuthorizeSimple
          [this,sUnderlying]( const idPortfolio_t& idPortfolio, const std::string& sName, bool bExists )->bool{

            iterUnderlyingWithStrategies_t iterUnderlyingWithStrategies
              = m_mapUnderlyingWithStrategies.find( sUnderlying );
            UnderlyingWithStrategies& uws( iterUnderlyingWithStrategies->second );

            iterStrategy_t iterStrategy = uws.mapStrategyActive.find( idPortfolio );
            assert( uws.mapStrategyActive.end() != iterStrategy );
            Strategy& strategy( *iterStrategy->second );

            ou::tf::MoneyManager& mm( ou::tf::MoneyManager::GlobalInstance() );
            bool bAuthorized = mm.Authorize( sName );
            if ( bAuthorized || bExists ) {
              if ( !strategy.bChartActivated ) {
                strategy.idTree = m_fChartAdd( m_idTreeStrategies, strategy.pManageStrategy->GetPortfolio()->Id(), strategy.pChartDataView );
                strategy.bChartActivated = true;
              }
            }
            return bAuthorized;
          },
    // ManageStrategy::m_fBar (6 second)
          [this](ManageStrategy& ms, const ou::tf::Bar& bar){
            // calculate sentiment
            //m_sentiment.Update( bar );
          }
      );

  return uws.pManageStrategyInWaiting;

} // ConstructStrategy

void MasterPortfolio::StartStrategies( const std::string& sUnderlying, pPortfolio_t pPortfolioUnderlying ) {

  const idPortfolio_t& idPortfolioUnderlying( pPortfolioUnderlying->Id() );

  mapStrategyArtifacts_iter iterStrategyArtifacts = m_mapStrategyArtifacts.find( idPortfolioUnderlying );
  if ( m_mapStrategyArtifacts.end() == iterStrategyArtifacts ) { // start empty strategy
    pManageStrategy_t pManageStrategy( ConstructStrategy( sUnderlying, pPortfolioUnderlying ) ); // initial strategy
    pManageStrategy->Run();
  }
  else { // process existing strategies
    StrategyArtifacts& artifacts( iterStrategyArtifacts->second );
    assert( artifacts.m_mapPosition.empty() );  // this works for now, no underlying positions

    //std::for_each(  // add existing underlying positions to the strategy, active or not
    //  artifacts.m_mapPosition.begin(), artifacts.m_mapPosition.end(),
    //  [this,&strategy](mapPosition_t::value_type& vt){
    //    strategy.pManageStrategy->AddPosition( vt.second );
    //  }
    //);

    // iterate the portfolioStrategies belonging to the portfolioUnderlying
    for ( mapPortfolio_t::value_type& vt: artifacts.m_mapPortfolio ) {
      // TODO: need to determine if comboPortfolio is active
      const idPortfolio_t& idPortfolioCombo( vt.second->Id() );

      mapStrategyArtifacts_iter iterStrategyArtifacts = m_mapStrategyArtifacts.find( idPortfolioCombo );
      StrategyArtifacts& artifactsCombo( iterStrategyArtifacts->second );
      assert( !artifactsCombo.m_mapPosition.empty() );
      pManageStrategy_t pManageStrategy( ConstructStrategy( sUnderlying, artifactsCombo.m_pPortfolio ) );

      bool bActivated( false );
      for ( mapPosition_t::value_type& vt: artifacts.m_mapPosition ) {
        if ( vt.second->IsActive() ) {
          pManageStrategy->AddPosition( vt.second );  // one or more active positions will move it
          bActivated = true;
        }
        if ( bActivated ) {
          pManageStrategy->Run();
        }

      }
      artifacts.m_bAccessed = true;
    }
  }

}

void MasterPortfolio::ClosePositions( void ) {
  std::for_each(
    m_mapUnderlyingWithStrategies.begin(), m_mapUnderlyingWithStrategies.end(),
    [](mapUnderlyingWithStrategies_t::value_type& uws){
      uws.second.ClosePositions();
    } );
}

void MasterPortfolio::SaveSeries( const std::string& sPrefix ) {
  std::string sPath( sPrefix + m_sTSDataStreamStarted );
  m_libor.SaveSeries( sPath );
  std::for_each(
    m_mapUnderlyingWithStrategies.begin(), m_mapUnderlyingWithStrategies.end(),
    [&sPath](mapUnderlyingWithStrategies_t::value_type& uws){
      uws.second.SaveSeries( sPath );
    } );
  std::cout << "done." << std::endl;
}

void MasterPortfolio::EmitInfo( void ) {
  double dblNet {};
  std::for_each(
    m_mapUnderlyingWithStrategies.begin(), m_mapUnderlyingWithStrategies.end(),
    [&dblNet](mapUnderlyingWithStrategies_t::value_type& uws){
      dblNet += uws.second.EmitInfo();
    } );
  std::cout << "Portfolio net: " << dblNet << std::endl;
}

void MasterPortfolio::CloseExpiryItm( boost::gregorian::date date ) {
  /*
  std::for_each(
    m_mapStrategy.begin(), m_mapStrategy.end(),
    [&date](mapStrategy_t::value_type& pair){
      Strategy& strategy( pair.second );
        strategy.pManageStrategy->CloseExpiryItm( date );
    } );
    */
}

void MasterPortfolio::CloseFarItm() {
  /*
  std::for_each(
    m_mapStrategy.begin(), m_mapStrategy.end(),
    [](mapStrategy_t::value_type& pair){
      Strategy& strategy( pair.second );
        strategy.pManageStrategy->CloseFarItm();
    } );
    */
}

void MasterPortfolio::CloseForProfits() {
  std::for_each(
    m_mapUnderlyingWithStrategies.begin(), m_mapUnderlyingWithStrategies.end(),
    [](mapUnderlyingWithStrategies_t::value_type& uws){
      uws.second.CloseForProfits();
    } );
}

void MasterPortfolio::CloseItmLeg() {
  /*
  std::for_each(
    m_mapStrategy.begin(), m_mapStrategy.end(),
    [](mapStrategy_t::value_type& pair){
      Strategy& strategy( pair.second );
        strategy.pManageStrategy->CloseItmLeg();
    } );
    */
}

void MasterPortfolio::AddCombo( bool bForced ) {
  /*
  for ( mapStrategy_t::value_type& vt: m_mapStrategy ) {
    Strategy& strategy( vt.second );
    strategy.pManageStrategy->AddCombo( bForced );
  }
  */
}

void MasterPortfolio::TakeProfits() {
  std::for_each(
    m_mapUnderlyingWithStrategies.begin(), m_mapUnderlyingWithStrategies.end(),
    [](mapUnderlyingWithStrategies_t::value_type& uws){
      uws.second.TakeProfits();
    } );
}
