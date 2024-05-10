/************************************************************************
 * Copyright(c) 2009, One Unified. All rights reserved.                 *
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

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_symbols.hpp>

#include <boost/fusion/include/std_pair.hpp>

#include <boost/phoenix/core.hpp>

#include "TradingEnumerations.h"

namespace ou { // One Unified
namespace tf { // TradeFrame

namespace InstrumentType {
  const char* Name[] = { "Unknown", "Stock", "Option", "Future",
    "FuturesOption", "Currency", "Index", "ETF", "Bond", "Commodity", "Metal",  };
}

namespace OptionSide {  // this doesn't work well with the existing enumerations
  const char* Name[] = { "Unknown", "Put", "Call" };
  const char* LongName[] = { "Unknown", "Put", "Call" };
  const char* ShortName[] = { "U", "P", "C" };
}

namespace Currency {

  namespace qi = boost::spirit::qi;

  const char* Name[] = { "USD", "GBP", "CAD", "CHF", "HKD", "JPY", "EUR", "KRW", "LTL", "AUD", "CZK", "DKK", "NZD", "HUF", "ILS", "VOID" };

  void FillSymbols(  qi::symbols<char, ECurrency>& list ) {
    list.add
      ( "USD", ECurrency::USD )
      ( "GBP", ECurrency::GBP )
      ( "CAD", ECurrency::CAD )
      ( "CHF", ECurrency::CHF )
      ( "HKD", ECurrency::HKD )
      ( "JPY", ECurrency::JPY )
      ( "EUR", ECurrency::EUR )
      ( "KRW", ECurrency::KRW )
      ( "LTL", ECurrency::LTL )
      ( "AUD", ECurrency::AUD )
      ( "CZK", ECurrency::CZK )
      ( "DKK", ECurrency::DKK )
      ( "NZD", ECurrency::NZD )
      ( "HUF", ECurrency::HUF )
      ( "ILS", ECurrency::ILS )
      ;
  }

  template<typename Iterator>
  struct ParserCurrencyPair: qi::grammar<Iterator, pair_t()> {
    ParserCurrencyPair(): ParserCurrencyPair::base_type( ruleStart ) {

      FillSymbols( symCurrency );

      ruleCurrency %= symCurrency;
      ruleStart %= ruleCurrency 
                >> -( qi::lit( '.' ) | qi::lit( ':' ) | qi::lit( '-' ) ) 
                >> ruleCurrency;

    }

    qi::symbols<char, ECurrency> symCurrency;

    qi::rule<Iterator, ECurrency()> ruleCurrency;
    qi::rule<Iterator, pair_t()> ruleStart;
  };

  pair_t Split( const std::string& sPair ) {
    static ParserCurrencyPair<std::string::const_iterator> parserCurrencyPair;

    pair_t pairCurrency( Currency::EUR, Currency::USD );
    bool b = parse( sPair.begin(), sPair.end(), parserCurrencyPair, pairCurrency );
    assert( b );
    return pairCurrency;
  }

  template<typename Iterator>
  struct ParserCurrencyName: qi::grammar<Iterator, ECurrency()> {
    ParserCurrencyName(): ParserCurrencyName::base_type( ruleCurrency ) {

      FillSymbols( symCurrency );

      ruleCurrency %= symCurrency;

    }

    qi::symbols<char, ECurrency> symCurrency;

    qi::rule<Iterator, ECurrency()> ruleCurrency;
  };

  ECurrency ParseName( const std::string& sName ) {
    static ParserCurrencyName<std::string::const_iterator> parserCurrencyName;
    ECurrency currency( Currency::USD );

    bool b = parse( sName.begin(), sName.end(), parserCurrencyName, currency );
    assert( b );
    return currency;
  }

}

namespace OrderSide {
  const char* Name[] = { "UNKN", "BUY", "SELL", "SSHORT" }; // for IB
}

namespace TimeInForce {
  const char* Name[] = { "UNKN", "DAY", "GTC", "ATO", "IOC", "FOK", "GTC", "GTD", "GAC", "ATC", "AUC", "OPG" };
}

} // namespace tf
} // namespace ou
