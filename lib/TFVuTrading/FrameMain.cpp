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

// Generic frame for wx based applications

#include <iostream>

#include <wx/menu.h>
#include <wx/statusbr.h>

#include "FrameMain.h"

FrameMain::FrameMain() {
  Init();
}

FrameMain::FrameMain( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style ) {
  Init();
  Create( parent, id, caption, pos, size, style );
}

FrameMain::~FrameMain() {
  //std::cout << "FrameMain::~FrameMain" << this->GetName() << std::endl;
}

bool FrameMain::Create( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style ) {

  wxFrame::Create( parent, id, caption, pos, size, style );

  CreateControls();
  Centre();
  return true;
}

void FrameMain::Init() {
  m_ixDynamicMenuItem = ID_DYNAMIC_MENU_ACTIONS;
  m_menuFile = 0;
  m_menuBar = nullptr;
  m_statusBar = nullptr;
}

void FrameMain::CreateControls() {

    FrameMain* itemFrame1 = this;

    m_menuBar = new wxMenuBar;
    itemFrame1->SetMenuBar(m_menuBar);

    m_menuFile = new wxMenu;
    m_menuFile->Append(ID_MENUEXIT, _("Exit"), wxEmptyString, wxITEM_NORMAL);
    m_menuBar->Append(m_menuFile, _("File"));

//    m_statusBar = new wxStatusBar( itemFrame1, ID_STATUSBAR, wxST_SIZEGRIP|wxNO_BORDER );
//    m_statusBar->SetFieldsCount(2);
//    itemFrame1->SetStatusBar(m_statusBar);

    Bind( wxEVT_COMMAND_MENU_SELECTED, &FrameMain::OnMenuExitClick, this, ID_MENUEXIT );
    Bind( wxEVT_DESTROY, &FrameMain::OnDestroy, this );
    Bind( wxEVT_CLOSE_WINDOW, &FrameMain::OnClose, this );
}

void FrameMain::OnMenuExitClick( wxCommandEvent& event ) {
  // Exit Steps:  #1 -> Appxxx::OnClose
  this->Close();
}

int FrameMain::AddFileMenuItem( const wxString& sText ) {
  m_ixDynamicMenuItem++;
  m_menuFile->Prepend(m_ixDynamicMenuItem, sText, wxEmptyString, wxITEM_NORMAL );
  return m_ixDynamicMenuItem;
}

wxMenu* FrameMain::AddDynamicMenu( const std::string& root, const vpItems_t& vItems ) {
  assert( 0 != vItems.size() );
  wxMenu* itemMenuAction = new wxMenu;
  for ( vpItems_t::const_iterator iter = vItems.begin(); vItems.end() != iter; ++iter ) {
//    structMenuItem* p( new structMenuItem( vItems[ ix ] ) );
    structMenuItem* p = *iter;
    p->ix = m_vPtrItems.size();
    m_vPtrItems.push_back( p );

    m_ixDynamicMenuItem++;
    itemMenuAction->Append(m_ixDynamicMenuItem, p->text, wxEmptyString, wxITEM_NORMAL);
    Bind( wxEVT_COMMAND_MENU_SELECTED, &FrameMain::OnDynamicActionClick, this, m_ixDynamicMenuItem, -1, p );
  }
  m_menuBar->Append(itemMenuAction, root );
	return itemMenuAction;
}

void FrameMain::OnDynamicActionClick( wxCommandEvent& event ) {
  structMenuItem* p = dynamic_cast<structMenuItem*>( event.m_callbackUserData );
  if ( 0 != p->OnActionHandler )
    p->OnActionHandler();
}

void FrameMain::OnDestroy( wxWindowDestroyEvent& event ) {
  Unbind( wxEVT_DESTROY, &FrameMain::OnDestroy, this );
  event.Skip();
}

void FrameMain::OnClose( wxCloseEvent& event ) {
  // Exit Steps: #3 -> Appxxx::OnExit
  Unbind( wxEVT_CLOSE_WINDOW, &FrameMain::OnClose, this );
//  Unbind( wxEVT_COMMAND_MENU_SELECTED, &FrameMain::OnMenuExitClick, this, ID_MENUEXIT );  // causes crash
  // http://docs.wxwidgets.org/trunk/classwx_close_event.html
  event.Skip();  // continue with base class stuff
}

wxBitmap FrameMain::GetBitmapResource( const wxString& name ) {
  wxUnusedVar(name);
  return wxNullBitmap;
}

wxIcon FrameMain::GetIconResource( const wxString& name ) {
  wxUnusedVar(name);
  return wxNullIcon;
}

