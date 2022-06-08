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
 * File:    Provider.hpp
 * Author:  raymond@burkholder.net
 * Project: lib/TFAlpaca
 * Created: June 5, 2022 16:04
 */

#pragma once

#include <boost/asio/ssl.hpp>

#include <TFTrading/ProviderInterface.h>

#include "Asset.hpp"

namespace asio  = boost::asio;      // from <boost/asio.hpp>
namespace ssl   = asio::ssl;        // from <boost/asio/ssl.hpp>

namespace ou {
namespace tf {
namespace alpaca {

class Provider:
  public ProviderInterface<Provider, Asset>
{
public:

  using pProvider_t = boost::shared_ptr<Provider>;
  using inherited_t = ProviderInterface<Provider,Asset>;
  using idSymbol_t = inherited_t::idSymbol_t ;
  using pSymbol_t = inherited_t::pSymbol_t;
  using pInstrument_t = inherited_t::pInstrument_t;
  using pOrder_t = ou::tf::Order::pOrder_t;

  Provider( const std::string& sHost, const std::string& sKey, const std::string& sSecret );
  virtual ~Provider();

  static pProvider_t Factory( const std::string& sHost, const std::string& sKey, const std::string& sSecret ) {
    return boost::make_shared<Provider>( sHost, sKey, sSecret );
  }

  // do these need to be virtual?  use crtp?
  virtual void Connect();
  virtual void Disconnect() {};

  // From ProviderInterface Execution Section
  void PlaceOrder( pOrder_t order );
  void CancelOrder( pOrder_t order );

protected:

  pSymbol_t NewCSymbol( pInstrument_t pInstrument );  // used by Add/Remove x handlers in base class

private:

  ssl::context m_ssl_context;

  std::string m_sHost;
  std::string m_sPort;
  std::string m_sAlpacaKeyId;
  std::string m_sAlpacaSecret;

};

} // namespace alpaca
} // namespace tf
} // namespace ou
