// Copyright 2009 Minor Gordon.
// This source comes from the XtreemFS project. It is licensed under the GPLv2 (see COPYING for terms and conditions).

#ifndef _ORG_XTREEMFS_INTERFACES_TYPES_H_
#define _ORG_XTREEMFS_INTERFACES_TYPES_H_

#include "yield/platform.h"
#include <string>
#include <vector>


namespace org
{
  namespace xtreemfs
  {
    namespace interfaces
    {

      class StringSet : public YIELD::Sequence, public std::vector<std::string>
      {
      public:
        StringSet() { }
        StringSet( const std::string& first_value ) { std::vector<std::string>::push_back( first_value ); }
        StringSet( size_type size ) : std::vector<std::string>( size ) { }
        virtual ~StringSet() { }

        // YIELD::Object
        YIELD_OBJECT_PROTOTYPES( StringSet, 1001 );
        void marshal( YIELD::Marshaller& marshaller ) const { size_type value_i_max = size(); for ( size_type value_i = 0; value_i < value_i_max; value_i++ ) { marshaller.write( YIELD::Marshaller::Declaration( "value" ), ( *this )[value_i] ); } }
        void unmarshal( YIELD::Unmarshaller& unmarshaller ) { std::string value; unmarshaller.readString( YIELD::Unmarshaller::Declaration( "value" ), value ); push_back( value ); }

        // YIELD::Sequence
        size_t get_size() const { return size(); }
      };

      class UserCredentials : public YIELD::Object
      {
      public:
        UserCredentials() { }
        UserCredentials( const std::string& user_id, const org::xtreemfs::interfaces::StringSet& group_ids, const std::string& password ) : user_id( user_id ), group_ids( group_ids ), password( password ) { }
        UserCredentials( const char* user_id, size_t user_id_len, const org::xtreemfs::interfaces::StringSet& group_ids, const char* password, size_t password_len ) : user_id( user_id, user_id_len ), group_ids( group_ids ), password( password, password_len ) { }
        virtual ~UserCredentials() { }

        void set_user_id( const std::string& user_id ) { set_user_id( user_id.c_str(), user_id.size() ); }
        void set_user_id( const char* user_id, size_t user_id_len ) { this->user_id.assign( user_id, user_id_len ); }
        const std::string& get_user_id() const { return user_id; }
        void set_group_ids( const org::xtreemfs::interfaces::StringSet&  group_ids ) { this->group_ids = group_ids; }
        const org::xtreemfs::interfaces::StringSet& get_group_ids() const { return group_ids; }
        void set_password( const std::string& password ) { set_password( password.c_str(), password.size() ); }
        void set_password( const char* password, size_t password_len ) { this->password.assign( password, password_len ); }
        const std::string& get_password() const { return password; }

        bool operator==( const UserCredentials& other ) const { return user_id == other.user_id && group_ids == other.group_ids && password == other.password; }

        // YIELD::Object
        YIELD_OBJECT_PROTOTYPES( UserCredentials, 1002 );

        // YIELD::Object
        void marshal( YIELD::Marshaller& marshaller ) const { marshaller.write( YIELD::Marshaller::Declaration( "user_id" ), user_id ); marshaller.write( YIELD::Marshaller::Declaration( "group_ids" ), group_ids ); marshaller.write( YIELD::Marshaller::Declaration( "password" ), password ); }
        void unmarshal( YIELD::Unmarshaller& unmarshaller ) { unmarshaller.readString( YIELD::Unmarshaller::Declaration( "user_id" ), user_id ); unmarshaller.readSequence( YIELD::Unmarshaller::Declaration( "group_ids" ), &group_ids ); unmarshaller.readString( YIELD::Unmarshaller::Declaration( "password" ), password ); }

      protected:
        std::string user_id;
        org::xtreemfs::interfaces::StringSet group_ids;
        std::string password;
      };



    };



  };



};
#endif
