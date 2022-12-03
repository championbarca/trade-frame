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
 * File:    WinRowElement.cpp
 * Author:  raymond@burkholder.net
 * Project: TFVuTrading/MarketDepth
 * Created on October 28, 2021, 16:29
 */

 //#include <iostream>

//#include <wx/sizer.h>
#include <wx/event.h>
//#include <wx/tooltip.h>
#include <wx/dcclient.h>

#include "WinRowElement.hpp"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace l2 { // market depth

WinRowElement::WinRowElement() {}

WinRowElement::WinRowElement(
  wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style
)
{
  Create(parent, id, pos, size, style);
}

WinRowElement::~WinRowElement() {
}

bool WinRowElement::Create(
  wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style
) {
  m_style = style;

  SetExtraStyle(wxWS_EX_BLOCK_EVENTS); // TODO: do we keep this or not?
  wxWindow::Create( parent, id, pos, size, style );

  Init();

  CreateControls();
  //if ( GetSizer() ) {
  //  GetSizer()->SetSizeHints( this );
  //}

  Paint();

  return true;
}

void WinRowElement::Init() {
  m_bFocusSet = false;
  m_bCanHaveFocus = false;
  m_fClick = nullptr;
  //m_ColourBackground = wxSystemSettings::GetColour( wxSystemColour::wxSYS_COLOUR_WINDOW ).GetRGB();
  //m_ColourForeground = wxSystemSettings::GetColour( wxSystemColour::wxSYS_COLOUR_WINDOWTEXT ).GetRGB();
}

void WinRowElement::CreateControls() {

  Bind( wxEVT_PAINT, &WinRowElement::OnPaint, this, GetId() );
  Bind( wxEVT_DESTROY, &WinRowElement::OnDestroy, this, GetId() );

  Bind( wxEVT_ENTER_WINDOW, &WinRowElement::OnMouseEnterWindow, this );
  Bind( wxEVT_LEAVE_WINDOW, &WinRowElement::OnMouseLeaveWindow, this );

  Bind( wxEVT_LEFT_UP, &WinRowElement::OnMouseLeftUp, this );
  Bind( wxEVT_MIDDLE_UP, &WinRowElement::OnMouseMiddleUp, this );
  Bind( wxEVT_RIGHT_UP, &WinRowElement::OnMouseRightUp, this );

  //Bind( wxEVT_CONTEXT_MENU, &WinRowElement::OnContextMenu, this, GetId() );
  //Bind( wxEVT_SET_FOCUS, &WinRowElement::OnFocusSet, this, GetId() );
  //Bind( wxEVT_KILL_FOCUS, &WinRowElement::OnFocusKill, this, GetId() );

}

void WinRowElement::SetCanHaveFocus( bool bCanHaveFocus ) {
  m_bCanHaveFocus = bCanHaveFocus;
}

void WinRowElement::SetText( const std::string& sText ) {
  m_sText = sText;
  // for tooltip, need to assign to undelrying window
  // will need to identify the the container under the cursor,
  //   and use that to populate the tooltip
  //if ( 0 == m_sText.size() ) {
  //  UnsetToolTip();
  //}
  //else {
  //  this->GetParent()->SetToolTip( m_sText );
  //}
  Paint();
}

void WinRowElement::SetText( const std::string& sText, bool bHighlight ) {
  wxWindow::SetBackgroundColour( bHighlight ? m_colourHighlight : m_colourBackground );
  SetText( sText );
}

void WinRowElement::SetText( const std::string& sText, EColour bg ) {
  SetColourBackground( bg );
  SetText( sText );
}

void WinRowElement::SetText( const std::string& sText, EColour fg, EColour bg ) {
  SetColourForeground( fg );
  SetColourBackground( bg );
  SetText( sText );
}

void WinRowElement::SetColourBackground( EColour colour ) {
  m_colourBackground = colour;
  wxWindow::SetBackgroundColour( colour );
}

void WinRowElement::SetColourForeground( EColour colour ) {
  m_colourForeground = colour;
  wxWindow::SetForegroundColour( colour );
}

void WinRowElement::SetColourHighlight( EColour colour ) {
  m_colourHighlight = colour;
}

void WinRowElement::SetColours( EColour colourB, EColour colourF, EColour colourH ) {
  SetColourBackground( colourB );
  SetColourForeground( colourF );
  SetColourHighlight( colourH );
}

void WinRowElement::Set( fClick_t&& fClick ) {
  m_fClick = std::move( fClick );
}

void WinRowElement::Paint() {
  wxWindowDC dc( this );
  Render( dc );
}

void WinRowElement::Render( wxDC& dc ) {
  wxSize size = dc.GetSize();
  dc.Clear();
  if ( m_bFocusSet ) {
    size.DecBy( 2, 2 );
    dc.DrawRectangle( wxPoint( 1, 1 ), size );
  }

  wxCoord width;
  wxCoord height;
  wxCoord x( 1 );
  GetTextExtent( m_sText, &width, &height );
  switch ( m_style ) {
    case wxRIGHT:
      x = width - 1;
      break;
    case wxCENTER:
      if ( width <= size.x ) {
        x = ( size.x - width ) / 2;
      }
      break;
    case wxLEFT:
      x = 1;
      break;
  }
  dc.DrawText( m_sText, wxPoint( x, 1 ) );
}

void WinRowElement::OnPaint( wxPaintEvent& event ) {
  wxPaintDC dc( this );
  Render( dc );
  event.Skip();
}

// requires a click
void WinRowElement::OnFocusSet( wxFocusEvent& event ) {
  //std::cout << "OnFocusSet" << std::endl;
  event.Skip();
}

void WinRowElement::OnFocusKill( wxFocusEvent& event ) {
  //std::cout << "OnFocusKill" << std::endl;
  event.Skip();
}

void WinRowElement::OnMouseLeftUp( wxMouseEvent& event ) {
  //std::cout << "left click" << std::endl;
  if ( m_fClick ) m_fClick( EButton::Left, event.ShiftDown(), event.ControlDown(), event.AltDown() );
  event.Skip();
}

void WinRowElement::OnMouseMiddleUp( wxMouseEvent& event ) {
  //std::cout << "middle click" << std::endl;
  if ( m_fClick ) m_fClick( EButton::Middle, event.ShiftDown(), event.ControlDown(), event.AltDown() );
  event.Skip();
}

void WinRowElement::OnMouseRightUp( wxMouseEvent& event ) {
  //std::cout << "right click" << std::endl;
  if ( m_fClick ) m_fClick( EButton::Right, event.ShiftDown(), event.ControlDown(), event.AltDown() );
  event.Skip();
}

void WinRowElement::OnMouseEnterWindow( wxMouseEvent& event ) {
  if ( m_bCanHaveFocus ) {
    m_bFocusSet = true;
    Paint();
  }
  event.Skip();
}

void WinRowElement::OnMouseLeaveWindow( wxMouseEvent& event ) {
  if ( m_bCanHaveFocus ) {
    m_bFocusSet = false;
    Paint();
  }
  event.Skip();
}

void WinRowElement::OnContextMenu( wxContextMenuEvent& event ) {
  event.Skip();
}

void WinRowElement::OnDestroy( wxWindowDestroyEvent& event ) {

  Unbind( wxEVT_PAINT, &WinRowElement::OnPaint, this, GetId() );
  Unbind( wxEVT_DESTROY, &WinRowElement::OnDestroy, this, GetId() );

  Unbind( wxEVT_ENTER_WINDOW, &WinRowElement::OnMouseEnterWindow, this );
  Unbind( wxEVT_LEAVE_WINDOW, &WinRowElement::OnMouseLeaveWindow, this );

  Unbind( wxEVT_LEFT_UP, &WinRowElement::OnMouseLeftUp, this );
  Unbind( wxEVT_MIDDLE_UP, &WinRowElement::OnMouseMiddleUp, this );
  Unbind( wxEVT_RIGHT_UP, &WinRowElement::OnMouseRightUp, this );

  //Unbind( wxEVT_CONTEXT_MENU, &WinRowElement::OnContextMenu, this, this->GetId() );
  //Unbind( wxEVT_SET_FOCUS, &WinRowElement::OnFocusSet, this, GetId() );
  //Unbind( wxEVT_KILL_FOCUS, &WinRowElement::OnFocusKill, this, GetId() );

  //event.Skip();  // no skip
}

} // market depth
} // namespace tf
} // namespace ou

/*
 wxEVT_LEFT_DOWN
 wxEVT_LEFT_UP
 wxEVT_MIDDLE_DOWN
 wxEVT_MIDDLE_UP
 wxEVT_RIGHT_DOWN
 wxEVT_RIGHT_UP
 wxEVT_MOTION
 wxEVT_ENTER_WINDOW
 wxEVT_LEAVE_WINDOW
 wxEVT_LEFT_DCLICK
 wxEVT_MIDDLE_DCLICK
 wxEVT_RIGHT_DCLICK
*/

