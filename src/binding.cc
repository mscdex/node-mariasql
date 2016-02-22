#if defined(_MSC_VER) && _MSC_VER < 1800
# define PRIi64 "I64i"
# define PRIu64 "I64u"
#else
# define __STDC_LIMIT_MACROS
# define __STDC_FORMAT_MACROS
# include <inttypes.h>
# include <stdint.h>
#endif

#ifndef _MSC_VER
# include <limits>
#else
# define strcasecmp _stricmp
#endif

#if defined(__clang__)
# ifndef __has_extension
#  define __has_extension __has_feature
# endif
# if __has_extension(cxx_nullptr)
#  define SUPPORTS_NULLPTR 1
# else
#  define SUPPORTS_NULLPTR 0
# endif
#else
# if defined(__GNUC__)
#  define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#  define GCC_VERSION_AT_LEAST(major, minor, patch) (GCC_VERSION >= (major * 10000 + minor * 100 + patch))
# else
#  define GCC_VERSION_AT_LEAST(major, minor, patch) 1 /* Assume new compiler */
# endif
# if GCC_VERSION_AT_LEAST(4, 6, 0)
#  define SUPPORTS_NULLPTR 1
# else
#  define SUPPORTS_NULLPTR 0
# endif
#endif
#if !SUPPORTS_NULLPTR
  const                        // this is a const object...
  class {
  public:
    template<class T>          // convertible to any type
      operator T*() const      // of null non-member
      { return 0; }            // pointer...
    template<class C, class T> // or any type of null
      operator T C::*() const  // member pointer...
      { return 0; }
  private:
    void operator&() const;    // whose address can't be taken
  } nullptr = {};              // and whose name is nullptr
#endif

#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>

// For Pre-VS2015
#include "snprintf.c"

#include <mysql.h>

using namespace node;
using namespace v8;

#define DEBUG 0


#if defined(DEBUG) && DEBUG
# define DBG_LOG(fmt, ...)                                                     \
    do { fprintf(stderr, "DEBUG: " fmt , ##__VA_ARGS__); } while (0)
#else
# define DBG_LOG(fmt, ...) (void(0))
#endif
#define FREE(p) if (p) { free(p); p = nullptr; }
#define IS_BINARY(f) ((f.flags & BINARY_FLAG) &&                               \
                      ((f.type == MYSQL_TYPE_TINY_BLOB)   ||                   \
                       (f.type == MYSQL_TYPE_MEDIUM_BLOB) ||                   \
                       (f.type == MYSQL_TYPE_BLOB)        ||                   \
                       (f.type == MYSQL_TYPE_LONG_BLOB)   ||                   \
                       (f.type == MYSQL_TYPE_STRING)      ||                   \
                       (f.type == MYSQL_TYPE_VAR_STRING)))
#define IS_DEAD_ERRNO(v) (v == 2006 || v == 2013 || v == 2055)
#define DEFAULT_CIPHER "ECDHE-RSA-AES128-SHA256:AES128-GCM-SHA256:RC4:HIGH"    \
                       ":!MD5:!aNULL:!EDH"
#define STATES                                                                 \
  X(CLOSED, 0)                                                                 \
  X(CONNECT, 1)                                                                \
  X(IDLE, 2)                                                                   \
  X(QUERY, 3)                                                                  \
  X(RESULT, 4)                                                                 \
  X(ROW, 5)                                                                    \
  X(NEXTRESULT, 6)                                                             \
  X(FREERESULT, 7)                                                             \
  X(STORERESULT, 8)                                                            \
  X(PING, 9)
#define EVENT_NAMES                                                            \
  X(connect)                                                                   \
  X(error)                                                                     \
  X(idle)                                                                      \
  X(resultinfo)                                                                \
  X(row)                                                                       \
  X(resultend)                                                                 \
  X(ping)                                                                      \
  X(close)
#define FIELD_TYPES                                                            \
  X(TINY, tiny, TINYINT)                                                       \
  X(SHORT, short, SMALLINT)                                                    \
  X(LONG, long, INTEGER)                                                       \
  X(INT24, int24, MEDIUMINT)                                                   \
  X(LONGLONG, big, BIGINT)                                                     \
  X(DECIMAL, dec, DECIMAL)                                                     \
  X(NEWDECIMAL, newdec, DECIMAL)                                               \
  X(FLOAT, float, FLOAT)                                                       \
  X(DOUBLE, double, DOUBLE)                                                    \
  X(BIT, bit, BIT)                                                             \
  X(TIMESTAMP, ts, TIMESTAMP)                                                  \
  X(DATE, date, DATE)                                                          \
  X(NEWDATE, newdate, DATE)                                                    \
  X(TIME, time, TIME)                                                          \
  X(DATETIME, dtime, DATETIME)                                                 \
  X(YEAR, year, YEAR)                                                          \
  X(STRING, char, CHAR)                                                        \
  X(VAR_STRING, vstr, VARCHAR)                                                 \
  X(VARCHAR, vchar, VARCHAR)                                                   \
  X(TINY_BLOB, tinyblob, TINYBLOB)                                             \
  X(MEDIUM_BLOB, medblob, MEDIUMBLOB)                                          \
  X(LONG_BLOB, lngblob, LONGBLOB)                                              \
  X(SET, set, SET)                                                             \
  X(ENUM, enum, ENUM)                                                          \
  X(GEOMETRY, geo, GEOMETRY)                                                   \
  X(NULL, null, NULL)
#define CFG_OPTIONS                                                            \
  X(user)                                                                      \
  X(password)                                                                  \
  X(host)                                                                      \
  X(port)                                                                      \
  X(unixSocket)                                                                \
  X(db)                                                                        \
  X(connTimeout)                                                               \
  X(secureAuth)                                                                \
  X(multiStatements)                                                           \
  X(compress)                                                                  \
  X(local_infile)                                                              \
  X(read_default_file)                                                         \
  X(read_default_group)                                                        \
  X(charset)                                                                   \
  X(tcpKeepalive)                                                              \
  X(tcpKeepaliveCnt)                                                           \
  X(tcpKeepaliveIntvl)                                                         \
  X(ssl)                                                                       \
  X(protocol)
#define CFG_OPTIONS_SSL                                                        \
  X(key)                                                                       \
  X(cert)                                                                      \
  X(ca)                                                                        \
  X(capath)                                                                    \
  X(cipher)                                                                    \
  X(rejectUnauthorized)

#ifdef _WIN32
# define CHECK_CONNRESET (WSAGetLastError() == WSAECONNRESET   ||              \
                          WSAGetLastError() == WSAENOTCONN     ||              \
                          WSAGetLastError() == WSAECONNABORTED ||              \
                          WSAGetLastError() == WSAENETRESET    ||              \
                          WSAGetLastError() == WSAENETDOWN)
#else
# include <errno.h>
# define CHECK_CONNRESET (errno == ECONNRESET || errno == ENOTCONN)
#endif

Nan::Persistent<FunctionTemplate> constructor;
//Nan::Persistent<FunctionTemplate> stmt_constructor;
Nan::Persistent<String> code_symbol;
Nan::Persistent<String> context_symbol;
Nan::Persistent<String> conncfg_symbol;
Nan::Persistent<String> neg_one_symbol;
char u64_buf[21];
char conn_check_buf[1];

#define X(state, val)                                                          \
const int STATE_##state = val;
STATES
#undef X

const char* state_strings[] = {
#define X(state, val)                                                          \
#state,
STATES
#undef X
};

#define X(name)                                                                \
Nan::Persistent<String> ev_##name##_symbol;
EVENT_NAMES
#undef X

#define X(suffix, abbr, literal)                                               \
Nan::Persistent<String> col_##abbr##_symbol;
FIELD_TYPES
#undef X
Nan::Persistent<String> col_unsup_symbol;

#define X(name)                                                                \
Nan::Persistent<String> cfg_##name##_symbol;
CFG_OPTIONS
CFG_OPTIONS_SSL
#undef X

struct sql_config {
  char* user;
  char* password;
  char* host;
  unsigned int port;
  char* unixSocket;
  char* db;
  unsigned long client_opts;
  unsigned int tcpka;
  unsigned int tcpkaCnt;
  unsigned int tcpkaIntvl;
  bool metadata;
  char* charset;

  // ssl
  char* ssl_key;
  char* ssl_cert;
  char* ssl_ca;
  char* ssl_capath;
  char* ssl_cipher;
};

const my_bool MY_BOOL_TRUE = 1;
const my_bool MY_BOOL_FALSE = 0;
const int PROTOCOL_TCP = MYSQL_PROTOCOL_TCP;
const int PROTOCOL_SOCKET = MYSQL_PROTOCOL_SOCKET;
const int PROTOCOL_PIPE = MYSQL_PROTOCOL_PIPE;
const int PROTOCOL_MEMORY = MYSQL_PROTOCOL_MEMORY;
const int PROTOCOL_DEFAULT = MYSQL_PROTOCOL_DEFAULT;

// ripped from libuv
#ifdef _WIN32
  int set_keepalive(SOCKET socket, int on, unsigned int delay) {
    if (setsockopt(socket,
                   SOL_SOCKET,
                   SO_KEEPALIVE,
                   (const char*)&on,
                   sizeof on) == -1) {
      return WSAGetLastError();
    }

    if (on && setsockopt(socket,
                         IPPROTO_TCP,
                         TCP_KEEPALIVE,
                         (const char*)&delay,
                         sizeof delay) == -1) {
      return WSAGetLastError();
    }

    return 0;
  }
  int set_keepalive_cnt(SOCKET socket, unsigned int count) {
#   ifdef TCP_KEEPCNT
    if (setsockopt(socket,
                   IPPROTO_TCP,
                   TCP_KEEPCNT,
                   (const char*)&count,
                   sizeof count) == -1) {
      return WSAGetLastError();
    }
#   endif
    return 0;
  }
  int set_keepalive_intvl(SOCKET socket, unsigned int intvl) {
#   ifdef TCP_KEEPINTVL
    if (setsockopt(socket,
                   IPPROTO_TCP,
                   TCP_KEEPINTVL,
                   (const char*)&intvl,
                   sizeof intvl) == -1) {
      return WSAGetLastError();
    }
#   endif
    return 0;
  }
#else
# include <errno.h>
  int set_keepalive(int fd, int on, unsigned int delay) {
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)))
      return -errno;

#   ifdef TCP_KEEPIDLE
    if (on && setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &delay, sizeof(delay)))
      return -errno;
#   endif

#   if defined(TCP_KEEPALIVE) && !defined(__sun)
    if (on && setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &delay, sizeof(delay)))
      return -errno;
#   endif
    return 0;
  }
  int set_keepalive_cnt(int fd, unsigned int count) {
#   ifdef TCP_KEEPCNT
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)))
      return -errno;
#   endif
    return 0;
  }
  int set_keepalive_intvl(int fd, unsigned int intvl) {
#   ifdef TCP_KEEPINTVL
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)))
      return -errno;
#   endif
    return 0;
  }
#endif

class Client : public Nan::ObjectWrap {
  public:
    Nan::Persistent<Object> context;
    uv_poll_t* poll_handle;
    uv_os_sock_t mysql_sock;
    MYSQL mysql;
    MYSQL* mysql_ret;
    sql_config config;
    bool initialized;
    bool is_cont;
    bool is_destructing;
    bool is_paused;
    bool is_buffering;
    Nan::Utf8String* cur_query;
    MYSQL_RES* cur_result;
    //MYSQL_STMT* cur_stmt;
    MYSQL_ROW cur_row;
    bool req_columns;
    bool need_columns;
    bool req_metadata;
    bool need_metadata;
    int state;
    int last_status;
    unsigned long threadId;
#define X(name)                                                                \
    Nan::Callback* on##name;
    EVENT_NAMES
#undef X

    Client() {
      DBG_LOG("Client()\n");
      state = STATE_CLOSED;

      is_destructing = false;
      initialized = false;
      threadId = 0;

#define X(name)                                                                \
      on##name = nullptr;
      EVENT_NAMES
#undef X
    }

    ~Client() {
      DBG_LOG("[%lu] ~Client()\n", threadId);
#define X(name)                                                                \
      if (on##name)                                                            \
        delete on##name;
      EVENT_NAMES
#undef X
      if (!context.IsEmpty())
        context.Reset();
      is_destructing = true;
      close();
    }

    bool init() {
      DBG_LOG("[%lu] init()\n", threadId);
      if (initialized)
        clear_state();

      poll_handle = nullptr;
      mysql_sock = 0;

      mysql_init(&mysql);
      if (mysql_options(&mysql, MYSQL_OPT_NONBLOCK, 0) != 0)
        return false;

      config.user = nullptr;
      config.password = nullptr;
      config.host = nullptr;
      config.port = 3306;
      config.unixSocket = nullptr;
      config.db = nullptr;
      config.client_opts = CLIENT_MULTI_RESULTS | CLIENT_REMEMBER_OPTIONS;
      config.tcpka = 0; // disabled by default
      config.tcpkaCnt = 0; // use system default
      config.tcpkaIntvl = 0; // use system default
      config.metadata = false;
      config.charset = nullptr;
      config.ssl_key = nullptr;
      config.ssl_cert = nullptr;
      config.ssl_ca = nullptr;
      config.ssl_capath = nullptr;
      config.ssl_cipher = nullptr;

      is_cont = false;

      is_paused = false;

      cur_query = nullptr;

      initialized = true;

      return true;
    }

    void clear_state() {
      DBG_LOG("clear_state()\n");
      FREE(config.user);
      FREE(config.password);
      FREE(config.host);
      FREE(config.unixSocket);
      FREE(config.db);
      FREE(config.ssl_key);
      FREE(config.ssl_cert);
      FREE(config.ssl_ca);
      FREE(config.ssl_capath);
      FREE(config.ssl_cipher);
      FREE(config.charset);

      if (cur_query) {
        delete cur_query;
        cur_query = nullptr;
      }
    }

    bool close(bool is_dead=false) {
      DBG_LOG("[%lu] close() state=%s,is_dead=%s,is_destructing=%s\n",
              threadId,
              state_strings[state],
              (is_dead ? "true" : "false"),
              (is_destructing ? "true" : "false"));
      initialized = false;

      clear_state();

      if (state != STATE_CLOSED || is_dead) {
        state = STATE_CLOSED;
        Unref();
        if (poll_handle) {
          if (is_destructing)
            uv_poll_stop(poll_handle);
          else if (is_dead)
            uv_close((uv_handle_t*)poll_handle, cb_close_dummy);
          else
            uv_close((uv_handle_t*)poll_handle, cb_close);
        }
        mysql_close(&mysql);
        return true;
      }
      return false;
    }

    bool connect() {
      DBG_LOG("[%lu] connect() state=%s\n", threadId, state_strings[state]);
      if (state == STATE_CLOSED) {
        Ref();
        state = STATE_CONNECT;
        do_work();
        return true;
      }
      return false;
    }

    bool query(Local<Value> qry, bool columns, bool metadata, bool buffer) {
      DBG_LOG("[%lu] query() state=%s,columns=%d,metadata=%d,buffer=%d,"
              "query=%s\n",
              threadId, state_strings[state], columns, metadata, buffer, qry);
      if (state == STATE_IDLE) {
        if (cur_query)
          delete cur_query;
        cur_query = new Nan::Utf8String(qry);
        req_columns = columns;
        req_metadata = metadata;
        is_buffering = buffer;
        state = STATE_QUERY;
        do_work();
        return true;
      }
      return false;
    }

    /*bool query(Statement* stmt, bool columns, bool metadata) {
      DBG_LOG("query(stmt) state=%s,columns=%d,metadata=%d\n",
              state_strings[state], columns, metadata);
      if (state == STATE_IDLE) {
        // lazy initialization
        if (!stmt->stmt || !stmt->stmt->mysql) {
          // libmariadbclient NULLs out the `mysql` property for all associated
          // prepared statements on disconnect
          if (stmt->stmt) {
            // normally this will block, but since the `mysql` handle is gone
            // it shouldn't do blocking network requests while doing any cleanup
            // that may still be needed ...
            mysql_stmt_close(stmt->stmt);
          }
          stmt->stmt = mysql_stmt_init(&mysql);
          if (!stmt->stmt) // out of memory
            return false;
        }
        cur_stmt = stmt;
        req_columns = columns;
        req_metadata = metadata;
        state = STATE_PREPARE;
        do_work();
        return true;
      }
      return false;
    }*/

    bool pause() {
      DBG_LOG("[%lu] pause() state=%s,is_paused=%d\n",
              threadId, state_strings[state], is_paused);
      if (state >= STATE_IDLE && !is_paused) {
        is_paused = true;
        return true;
      }
      return false;
    }

    bool resume() {
      DBG_LOG("[%lu] resume() state=%s,is_paused=%d\n",
              threadId, state_strings[state], is_paused);
      if (state == STATE_ROW && is_paused) {
        is_paused = false;
        do_work(last_status);
        return true;
      }
      return false;
    }

    bool ping() {
      DBG_LOG("[%lu] ping() state=%s\n", threadId, state_strings[state]);
      if (state == STATE_IDLE) {
        state = STATE_PING;
        do_work();
        return true;
      }
      return false;
    }

    uint64_t lastInsertId() {
      DBG_LOG("[%lu] lastInsertId() state=%s\n",
              threadId, state_strings[state]);

      return static_cast<uint64_t>(mysql_insert_id(&mysql));
    }

    unsigned long escape(const char* src, unsigned long src_len, char* dest) {
      return mysql_real_escape_string(&mysql, dest, src, src_len);
    }

    void do_work(int event = 0) {
      DBG_LOG("[%lu] do_work() state=%s,event=%s\n",
              threadId,
              state_strings[state],
              ((event & (UV_READABLE|UV_WRITABLE)) == (UV_READABLE|UV_WRITABLE)
               ? "READABLE,WRITABLE"
               : event & UV_READABLE
                 ? "READABLE"
                 : event & UV_WRITABLE
                   ? "WRITABLE"
                   : "NONE"));
      int status = 0;
      int new_events = 0;
      int err;
      bool done = false;

      if (state == STATE_CLOSED)
        return;
      else if (state == STATE_IDLE && event) {
        // Check for closed socket since we don't expect events if we are idle
        int r = recv(mysql_sock, conn_check_buf, 1, MSG_PEEK);
        if (r == 0 || (r == -1 && CHECK_CONNRESET)) {
          on_error(true, 2006, "MySQL server has gone away");
          return;
        }
      }

      while (!done) {
        DBG_LOG("[%lu] do_work() loop begin, state=%s,is_cont=%d\n",
                threadId, state_strings[state], is_cont);
        switch (state) {
          case STATE_CONNECT:
            if (!is_cont) {
              status = mysql_real_connect_start(&mysql_ret,
                                                &mysql,
                                                config.host,
                                                config.user,
                                                config.password,
                                                config.db,
                                                config.port,
                                                config.unixSocket,
                                                config.client_opts);
              if (!mysql_ret && mysql_errno(&mysql) > 0)
                return on_error(true);

              mysql_sock = mysql_get_socket(&mysql);

              if (config.tcpka > 0) {
                set_keepalive(mysql_sock, 1, config.tcpka);
                if (config.tcpkaCnt > 0)
                  set_keepalive_cnt(mysql_sock, config.tcpkaCnt);
                if (config.tcpkaIntvl > 0)
                  set_keepalive_intvl(mysql_sock, config.tcpkaIntvl);
              }

              if (!poll_handle)
                poll_handle = (uv_poll_t*)malloc(sizeof(uv_poll_t));
              uv_poll_init_socket(uv_default_loop(), poll_handle, mysql_sock);
              uv_poll_start(poll_handle, UV_READABLE, cb_poll);
              poll_handle->data = this;

              if (status) {
                done = true;
                is_cont = true;
              } else {
                state = STATE_IDLE;
                threadId = mysql_thread_id(&mysql);
                on_connect();
                return;
              }
            } else {
              status = mysql_real_connect_cont(&mysql_ret, &mysql, event);

              if (status)
                done = true;
              else {
                is_cont = false;
                if (!mysql_ret) {
                  on_error(true);
                  return;
                }
                state = STATE_IDLE;
                threadId = mysql_thread_id(&mysql);
                on_connect();
                return;
              }
            }
          break;
          case STATE_QUERY:
            if (!is_cont) {
              status = mysql_real_query_start(&err,
                                              &mysql,
                                              **cur_query,
                                              static_cast<unsigned long>(
                                                cur_query->length()
                                              ));
              if (status) {
                done = true;
                is_cont = true;
              } else {
                if (cur_query) {
                  delete cur_query;
                  cur_query = nullptr;
                }
                if (err) {
                  state = STATE_IDLE;
                  on_error();
                  if (state != STATE_CLOSED)
                    on_idle();
                } else {
                  if (is_buffering)
                    state = STATE_STORERESULT;
                  else
                    state = STATE_RESULT;
                }
              }
            } else {
              status = mysql_real_query_cont(&err, &mysql, event);
              if (status)
                done = true;
              else {
                is_cont = false;
                if (cur_query) {
                  delete cur_query;
                  cur_query = nullptr;
                }
                if (err) {
                  state = STATE_IDLE;
                  on_error();
                  if (state != STATE_CLOSED)
                    on_idle();
                } else {
                  if (is_buffering)
                    state = STATE_STORERESULT;
                  else
                    state = STATE_RESULT;
                }
              }
            }
          break;
          case STATE_RESULT:
            // we have a result and now we check for rows

            // make sure a pause for a previous result does not interfere with
            // future results/queries
            is_paused = false;

            cur_result = mysql_use_result(&mysql);
            if (!cur_result) {
              if (mysql_errno(&mysql))
                on_error();
              else {
                // this is needed for statements that do not return a result set
                // so that javascript land can at least emit a stream with the
                // appropriate query info attached ...
                on_resultend();
              }
              if (mysql_more_results(&mysql))
                state = STATE_NEXTRESULT;
              else {
                state = STATE_IDLE;
                on_idle();
                return;
              }
            } else {
              need_columns = req_columns;
              need_metadata = req_metadata;
              state = STATE_ROW;
            }
          break;
          case STATE_ROW:
            if (!is_cont) {
              status = mysql_fetch_row_start(&cur_row, cur_result);
              if (status) {
                done = true;
                is_cont = true;
              } else {
                if (mysql_errno(&mysql)) {
                  state = STATE_NEXTRESULT;
                  on_error();
                } else {
                  if (cur_row) {
                    on_row();
                    if (is_paused)
                      done = true;
                  } else {
                    // no more rows
                    state = STATE_FREERESULT;
                    on_resultend();
                  }
                }
              }
            } else {
              status = mysql_fetch_row_cont(&cur_row, cur_result, event);
              if (status)
                done = true;
              else {
                is_cont = false;
                if (mysql_errno(&mysql)) {
                  state = STATE_NEXTRESULT;
                  on_error();
                } else {
                  if (cur_row) {
                    on_row();
                    if (is_paused)
                      done = true;
                  } else {
                    // no more rows
                    state = STATE_FREERESULT;
                  }
                }
              }
            }
          break;
          case STATE_NEXTRESULT:
            // here we attempt to fetch another result for the current query
            if (!is_cont) {
              status = mysql_next_result_start(&err, &mysql);
              if (status) {
                done = true;
                is_cont = true;
              } else {
                if (err) {
                  on_error();
                  if (cur_result)
                    state = STATE_FREERESULT;
                  else {
                    state = STATE_IDLE;
                    on_idle();
                    return;
                  }
                } else {
                  if (is_buffering)
                    state = STATE_STORERESULT;
                  else
                    state = STATE_RESULT;
                }
              }
            } else {
              status = mysql_next_result_cont(&err, &mysql, event);
              if (status)
                done = true;
              else {
                is_cont = false;
                if (err) {
                  on_error();
                  if (cur_result)
                    state = STATE_FREERESULT;
                  else {
                    state = STATE_IDLE;
                    on_idle();
                    return;
                  }
                } else {
                  if (is_buffering)
                    state = STATE_STORERESULT;
                  else
                    state = STATE_RESULT;
                }
              }
            }
          break;
          case STATE_FREERESULT:
            if (!is_cont) {
              status = mysql_free_result_start(cur_result);
              if (status) {
                done = true;
                is_cont = true;
              } else {
                cur_result = nullptr;
                if (mysql_more_results(&mysql))
                  state = STATE_NEXTRESULT;
                else {
                  state = STATE_IDLE;
                  on_idle();
                  return;
                }
              }
            } else {
              status = mysql_free_result_cont(cur_result, event);
              if (status)
                done = true;
              else {
                cur_result = nullptr;
                is_cont = false;
                if (mysql_more_results(&mysql))
                  state = STATE_NEXTRESULT;
                else {
                  state = STATE_IDLE;
                  on_idle();
                  return;
                }
              }
            }
          break;
          case STATE_STORERESULT:
            if (!is_cont) {
              need_columns = req_columns;
              need_metadata = req_metadata;
              status = mysql_store_result_start(&cur_result, &mysql);
              if (status) {
                done = true;
                is_cont = true;
              } else {
                if (mysql_errno(&mysql))
                  on_error();
                else {
                  on_rows();
                  on_resultend();
                }
                state = STATE_FREERESULT;
              }
            } else {
              status = mysql_store_result_cont(&cur_result, &mysql, event);
              if (status)
                done = true;
              else {
                is_cont = false;
                if (mysql_errno(&mysql))
                  on_error();
                else {
                  on_rows();
                  on_resultend();
                }
                state = STATE_FREERESULT;
              }
            }
          break;
          case STATE_PING:
            if (!is_cont) {
              status = mysql_ping_start(&err, &mysql);
              if (status) {
                done = true;
                is_cont = true;
              } else {
                state = STATE_IDLE;
                if (err)
                  on_error(true);
                else
                  on_ping();
                //on_idle();
                return;
              }
            } else {
              status = mysql_ping_cont(&err, &mysql, event);
              if (status)
                done = true;
              else {
                is_cont = false;
                state = STATE_IDLE;
                if (err)
                  on_error(true);
                else
                  on_ping();
                //on_idle();
                return;
              }
            }
          break;
          default:
            done = true;
        }
        DBG_LOG("[%lu] do_work() loop end, state=%s,is_cont=%d,done=%d\n",
                threadId, state_strings[state], is_cont, done);
      }

      // if we're currently paused due to backpressure, it is important that we
      // do *not* execute `uv_poll_start()` again since doing so *can* lead to
      // the poll handle becoming inactive, causing the db connection to no
      // longer keep the event loop alive (despite the handle still being
      // ref'ed)
      if (is_paused)
        return;

      if (status & MYSQL_WAIT_READ)
        new_events |= UV_READABLE;
      if (status & MYSQL_WAIT_WRITE)
        new_events |= UV_WRITABLE;

      // always store the most recent libmariadbclient status flags so that the
      // mysql_* functions will continue properly if we resume from a paused
      // state
      last_status = status;

      if (state == STATE_CLOSED)
        new_events = 0;
      else
        uv_poll_start(poll_handle, new_events, cb_poll);

      DBG_LOG("[%lu] do_work() end, new_events=%s\n",
              threadId,
              ((event & (UV_READABLE|UV_WRITABLE)) == (UV_READABLE|UV_WRITABLE)
               ? "READABLE,WRITABLE"
               : event & UV_READABLE
                 ? "READABLE"
                 : event & UV_WRITABLE
                   ? "WRITABLE"
                   : "NONE"));
    }

    static void cb_close_dummy(uv_handle_t* handle) {
    }

    static void cb_close(uv_handle_t* handle) {
      Nan::HandleScope scope;

      Client* obj = (Client*)handle->data;
      DBG_LOG("[%lu] cb_close() state=%s\n",
              obj->threadId, state_strings[obj->state]);

      obj->onclose->Call(Nan::New<Object>(obj->context), 0, nullptr);
    }

    static void cb_poll(uv_poll_t* handle, int status, int events) {
      Client* obj = (Client*)handle->data;
      DBG_LOG("[%lu] cb_poll() state=%s\n",
              obj->threadId, state_strings[obj->state]);

      int mysql_status;

      // When Linux cannot connect, EBADF is raised. Let libmariadbclient know
      // about it by faking a read event so we get a proper error message ...
      // Additionally, we have to check `status != 0` because node v0.10 does
      // not actually pass in UV_EBADF for `status`, but -1 instead.
      if (status != 0 && obj->state == STATE_CONNECT)
        mysql_status = MYSQL_WAIT_READ;
      else  {
        assert(status == 0);

        mysql_status = 0;
        if (events & UV_READABLE)
          mysql_status |= MYSQL_WAIT_READ;
        if (events & UV_WRITABLE)
          mysql_status |= MYSQL_WAIT_WRITE;
      }

      obj->do_work(mysql_status);
    }

    void on_connect() {
      Nan::HandleScope scope;
      DBG_LOG("[%lu] on_connect() state=%s\n", threadId, state_strings[state]);
      onconnect->Call(Nan::New<Object>(context), 0, nullptr);
    }

    void on_error(bool doClose = false,
                  unsigned int errNo = 0,
                  const char* errMsg = nullptr) {
      Nan::HandleScope scope;

      unsigned int errCode = mysql_errno(&mysql);

      DBG_LOG("[%lu] on_error() state=%s,doClose=%s,errNo=%d,errMsg=%s,"
               "mysql_errno=%u\n",
              threadId,
              state_strings[state],
              (doClose ? "true" : "false"),
              errNo,
              errMsg,
              errCode);

      if (errNo > 0)
        errCode = errNo;

      if (IS_DEAD_ERRNO(errCode))
        state = STATE_CLOSED;

      Local<Object> err =
          Nan::Error(errMsg ? errMsg : mysql_error(&mysql))->ToObject();
      err->Set(Nan::New<String>(code_symbol), Nan::New<Integer>(errCode));

      if (doClose || IS_DEAD_ERRNO(errCode))
        close(IS_DEAD_ERRNO(errCode));

      Local<Value> argv[1] = { err };
      onerror->Call(Nan::New<Object>(context), 1, argv);
    }

    void on_row() {
      DBG_LOG("[%lu] on_row() state=%s,need_columns=%d,need_metadata=%d\n",
              threadId, state_strings[state], need_columns, need_metadata);

      unsigned int n_fields = (cur_result ? mysql_num_fields(cur_result) : 0);

      if (n_fields == 0)
        return;

      Nan::HandleScope scope;

      MYSQL_FIELD* fields = mysql_fetch_fields(cur_result);
      unsigned long* lengths = mysql_fetch_lengths(cur_result);
      Local<Value> field_value;
      Local<Array> row = Nan::New<Array>(n_fields);
      // binary field vars
      unsigned int vlen;
      unsigned char* buf;
      uint16_t* new_buf;

      on_resultinfo(fields, n_fields);

      for (unsigned int f = 0; f < n_fields; ++f) {
        if (cur_row[f] == nullptr)
          field_value = Nan::Null();
        else if (IS_BINARY(fields[f])) {
          vlen = lengths[f];
          buf = (unsigned char*)(cur_row[f]);
          new_buf = new uint16_t[vlen];
          for (unsigned long b = 0; b < vlen; ++b)
            new_buf[b] = buf[b];
          field_value = Nan::New<String>(new_buf, vlen).ToLocalChecked();
          delete[] new_buf;
        } else {
          field_value =
            Nan::New<String>(cur_row[f], lengths[f]).ToLocalChecked();
        }

        row->Set(f, field_value);
      }

      Local<Value> argv[1] = {
        row
      };
      onrow->Call(Nan::New<Object>(context), 1, argv);
    }

    void on_rows() {
      DBG_LOG("[%lu] on_rows() state=%s,need_columns=%d,need_metadata=%d\n",
              threadId, state_strings[state], need_columns, need_metadata);

      unsigned int n_fields = (cur_result ? mysql_num_fields(cur_result) : 0);

      if (n_fields == 0)
        return;

      Nan::HandleScope scope;

      MYSQL_FIELD* fields = mysql_fetch_fields(cur_result);
      MYSQL_ROW dbrow;
      uint64_t n_rows = mysql_num_rows(cur_result);
      unsigned long* lengths;
      Local<Value> field_value;
      Local<Array> row;
      Local<Array> rows;

      // binary field vars
      unsigned int vlen;
      unsigned char* buf;
      uint16_t* new_buf;

      if (n_rows <= INT32_MAX)
        rows = Nan::New<Array>(static_cast<int>(n_rows));
      else
        rows = Nan::New<Array>();

      on_resultinfo(fields, n_fields);

      for (uint64_t i = 0; i < n_rows; ++i) {
        dbrow = mysql_fetch_row(cur_result);
        lengths = mysql_fetch_lengths(cur_result);
        row = Nan::New<Array>(n_fields);
        for (unsigned int f = 0; f < n_fields; ++f) {
          if (dbrow[f] == nullptr)
            field_value = Nan::Null();
          else if (IS_BINARY(fields[f])) {
            vlen = lengths[f];
            buf = (unsigned char*)(dbrow[f]);
            new_buf = new uint16_t[vlen];
            for (unsigned long b = 0; b < vlen; ++b)
              new_buf[b] = buf[b];
            field_value = Nan::New<String>(new_buf, vlen).ToLocalChecked();
            delete[] new_buf;
          } else {
            field_value =
              Nan::New<String>(dbrow[f], lengths[f]).ToLocalChecked();
          }
          row->Set(f, field_value);
        }
        rows->Set(i, row);
      }

      Local<Value> argv[1] = {
        rows
      };
      onrow->Call(Nan::New<Object>(context), 1, argv);
    }

    void on_resultinfo(MYSQL_FIELD* fields, unsigned int n_fields) {
      if (need_metadata || need_columns) {
        Nan::HandleScope scope;

        MYSQL_FIELD field;
        unsigned int m = 0;
        Local<Array> columns;
        Local<Array> metadata;
        Local<Value> columns_v;
        Local<Value> metadata_v;

        if (need_metadata)
          metadata_v = metadata = Nan::New<Array>(n_fields * 7);
        else
          metadata_v = Nan::Undefined();
        if (need_columns)
          columns_v = columns = Nan::New<Array>(n_fields);
        else
          columns_v = Nan::Undefined();

        for (unsigned int f = 0; f < n_fields; ++f) {
          field = fields[f];
          if (need_metadata) {
            Local<String> ret;
            // http://dev.mysql.com/doc/refman/5.7/en/c-api-data-structures.html
            switch (field.type) {
#define X(suffix, abbr, literal)                                               \
              case MYSQL_TYPE_##suffix:                                        \
                ret = Nan::New<String>(col_##abbr##_symbol);                   \
              break;
              FIELD_TYPES
#undef X
              default:
                ret = Nan::New<String>(col_unsup_symbol);
            }
            metadata->Set(m++, Nan::New<String>(field.name).ToLocalChecked());
            metadata->Set(m++,
                          Nan::New<String>(field.org_name).ToLocalChecked());
            metadata->Set(m++, ret);
            metadata->Set(m++, Nan::New<Integer>(field.flags));
            metadata->Set(m++, Nan::New<Integer>(field.charsetnr));
            metadata->Set(m++, Nan::New<String>(field.db).ToLocalChecked());
            metadata->Set(m++, Nan::New<String>(field.table).ToLocalChecked());
            metadata->Set(m++,
                          Nan::New<String>(field.org_table).ToLocalChecked());
          }
          if (need_columns) {
            columns->Set(f,
                         Nan::New<String>(field.name,
                                          field.name_length).ToLocalChecked());
          }
        }

        need_columns = need_metadata = false;

        Local<Value> resinfo_argv[2] = { columns_v, metadata_v };
        onresultinfo->Call(Nan::New<Object>(context), 2, resinfo_argv);
      }
    }

    void on_resultend() {
      Nan::HandleScope scope;

      uint64_t numRows = (cur_result ? mysql_num_rows(cur_result) : 0);
      uint64_t affRows = mysql_affected_rows(&mysql);
      uint64_t insertId = mysql_insert_id(&mysql);

      if (affRows == (my_ulonglong)-1) {
        DBG_LOG("[%lu] on_resultend() state=%s,"
                 "numRows=%" PRIu64 ",affRows=-1,insertId=%" PRIu64 "\n",
                threadId,
                state_strings[state],
                numRows,
                insertId);
      } else {
        DBG_LOG("[%lu] on_resultend() state=%s,"
                 "numRows=%" PRIu64 ",affRows=%" PRIu64 ",insertId=%"
                 PRIu64 "\n",
                threadId,
                state_strings[state],
                numRows,
                affRows,
                insertId);
      }

      Local<Value> argv[3];
      int r;

      r = snprintf(u64_buf, sizeof(u64_buf), "%" PRIu64, numRows);
      if (r <= 0 || r >= sizeof(u64_buf))
        argv[0] = Nan::EmptyString();
      else
        argv[0] = Nan::New<String>(u64_buf, r).ToLocalChecked();

      if (affRows == (my_ulonglong)-1)
        argv[1] = Nan::New<String>(neg_one_symbol);
      else {
        r = snprintf(u64_buf, sizeof(u64_buf), "%" PRIu64, affRows);
        if (r <= 0 || r >= sizeof(u64_buf))
          argv[1] = Nan::EmptyString();
        else
          argv[1] = Nan::New<String>(u64_buf, r).ToLocalChecked();
      }

      r = snprintf(u64_buf, sizeof(u64_buf), "%" PRIu64, insertId);
      if (r <= 0 || r >= sizeof(u64_buf))
        argv[2] = Nan::EmptyString();
      else
        argv[2] = Nan::New<String>(u64_buf, r).ToLocalChecked();

      onresultend->Call(Nan::New<Object>(context), 3, argv);
    }

    void on_ping() {
      Nan::HandleScope scope;

      DBG_LOG("[%lu] on_ping() state=%s\n", threadId, state_strings[state]);
      onping->Call(Nan::New<Object>(context), 0, nullptr);
    }

    void on_idle() {
      Nan::HandleScope scope;

      DBG_LOG("[%lu] on_idle() state=%s\n", threadId, state_strings[state]);
      onidle->Call(Nan::New<Object>(context), 0, nullptr);
    }

    bool apply_config(Local<Object> cfg) {
      DBG_LOG("[%lu] apply_config()\n", threadId);

      if (state != STATE_CLOSED)
        return false;

      if (!init()) {
        Nan::ThrowError(
          "The async interface is not currently supported on your platform."
        );
        return false;
      }

#define X(name)                                                                \
      Local<Value> name##_v =                                                  \
        cfg->Get(Nan::New<String>(cfg_##name##_symbol));
      CFG_OPTIONS
#undef X

      if (!user_v->IsString() || user_v->ToString()->Length() == 0)
        config.user = nullptr;
      else {
        Nan::Utf8String user_s(user_v);
        config.user = strdup(*user_s);
      }

      if (!password_v->IsString() || password_v->ToString()->Length() == 0)
        config.password = nullptr;
      else {
        Nan::Utf8String password_s(password_v);
        config.password = strdup(*password_s);
      }

      if (!host_v->IsString() || host_v->ToString()->Length() == 0)
        config.host = nullptr;
      else {
        Nan::Utf8String host_s(host_v);
        config.host = strdup(*host_s);
        mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &PROTOCOL_TCP);
      }

      if (!port_v->IsUint32() || port_v->Uint32Value() == 0)
        config.port = 3306;
      else {
        config.port = port_v->Uint32Value();
        mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &PROTOCOL_TCP);
      }

      if (!unixSocket_v->IsString() || unixSocket_v->ToString()->Length() == 0)
        config.unixSocket = nullptr;
      else {
        Nan::Utf8String unixSocket_s(unixSocket_v);
        config.unixSocket = strdup(*unixSocket_s);
        mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &PROTOCOL_SOCKET);
      }

      if (protocol_v->IsString() && protocol_v->ToString()->Length() > 0) {
        Nan::Utf8String protocol_s(protocol_v);
        const char* protocol = *protocol_s;
        if (strcasecmp(protocol, "tcp") == 0)
          mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &PROTOCOL_TCP);
        else if (strcasecmp(protocol, "socket") == 0)
          mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &PROTOCOL_SOCKET);
        else if (strcasecmp(protocol, "pipe") == 0)
          mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &PROTOCOL_PIPE);
        else if (strcasecmp(protocol, "memory") == 0)
          mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &PROTOCOL_MEMORY);
        else
          mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &PROTOCOL_DEFAULT);
      }

      if (db_v->IsString() && db_v->ToString()->Length() > 0) {
        Nan::Utf8String db_s(db_v);
        config.db = strdup(*db_s);
      }

      unsigned int timeout = 10;
      if (connTimeout_v->IsUint32() && connTimeout_v->Uint32Value() > 0)
        timeout = connTimeout_v->Uint32Value();
      mysql_options(&mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

      if (!secureAuth_v->IsBoolean() || secureAuth_v->BooleanValue())
        mysql_options(&mysql, MYSQL_SECURE_AUTH, &MY_BOOL_TRUE);
      else
        mysql_options(&mysql, MYSQL_SECURE_AUTH, &MY_BOOL_FALSE);

      if (multiStatements_v->IsBoolean() && multiStatements_v->BooleanValue())
        config.client_opts |= CLIENT_MULTI_STATEMENTS;

      if (compress_v->IsBoolean() && compress_v->BooleanValue())
        mysql_options(&mysql, MYSQL_OPT_COMPRESS, 0);

      if (local_infile_v->IsBoolean() && local_infile_v->BooleanValue())
        mysql_options(&mysql, MYSQL_OPT_LOCAL_INFILE, &MY_BOOL_TRUE);

      if (read_default_file_v->IsString()
          && read_default_file_v->ToString()->Length() > 0) {
        Nan::Utf8String def_file_s(read_default_file_v);
        mysql_options(&mysql, MYSQL_READ_DEFAULT_FILE,
                      *def_file_s);
      }

      if (read_default_group_v->IsString()
          && read_default_group_v->ToString()->Length() > 0) {
        Nan::Utf8String def_grp_s(read_default_group_v);
        mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP,
                      *def_grp_s);
      }

      if (tcpKeepalive_v->IsUint32())
        config.tcpka = tcpKeepalive_v->Uint32Value();
      if (tcpKeepaliveCnt_v->IsUint32())
        config.tcpkaCnt = tcpKeepaliveCnt_v->Uint32Value();
      if (tcpKeepaliveIntvl_v->IsUint32())
        config.tcpkaIntvl = tcpKeepaliveIntvl_v->Uint32Value();

      if (charset_v->IsString() && charset_v->ToString()->Length() > 0) {
        Nan::Utf8String charset_s(charset_v);
        config.charset = strdup(*charset_s);
        mysql_options(&mysql, MYSQL_SET_CHARSET_NAME, config.charset); 
      }

      if (ssl_v->IsObject() || (ssl_v->IsBoolean() && ssl_v->BooleanValue())) {
        bool use_default_ciphers = true;
        if (!ssl_v->IsBoolean()) {
          Local<Object> ssl = ssl_v->ToObject();
#define X(name)                                                                \
          Local<Value> name##_v =                                              \
              ssl->Get(Nan::New<String>(cfg_##name##_symbol));
          CFG_OPTIONS_SSL
#undef X

          if (key_v->IsString() && key_v->ToString()->Length() > 0) {
            Nan::Utf8String key_s(key_v);
            config.ssl_key = strdup(*key_s);
          }
          if (cert_v->IsString() && cert_v->ToString()->Length() > 0) {
            Nan::Utf8String cert_s(cert_v);
            config.ssl_cert = strdup(*cert_s);
          }
          if (ca_v->IsString() && ca_v->ToString()->Length() > 0) {
            Nan::Utf8String ca_s(ca_v);
            config.ssl_ca = strdup(*ca_s);
          }
          if (capath_v->IsString() && capath_v->ToString()->Length() > 0) {
            Nan::Utf8String capath_s(capath_v);
            config.ssl_capath = strdup(*capath_s);
          }
          if (cipher_v->IsString() && cipher_v->ToString()->Length() > 0) {
            Nan::Utf8String cipher_s(cipher_v);
            config.ssl_cipher = strdup(*cipher_s);
            use_default_ciphers = false;
          }

          mysql_options(&mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                        (rejectUnauthorized_v->IsBoolean()
                         && rejectUnauthorized_v->BooleanValue()
                         ? &MY_BOOL_TRUE
                         : &MY_BOOL_FALSE));
        }

        mysql_ssl_set(&mysql,
                      config.ssl_key,
                      config.ssl_cert,
                      config.ssl_ca,
                      config.ssl_capath,
                      use_default_ciphers ? DEFAULT_CIPHER : config.ssl_cipher);
      }

      // always disable auto-reconnect
      my_bool reconnect = 0;
      mysql_options(&mysql, MYSQL_OPT_RECONNECT, &reconnect);

      return true;
    }

    static NAN_METHOD(New) {
      DBG_LOG("new ClientBinding()\n");

      if (!info.IsConstructCall()) {
        return Nan::ThrowTypeError(
          "Use `new` to create instances of this object."
        );
      }
      if (info.Length() == 0 || !info[0]->IsObject())
        return Nan::ThrowTypeError("Missing setup object");

      Local<Object> cfg = info[0]->ToObject();
#define X(name)                                                                \
      Local<Value> v_on##name =                                                \
        cfg->Get(Nan::New<String>(ev_##name##_symbol));                        \
      if (!v_on##name->IsFunction())                                           \
        return Nan::ThrowTypeError("Missing on" #name " handler");
      EVENT_NAMES
#undef X
      Local<Value> context_v =
        cfg->Get(Nan::New<String>(context_symbol));
      Local<Value> conncfg_v =
        cfg->Get(Nan::New<String>(conncfg_symbol));

      Client* obj = new Client();

#define X(name)                                                                \
      obj->on##name = new Nan::Callback(Local<Function>::Cast(v_on##name));
      EVENT_NAMES
#undef X
      if (context_v->IsObject())
        obj->context.Reset(context_v->ToObject());
      else
        obj->context.Reset(Nan::GetCurrentContext()->Global());
      if (conncfg_v->IsObject())
        obj->apply_config(conncfg_v->ToObject());
      obj->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }

    static NAN_METHOD(SetConfig) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->setConfig()\n", obj->threadId);

      if (info.Length() > 0 && info[0]->IsObject()) {
        Local<Object> cfg = info[0]->ToObject();
        obj->apply_config(cfg);
      }
    }

    static NAN_METHOD(Connect) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->connect()\n", obj->threadId);

      if (obj->state != STATE_CLOSED)
        return Nan::ThrowError("Already connected");

      if (info.Length() > 0 && info[0]->IsObject()) {
        Local<Object> cfg = info[0]->ToObject();
        if (!obj->apply_config(cfg))
          return;
      } else if (!obj->init()) {
        return Nan::ThrowError(
          "The async interface is not currently supported on your platform."
        );
      }

      obj->connect();
    }

    static NAN_METHOD(Close) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->close()\n", obj->threadId);

      if (!obj->close())
        Nan::ThrowError("Not connected");
    }

    static NAN_METHOD(Pause) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->pause()\n", obj->threadId);

      obj->pause();
    }

    static NAN_METHOD(Resume) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->resume()\n", obj->threadId);

      obj->resume();
    }

    static NAN_METHOD(Ping) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->ping()\n", obj->threadId);

      obj->ping();
    }

    static NAN_METHOD(LastInsertId) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->lastInsertId()\n", obj->threadId);

      uint64_t insertId = obj->lastInsertId();

      int r = snprintf(u64_buf, sizeof(u64_buf), "%" PRIu64, insertId);
      if (r <= 0 || r >= sizeof(u64_buf))
        info.GetReturnValue().Set(Nan::EmptyString());
      else {
        info.GetReturnValue().Set(
          Nan::New<String>(u64_buf, r).ToLocalChecked()
        );
      }
    }

    static NAN_METHOD(Query) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->query()\n", obj->threadId);

      if (obj->state != STATE_IDLE)
        return Nan::ThrowError("Not ready to query");
      if (info.Length() < 3)
        return Nan::ThrowTypeError("Missing arguments");
      if (!info[0]->IsString())
        return Nan::ThrowTypeError("query argument must be a string");
      /*if (!info[0]->IsString() && !stmt_constructor->HasInstance(info[0])) {
        return Nan::ThrowTypeError(
            "query argument must be a string or Statement instance"
        );
      }*/
      if (!info[1]->IsBoolean())
        return Nan::ThrowTypeError("columns argument must be a boolean");
      if (!info[2]->IsBoolean())
        return Nan::ThrowTypeError("metadata argument must be a boolean");
      if (!info[3]->IsBoolean())
        return Nan::ThrowTypeError("buffered argument must be a boolean");

      //if (info[0]->IsString()) {
        obj->query(info[0],
                   info[1]->BooleanValue(),
                   info[2]->BooleanValue(),
                   info[3]->BooleanValue());
      /*} else {
        Local<Object> stmt_obj = info[0]->ToObject();
        Statement* stmt = Nan::ObjectWrap::Unwrap<Statement>(stmt_obj);
        obj->query(stmt, info[1]->BooleanValue(), info[2]->BooleanValue());
      }*/
    }

    static NAN_METHOD(Escape) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->escape()\n", obj->threadId);

      if (obj->state == STATE_CLOSED)
        return Nan::ThrowError("Not connected");
      else if (info.Length() == 0 || !info[0]->IsString())
        return Nan::ThrowTypeError("You must supply a string");

      Nan::Utf8String arg_v(info[0]);
      unsigned long arg_len = arg_v.length();
      char* result = (char*) malloc(arg_len * 2 + 1);
      unsigned long result_len = obj->escape((char*)*arg_v, arg_len, result);
      Local<String> escaped_s =
        Nan::New<String>(result, result_len).ToLocalChecked();
      free(result);
      info.GetReturnValue().Set(escaped_s);
    }

    static NAN_METHOD(IsMariaDB) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->isMariaDB()\n", obj->threadId);

      if (obj->state == STATE_CLOSED)
        return Nan::ThrowError("Not connected");

      info.GetReturnValue().Set(
        Nan::New<Boolean>(mariadb_connection(&obj->mysql) == 1)
      );
    }

    static NAN_METHOD(ServerVersion) {
      Client* obj = Nan::ObjectWrap::Unwrap<Client>(info.This());
      DBG_LOG("[%lu] clientBinding->serverVersion()\n", obj->threadId);

      if (obj->state == STATE_CLOSED)
        return Nan::ThrowError("Not connected");

      info.GetReturnValue().Set(
        Nan::New<String>(mysql_get_server_info(&obj->mysql)).ToLocalChecked()
      );
    }

    static void Initialize(Handle<Object> target) {
      Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
      Local<String> name = Nan::New<String>("ClientBinding").ToLocalChecked();

      constructor.Reset(tpl);
      tpl->InstanceTemplate()->SetInternalFieldCount(1);
      tpl->SetClassName(name);

      code_symbol.Reset(Nan::New<String>("code").ToLocalChecked());
      context_symbol.Reset(Nan::New<String>("context").ToLocalChecked());
      conncfg_symbol.Reset(Nan::New<String>("config").ToLocalChecked());
      neg_one_symbol.Reset(Nan::New<String>("-1").ToLocalChecked());

#define X(name)                                                                \
      ev_##name##_symbol.Reset(Nan::New<String>("on" #name).ToLocalChecked());
      EVENT_NAMES
#undef X

#define X(suffix, abbr, literal)                                               \
      col_##abbr##_symbol.Reset(Nan::New<String>(#literal).ToLocalChecked());
      FIELD_TYPES
#undef X
      col_unsup_symbol.Reset(
        Nan::New<String>("[Unknown field type]").ToLocalChecked()
      );

#define X(name)                                                                \
      cfg_##name##_symbol.Reset(Nan::New<String>(#name).ToLocalChecked());
      CFG_OPTIONS
      CFG_OPTIONS_SSL
#undef X

      Nan::SetPrototypeMethod(tpl, "connect", Connect);
      Nan::SetPrototypeMethod(tpl, "query", Query);
      Nan::SetPrototypeMethod(tpl, "setConfig", SetConfig);
      Nan::SetPrototypeMethod(tpl, "pause", Pause);
      Nan::SetPrototypeMethod(tpl, "resume", Resume);
      Nan::SetPrototypeMethod(tpl, "ping", Ping);
      Nan::SetPrototypeMethod(tpl, "escape", Escape);
      Nan::SetPrototypeMethod(tpl, "close", Close);
      Nan::SetPrototypeMethod(tpl, "isMariaDB", IsMariaDB);
      Nan::SetPrototypeMethod(tpl, "serverVersion", ServerVersion);
      Nan::SetPrototypeMethod(tpl, "lastInsertId", LastInsertId);

      target->Set(name, tpl->GetFunction());
    }
};

static NAN_METHOD(Escape) {
  DBG_LOG("ClientBinding::escape()\n");

  if (info.Length() == 0 || !info[0]->IsString())
    return Nan::ThrowTypeError("You must supply a string");

  Nan::Utf8String arg_v(info[0]);
  unsigned long arg_len = arg_v.length();
  char* result = (char*) malloc(arg_len * 2 + 1);
  unsigned long result_len =
      mysql_escape_string_ex(result, (char*)*arg_v, arg_len, "utf8");
  Local<String> escaped_s =
    Nan::New<String>(result, result_len).ToLocalChecked();
  free(result);

  info.GetReturnValue().Set(escaped_s);
}

static NAN_METHOD(Version) {
  DBG_LOG("ClientBinding::version()\n");

  info.GetReturnValue().Set(
    Nan::New<String>(mysql_get_client_info()).ToLocalChecked()
  );
}

// =============================================================================
// TODO: Implement (completely) prepared statements.
//       The C API for prepared statements is absolutely ridiculous and you
//       couldn't pay me enough money to write a binding for that part of the
//       API.
/*class Statement : public Nan::ObjectWrap {
  public:
    MYSQL_STMT* stmt;
    MYSQL_BIND* params;
    uint32_t params_len;
    char* query;
    bool is_prepared = false;

    Statement() {
      DBG_LOG("Statement()\n");
      stmt = nullptr;
      params = nullptr;
      params_len = 0;
      query = nullptr;
      is_prepared = false;
    }

    ~Statement() {
      DBG_LOG("~Statement()\n");
      clear_params();
      FREE(query);
    }

    void clear_params() {
      if (params) {
        for (int i=0; i<field_count; i++) {
            (*result_data)[i] = (char *)malloc((STRING_SIZE+1) *  sizeof(char));
        }
      }
    }

    static NAN_METHOD(New) {
      DBG_LOG("new Statement()\n");

      if (!info.IsConstructCall()) {
        return Nan::ThrowTypeError(
          "Use `new` to create instances of this object."
        );
      }

      if (info.Length() == 0 || !info[0]->IsString())
        return Nan::ThrowTypeError("Missing query string");

      Statement *obj = new Statement();
      obj->query = strdup(*(String::Utf8Value(info[0])));
      obj->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }

    static NAN_METHOD(Bind) {
      DBG_LOG("statement->bind()\n");

      if (info.Length() == 0 || !info[0]->IsArray())
        return Nan::ThrowTypeError("Missing array of parameters to bind");

      Statement *obj = Nan::ObjectWrap::Unwrap<Statement>(info.This());
      Local<Array> arr = Local<Array>::Cast(info[0]);
      uint32_t len = arr->Length();

      // validate param count early if our statement is already prepared
      if (obj->stmt
          && obj->is_prepared
          && len != mysql_stmt_param_count(obj->stmt))
        return Nan::ThrowError("Wrong parameter count");

      obj->clear_params();

      uint32_t old_params_len = obj->params_len;
      if (old_params_len != len) {
        FREE(obj->params);
        obj->params = malloc(sizeof(MYSQL_BIND) * len);
        obj->params_len = len;
      }
      MYSQL_BIND params[] = obj->params;
      for (uint32_t i = 0; i < len; ++i) {
        Local<Value> val = arr->Get(i);
        if (val->IsString()) {
          
        }
      }

      return;
    }
    
    static void Initialize(Handle<Object> target) {
      Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);

      stmt_constructor.Reset(tpl);
      tpl->InstanceTemplate()->SetInternalFieldCount(1);
      tpl->SetClassName(Nan::New<String>("Statement").ToLocalChecked());

      Nan::SetPrototypeMethod(tpl, "bind", Bind);

      target->Set(Nan::New<String>("Statement").ToLocalChecked(),
                  tpl->GetFunction());
    }
};*/
// =============================================================================

extern "C" {
  void init(Handle<Object> target) {
    Client::Initialize(target);
    //Statement::Initialize(target);
    target->Set(Nan::New<String>("escape").ToLocalChecked(),
                Nan::New<FunctionTemplate>(Escape)->GetFunction());
    target->Set(Nan::New<String>("version").ToLocalChecked(),
                Nan::New<FunctionTemplate>(Version)->GetFunction());
  }

  NODE_MODULE(sqlclient, init);
}

