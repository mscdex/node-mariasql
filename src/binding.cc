#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include <mysql.h>

using namespace node;
using namespace v8;

#define DEBUG 1


#if defined(DEBUG) && DEBUG
# define DBG_LOG(fmt, ...)                                          \
    do { fprintf(stderr, "DEBUG: " fmt, __VA_ARGS__); } while (0)
#else
# define DBG_LOG
#endif
#define FREE(p) if (p) { free(p); p = NULL; }
#define IS_BINARY(f) ((f.flags & BINARY_FLAG) &&                    \
                      ((f.type == MYSQL_TYPE_TINY_BLOB)   ||        \
                       (f.type == MYSQL_TYPE_MEDIUM_BLOB) ||        \
                       (f.type == MYSQL_TYPE_BLOB)        ||        \
                       (f.type == MYSQL_TYPE_LONG_BLOB)   ||        \
                       (f.type == MYSQL_TYPE_STRING)      ||        \
                       (f.type == MYSQL_TYPE_VAR_STRING)))
#define IS_DEAD_ERRNO(v) (v == 2006 || v == 2013 || v == 2055)
#define DEFAULT_CIPHER "ECDHE-RSA-AES128-SHA256:AES128-GCM-SHA256:RC4:HIGH:!MD5:!aNULL:!EDH"
#define STATES                        \
  X(CLOSED, 0)                        \
  X(CONNECT, 1)                       \
  X(IDLE, 2)                          \
  X(QUERY, 3)                         \
  X(RESULT, 4)                        \
  X(ROW, 5)                           \
  X(NEXTRESULT, 6)                    \
  X(FREERESULT, 7)                    \
  X(PING, 8)
#define EVENT_NAMES                   \
  X(connect)                          \
  X(error)                            \
  X(idle)                             \
  X(resultinfo)                       \
  X(row)                              \
  X(resultend)                        \
  X(ping)                             \
  X(close)
#define FIELD_TYPES                   \
  X(TINY, tiny, TINYINT)              \
  X(SHORT, short, SMALLINT)           \
  X(LONG, long, INTEGER)              \
  X(INT24, int24, MEDIUMINT)          \
  X(LONGLONG, big, BIGINT)            \
  X(DECIMAL, dec, DECIMAL)            \
  X(NEWDECIMAL, newdec, DECIMAL)      \
  X(FLOAT, float, FLOAT)              \
  X(DOUBLE, double, DOUBLE)           \
  X(BIT, bit, BIT)                    \
  X(TIMESTAMP, ts, TIMESTAMP)         \
  X(DATE, date, DATE)                 \
  X(NEWDATE, newdate, DATE)           \
  X(TIME, time, TIME)                 \
  X(DATETIME, dtime, DATETIME)        \
  X(YEAR, year, YEAR)                 \
  X(STRING, char, CHAR)               \
  X(VAR_STRING, vstr, VARCHAR)        \
  X(VARCHAR, vchar, VARCHAR)          \
  X(TINY_BLOB, tinyblob, TINYBLOB)    \
  X(MEDIUM_BLOB, medblob, MEDIUMBLOB) \
  X(LONG_BLOB, lngblob, LONGBLOB)     \
  X(SET, set, SET)                    \
  X(ENUM, enum, ENUM)                 \
  X(GEOMETRY, geo, GEOMETRY)          \
  X(NULL, null, NULL)
#define CFG_OPTIONS                   \
  X(user)                             \
  X(password)                         \
  X(host)                             \
  X(port)                             \
  X(unixSocket)                       \
  X(db)                               \
  X(connTimeout)                      \
  X(secureAuth)                       \
  X(multiStatements)                  \
  X(compress)                         \
  X(metadata)                         \
  X(local_infile)                     \
  X(read_default_file)                \
  X(read_default_group)               \
  X(charset)                          \
  X(tcpKeepalive)                     \
  X(tcpKeepaliveCnt)                  \
  X(tcpKeepaliveIntvl)                \
  X(ssl)
#define CFG_OPTIONS_SSL               \
  X(key)                              \
  X(cert)                             \
  X(ca)                               \
  X(capath)                           \
  X(cipher)                           \
  X(rejectUnauthorized)

static Persistent<FunctionTemplate> constructor;
static Persistent<String> code_symbol;
static Persistent<String> context_symbol;
static Persistent<String> conncfg_symbol;

#define X(state, val)                 \
const int STATE_##state## = ##val##;
STATES
#undef X

const char* state_strings[] = {
#define X(state, val)  \
#state,
STATES
#undef X
};

#define X(name)                                \
static Persistent<String> ev_##name##_symbol;
EVENT_NAMES
#undef X

#define X(suffix, abbr, literal)               \
static Persistent<String> col_##abbr##_symbol;
FIELD_TYPES
#undef X
static Persistent<String> col_unsup_symbol;

#define X(name)                                \
static Persistent<String> cfg_##name##_symbol;
CFG_OPTIONS
CFG_OPTIONS_SSL
#undef X

struct sql_config {
  char *user;
  char *password;
  char *host;
  unsigned int port;
  char *unixSocket;
  char *db;
  unsigned long client_opts;
  unsigned int tcpka;
  unsigned int tcpkaCnt;
  unsigned int tcpkaIntvl;
  bool metadata;
  char *charset;

  // ssl
  char *ssl_key;
  char *ssl_cert;
  char *ssl_ca;
  char *ssl_capath;
  char *ssl_cipher;
};

const my_bool MY_BOOL_TRUE = 1;
const my_bool MY_BOOL_FALSE = 0;

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

class Client : public ObjectWrap {
  public:
    Persistent<Object> context;
    uv_poll_t *poll_handle;
    uv_os_sock_t mysql_sock;
    MYSQL mysql;
    MYSQL *mysql_ret;
    sql_config config;
    bool initialized;
    bool is_cont;
    bool is_aborting;
    bool is_destructing;
    bool is_paused;
    char* cur_query;
    MYSQL_RES *cur_result;
    MYSQL_ROW cur_row;
    bool req_columns;
    bool need_columns;
    bool req_metadata;
    bool need_metadata;
    int state;
#define X(name)  \
    NanCallback *on##name##;
    EVENT_NAMES
#undef X

    Client() {
      DBG_LOG("Client()\n");
      state = STATE_CLOSED;

      is_destructing = false;
      initialized = false;

#define X(name)  \
      on##name## = NULL;
      EVENT_NAMES
#undef X
    }

    ~Client() {
      DBG_LOG("~Client()\n");
#define X(name)       \
      if (on##name##) \
        delete on##name##;
      EVENT_NAMES
#undef X
      if (!context.IsEmpty())
        NanDisposePersistent(context);
      is_destructing = true;
      close();
    }

    void init() {
      DBG_LOG("init()\n");
      if (initialized)
        clear_state();

      poll_handle = NULL;
      mysql_sock = 0;

      mysql_init(&mysql);
      mysql_options(&mysql, MYSQL_OPT_NONBLOCK, 0);

      config.user = NULL;
      config.password = NULL;
      config.host = NULL;
      config.port = 3306;
      config.unixSocket = NULL;
      config.db = NULL;
      config.client_opts = CLIENT_MULTI_RESULTS | CLIENT_REMEMBER_OPTIONS;
      config.tcpka = 0; // disabled by default
      config.tcpkaCnt = 0; // use system default
      config.tcpkaIntvl = 0; // use system default
      config.metadata = false;
      config.charset = NULL;
      config.ssl_key = NULL;
      config.ssl_cert = NULL;
      config.ssl_ca = NULL;
      config.ssl_capath = NULL;
      config.ssl_cipher = NULL;

      is_cont = false;

      is_aborting = false;

      is_paused = false;

      cur_query = NULL;

      initialized = true;
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

      FREE(cur_query);
    }

    bool close() {
      DBG_LOG("close() state=%s\n", state_strings[state]);
      initialized = false;

      clear_state();

      if (state != STATE_CLOSED) {
        Unref();
        mysql_close(&mysql);
        if (is_destructing) {
          if (poll_handle)
            uv_poll_stop(poll_handle);
          FREE(poll_handle);
        } else {
          state = STATE_CLOSED;
          uv_close((uv_handle_t*)poll_handle, cb_close);
        }
        return true;
      }
      return false;
    }

    bool connect() {
      DBG_LOG("connect() state=%s\n", state_strings[state]);
      if (state == STATE_CLOSED) {
        Ref();
        state = STATE_CONNECT;
        do_work();
        return true;
      }
      return false;
    }

    bool query(const char *qry, bool columns, bool metadata) {
      DBG_LOG("query() state=%s,columns=%d,metadata=%d,query=%s\n",
              state_strings[state], columns, metadata, qry);
      if (state == STATE_IDLE) {
        FREE(cur_query);
        cur_query = strdup(qry);
        req_columns = columns;
        req_metadata = metadata;
        state = STATE_QUERY;
        do_work();
        return true;
      }
      return false;
    }

    bool pause() {
      DBG_LOG("pause() state=%s,is_paused=%d\n",
              state_strings[state], is_paused);
      if (state >= STATE_IDLE && !is_paused) {
        is_paused = true;
        return true;
      }
      return false;
    }

    bool resume() {
      DBG_LOG("resume() state=%s,is_paused=%d\n",
              state_strings[state], is_paused);
      if (state == STATE_ROW && is_paused) {
        is_paused = false;
        do_work();
        return true;
      }
      return false;
    }

    bool ping() {
      DBG_LOG("ping() state=%s\n", state_strings[state]);
      if (state == STATE_IDLE) {
        state = STATE_PING;
        do_work();
        return true;
      }
      return false;
    }

    unsigned long escape(const char *src, unsigned long src_len, char* dest) {
      return mysql_real_escape_string(&mysql, dest, src, src_len);
    }

    void do_work(int event = 0) {
      DBG_LOG("do_work() state=%s,event=%s\n",
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
      while (!done) {
        DBG_LOG("do_work() loop begin, state=%s\n", state_strings[state]);
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

              poll_handle = (uv_poll_t*)malloc(sizeof(uv_poll_t));
              uv_poll_init_socket(uv_default_loop(),
                                  poll_handle,
                                  mysql_sock);
              uv_poll_start(poll_handle, UV_READABLE, cb_poll);
              poll_handle->data = this;

              if (status) {
                done = true;
                is_cont = true;
              } else {
                state = STATE_IDLE;
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
                on_connect();
                return;
              }
            }
          break;
          case STATE_QUERY:
            if (!is_cont) {
              status = mysql_real_query_start(&err,
                                              &mysql,
                                              cur_query,
                                              strlen(cur_query));
              FREE(cur_query);
              if (status) {
                done = true;
                is_cont = true;
              } else {
                if (err) {
                  state = STATE_IDLE;
                  on_error();
                  on_idle();
                } else
                  state = STATE_RESULT;
              }
            } else {
              status = mysql_real_query_cont(&err, &mysql, event);
              if (status)
                done = true;
              else {
                is_cont = false;
                if (err) {
                  state = STATE_IDLE;
                  on_error();
                  on_idle();
                } else
                  state = STATE_RESULT;
              }
            }
          break;
          case STATE_RESULT:
            // we have a result and now we check for rows
            cur_result = mysql_use_result(&mysql);
            if (!cur_result) {
              if (mysql_errno(&mysql))
                on_error();
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
                  if (cur_result)
                    state = STATE_FREERESULT;
                  else {
                    state = STATE_IDLE;
                    on_idle();
                    return;
                  }
                } else
                  state = STATE_RESULT;
              }
            } else {
              status = mysql_next_result_cont(&err, &mysql, event);
              if (status)
                done = true;
              else {
                is_cont = false;
                if (err) {
                  if (cur_result)
                    state = STATE_FREERESULT;
                  else {
                    state = STATE_IDLE;
                    on_idle();
                    return;
                  }
                } else
                  state = STATE_RESULT;
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
        DBG_LOG("do_work() loop end, state=%s\n", state_strings[state]);
      }
      if (status & MYSQL_WAIT_READ)
        new_events |= UV_READABLE;
      if (status & MYSQL_WAIT_WRITE)
        new_events |= UV_WRITABLE;
      uv_poll_start(poll_handle, new_events, cb_poll);
    }

    static void cb_close(uv_handle_t *handle) {
      Client *obj = (Client*)handle->data;
      DBG_LOG("cb_close() state=%s\n", state_strings[obj->state]);

      FREE(obj->poll_handle);
      obj->onclose->Call(obj->context, 0, NULL);
    }

    static void cb_poll(uv_poll_t *handle, int status, int events) {
      Client *obj = (Client*)handle->data;
      DBG_LOG("cb_poll() state=%s\n", state_strings[obj->state]);

      assert(status == 0);

      int mysql_status = 0;
      if (events & UV_READABLE)
        mysql_status |= MYSQL_WAIT_READ;
      if (events & UV_WRITABLE)
        mysql_status |= MYSQL_WAIT_WRITE;

      obj->do_work(mysql_status);
    }

    void on_connect() {
      DBG_LOG("on_connect() state=%s\n", state_strings[state]);
      onconnect->Call(context, 0, NULL);
    }

    void on_error(bool doClose = false, unsigned int errNo = 0,
                  const char *errMsg = NULL) {
      DBG_LOG("on_error() state=%s\n", state_strings[state]);
      NanScope();
      unsigned int errCode = mysql_errno(&mysql);

      if (errNo > 0)
        errCode = errNo;

      Local<Object> err =
          NanError(errMsg ? errMsg : mysql_error(&mysql))->ToObject();
      err->Set(NanNew<String>(code_symbol), NanNew<Integer>(errCode));

      Handle<Value> argv[1] = { err };
      onerror->Call(context, 1, argv);

      if (doClose || IS_DEAD_ERRNO(errCode))
        close();
    }

    void on_row() {
      DBG_LOG("on_row() state=%s,need_columns=%d,need_metadata=%d\n",
              state_strings[state], need_columns, need_metadata);
      NanScope();

      MYSQL_FIELD field;
      MYSQL_FIELD *fields;
      unsigned int n_fields = mysql_num_fields(cur_result);
      unsigned int f = 0;
      unsigned int i = 0;
      unsigned int m = 0;
      unsigned int vlen;
      unsigned char *buf;
      uint16_t *new_buf;
      unsigned long *lengths;
      Handle<Value> field_value;
      Local<Array> row;

      if (n_fields == 0)
        return;

      lengths = mysql_fetch_lengths(cur_result);
      fields = mysql_fetch_fields(cur_result);
      row = NanNew<Array>(n_fields);

      if (need_metadata || need_columns) {
        Local<Array> columns;
        Local<Array> metadata;
        Local<Value> columns_v;
        Local<Value> metadata_v;

        if (need_metadata)
          metadata_v = metadata = NanNew<Array>(n_fields * 7);
        else
          metadata_v = NanUndefined();
        if (need_columns)
          columns_v = columns = NanNew<Array>(n_fields);
        else
          columns_v = NanUndefined();

        for (f = 0; f < n_fields; ++f) {
          field = fields[f];
          if (need_metadata) {
            metadata->Set(m++, FieldTypeToString(field.type));
            metadata->Set(m++, NanNew<Integer>(field.charsetnr));
            metadata->Set(m++, NanNew<String>(field.db));
            metadata->Set(m++, NanNew<String>(field.table));
            metadata->Set(m++, NanNew<String>(field.org_table));
            metadata->Set(m++, NanNew<String>(field.name));
            metadata->Set(m++, NanNew<String>(field.org_name));
          }
          if (need_columns)
            columns->Set(f, NanNew<String>(field.name, field.name_length));
        }

        need_columns = need_metadata = false;

        Handle<Value> resinfo_argv[2] = { columns_v, metadata_v };
        onresultinfo->Call(context, 2, resinfo_argv);
      }

      for (f = 0; f < n_fields; ++f) {
        if (cur_row[f] == NULL)
          field_value = NanNull();
        else if (IS_BINARY(fields[f])) {
          vlen = lengths[f];
          buf = (unsigned char*)(cur_row[f]);
          new_buf = new uint16_t[vlen];
          for (i = 0; i < vlen; ++i)
            new_buf[i] = buf[i];
          field_value = NanNew<String>(new_buf, vlen);
          delete[] new_buf;
        } else
          field_value = NanNew<String>(cur_row[f], lengths[f]);

        row->Set(f, field_value);
      }
      Handle<Value> argv[1] = {
        row
      };
      onrow->Call(context, 1, argv);
    }

    void on_resultend() {
      NanScope();

      my_ulonglong numRows = mysql_num_rows(cur_result);
      my_ulonglong affRows = mysql_affected_rows(&mysql);
      my_ulonglong insId = mysql_insert_id(&mysql);

      DBG_LOG("on_resultend() state=%s,numRows=%d,affRows=%d,insId=%d\n",
              state_strings[state], numRows, affRows, insId);

      Handle<Value> argv[3] = {
        NanNew<Number>(numRows),
        (affRows == (my_ulonglong)-1
         ? NanNew<Number>(-1)
         : NanNew<Number>(affRows)),
        NanNew<Number>(insId)
      };
      onresultend->Call(context, 3, argv);
    }

    void on_ping() {
      DBG_LOG("on_ping() state=%s\n", state_strings[state]);
      onping->Call(context, 0, NULL);
    }

    void on_idle() {
      DBG_LOG("on_idle() state=%s\n", state_strings[state]);
      onidle->Call(context, 0, NULL);
    }

    void apply_config(Handle<Object> cfg) {
      DBG_LOG("apply_config()\n");
      NanScope();

      if (state != STATE_CLOSED)
        return;

      init();

#define X(name)                                                                \
      Local<Value> ##name##_v = cfg->Get(NanNew<String>(cfg_##name##_symbol));
      CFG_OPTIONS
#undef X

      if (!user_v->IsString() || user_v->ToString()->Length() == 0)
        config.user = NULL;
      else {
        String::Utf8Value user_s(user_v);
        config.user = strdup(*user_s);
      }

      if (!password_v->IsString() || password_v->ToString()->Length() == 0)
        config.password = NULL;
      else {
        String::Utf8Value password_s(password_v);
        config.password = strdup(*password_s);
      }

      if (!host_v->IsString() || host_v->ToString()->Length() == 0)
        config.host = NULL;
      else {
        String::Utf8Value host_s(host_v);
        config.host = strdup(*host_s);
      }

      if (!port_v->IsUint32() || port_v->Uint32Value() == 0)
        config.port = 3306;
      else
        config.port = port_v->Uint32Value();

      if (!unixSocket_v->IsString() || unixSocket_v->ToString()->Length() == 0)
        config.unixSocket = NULL;
      else {
        String::Utf8Value unixSocket_s(unixSocket_v);
        config.unixSocket = strdup(*unixSocket_s);
      }

      if (db_v->IsString() && db_v->ToString()->Length() > 0) {
        String::Utf8Value db_s(db_v);
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

      if (local_infile_v->IsBoolean() && local_infile_v->BooleanValue()) {
        mysql_options(&mysql, MYSQL_OPT_LOCAL_INFILE, &MY_BOOL_TRUE);
      }

      if (read_default_file_v->IsString()
          && read_default_file_v->ToString()->Length() > 0) {
        mysql_options(&mysql, MYSQL_READ_DEFAULT_FILE,
                      *String::Utf8Value(read_default_file_v));
      }

      if (read_default_group_v->IsString()
          && read_default_group_v->ToString()->Length() > 0) {
        mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP,
                      *String::Utf8Value(read_default_group_v));
      }

      if (tcpKeepalive_v->IsUint32()) {
        config.tcpka = tcpKeepalive_v->Uint32Value();
        if (config.tcpka > 0) {
          
        }
      }

      if (charset_v->IsString() && charset_v->ToString()->Length() > 0) {
        config.charset = strdup(*(String::Utf8Value(charset_v)));
        mysql_options(&mysql, MYSQL_SET_CHARSET_NAME, config.charset); 
      }

      if (ssl_v->IsObject() || (ssl_v->IsBoolean() && ssl_v->BooleanValue())) {
        if (ssl_v->IsBoolean())
          config.ssl_cipher = DEFAULT_CIPHER;
        else {
          Local<Object> ssl = ssl_v->ToObject();
#define X(name)                                                 \
          Local<Value> ##name##_v =                             \
              ssl->Get(NanNew<String>(cfg_##name##_symbol));
          CFG_OPTIONS_SSL
#undef X

          if (key_v->IsString() && key_v->ToString()->Length() > 0)
            config.ssl_key = strdup(*(String::Utf8Value(key_v)));
          if (cert_v->IsString() && cert_v->ToString()->Length() > 0)
            config.ssl_cert = strdup(*(String::Utf8Value(cert_v)));
          if (ca_v->IsString() && ca_v->ToString()->Length() > 0)
            config.ssl_ca = strdup(*(String::Utf8Value(ca_v)));
          if (capath_v->IsString() && capath_v->ToString()->Length() > 0)
            config.ssl_capath = strdup(*(String::Utf8Value(capath_v)));
          if (cipher_v->IsString() && cipher_v->ToString()->Length() > 0)
            config.ssl_cipher = strdup(*(String::Utf8Value(cipher_v)));
          else if (!(cipher_v->IsBoolean() && !cipher_v->BooleanValue()))
            config.ssl_cipher = DEFAULT_CIPHER;

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
                      config.ssl_cipher);
      }

      // always disable auto-reconnect
      my_bool reconnect = 0;
      mysql_options(&mysql, MYSQL_OPT_RECONNECT, &reconnect);
    }

    inline Handle<String> FieldTypeToString(enum_field_types v) {
      NanScope();
      Local<String> ret;
      // http://dev.mysql.com/doc/refman/5.7/en/c-api-data-structures.html
      switch (v) {
#define X(suffix, abbr, literal)                      \
        case MYSQL_TYPE_##suffix##:                   \
          ret = NanNew<String>(col_##abbr##_symbol);  \
        break;
        FIELD_TYPES
#undef X
        default:
          ret = NanNew<String>(col_unsup_symbol);
      }
      NanReturnValue(ret);
    }

    static NAN_METHOD(New) {
      DBG_LOG("new Client()\n");
      NanScope();

      if (!args.IsConstructCall()) {
        return NanThrowTypeError(
          "Use `new` to create instances of this object."
        );
      }
      if (args.Length() == 0 || !args[0]->IsObject())
        return NanThrowTypeError("Missing setup object");

      Local<Object> cfg = args[0]->ToObject();
#define X(name)                                                                \
      Local<Value> v_on##name## = cfg->Get(NanNew<String>(ev_##name##_symbol));\
      if (!v_on##name##->IsFunction())                                         \
        return NanThrowTypeError("Missing on" #name " handler");
      EVENT_NAMES
#undef X
      Local<Value> context_v = cfg->Get(NanNew<String>(context_symbol));
      Local<Value> conncfg_v = cfg->Get(NanNew<String>(conncfg_symbol));

      Client *obj = new Client();

#define X(name)                                                                \
      obj->on##name## = new NanCallback(Local<Function>::Cast(v_on##name##));
      EVENT_NAMES
#undef X
      if (context_v->IsObject())
        NanAssignPersistent(obj->context, context_v->ToObject());
      else
        NanAssignPersistent(obj->context, NanGetCurrentContext()->Global());
      if (conncfg_v->IsObject())
        obj->apply_config(conncfg_v->ToObject());
      obj->Wrap(args.This());

      NanReturnValue(args.This());
    }

    static NAN_METHOD(SetConfig) {
      DBG_LOG("client->setConfig()\n");
      NanScope();
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      if (args.Length() > 0 && args[0]->IsObject()) {
        Local<Object> cfg = args[0]->ToObject();
        obj->apply_config(cfg);
      }

      NanReturnUndefined();
    }

    static NAN_METHOD(Connect) {
      DBG_LOG("client->connect()\n");
      NanScope();
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      if (obj->state != STATE_CLOSED)
        return NanThrowError("Already connected");

      if (args.Length() > 0 && args[0]->IsObject()) {
        Local<Object> cfg = args[0]->ToObject();
        obj->apply_config(cfg);
      } else
        obj->init();

      obj->connect();

      NanReturnUndefined();
    }

    static NAN_METHOD(Close) {
      DBG_LOG("client->close()\n");
      NanScope();
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      if (!obj->close())
        return NanThrowError("Not connected");

      NanReturnUndefined();
    }

    static NAN_METHOD(Pause) {
      DBG_LOG("client->pause()\n");
      NanScope();
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      obj->pause();

      NanReturnUndefined();
    }

    static NAN_METHOD(Resume) {
      DBG_LOG("client->resume()\n");
      NanScope();
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      obj->resume();

      NanReturnUndefined();
    }

    static NAN_METHOD(Query) {
      DBG_LOG("client->query()\n");
      NanScope();
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      if (obj->state != STATE_IDLE)
        return NanThrowError("Not ready to query");
      if (args.Length() < 3)
        return NanThrowTypeError("Missing arguments");
      if (!args[0]->IsString())
        return NanThrowTypeError("query argument must be a string");
      if (!args[1]->IsBoolean())
        return NanThrowTypeError("columns argument must be a boolean");
      if (!args[2]->IsBoolean())
        return NanThrowTypeError("metadata argument must be a boolean");

      String::Utf8Value query(args[0]);
      obj->query(*query, args[1]->BooleanValue(), args[2]->BooleanValue());

      NanReturnUndefined();
    }

    static NAN_METHOD(Escape) {
      DBG_LOG("client->escape()\n");
      NanScope();
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      if (obj->state == STATE_CLOSED)
        return NanThrowError("Not connected");
      else if (args.Length() == 0 || !args[0]->IsString())
        return NanThrowTypeError("You must supply a string");

      String::Utf8Value arg_v(args[0]);
      unsigned long arg_len = arg_v.length();
      char *result = (char*) malloc(arg_len * 2 + 1);
      unsigned long result_len = obj->escape((char*)*arg_v, arg_len, result);
      Local<String> escaped_s = NanNew<String>(result, result_len);
      free(result);
      NanReturnValue(escaped_s);
    }

    static NAN_METHOD(IsMariaDB) {
      DBG_LOG("client->isMariaDB()\n");
      NanScope();
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      if (obj->state == STATE_CLOSED)
        return NanThrowError("Not connected");

      NanReturnValue(NanNew<Boolean>(mariadb_connection(&obj->mysql) == 1));
    }

    static void Initialize(Handle<Object> target) {
      NanScope();

      Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);

      NanAssignPersistent(constructor, tpl);
      tpl->InstanceTemplate()->SetInternalFieldCount(1);
      tpl->SetClassName(NanNew<String>("Client"));

      NanAssignPersistent(code_symbol, NanNew<String>("code"));
      NanAssignPersistent(context_symbol, NanNew<String>("context"));
      NanAssignPersistent(conncfg_symbol, NanNew<String>("config"));

#define X(name)                                                                \
      NanAssignPersistent(ev_##name##_symbol, NanNew<String>("on" #name));
      EVENT_NAMES
#undef X

#define X(suffix, abbr, literal)                                               \
      NanAssignPersistent(col_##abbr##_symbol, NanNew<String>(#literal));
      FIELD_TYPES
#undef X
      NanAssignPersistent(col_unsup_symbol,
                          NanNew<String>("[Unknown field type]"));

#define X(name)                                                                \
      NanAssignPersistent(cfg_##name##_symbol, NanNew<String>(#name));
      CFG_OPTIONS
      CFG_OPTIONS_SSL
#undef X

      NODE_SET_PROTOTYPE_METHOD(tpl, "connect", Connect);
      NODE_SET_PROTOTYPE_METHOD(tpl, "query", Query);
      NODE_SET_PROTOTYPE_METHOD(tpl, "setConfig", SetConfig);
      NODE_SET_PROTOTYPE_METHOD(tpl, "pause", Pause);
      NODE_SET_PROTOTYPE_METHOD(tpl, "resume", Resume);
      //NODE_SET_PROTOTYPE_METHOD(tpl, "abortQuery", AbortQuery);
      NODE_SET_PROTOTYPE_METHOD(tpl, "escape", Escape);
      NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
      NODE_SET_PROTOTYPE_METHOD(tpl, "isMariaDB", IsMariaDB);

      target->Set(NanNew<String>("Client"), tpl->GetFunction());
    }
};

static NAN_METHOD(Escape) {
  DBG_LOG("Client::escape()\n");
  NanScope();

  if (args.Length() == 0 || !args[0]->IsString())
    return NanThrowTypeError("You must supply a string");

  String::Utf8Value arg_v(args[0]);
  unsigned long arg_len = arg_v.length();
  char *result = (char*) malloc(arg_len * 2 + 1);
  unsigned long result_len =
      mysql_escape_string_ex(result, (char*)*arg_v, arg_len, "utf8");
  Local<String> escaped_s = NanNew<String>(result, result_len);
  free(result);

  NanReturnValue(escaped_s);
}

static Handle<Value> Version(const Arguments& args) {
  DBG_LOG("version()\n");
  NanScope();

  NanReturnValue(NanNew<String>(mysql_get_client_info()));
}

extern "C" {
  void init(Handle<Object> target) {
    NanScope();

    Client::Initialize(target);
    target->Set(NanNew<String>("escape"),
                NanNew<FunctionTemplate>(Escape)->GetFunction());
    target->Set(NanNew<String>("version"),
                NanNew<FunctionTemplate>(Version)->GetFunction());
  }

  NODE_MODULE(sqlclient, init);
}
