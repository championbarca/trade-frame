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
 * File:    AppIndicatorTrading.h
 * Author:  raymond@burkholder.net
 * Project: IndicatorTrading
 * Created: February 8, 2022 00:12
 */

#pragma once

#include <string>

#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>

#include <wx/app.h>
#include <wx/splitter.h>

#include <TFTrading/DBWrapper.h>

#include <TFBitsNPieces/FrameWork01.h>

#include "Config.h"

class wxBoxSizer;
class wxTreeCtrl;
class wxTreeEvent;

class FrameMain;
class InteractiveChart;

namespace ou {
namespace tf {
  class TreeItem;
  class PanelLogging;
  class FrameControls;
  class BuildInstrument;
  class PanelOrderButtons;
namespace iqfeed {
  class OptionChainQuery;
}
}
}

class AppIndicatorTrading:
  public wxApp,
  public ou::tf::FrameWork01<AppIndicatorTrading>
{
  friend ou::tf::FrameWork01<AppIndicatorTrading>;
  friend class boost::serialization::access;
public:
protected:
private:

  using TreeItem = ou::tf::TreeItem;
  using pPosition_t = ou::tf::Position::pPosition_t;
  using pPortfolio_t = ou::tf::Portfolio::pPortfolio_t;
  using pInstrument_t = ou::tf::Instrument::pInstrument_t;

  FrameMain* m_pFrameMain;
  ou::tf::PanelLogging* m_pPanelLogging;
  ou::tf::FrameControls* m_pFrameControls;
  ou::tf::PanelOrderButtons* m_pPanelOrderButtons;
  InteractiveChart* m_pInteractiveChart;

  std::string m_sTSDataStreamStarted;

  TreeItem* m_pTreeItemRoot; // root of custom tree items
  wxTreeCtrl* m_ptreeTradables;  // http://docs.wxwidgets.org/trunk/classwx_tree_ctrl.html

  wxBoxSizer* m_sizerFrame;
  wxSplitterWindow* m_splitterRow;

  std::unique_ptr<ou::tf::BuildInstrument> m_pBuildInstrument;
  std::unique_ptr<ou::tf::BuildInstrument> m_pBuildInstrumentIQFeed;

  std::unique_ptr<ou::tf::db> m_pdb;

  config::Options m_config;

  pPortfolio_t m_pPortfolio;

  using pOptionChainQuery_t = std::shared_ptr<ou::tf::iqfeed::OptionChainQuery>;
  pOptionChainQuery_t m_pOptionChainQuery; // need to disconnect

  virtual bool OnInit();
  virtual int OnExit();
  void OnClose( wxCloseEvent& event );

  void OnData1Connected( int );
  void OnData2Connected( int );
  void OnExecConnected( int );
  void OnData1Disconnected( int );
  void OnData2Disconnected( int );
  void OnExecDisconnected( int );

  void HandleMenuActionStartWatch();
  void HandleMenuActionStopWatch();
  void HandleMenuActionSaveValues();
  void HandleMenuActionEmitChainsSummary();
  void HandleMenuActionEmitChainsFull();
  void HandleMenuActionProcessChains();
  void HandleMenuActionEmitOptionVolume();
  void HandleMenuActionStartChart();
  void HandleMenuActionStopChart();

  void HandleMenuActionOptionWatchStart();
  void HandleMenuActionOptionQuoteShow();
  void HandleMenuActionOptionWatchStop();
  void HandleMenuActionOptionEmit();

  void StartChainQuery();
  void ConstructUnderlying();
  void InitializeUnderlying( pInstrument_t pInstrument );
  pPosition_t ConstructPosition( pInstrument_t );
  void SetInteractiveChart( pPosition_t );

  void SaveState();
  void LoadState();

  template<typename Archive>
  void save( Archive& ar, const unsigned int version ) const {
    ar & *m_pFrameMain;
    ar & m_splitterRow->GetSashPosition();
    ar & *m_pFrameControls;
  }

  template<typename Archive>
  void load( Archive& ar, const unsigned int version ) {
    ar & *m_pFrameMain;
    int x;
    ar & x;
    m_splitterRow->SetSashPosition( x );
    if ( 3 <= version ) {
      ar & *m_pFrameControls;
    }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER()

};

BOOST_CLASS_VERSION(AppIndicatorTrading, 3)

DECLARE_APP(AppIndicatorTrading)

