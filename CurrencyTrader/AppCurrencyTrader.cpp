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
 * File:    AppCurrencyTrader.cpp
 * Author:  raymond@burkholder.net
 * Project: CurrencyTrader
 * Created: March 09, 2024 19:58:27
 */

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/lexical_cast.hpp>

#include <wx/sizer.h>

#include <TFVuTrading/FrameMain.h>
#include <TFVuTrading/TreeItem.hpp>
#include <TFVuTrading/PanelLogging.h>
#include <TFVuTrading/WinChartView.h>
#include <TFVuTrading/PanelProviderControlv2.hpp>

#include "AppCurrencyTrader.hpp"

namespace {
  static const std::string c_sAppName( "CurrencyTrader" );
  static const std::string c_sVendorName( "One Unified Net Limited" );

  static const std::string c_sDirectory( "" );
  static const std::string c_sDbName(          c_sDirectory + '/' + c_sAppName + ".db" );
  static const std::string c_sStateFileName(   c_sDirectory + '/' + c_sAppName + ".state" );
  static const std::string c_sChoicesFilename( c_sDirectory + '/' + c_sAppName + ".cfg" );
}

IMPLEMENT_APP(AppCurrencyTrader)

bool AppCurrencyTrader::OnInit() {

  wxApp::SetVendorName( c_sVendorName );
  wxApp::SetAppDisplayName( c_sAppName );
  wxApp::SetVendorDisplayName( "(c)2024 " + c_sVendorName );

  wxApp::OnInit();

  m_nTSDataStreamSequence = 0;

  if ( config::Load( c_sChoicesFilename, m_choices ) ) {
  }
  else {
    return false;
  }

  {
    std::stringstream ss;
    auto dt = ou::TimeSource::GlobalInstance().External();
    ss
      << ou::tf::Instrument::BuildDate( dt.date() )
      << " "
      << dt.time_of_day()
      ;
    m_sTSDataStreamStarted = ss.str();  // will need to make this generic if need some for multiple providers.
  }

  m_tws = ou::tf::ib::TWS::Factory();
  m_tws->SetName( "ib01" );
  m_tws->SetClientId( m_choices.m_nIbInstance );

  m_pFrameMain = new FrameMain( 0, wxID_ANY,c_sAppName );
  wxWindowID idFrameMain = m_pFrameMain->GetId();

  m_pFrameMain->SetSize( 800, 500 );
  //m_pFrameMain->SetAutoLayout( true );

  SetTopWindow( m_pFrameMain );

  wxBoxSizer* sizerFrame;
  wxBoxSizer* sizerUpper;
  wxBoxSizer* sizerLower;

  sizerFrame = new wxBoxSizer(wxVERTICAL);
  m_pFrameMain->SetSizer(sizerFrame);

  sizerUpper = new wxBoxSizer(wxHORIZONTAL);
  sizerFrame->Add( sizerUpper, 0, wxEXPAND, 2);

  // TODO: make the panel conditional on simulation flag
  m_pPanelProviderControl = new ou::tf::v2::PanelProviderControl( m_pFrameMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
  m_pPanelProviderControl->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
  m_pPanelProviderControl->Show();

  sizerUpper->Add( m_pPanelProviderControl, 0, wxALIGN_LEFT, 2);

  m_pPanelProviderControl->Add(
    m_tws,
    false, false, true, false,
    [](){}, // fConnecting
    [this]( bool bD1, bool bD2, bool bX1, bool bX2 ){ // fConnected
      //ConfirmProviders();
    },
    [](){}, // fDisconnecting
    [](){}  // fDisconnected
  );

  // for now, overwrite old database on each start
  if ( boost::filesystem::exists( c_sDbName ) ) {
    boost::filesystem::remove( c_sDbName );
  }

  // this needs to be placed after the providers are registered
  m_pdb = std::make_unique<ou::tf::db>( c_sDbName ); // construct database

  m_pWinChartView->SetChartDataView( &m_dvChart );

  m_pFrameMain->Bind( wxEVT_CLOSE_WINDOW, &AppCurrencyTrader::OnClose, this );  // start close of windows and controls

  CallAfter(
    [this](){
      LoadState();
    }
  );

  m_pFrameMain->Show( true );

  return 1;
}

void AppCurrencyTrader::HandleMenuActionSaveValues() {
  std::cout << "Saving collected values ... " << std::endl;
  CallAfter(
    [this](){
      m_nTSDataStreamSequence++;
      //m_pStrategy->SaveWatch(
      //  "/app/" + c_sAppNamePrefix + "/" +
      //  m_sTSDataStreamStarted + "-" +
      //  boost::lexical_cast<std::string>( m_nTSDataStreamSequence ) ); // sequence number on each save
      std::cout << "  ... Done " << std::endl;
    }
  );
}


void AppCurrencyTrader::SaveState() {
  std::cout << "Saving Config ..." << std::endl;
  std::ofstream ofs( c_sStateFileName );
  boost::archive::text_oarchive oa(ofs);
  oa & *this;
  std::cout << "  done." << std::endl;
}

void AppCurrencyTrader::LoadState() {
  try {
    std::cout << "Loading Config ..." << std::endl;
    std::ifstream ifs( c_sStateFileName );
    boost::archive::text_iarchive ia(ifs);
    ia & *this;
    std::cout << "  done." << std::endl;
  }
  catch(...) {
    std::cout << "load exception" << std::endl;
  }
}

void AppCurrencyTrader::OnClose( wxCloseEvent& event ) {
  // Exit Steps: #2 -> FrameMain::OnClose

  //m_pWinChartView->SetChartDataView( nullptr, false );
  //delete m_pChartData;
  //m_pChartData = nullptr;

  //m_pFrameControls->Close();

  //if ( !m_choices.bStartSimulator ) {
  //  m_timerOneSecond.Stop();
  //  Unbind( wxEVT_TIMER, &AppAutoTrade::HandleOneSecondTimer, this, m_timerOneSecond.GetId() );
  //}

  //m_mapStrategy.clear();

  //m_pBuildInstrument.reset();

  if ( m_pdb ) m_pdb.reset();

  // NOTE: when running the simuliation, perform a deletion instead
  //   use the boost file system utilities?
  //   or the object Delete() operator may work
//  if ( m_choices.bStartSimulator ) {
//    if ( m_pFile ) { // performed at exit to ensure no duplication in file
      //m_pFile->Delete(); // double free or corruption here
//    }
//  }
//  else {
//    if ( m_pFile ) { // performed at exit to ensure no duplication in file
//      m_pFile->Write();
//    }
//  }

//  if ( 0 != OnPanelClosing ) OnPanelClosing();
  // event.Veto();  // possible call, if needed
  // event.CanVeto(); // if not a
  SaveState();

  event.Skip();  // auto followed by Destroy();
}

int AppCurrencyTrader::OnExit() {
  // Exit Steps: #4
//  DelinkFromPanelProviderControl();  generates stack errors

  return wxAppConsole::OnExit();
}
