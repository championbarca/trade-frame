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

#ifdef WIN32
#include "StdAfx.h"
#endif

#include "EnvManager.h"

// CBerkeleyDBEnvManager =====

const char CBerkeleyDBEnvManager::m_BDBFileName[] = "TradeFrame.bdb";

/*
Environment and database handles can optionally be shared across threads. If handles are shared, 
they must be registered in each thread that is using the handle (either directly, or 
indirectly using the containers that own the handles). You do this using the 
dbstl::register_db() and dbstl::register_db_env() functions. Note that these 
functions are not necessary if the current thread called dbstl::open_db() or 
dbstl::open_env() for the handle that is being shared. This is because the open 
functions automatically register the handle for you.

Note that the get/set functions that provide access to container data members 
are not mutex-protected because these data members are supposed to be set only 
once at container object initialization. Applications wishing to modify them 
after initialization must supply their own protection.

While container objects can be shared between multiple threads, 
iterators and transactions can not be shared. 

When using environments/databases across threads, read: Registering database and environment handles

*/

CBerkeleyDBEnvManager::CBerkeleyDBEnvManager( void ) 
: CSingleton<CBerkeleyDBEnvManager>()
{
  m_pDbEnv = new DbEnv( 0 );
  // http://www.oracle.com/technology/documentation/berkeley-db/db/api_reference/CXX/frame_main.html
//  m_pDbEnv->open( ".", DB_INIT_CDB | DB_INIT_MPOOL | DB_THREAD | DB_INIT_TXN | DB_INIT_LOG | DB_CREATE | DB_RECOVER | DB_REGISTER, 0 );  // DB_INIT_LOCK
//  m_pDbEnv->open( ".", DB_INIT_CDB | DB_INIT_MPOOL | DB_THREAD | DB_INIT_TXN | DB_CREATE | DB_RECOVER | DB_REGISTER, 0 );  // DB_INIT_LOCK
  //m_pDbEnv->open( ".", DB_INIT_LOCK | DB_INIT_MPOOL | DB_THREAD | DB_INIT_TXN | DB_CREATE | DB_RECOVER | DB_REGISTER, 0 );  // works, but slow with transactioning
  m_pDbEnv->open( ".", DB_INIT_LOCK | DB_INIT_MPOOL | DB_THREAD | DB_CREATE, 0 );
  // dbstl::register_db() and dbstl::register_db_env()
  // dbstl::open_db() or dbstl::open_env()
  //dbstl::dbstl_startup();
}

CBerkeleyDBEnvManager::~CBerkeleyDBEnvManager( void ) {
  //dbstl::dbstl_exit();
//  m_pDbEnv->close( 0 ); // self deleting
//  delete m_pDbEnv;
  m_pDbEnv = NULL;
}
