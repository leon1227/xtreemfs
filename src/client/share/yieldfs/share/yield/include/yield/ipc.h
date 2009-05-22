// Copyright 2003-2008 Minor Gordon, with original implementations and ideas contributed by Felix Hupfeld.
// This source comes from the Yield project. It is licensed under the GPLv2 (see COPYING for terms and conditions).

#ifndef YIELD_IPC_H
#define YIELD_IPC_H

#include "yield/arch.h"


#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__MACH__)
#define YIELD_HAVE_FREEBSD_KQUEUE 1
struct kevent;
#elif defined(__linux__)
#define YIELD_HAVE_LINUX_EPOLL 1
struct epoll_event;
#elif defined(YIELD_HAVE_SOLARIS_EVENT_PORTS)
struct port_event;
typedef port_event port_event_t;
#endif

#if defined(_WIN32)
struct fd_set;
#elif !defined(YIELD_HAVE_QUEUE) && !defined(YIELD_HAVE_LINUX_EPOLL) && !defined(YIELD_HAVE_SOLARIS_EVENT_PORTS)
#include <poll.h>
#endif

#ifdef YIELD_HAVE_OPENSSL
#include <openssl/ssl.h>
#endif
#ifdef YIELD_HAVE_ZLIB
#ifdef _WIN32
#undef ZLIB_WINAPI // So zlib doesn't #include windows.h
#endif
#include "zlib.h"
#endif


struct addrinfo;
struct sockaddr;
struct sockaddr_storage;
struct UriUriStructA;
struct yajl_gen_t;
typedef struct yajl_gen_t* yajl_gen;


#define YIELD_IPC_RFC822_HEADERS_STACK_BUFFER_LENGTH 128
#define YIELD_IPC_RFC822_HEADERS_STACK_IOVECS_LENGTH 32


namespace YIELD
{
  class FDEventQueue;
  class FDAndInternalEventQueue;
  class ONCRPCRecordInputStream;
  class Socket;
  class SocketAddress;
  class SocketFactory;
#ifdef YIELD_HAVE_OPENSSL
  class SSLContext;
  class SSLSocket;
#endif
  class TCPSocket;
  class TCPSocketFactory;
  class TimerEvent;
  class URI;


  template <class ProtocolRequestType, class ProtocolResponseType>
  class Client
  {
  public:    
    const static uint64_t OPERATION_TIMEOUT_DEFAULT = 30 * NS_IN_S;
    const static uint8_t RECONNECT_TRIES_MAX_DEFAULT = UINT8_MAX;


    auto_Object<Log> get_log() const { return log; }
    const Time& get_operation_timeout() const { return operation_timeout; }
    uint8_t get_reconnect_tries_max() const { return reconnect_tries_max; }
    auto_Object<SocketFactory> get_socket_factory() const { return socket_factory; }

  protected:
    Client( auto_Object<FDAndInternalEventQueue> fd_event_queue, auto_Object<Log> log, const Time& operation_timeout, auto_Object<SocketAddress> peer_sockaddr, uint8_t reconnect_tries_max, auto_Object<SocketFactory> socket_factory );
    virtual ~Client();

    void handleEvent( Event& );

    virtual auto_Object<ProtocolRequestType> createProtocolRequest( auto_Object<Request> request ) = 0;
    virtual auto_Object<ProtocolResponseType> createProtocolResponse( auto_Object<ProtocolRequestType> protocol_request ) = 0;
    virtual void respond( auto_Object<ProtocolRequestType> protocol_request, auto_Object<ProtocolResponseType> protocol_response ) = 0;
    virtual void respond( auto_Object<ProtocolRequestType> protocol_request, auto_Object<ExceptionResponse> exception_response ) = 0;

  private:
    auto_Object<FDAndInternalEventQueue> fd_event_queue;
    auto_Object<Log> log;
    Time operation_timeout;
    auto_Object<SocketAddress> peer_sockaddr;
    uint8_t reconnect_tries_max;
    auto_Object<SocketFactory> socket_factory;    



    class Connection : public Object, public InputStream, public OutputStream
    {
    public:
      enum State { IDLE = 0, CONNECTING = 1, WRITING = 2, READING = 3 };

      Connection( auto_Object<Socket> _socket, uint8_t reconnect_tries_max );

      const Time& get_last_activity_time() const { return last_activity_time; }
      auto_Object<ProtocolRequestType> get_protocol_request() const { return protocol_request; }
      inline State get_state() const { return state; }
      void set_protocol_request( auto_Object<ProtocolRequestType> protocol_request ) { this->protocol_request = protocol_request; }
      auto_Object<ProtocolResponseType> get_protocol_response() const { return protocol_response; }      
      auto_Object<Socket> get_socket() const { return _socket; }
      uint8_t get_reconnect_tries_left() const { return reconnect_tries_left; }
      void set_protocol_response( auto_Object<ProtocolResponseType> protocol_response ) { this->protocol_response = protocol_response; }
      void set_reconnect_tries_left( uint8_t reconnect_tries_left ) { this->reconnect_tries_left = reconnect_tries_left; }
      inline void set_state( State state ) { this->state = state; }

      bool operator==( const Connection& other ) const { return _socket == other._socket; }

      bool close();
      Stream::Status connect( auto_Object<SocketAddress> connect_to_sockaddr );
      bool shutdown();
      void touch() { last_activity_time = Time(); }

      // Object
      YIELD_OBJECT_PROTOTYPES( Connection, 201 );

      // InputStream
      YIELD_INPUT_STREAM_PROTOTYPES;

      // OutputStream
      YIELD_OUTPUT_STREAM_PROTOTYPES;

    private:
      ~Connection() { }

      auto_Object<Socket> _socket;
      uint8_t reconnect_tries_left;

      State state;
      Time last_activity_time;

      auto_Object<ProtocolRequestType> protocol_request;
      auto_Object<ProtocolResponseType> protocol_response;
    };


    std::vector<Connection*> connections;
    Connection* createConnection();
    void destroyConnection( Connection* );
  };


  class FDEvent : public Event
  {
  public:
    FDEvent( Object* context, uint32_t error_code, bool _want_read )
      : context( context ), error_code( error_code ), _want_read( _want_read )
    { }

    inline Object* get_context() const { return context; }
    inline uint32_t get_error_code() const { return error_code; }
    inline bool want_read() const { return _want_read; }

    // Object
    YIELD_OBJECT_PROTOTYPES( FDEvent, 202 );

  private:
    ~FDEvent() { }

    Object* context;
    uint32_t error_code;
    bool _want_read;
  };


  class FDEventQueue : public EventQueue
  {
  public:
    FDEventQueue();

#ifdef _WIN32
    bool attach( unsigned int _socket, Object* context, bool enable_read = true, bool enable_write = false );
    void detach( unsigned int _socket );
    bool toggle( unsigned int _socket, Object* context, bool enable_read, bool enable_write );
#else
    bool attach( int fd, Object* context, bool enable_read = true, bool enable_write = false );
    void detach( int fd );
    bool toggle( int fd, Object* context, bool enable_read, bool enable_write );
#endif

    void signal();
    auto_Object<TimerEvent> timer_create( const Time& timeout, auto_Object<> context = NULL ) { return timer_create( timeout, Time( static_cast<uint64_t>( 0 ) ), context ); }
    auto_Object<TimerEvent> timer_create( const Time& timeout, const Time& period, auto_Object<> context = NULL );

    // Object
    YIELD_OBJECT_PROTOTYPES( FDEventQueue, 203 );

    // EventQueue
    virtual bool enqueue( Event& ); // Discards events
    virtual Event* dequeue();
    virtual Event* dequeue( uint64_t timeout_ns );

  protected:
    virtual ~FDEventQueue();

  private:
#ifdef YIELD_HAVE_LINUX_EVENTFD
    auto_Object<File> signal_read_end, signal_write_end;
#else
    auto_Object<TCPSocket> signal_read_end, signal_write_end;
#endif
    int active_fds;

#if defined(YIELD_HAVE_LINUX_EPOLL) || defined(YIELD_HAVE_FREEBSD_KQUEUE) || defined(YIELD_HAVE_SOLARIS_EVENT_PORTS)
    int poll_fd;
#endif

#if defined(YIELD_HAVE_LINUX_EPOLL)
    struct epoll_event* returned_events;
#elif defined(YIELD_HAVE_FREEBSD_KQUEUE)
    struct kevent* returned_events;
#elif defined(YIELD_HAVE_SOLARIS_EVENT_PORTS)
    port_event_t* returned_events;
#else
    STLHashMap<Object*> fd_to_context_map;
#ifdef _WIN32
    fd_set *read_fds, *read_fds_copy, *write_fds, *write_fds_copy, *except_fds, *except_fds_copy;
    STLHashMap<Object*>::iterator next_fd_to_check;
#else
    typedef std::vector<pollfd> pollfd_vector;
    pollfd_vector::size_type next_pollfd_to_check;
    pollfd_vector pollfds;
#endif
#endif

    std::vector<TimerEvent*> timers;

    void clearSignal();
    FDEvent* dequeueFDEvent();
    TimerEvent* dequeueTimerEvent();
    int poll();
    int poll( uint64_t timeout_ns );
  };


  class FDAndInternalEventQueue : public FDEventQueue, private NonBlockingFiniteQueue<Event*, 2048>
  {
  public:
    FDAndInternalEventQueue();

    // Object
    YIELD_OBJECT_PROTOTYPES( FDAndInternalEventQueue, 204 );

    // EventQueue
    Event* dequeue();
    Event* dequeue( uint64_t timeout_ns );
    bool enqueue( Event& );
    Event* try_dequeue();    

  private:
    ~FDAndInternalEventQueue() { }

    bool dequeue_blocked;
  };


  class Server
  {
  protected:
    template <class ProtocolRequestType>
    class ProtocolRequestReader : public EventHandler
    {
    public:
      // Object
      YIELD_OBJECT_PROTOTYPES( EventHandler, 0 );    

      // EventHandler
      void handleEvent( Event& );

    protected:      
      ProtocolRequestReader( auto_Object<FDAndInternalEventQueue> fd_event_queue, auto_Object<Log> log )
        : log( log ), fd_event_queue( fd_event_queue )
      { }

      virtual ~ProtocolRequestReader() { }


      auto_Object<Log> log;


      virtual auto_Object<ProtocolRequestType> createProtocolRequest( auto_Object<Socket> _socket ) = 0;
      virtual bool sendProtocolRequest( auto_Object<ProtocolRequestType> protocol_request ) = 0;

    private:
      auto_Object<FDAndInternalEventQueue> fd_event_queue;
    };


    template <class ProtocolResponseType>
    class ProtocolResponseWriter : public EventHandler
    {
    public:
      ProtocolResponseWriter() { }

      // Object
      YIELD_OBJECT_PROTOTYPES( EventHandler, 0 );

      // EventHandler
      void handleEvent( Event& );

    protected:
      virtual ~ProtocolResponseWriter() { }
    };
  };


  class RFC822Headers
  {
  public:
    RFC822Headers();
    virtual ~RFC822Headers();

    Stream::Status deserialize( InputStream& input_stream, size_t* out_bytes_read );
    char* get_header( const char* header_name, const char* default_value="" );
    char* operator[]( const char* header_name ) { return get_header( header_name ); }
    // void set_header( const char* header, size_t header_len ); // Mutable header with name: value in one string, will copy both
    void set_header( const char* header_name, const char* header_value ); // Literal header
    void set_header( const char* header_name, char* header_value ); // Mutable header, will copy value
    void set_header( char* header_name, char* header_value ); // Mutable name and mutable value, will copy both
    void set_header( const std::string& header_name, const std::string& header_value ); // Mutable name and mutable value, will copy both
    Stream::Status serialize( OutputStream& output_stream, size_t* out_bytes_written );

  protected:
    void copy_iovec( const char* data, size_t len );
    void set_iovec( const char* data, size_t len ) { struct iovec _iovec; _iovec.iov_base = const_cast<char*>( data ); _iovec.iov_len = len; set_iovec( _iovec ); }
    void set_iovec( const struct iovec& out_iovec );

  private:
    enum { DESERIALIZING_LEADING_WHITESPACE, DESERIALIZING_HEADER_NAME, DESERIALIZING_HEADER_NAME_VALUE_SEPARATOR, DESERIALIZING_HEADER_VALUE, DESERIALIZING_HEADER_VALUE_TERMINATOR, DESERIALIZING_TRAILING_CRLF, DESERIALIZE_DONE } deserialize_state;
    char stack_buffer[YIELD_IPC_RFC822_HEADERS_STACK_BUFFER_LENGTH], *heap_buffer, *buffer_p;
    size_t heap_buffer_len;
    iovec stack_iovecs[YIELD_IPC_RFC822_HEADERS_STACK_IOVECS_LENGTH], *heap_iovecs; uint8_t iovecs_filled;

    inline void advanceBufferPointer()
    {
      buffer_p++;

      if ( heap_buffer == NULL )
      {
        if ( buffer_p - stack_buffer < YIELD_IPC_RFC822_HEADERS_STACK_BUFFER_LENGTH )
          return;
        else
          allocateHeapBuffer();
      }
      else if ( static_cast<size_t>( buffer_p - heap_buffer ) < heap_buffer_len )
        return;
      else
        allocateHeapBuffer();
    }

    void allocateHeapBuffer();
  };


  class HTTPRequest : public Request, public RFC822Headers
  {
  public:
    HTTPRequest(); // Incoming
    HTTPRequest( const char* method, const char* relative_uri, const char* host, auto_Object<> body = NULL ); // Outgoing
    HTTPRequest( const char* method, const URI& absolute_uri, auto_Object<> body = NULL ); // Outgoing

    uint8_t get_http_version() const { return http_version; }
    const char* get_method() const { return method; }
    const char* get_uri() const { return uri; }
    auto_Object<> get_body() const { return body; }

    virtual bool respond( uint16_t status_code );
    virtual bool respond( uint16_t status_code, auto_Object<Object> body );
    virtual bool respond( Response& response ) { return Request::respond( response ); }

    // Object
    YIELD_OBJECT_PROTOTYPES( HTTPRequest, 205 );
    Stream::Status deserialize( InputStream&, size_t* out_bytes_read = NULL );
    Stream::Status serialize( OutputStream&, size_t* out_bytes_written = NULL );

  protected:
    virtual ~HTTPRequest();

  private:
    HTTPRequest( const HTTPRequest& ) { DebugBreak(); } // Prevent copying

    void init( const char* method, const char* relative_uri, const char* host, auto_Object<> body );

    char method[16];
    char* uri; size_t uri_len;
    uint8_t http_version;
    auto_Object<> body;

    enum { DESERIALIZING_METHOD, DESERIALIZING_URI, DESERIALIZING_HTTP_VERSION, DESERIALIZING_HEADERS, DESERIALIZING_BODY, DESERIALIZE_DONE } deserialize_state;
    enum { SERIALIZING_METHOD, SERIALIZING_METHOD_URI_SEPARATOR, SERIALIZING_URI, SERIALIZING_HTTP_VERSION, SERIALIZING_HEADERS, SERIALIZING_BODY, SERIALIZE_DONE } serialize_state;
  };


  class HTTPResponse : public Response, protected RFC822Headers
  {
  public:
    HTTPResponse(); // Incoming
    HTTPResponse( uint16_t status_code ); // Outgoing
    HTTPResponse( uint16_t status_code, auto_Object<> body ); // Outgoing

    auto_Object<> get_body() const { return body; }
    uint8_t get_http_version() const { return http_version; }
    uint16_t get_status_code() const { return status_code; }
    void set_body( auto_Object<> body ) { this->body = body; }
    void set_status_code( uint16_t status_code ) { this->status_code = status_code; }

    // Object
    YIELD_OBJECT_PROTOTYPES( HTTPResponse, 206 );
    Stream::Status deserialize( InputStream&, size_t* out_bytes_read = NULL );
    Stream::Status serialize( OutputStream&, size_t* out_bytes_written = NULL );

  protected:
    virtual ~HTTPResponse() { }

  private:
    HTTPResponse( const HTTPResponse& ) { DebugBreak(); } // Prevent copying

    uint8_t http_version;
    union { char status_code_str[4]; uint16_t status_code; };
    auto_Object<> body;

    enum { DESERIALIZING_HTTP_VERSION, DESERIALIZING_STATUS_CODE, DESERIALIZING_REASON, DESERIALIZING_HEADERS, DESERIALIZING_BODY, DESERIALIZE_DONE } deserialize_state;
    enum { SERIALIZING_STATUS_LINE, SERIALIZING_HEADERS, SERIALIZING_BODY, SERIALIZE_DONE } serialize_state;
  };


  class HTTPClient : public EventHandler, public Client<HTTPRequest, HTTPResponse>
  {
  public:
    static auto_Object<HTTPClient> create( const URI& absolute_uri, auto_Object<StageGroup> stage_group, auto_Object<Log> log = NULL, const Time& operation_timeout = OPERATION_TIMEOUT_DEFAULT, uint8_t reconnect_tries_max = RECONNECT_TRIES_MAX_DEFAULT );

    static auto_Object<HTTPResponse> GET( const URI& absolute_uri, auto_Object<Log> log = NULL );
    static auto_Object<HTTPResponse> PUT( const URI& absolute_uri, auto_Object<> body, auto_Object<Log> log = NULL );
    static auto_Object<HTTPResponse> PUT( const URI& absolute_uri, const Path& body_file_path, auto_Object<Log> log = NULL );

    // Object
    YIELD_OBJECT_PROTOTYPES( HTTPClient, 207 );

    // EventHandler
    virtual void handleEvent( Event& ev ) { Client<HTTPRequest, HTTPResponse>::handleEvent( ev ); }

  private:
    HTTPClient( auto_Object<FDAndInternalEventQueue> fd_event_queue, auto_Object<Log> log, const Time& operation_timeout, auto_Object<SocketAddress> peer_sockaddr, uint8_t reconnect_tries_max, auto_Object<SocketFactory> socket_factory );
    virtual ~HTTPClient() { }

    static auto_Object<HTTPResponse> sendHTTPRequest( const char* method, const YIELD::URI& uri, auto_Object<> body, auto_Object<Log> log );

    // Client
    virtual auto_Object<HTTPRequest> createProtocolRequest( auto_Object<Request> request );
    virtual auto_Object<HTTPResponse> createProtocolResponse( auto_Object<HTTPRequest> http_request );
    virtual void respond( auto_Object<HTTPRequest> http_request, auto_Object<HTTPResponse> http_response );
    virtual void respond( auto_Object<HTTPRequest> http_request, auto_Object<ExceptionResponse> exception_response );
  };
    

  class HTTPServer : private Server
  {
  public:
    template <class StageGroupType> 
    static bool create( auto_Object<EventTarget> http_request_target, 
                        auto_Object<StageGroupType> stage_group,
                        auto_Object<SocketAddress> local_sockaddr = NULL, // Defaults to *:80
                        auto_Object<Log> log = NULL );

#ifdef YIELD_HAVE_OPENSSL
    template <class StageGroupType> 
    static bool create( auto_Object<EventTarget> http_request_target, 
                        auto_Object<SSLContext> ssl_context,      
                        auto_Object<StageGroupType> stage_group,
                        auto_Object<SocketAddress> local_sockaddr = NULL, // Defaults to *:443
                        auto_Object<Log> log = NULL );
#endif

  protected:
    class HTTPRequest : public YIELD::HTTPRequest
    {
    public:
      HTTPRequest( auto_Object<Stage> http_response_writer_stage, auto_Object<Socket> tcp_socket )
        : http_response_writer_stage( http_response_writer_stage ), tcp_socket( tcp_socket )
      { }

      auto_Object<Socket> get_socket() const { return tcp_socket; }

      // HTTPRequest
      bool respond( uint16_t status_code )
      {
        return http_response_writer_stage->send( *( new HTTPServer::HTTPResponse( status_code, tcp_socket ) ) );
      }

      bool respond( uint16_t status_code, auto_Object<> body )
      {
        return http_response_writer_stage->send( *( new HTTPServer::HTTPResponse( status_code, body, tcp_socket ) ) );
      }

      bool respond( Response& ) { DebugBreak(); return false; }

    private:
      auto_Object<Stage> http_response_writer_stage;
      auto_Object<Socket> tcp_socket;
    };


    class HTTPRequestReader : public ProtocolRequestReader<HTTPRequest>
    {
    public:
      HTTPRequestReader( auto_Object<FDAndInternalEventQueue> fd_event_queue, auto_Object<EventTarget> http_request_target, auto_Object<Stage> http_response_writer_stage )
        : ProtocolRequestReader<HTTPRequest>( fd_event_queue, NULL ), http_request_target( http_request_target ), http_response_writer_stage( http_response_writer_stage )

      { }

      // Object
      YIELD_OBJECT_PROTOTYPES( HTTPRequestReader, 0 );    

    private:
      ~HTTPRequestReader() { }

      auto_Object<EventTarget> http_request_target;
      auto_Object<Stage> http_response_writer_stage;

      // ProtocolRequestReader
      auto_Object<HTTPRequest> createProtocolRequest( auto_Object<Socket> _socket )
      {
        return new HTTPRequest( http_response_writer_stage, _socket );
      }

      bool sendProtocolRequest( auto_Object<HTTPRequest> http_request )
      {
        return http_request_target->send( *http_request.release() );
      }
    };


    class HTTPResponse : public YIELD::HTTPResponse
    {
    public:
      HTTPResponse( uint16_t status_code, auto_Object<Socket> _socket )
        : YIELD::HTTPResponse( status_code ), tcp_socket( tcp_socket )
      { }

      HTTPResponse( uint16_t status_code, auto_Object<> body, auto_Object<Socket> tcp_socket )
        : YIELD::HTTPResponse( status_code, body ), tcp_socket( tcp_socket )
      { }

      auto_Object<Socket> get_socket() const { return tcp_socket; }

    private:
      ~HTTPResponse() { }

      auto_Object<Socket> tcp_socket;
    };


    typedef ProtocolResponseWriter<HTTPResponse> HTTPResponseWriter;
  };


  class JSONValue;

  class JSONInputStream : public StructuredInputStream
  {
  public:
    JSONInputStream( InputStream& underlying_input_stream );
    virtual ~JSONInputStream();

    // StructuredInputStream
    YIELD_STRUCTURED_INPUT_STREAM_PROTOTYPES;

  protected:
    JSONInputStream( const Declaration& root_decl, JSONValue& root_json_value );

  private:
    const Declaration* root_decl;
    JSONValue *root_json_value, *next_json_value;

    void readSequence( Object& );
    void readMap( Object& );
    void readStruct( Object& );
    JSONValue* readJSONValue( const Declaration& decl );
  };


  class JSONOutputStream : public StructuredOutputStream
  {
  public:
    JSONOutputStream( OutputStream& underlying_output_stream, bool write_empty_strings = true );
    virtual ~JSONOutputStream(); // If the stream is wrapped in map, sequence, etc. then the constructor will append the final } or [, so the underlying output stream should not be deleted before this object!

    JSONOutputStream& operator=( const JSONOutputStream& ) { return *this; }

    // StructuredOutputStream
    YIELD_STRUCTURED_OUTPUT_STREAM_PROTOTYPES;
    virtual void writePointer( const Declaration& decl, void* value );

  protected:
    JSONOutputStream( OutputStream& underlying_output_stream, bool write_empty_strings, yajl_gen writer, const Declaration& root_decl );

    virtual void writeDeclaration( const Declaration& );
    virtual void writeSequence( Object* ); // Can be NULL for empty arrays
    virtual void writeMap( Object* ); // Can be NULL for empty maps
    virtual void writeStruct( Object* );

  private:
    OutputStream& underlying_output_stream;
    bool write_empty_strings;

    const Declaration* root_decl; // Mostly for debugging, also used to indicate if this is the root JSONOutputStream
    yajl_gen writer;
    bool in_map;

    void flushYAJLBuffer();
  };
 

  class ONCRPCMessage
  {
  public:
    auto_Object<> get_body() const { return body; }
    uint32_t get_xid() const { return xid; }

  protected:
    ONCRPCMessage( uint32_t xid, auto_Object<Interface> _interface, auto_Object<> body, auto_Object<Log> log )
      : xid( xid ), _interface( _interface ), body( body ), log( log )
    {
      oncrpc_record_input_stream = NULL;
    }

    virtual ~ONCRPCMessage();

    ONCRPCRecordInputStream& get_oncrpc_record_input_stream( InputStream& underlying_input_stream );

    uint32_t xid;
    auto_Object<Interface> _interface;
    auto_Object<> body;
    auto_Object<Log> log;

  private:
    ONCRPCRecordInputStream* oncrpc_record_input_stream;
  };


  class ONCRPCRequest : public Request, public ONCRPCMessage
  {
  public:
    const static uint32_t AUTH_NONE = 0;

    ONCRPCRequest( auto_Object<Interface> _interface, auto_Object<Log> log = NULL ); // Incoming
    ONCRPCRequest( uint32_t prog, uint32_t proc, uint32_t vers, auto_Object<> body, auto_Object<Log> log = NULL ); // Outgoing
    ONCRPCRequest( uint32_t prog, uint32_t proc, uint32_t vers, uint32_t credential_auth_flavor, auto_Object<> credential, auto_Object<> body, auto_Object<Log> log = NULL ); // Outgoing

    uint32_t get_credential_auth_flavor() const { return credential_auth_flavor; }
    auto_Object<> get_credential() const { return credential; }
    uint32_t get_prog() const { return prog; }
    uint32_t get_proc() const { return proc; }
    uint32_t get_vers() const { return vers; }
    void set_credential_auth_flavor( uint32_t credential_auth_flavor ) { this->credential_auth_flavor = credential_auth_flavor; }
    void set_credential( auto_Object<> credential ) { this->credential = credential; }

    // Object
    YIELD_OBJECT_PROTOTYPES( ONCRPCRequest, 213 );
    Stream::Status deserialize( InputStream&, size_t* out_bytes_read = 0 );
    Stream::Status serialize( OutputStream&, size_t* out_bytes_read = 0 );

  protected:
    virtual ~ONCRPCRequest() { }

  private:
    uint32_t prog, proc, vers, credential_auth_flavor;
    auto_Object<> credential;
  };


  class ONCRPCResponse : public Response, public ONCRPCMessage
  {
  public:
    ONCRPCResponse( auto_Object<Interface> _interface, auto_Object<> body, auto_Object<Log> log = NULL ); // Incoming
    ONCRPCResponse( uint32_t xid, auto_Object<> body, auto_Object<Log> log = NULL ); // Outgoing

    // Object
    YIELD_OBJECT_PROTOTYPES( ONCRPCResponse, 208 );
    Stream::Status deserialize( InputStream&, size_t* out_bytes_read = 0 );
    Stream::Status serialize( OutputStream&, size_t* out_bytes_read = 0 );

  protected:
    virtual ~ONCRPCResponse() { }
  };


  template <class InterfaceType>
  class ONCRPCClient : public InterfaceType, public Client<ONCRPCRequest, ONCRPCResponse>
  {
  public:
    static auto_Object< ONCRPCClient<InterfaceType> > create( const URI& absolute_uri, 
                                                              auto_Object<StageGroup> stage_group, 
                                                              auto_Object<Log> log = NULL, 
                                                              const Time& operation_timeout = OPERATION_TIMEOUT_DEFAULT, 
                                                              uint8_t reconnect_tries_max = RECONNECT_TRIES_MAX_DEFAULT, 
                                                              auto_Object<SocketFactory> socket_factory = NULL );

    static auto_Object<Response> send( const URI& absolute_uri, auto_Object<Request> request, auto_Object<Log> log = NULL, const Time& operation_timeout = OPERATION_TIMEOUT_DEFAULT, uint8_t reconnect_tries_max = RECONNECT_TRIES_MAX_DEFAULT, auto_Object<SocketFactory> socket_factory = NULL )
    {
      auto_Object<StageGroup> stage_group = new SEDAStageGroup( "ONCRPCClient", 0, NULL, log );
      auto_Object<ONCRPCClient> oncrpc_client = ONCRPCClient<InterfaceType>::create( absolute_uri, stage_group, log, operation_timeout, reconnect_tries_max, socket_factory );
      auto_Object<ONCRPCRequest> oncrpc_request = new ONCRPCRequest( 0x20000000 + oncrpc_client->get_tag(), request->get_tag(), oncrpc_client->get_tag(), request->incRef(), log );
      auto_Object< OneSignalEventQueue< NonBlockingFiniteQueue<Event*, 16 > > > oncrpc_response_queue( new OneSignalEventQueue< NonBlockingFiniteQueue<Event*, 16 > > );
      request->set_response_target( oncrpc_response_queue->incRef() );
      oncrpc_request->set_response_target( oncrpc_response_queue->incRef() );
      static_cast<EventHandler&>( *oncrpc_client ).send( *oncrpc_request.release() );
      return static_cast<Response*>( oncrpc_response_queue->dequeue() ); // Dangerous
    }

    // EventHandler
    virtual void handleEvent( Event& ev ) { Client<ONCRPCRequest, ONCRPCResponse>::handleEvent( ev ); }

  protected:
    ONCRPCClient( auto_Object<FDAndInternalEventQueue> fd_event_queue, auto_Object<Log> log, const Time& operation_timeout, auto_Object<SocketAddress> peer_sockaddr, uint8_t reconnect_tries_max, auto_Object<SocketFactory> socket_factory )
      : Client<ONCRPCRequest, ONCRPCResponse>( fd_event_queue, log, operation_timeout, peer_sockaddr, reconnect_tries_max, socket_factory )
    { }

    virtual ~ONCRPCClient() { }

    // Client
    virtual auto_Object<ONCRPCRequest> createProtocolRequest( auto_Object<Request> request )
    {
      if ( request->get_tag() == YIELD_OBJECT_TAG( ONCRPCRequest ) )
        return static_cast<ONCRPCRequest*>( request.release() );
      else
        return new ONCRPCRequest( 0x20000000 + InterfaceType::get_tag(), request->get_tag(), InterfaceType::get_tag(), request.release(), get_log() );
    }

    virtual auto_Object<ONCRPCResponse> createProtocolResponse( auto_Object<ONCRPCRequest> oncrpc_request )
    {
      auto_Object<Response> response = InterfaceType::createResponse( static_cast<Request*>( oncrpc_request->get_body().get() )->get_tag() );
      if ( response != NULL )
        return new ONCRPCResponse( this->incRef(), response.release(), get_log() );
      else
        return NULL;
    }

    virtual void respond( auto_Object<ONCRPCRequest> oncrpc_request, auto_Object<ONCRPCResponse> oncrpc_response )
    {
      static_cast<Request*>( oncrpc_request->get_body().get() )->respond( static_cast<Response*>( oncrpc_response->get_body().get() )->incRef() );
    }

    virtual void respond( auto_Object<ONCRPCRequest> oncrpc_request, auto_Object<ExceptionResponse> exception_response )
    {
      static_cast<Request*>( oncrpc_request->get_body().get() )->respond( *exception_response.release() );
    }
  };

  
  template <class InterfaceType>
  class ONCRPCServer : private Server
  {
  public:
    static bool create( auto_Object<SocketAddress> local_sockaddr, 
                        auto_Object<StageGroup> stage_group, 
                        auto_Object<Log> log = NULL, 
                        auto_Object<SocketFactory> socket_factory = NULL );

  private:
    class ONCRPCRequest : public YIELD::ONCRPCRequest
    {
    public:
      ONCRPCRequest( auto_Object<Interface> _interface, auto_Object<Log> log, auto_Object<Socket> _socket )
        : YIELD::ONCRPCRequest( _interface, log ), _socket( _socket )
      { }

      auto_Object<Log> get_log() const { return log; }
      auto_Object<Socket> get_socket() const { return _socket; }

    private:
      auto_Object<Socket> _socket;
    };


    class ONCRPCResponse : public YIELD::ONCRPCResponse
    {
    public:
      ONCRPCResponse( uint32_t xid, auto_Object<> body, auto_Object<Log> log, auto_Object<Socket> _socket )
        : YIELD::ONCRPCResponse( xid, body, log ), _socket( _socket )
      { }

      auto_Object<Socket> get_socket() const { return _socket; }

    private:
      ~ONCRPCResponse() { }

      auto_Object<Socket> _socket;
    };


    class ONCRPCResponder : public EventTarget
    {
    public:
      ONCRPCResponder( auto_Object<ONCRPCRequest> oncrpc_request, auto_Object<Stage> oncrpc_response_writer_stage )
        : oncrpc_request( oncrpc_request ), oncrpc_response_writer_stage( oncrpc_response_writer_stage )
      { }

      // Object    
      YIELD_OBJECT_PROTOTYPES( ONCRPCResponder, 0 );

      // EventTarget
      bool send( Event& ev )
      {
        ONCRPCResponse* oncrpc_response = new ONCRPCResponse( oncrpc_request->get_xid(), ev, oncrpc_request->get_log(), oncrpc_request->get_socket() );
        return oncrpc_response_writer_stage->send( *oncrpc_response );
      }

    private:
      auto_Object<ONCRPCRequest> oncrpc_request;
      auto_Object<Stage> oncrpc_response_writer_stage;
    };


    class ONCRPCRequestReader : public ProtocolRequestReader<ONCRPCRequest>
    {
    public:
      ONCRPCRequestReader( auto_Object<FDAndInternalEventQueue> fd_event_queue, auto_Object<Interface> _interface, auto_Object<Log> log, auto_Object<Stage> oncrpc_response_writer_stage )
        : ProtocolRequestReader<ONCRPCRequest>( fd_event_queue, log ), _interface( _interface ), oncrpc_response_writer_stage( oncrpc_response_writer_stage )
      { }

    private:
      ~ONCRPCRequestReader() { }

      auto_Object<Interface> _interface;
      auto_Object<Stage> oncrpc_response_writer_stage;

      // ProtocolRequestReader
      auto_Object<ONCRPCRequest> createProtocolRequest( auto_Object<Socket> _socket )
      {
        return new ONCRPCRequest( _interface, this->log, _socket );
      }

      bool sendProtocolRequest( auto_Object<ONCRPCRequest> oncrpc_request )
      {
        Request* body = static_cast<Request*>( oncrpc_request->get_body().release() ); // Dangerous
        body->set_response_target( new ONCRPCResponder( oncrpc_request, oncrpc_response_writer_stage ) );
        return _interface->send( *body );
      }
    };


    typedef ProtocolResponseWriter<ONCRPCResponse> ONCRPCResponseWriter;
  };


  class SocketAddress : public Object
  {
  public:
    SocketAddress();
    SocketAddress( uint16_t port, bool loopback = true ); // port is in host byte order, loopback = false => INADDR_ANY
    SocketAddress( const char* hostname );
    SocketAddress( const char* hostname, uint16_t port );
    SocketAddress( const URI& ); // URI ports should obviously be in host byte order    
    SocketAddress( const struct sockaddr_storage& );

#ifdef _WIN32
    bool as_struct_sockaddr( int family, struct sockaddr*& out_sockaddr, int32_t& out_sockaddrlen );
#else
    bool as_struct_sockaddr( int family, struct sockaddr*& out_sockaddr, uint32_t& out_sockaddrlen );
#endif
    bool getnameinfo( std::string& out_hostname, bool numeric = true ) const;
    bool getnameinfo( char* out_hostname, uint32_t out_hostname_len, bool numeric = true ) const;
    uint16_t get_port() const;
    bool operator==( const SocketAddress& ) const;
    bool operator!=( const SocketAddress& other ) const { return !operator==( other ); }

    // Object
    YIELD_OBJECT_PROTOTYPES( SocketAddress, 210 );

  private:
    SocketAddress( const SocketAddress& ) { DebugBreak(); } // Prevent copying
    ~SocketAddress();

    struct addrinfo* addrinfo_list; // Multiple sockaddr's obtained from getaddrinfo(3)
    struct sockaddr_storage* _sockaddr_storage; // A single sockaddr passed in the constructor and copied

    void init( const char* hostname, uint16_t port );
  };

  static inline std::ostream& operator<<( std::ostream& os, auto_Object<SocketAddress> sockaddr )
  {
    char nameinfo[1025];
    if ( sockaddr->getnameinfo( nameinfo, 1025, true ) )
      os << "[" << nameinfo << "]";
    else
      os << "[could not resolve socket address]";
    return os;
  }


  class Socket : public Event, public InputStream, public OutputStream
  {
  public:
    bool attach( auto_Object<FDEventQueue> to_fd_event_queue, Object* context );
    bool bind( auto_Object<SocketAddress> to_sockaddr );
    virtual Stream::Status connect( auto_Object<SocketAddress> to_sockaddr );
    virtual bool close();
    bool get_blocking_mode() const { return blocking_mode; }
    int get_domain() const { return domain; }
    auto_Object<Log> get_log() const { return log; }
    auto_Object<SocketAddress> getpeername();
    auto_Object<SocketAddress> getsockname();
#ifdef _WIN32
    operator unsigned int() const { return _socket; }
#else
    operator int() const { return _socket; }
#endif    
    bool operator==( const Socket& other ) const { return this->_socket == other._socket; }
    std::ostream& operator<<( std::ostream& os ) const { os << "socket #" << static_cast<int>( _socket ); return os; }
    OutputStream& operator<<( OutputStream& output_stream ) const { output_stream.write( "socket" ); return output_stream; }
    bool set_blocking_mode( bool blocking );
    virtual bool shutdown() { return true; }

    // Object
    YIELD_OBJECT_PROTOTYPES( Socket, 211 );

    // InputStream
    YIELD_INPUT_STREAM_PROTOTYPES;

    // OutputStream
    YIELD_OUTPUT_STREAM_PROTOTYPES;

  protected:
    Socket( int domain, int type, int protocol, auto_Object<Log> log ); // Creates a socket
#ifdef _WIN32
    Socket( int domain, int type, int protocol, unsigned int _socket, auto_Object<Log> log )
#else
    Socket( int domain, int type, int protocol, int _socket, auto_Object<Log> log )
#endif
      : log( log ), domain( domain ), type( type ), protocol( protocol ), _socket( _socket )
    { 
      blocking_mode = true;
    }

    virtual ~Socket()
    {
      close();
    }

    auto_Object<FDEventQueue> attached_to_fd_event_queue; Object* fd_event_queue_context;
    auto_Object<Log> log;

  private:
    Socket( const Socket& ) { DebugBreak(); } // Prevent copying

    int domain, type, protocol;
#ifdef _WIN32
    unsigned int _socket;
#else
    int _socket;
#endif

    bool blocking_mode;
  };


  class SocketFactory : public Object
  {
  public:
    virtual auto_Object<Socket> createSocket( auto_Object<Log> log = NULL ) = 0;

  protected:
    SocketFactory( auto_Object<Log> log = NULL )
      : log( log )
    { }

    virtual ~SocketFactory() { }

    auto_Object<Log> log;
  };


  class TCPListener : public EventHandler
  {
  public:
    static auto_Object<TCPListener> create( auto_Object<SocketAddress> local_sockaddr, auto_Object<EventTarget> reader_target, auto_Object<StageGroup> stage_group, auto_Object<TCPSocketFactory> tcp_socket_factory, auto_Object<Log> log = NULL );

    // Object
    YIELD_OBJECT_PROTOTYPES( TCPListener, 212 );

    // EventHandler
    void handleEvent( Event& );

  private:
    TCPListener( auto_Object<EventTarget> reader_target )
      : reader_target( reader_target )
    { }

    ~TCPListener() { }

    auto_Object<EventTarget> reader_target;
  };


  class TCPSocket : public Socket
  {
  public:
    TCPSocket( auto_Object<Log> log = NULL ); // Defaults to domain = AF_INET6

    virtual auto_Object<TCPSocket> accept();
    virtual bool listen();
    virtual bool shutdown();

    // Object
    YIELD_OBJECT_PROTOTYPES( TCPSocket, 213 );

    // OutputStream
    YIELD_OUTPUT_STREAM_PROTOTYPES;

  protected:
#ifdef _WIN32
    TCPSocket( int domain, unsigned int _socket, auto_Object<Log> log ); // Accepted socket
#else
    TCPSocket( int domain, int _socket, auto_Object<Log> log ); // Accepted socket
#endif

    virtual ~TCPSocket() { }

#ifdef _WIN32
    unsigned int _accept();
#else
    int _accept();
#endif

  private:
    size_t partial_write_len;
  };


  class TCPSocketFactory : public SocketFactory
  {
  public:
    TCPSocketFactory( auto_Object<Log> log = NULL )
      : SocketFactory( log )
    { }

    virtual auto_Object<TCPSocket> createTCPSocket( auto_Object<Log> log = NULL )
    {
      return new TCPSocket( log != NULL ? log : this->log );
    }

    // Object
    YIELD_OBJECT_PROTOTYPES( TCPSocketFactory, 214 );

    // SocketFactory
    auto_Object<Socket> createSocket( auto_Object<Log> log = NULL )
    {
      return new TCPSocket( log != NULL ? log : this->log );
    }

  protected:
    virtual ~TCPSocketFactory() { }
  };


#ifdef YIELD_HAVE_OPENSSL

  class SSLContext : public Object
	{
	public:
    SSLContext( SSL_METHOD* method = SSLv23_client_method() ); // No certificate
    SSLContext( SSL_METHOD* method, const Path& pem_certificate_file_path, const Path& pem_private_key_file_path, const std::string& pem_private_key_passphrase );
    SSLContext( SSL_METHOD* method, const std::string& pem_certificate_str, const std::string& pem_private_key_str, const std::string& pem_private_key_passphrase );
    SSLContext( SSL_METHOD* method, const Path& pkcs12_file_path, const std::string& pkcs12_passphrase );

    SSL_CTX* get_ssl_ctx() const { return ctx; }

    // Object
    YIELD_OBJECT_PROTOTYPES( SSLContext, 215 );

  private:
    ~SSLContext();

    std::string pem_private_key_passphrase;

    SSL_CTX* ctx;


    SSL_CTX* createSSL_CTX( SSL_METHOD* method );
    static int pem_password_callback( char *buf, int size, int, void *userdata );
    void throwOpenSSLException();
	};


  class SSLSocket : public TCPSocket
  {
  public:
    SSLSocket( auto_Object<SSLContext> ctx, auto_Object<Log> log = NULL );

    // Object
    YIELD_OBJECT_PROTOTYPES( SSLSocket, 216 );

    // InputStream
    YIELD_INPUT_STREAM_PROTOTYPES;

    // OutputStream
    YIELD_OUTPUT_STREAM_PROTOTYPES;

    // TCPSocket
    auto_Object<TCPSocket> accept();
    Stream::Status connect( auto_Object<SocketAddress> peer_sockaddr );
    bool shutdown();

  private:
    SSLSocket( int domain, SSL& ssl, auto_Object<Log> log );
    ~SSLSocket();


    auto_Object<SSLContext> ctx;
    SSL* ssl;
    unsigned char *write_buffer, *write_buffer_p; size_t write_buffer_len;


    static void info_callback( const SSL* ssl, int where, int ret );
    void init( auto_Object<Log> log );
    Stream::Status returnSSLStatus();
  };


  class SSLSocketFactory : public TCPSocketFactory
  {
  public:
    SSLSocketFactory( auto_Object<SSLContext> ssl_context, auto_Object<Log> log = NULL )
      : TCPSocketFactory( log ), ssl_context( ssl_context )
    { }

    // Object
    YIELD_OBJECT_PROTOTYPES( SSLSocketFactory, 217 );

    // SocketFactory
    auto_Object<Socket> createSocket( auto_Object<Log> log = NULL )
    {
      return new SSLSocket( ssl_context, log != NULL ? log : this->log );
    }

    // TCPSocketFactory
    auto_Object<TCPSocket> createTCPSocket( auto_Object<Log> log = NULL )
    {
      return new SSLSocket( ssl_context, log != NULL ? log : this->log );
    }
    
  private:
    ~SSLSocketFactory() { }

    auto_Object<SSLContext> ssl_context;
  };

#endif


  class TimerEvent : public Event
  {
  public:
    TimerEvent( const Time& timeout, const Time& period, auto_Object<> context = NULL )
      : context( context ), 
        fire_time( Time() + timeout ), 
        timeout( timeout ), period( period )
    { }

    auto_Object<> get_context() const { return context; }
    const Time& get_fire_time() const { return fire_time; }
    const Time& get_period() const { return period; }
    const Time& get_timeout() const { return timeout; }

    // Object
    YIELD_OBJECT_PROTOTYPES( TimerEvent, 218 );    

  private:
    ~TimerEvent() { }

    auto_Object<> context;
    Time fire_time, timeout, period;
  };


  class UDPSocket : public Socket
  {
  public:
    UDPSocket( auto_Object<Log> log = NULL );

//    bool joinMulticastGroup( auto_Object<SocketAddress> multicast_group_sockaddr, bool loopback );
//    bool leaveMulticastGroup( auto_Object<SocketAddress> multicast_group_sockaddr );

    // Object
    YIELD_OBJECT_PROTOTYPES( UDPSocket, 219 );

    // InputStream
    YIELD_INPUT_STREAM_PROTOTYPES;

  private:
    ~UDPSocket() { }
  };


  class UDPSocketFactory : public SocketFactory
  {
  public:
    UDPSocketFactory( auto_Object<Log> log = NULL )
      : SocketFactory( log )
    { }

    // Object
    YIELD_OBJECT_PROTOTYPES( UDPSocketFactory, 220 );

    // SocketFactory
    auto_Object<Socket> createSocket( auto_Object<Log> log = NULL )
    {
      return new UDPSocket( log != NULL ? log : this->log );
    }

  private:
    ~UDPSocketFactory() { }
  };


  class URI : public Object
  {
  public:
    // Factory methods return NULL instead of throwing exceptions
    static auto_Object<URI> parse( const char* uri ) { return parse( uri, strnlen( uri, UINT16_MAX ) ); }
    static auto_Object<URI> parse( const std::string& uri ) { return parse( uri.c_str(), uri.size() ); }
    static auto_Object<URI> parse( const char* uri, size_t uri_len );

    // Constructors throw exceptions
    URI( const char* uri ) { init( uri, strnlen( uri, UINT16_MAX ) ); }
    URI( const std::string& uri ) { init( uri.c_str(), uri.size() ); }
    URI( const char* uri, size_t uri_len ) { init( uri, uri_len ); }

    URI( const char* scheme, const char* host, uint16_t port ) // For testing
      : scheme( scheme ), host( host ), port( port ), resource( "/" )
    { }    

    URI( const char* scheme, const char* host, uint16_t port, const char* resource ) // For testing
      : scheme( scheme ), host( host ), port( port ), resource( resource )
    { }

    URI( const URI& other );
    virtual ~URI() { }

    const std::string& get_scheme() const { return scheme; }
    const std::string& get_host() const { return host; }
    const std::string& get_password() const { return password; }
    unsigned short get_port() const { return port; }
    const std::string& get_resource() const { return resource; }
    const std::string& get_user() const { return user; }
    void set_port( unsigned short port ) { this->port = port; }

    // Object
    YIELD_OBJECT_PROTOTYPES( URI, 221 );

  private:
    URI( UriUriStructA& parsed_uri )
    {
      init( parsed_uri );
    }

    void init( const char* uri, size_t uri_len );
    void init( UriUriStructA& parsed_uri );

    std::string scheme, user, password, host;
    unsigned short port;
    std::string resource;
  };


  static inline std::ostream& operator<<( std::ostream& os, const URI& uri )
  {
    os << uri.get_scheme();
    os << "://";
    os << uri.get_host();
    if ( uri.get_port() != 0 )
      os << ":" << uri.get_port();
    os << uri.get_resource();
    return os;
  }


#ifdef YIELD_HAVE_ZLIB
  class zlibOutputStream : public OutputStream
  {
  public:
    zlibOutputStream();

    auto_Object<String> serialize( String& s, int level = Z_BEST_COMPRESSION );

  private:
    z_stream zstream;
    char zout[4096]; size_t total_bytes_written;
    String* out_s;

    // OutputStream
    YIELD_OUTPUT_STREAM_PROTOTYPES;
  };
#endif


   template <class InterfaceType>
   auto_Object< ONCRPCClient<InterfaceType> > ONCRPCClient<InterfaceType>::create( const URI& absolute_uri, 
                                                                                   auto_Object<StageGroup> stage_group, 
                                                                                   auto_Object<Log> log, 
                                                                                   const Time& operation_timeout, 
                                                                                   uint8_t reconnect_tries_max, 
                                                                                   auto_Object<SocketFactory> socket_factory )
  {
    auto_Object<SocketAddress> peer_sockaddr = new SocketAddress( absolute_uri );
    if ( peer_sockaddr != NULL && peer_sockaddr->get_port() != 0 )
    {
      if ( socket_factory == NULL )
      {
#ifdef YIELD_HAVE_OPENSSL
        if ( absolute_uri.get_scheme() == "oncrpcs" )
          socket_factory = new SSLSocketFactory( new SSLContext( SSLv23_client_method() ) );
        else
#endif
          socket_factory = new TCPSocketFactory;
      }

      auto_Object<FDAndInternalEventQueue> fd_event_queue = new FDAndInternalEventQueue;
      auto_Object< ONCRPCClient<InterfaceType> > oncrpc_client = new ONCRPCClient<InterfaceType>( fd_event_queue, log, operation_timeout, peer_sockaddr, reconnect_tries_max, socket_factory );
      stage_group->createStage( oncrpc_client->incRef(), 1, fd_event_queue->incRef() );
      return oncrpc_client;
    }
    else
      return NULL;
  }


  template <class ProtocolRequestType>
  void Server::ProtocolRequestReader<ProtocolRequestType>::handleEvent( Event& ev )
  {
    auto_Object<Socket> _socket;
    ProtocolRequestType* protocol_request;

    switch ( ev.get_tag() )
    {
      case YIELD_OBJECT_TAG( FDEvent ):
      {
        FDEvent& fd_event = static_cast<FDEvent&>( ev );
        protocol_request = static_cast<ProtocolRequestType*>( fd_event.get_context() );
        _socket = protocol_request->get_socket();
        Object::decRef( ev );
      }
      break; // Drop down to try to deserialize

  #ifdef YIELD_HAVE_OPENSSL
      case YIELD_OBJECT_TAG( SSLSocket ):
  #endif
      case YIELD_OBJECT_TAG( TCPSocket ):
      case YIELD_OBJECT_TAG( UDPSocket ):
      {
        _socket = static_cast<Socket*>( &ev );
        protocol_request = createProtocolRequest( _socket ).release();
        _socket->attach( fd_event_queue->incRef(), protocol_request ); // Attach the original reference of protocol_request to the fd_event_queue
      }
      break; // Drop down to try to deserialize

      default: handleUnknownEvent( ev ); return;
    }

    _socket->set_blocking_mode( false );
    Stream::Status deserialize_status = protocol_request->deserialize( *_socket );

    switch ( deserialize_status )
    {
      case Stream::STREAM_STATUS_OK:
      {
        sendProtocolRequest( static_cast<ProtocolRequestType&>( protocol_request->incRef() ) );
      }
      break;

      case Stream::STREAM_STATUS_ERROR:
      {
        _socket->close(); // Force a detach
        Object::decRef( *protocol_request ); // The reference that's attached to fd_event_queue
        // tcp_socket should be dead here, since http_request should have had the last reference to it
      }
      break;

      default: break;
    }
  }


  template <class ProtocolResponseType>
  void Server::ProtocolResponseWriter<ProtocolResponseType>::handleEvent( Event& ev )
  {
    switch( ev.get_tag() )
    {
      case YIELD_OBJECT_TAG( ProtocolResponseType ):
      {
        ProtocolResponseType& protocol_response = static_cast<ProtocolResponseType&>( ev );
        auto_Object<Socket> _socket = protocol_response.get_socket();

        _socket->set_blocking_mode( true );
        protocol_response.serialize( *_socket );
        Object::decRef( protocol_response );
      }
      break;

      default: handleUnknownEvent( ev ); break;
    }
  }


  template <class InterfaceType>
  bool ONCRPCServer<InterfaceType>::create( auto_Object<SocketAddress> local_sockaddr, 
                                                   auto_Object<StageGroup> stage_group, 
                                                   auto_Object<Log> log, 
                                                   auto_Object<SocketFactory> socket_factory )
  {
    auto_Object<ONCRPCResponseWriter> oncrpc_response_writer = new ONCRPCResponseWriter;
    auto_Object<Stage> oncrpc_response_writer_stage = stage_group->createStage( oncrpc_response_writer->incRef(), 1, NULL, NULL, log );

    auto_Object<FDAndInternalEventQueue> fd_event_queue = new FDAndInternalEventQueue;
    auto_Object<ONCRPCRequestReader> oncrpc_request_reader = new ONCRPCRequestReader( fd_event_queue->incRef(), new InterfaceType, log, oncrpc_response_writer_stage );
    auto_Object<Stage> oncrpc_request_reader_stage = stage_group->createStage( oncrpc_request_reader->incRef(), 1, fd_event_queue->incRef(), NULL, log );

    if ( socket_factory != NULL && socket_factory->get_tag() == YIELD_OBJECT_TAG( UDPSocketFactory ) )
    {
      DebugBreak();
      return false;
    }
    else
    {    
      auto_Object<TCPListener> tcp_listener = TCPListener::create( local_sockaddr, oncrpc_request_reader_stage.release(), stage_group->incRef(), static_cast<TCPSocketFactory*>( socket_factory.release() ), log );
      return tcp_listener != NULL;    
    }
  }
};

#endif
