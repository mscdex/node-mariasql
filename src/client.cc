#include <node.h>
#include <node_buffer.h>
#include <stdlib.h>
#include <stdio.h>
#include <mysql/mysql.h>

using namespace node;
using namespace v8;

static Persistent<FunctionTemplate> Client_constructor;
static Persistent<String> emit_symbol;
static Persistent<String> result_symbol;
static Persistent<String> done_symbol;
static Persistent<String> code_symbol;
static Persistent<String> close_symbol;
static Persistent<String> insert_id_symbol;
static Persistent<String> affected_rows_symbol;
static Persistent<String> num_rows_symbol;
static Persistent<String> cfg_user_symbol;
static Persistent<String> cfg_pwd_symbol;
static Persistent<String> cfg_host_symbol;
static Persistent<String> cfg_port_symbol;
static Persistent<String> cfg_db_symbol;
static Persistent<String> cfg_compress_symbol;
static Persistent<String> cfg_ssl_symbol;
static Persistent<Function> Emit;

const int STATE_NULL = -100,
          STATE_CLOSE = -2,
          STATE_CLOSED = -1,
          STATE_CONNECT = 0,
          STATE_CONNECTING = 1,
          STATE_CONNECTED = 2,
          STATE_QUERY = 3,
          STATE_QUERYING = 4,
          STATE_QUERIED = 5,
          STATE_ROWSTREAM = 6,
          STATE_ROWSTREAMING = 7,
          STATE_ROWSTREAMED = 8,
          STATE_RESULTFREE = 9,
          STATE_RESULTFREEING = 10,
          STATE_RESULTFREED = 11,
          STATE_RESULTERR = 12,
          STATE_QUERYABORTED = 13;

struct sql_config {
  char* user;
  char* password;
  char* ip;
  char* db;
  unsigned int port;
  bool compress;
};

// used with recv peek to check for disconnection during idle,
// long-running query, etc.
char* conn_check_buf = (char*) malloc(1);

#ifdef _WIN32
# define CHECK_CONNRESET (WSAGetLastError() == WSAECONNRESET   ||  \
                          WSAGetLastError() == WSAENOTCONN     ||  \
                          WSAGetLastError() == WSAECONNABORTED ||  \
                          WSAGetLastError() == WSAENETRESET    ||  \
                          WSAGetLastError() == WSAENETDOWN)
#else
# include <errno.h>
# define CHECK_CONNRESET (errno == ECONNRESET || errno == ENOTCONN)
#endif

#define ERROR_HANGUP 10001
#define STR_ERROR_HANGUP "Disconnected from the server"

#define FREE(v) if (v) { free(v); v = NULL; }
#define IS_BINARY(f) ((f->flags & BINARY_FLAG) &&             \
                      ((f->type == MYSQL_TYPE_TINY_BLOB)   || \
                       (f->type == MYSQL_TYPE_MEDIUM_BLOB) || \
                       (f->type == MYSQL_TYPE_BLOB)        || \
                       (f->type == MYSQL_TYPE_LONG_BLOB)   || \
                       (f->type == MYSQL_TYPE_STRING)      || \
                       (f->type == MYSQL_TYPE_VAR_STRING)))

class Client : public ObjectWrap {
  public:
    uv_poll_t* poll_handle;
    uv_os_sock_t mysql_sock;
    MYSQL mysql, *mysql_ret;
    MYSQL_RES *mysql_res;
    MYSQL_ROW mysql_row;
    int mysql_qerr;
    char* cur_query;
    bool query_use_array;
    sql_config config;
    bool had_error, aborting, destructing;
    int state, deferred_state;

    Client() {
      state = STATE_CLOSED;
    }

    ~Client() {
      destructing = true;
      close();
    }

    void init() {
      config.user = NULL;
      config.password = NULL;
      config.ip = NULL;
      config.db = NULL;
      cur_query = NULL;
      had_error = aborting = destructing = false;
      deferred_state = STATE_NULL;
      poll_handle = NULL;

      mysql_sock = 0;
      mysql_init(&mysql);
      mysql_options(&mysql, MYSQL_OPT_NONBLOCK, 0);
    }

    void connect() {
      if (state == STATE_CLOSED) {
        state = STATE_CONNECT;
        do_work();
      }
    }

    void close() {
      FREE(config.user);
      FREE(config.password);
      FREE(config.ip);
      FREE(config.db);
      FREE(cur_query);
      if (state != STATE_CLOSED) {
        if (destructing) {
          if (poll_handle)
            uv_poll_stop(poll_handle);
          FREE(poll_handle);
          mysql_close(&mysql);
        } else if (mysql_errno(&mysql) == 2013) {
          mysql_close(&mysql);
          state = STATE_CLOSED;
          uv_close((uv_handle_t*) poll_handle, cb_close);
        } else {
          state = STATE_CLOSE;
          do_work();
        }
      }
    }

    static void cb_close(uv_handle_t* handle) {
      HandleScope scope;
      Client* obj = (Client*) handle->data;
      TryCatch try_catch;
      Handle<Value> emit_argv[2] = {
        close_symbol,
        Local<Boolean>::New(Boolean::New(obj->had_error))
      };
      Emit->Call(obj->handle_, 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
      FREE(obj->poll_handle);
    }

    char* escape(const char* str) {
      unsigned int str_len = strlen(str);
      char* dest = (char*) malloc(str_len * 2 + 1);
      mysql_real_escape_string(&mysql, dest, str, str_len);
      return dest;
    }

    void abort_query() {
      if (state == STATE_QUERY || state == STATE_QUERIED
          || (state >= STATE_ROWSTREAM && state <= STATE_ROWSTREAMED)) {
        FREE(cur_query);
        aborting = true;
      }
    }

    void query(const char* qry, bool use_array = false) {
      if (state == STATE_CONNECTED) {
        FREE(cur_query);
        cur_query = strdup(qry);
        query_use_array = use_array;
        state = STATE_QUERY;
        do_work();
      }
    }

    void do_work(int event = 0) {
      int status = 0, new_events = 0;
      bool done = false;
      while (!done) {
        switch (state) {
          case STATE_CONNECT:
            status = mysql_real_connect_start(&mysql_ret, &mysql,
                                              config.ip,
                                              config.user,
                                              config.password,
                                              config.db,
                                              config.port, NULL, 0);
            mysql_sock = mysql_get_socket(&mysql);
            poll_handle = (uv_poll_t*)malloc(sizeof(uv_poll_t));
            uv_poll_init_socket(uv_default_loop(), poll_handle,
                                mysql_sock);
            uv_poll_start(poll_handle, UV_READABLE, cb_poll);
            poll_handle->data = this;
            if (status) {
              done = true;
              state = STATE_CONNECTING;
            } else {
              state = STATE_CONNECTED;
              return emit("connect");
            }
            break;
          case STATE_CONNECTING:
            status = mysql_real_connect_cont(&mysql_ret, &mysql, event);
            if (status)
              done = true;
            else {
              if (!mysql_ret)
                return emit_error("conn.error", true);
              state = STATE_CONNECTED;
              return emit("connect");
            }
            break;
          case STATE_CONNECTED:
            return;
          case STATE_QUERY:
            if (aborting) {
              FREE(cur_query);
              aborting = false;
              state = STATE_CONNECTED;
              return emit("query.abort");
            }
            status = mysql_real_query_start(&mysql_qerr, &mysql, cur_query,
                                            strlen(cur_query));
            if (status) {
              state = STATE_QUERYING;
              done = true;
            } else {
              FREE(cur_query);
              state = STATE_QUERIED;
            }
            break;
          case STATE_QUERYING:
            status = mysql_real_query_cont(&mysql_qerr, &mysql,
                                           mysql_status(event));
            if (status)
              done = true;
            else {
              FREE(cur_query);
              if (mysql_qerr) {
                state = STATE_CONNECTED;
                return emit_error("query.error");
              }
              state = STATE_QUERIED;
            }
            break;
          case STATE_QUERIED:
            if (aborting) {
              aborting = false;
              state = STATE_CONNECTED;
              return emit("query.abort");
            }
            mysql_res = mysql_use_result(&mysql);
            if (!mysql_res) {
              if (mysql_errno(&mysql)) {
                state = STATE_CONNECTED;
                return emit_error("query.error");
              }
              my_ulonglong insert_id = mysql_insert_id(&mysql),
                           affected_rows = mysql_affected_rows(&mysql);
              state = STATE_CONNECTED;
              return emit_done(insert_id, affected_rows);
            } else
              state = STATE_ROWSTREAM;
            break;
          case STATE_ROWSTREAM:
            if (aborting) {
              aborting = false;
              state = STATE_RESULTFREE;
              deferred_state = STATE_QUERYABORTED;
              return;
            }
            status = mysql_fetch_row_start(&mysql_row, mysql_res);
            if (status) {
              done = true;
              state = STATE_ROWSTREAMING;
            } else
              state = STATE_ROWSTREAMED;
            break;
          case STATE_ROWSTREAMING:
            if (aborting) {
              aborting = false;
              state = STATE_RESULTFREE;
              deferred_state = STATE_QUERYABORTED;
              return;
            }
            status = mysql_fetch_row_cont(&mysql_row, mysql_res,
                                          mysql_status(event));
            if (status)
              done = true;
            else
              state = STATE_ROWSTREAMED;
            break;
          case STATE_ROWSTREAMED:
            if (aborting) {
              aborting = false;
              state = STATE_RESULTFREE;
              deferred_state = STATE_QUERYABORTED;
              done = true;
            } else if (mysql_row) {
              state = STATE_ROWSTREAM;
              emit_row();
            } else {
              if (mysql_errno(&mysql)) {
                state = STATE_RESULTFREE;
                deferred_state = STATE_RESULTERR;
              } else {
                // no more rows
                my_ulonglong insert_id = mysql_insert_id(&mysql),
                             affected_rows = mysql_affected_rows(&mysql),
                             num_rows = mysql_num_rows(mysql_res);
                mysql_free_result(mysql_res);
                state = STATE_CONNECTED;
                return emit_done(insert_id, affected_rows, num_rows);
              }
            }
            break;
          case STATE_RESULTERR:
            state = STATE_CONNECTED;
            return emit_error("result.error");
          case STATE_QUERYABORTED:
            state = STATE_CONNECTED;
            return emit("query.abort");
          case STATE_RESULTFREE:
            status = mysql_free_result_start(mysql_res);
            if (status) {
              state = STATE_RESULTFREEING;
              done = true;
            } else
              state = STATE_RESULTFREED;
            break;
          case STATE_RESULTFREEING:
            status = mysql_free_result_cont(mysql_res, mysql_status(event));
            if (status)
              done = true;
            else
              state = STATE_RESULTFREED;
            break;
          case STATE_RESULTFREED:
            state = STATE_CONNECTED;
            if (deferred_state == STATE_NULL)
              return emit("query.abort");
            else {
              state = deferred_state;
              deferred_state = STATE_NULL;
            }
            break;
          case STATE_CLOSE:
            mysql_close(&mysql);
            state = STATE_CLOSED;
            uv_close((uv_handle_t*) poll_handle, cb_close);
            return;
          case STATE_CLOSED:
            return;
        }
      }
      if (status & MYSQL_WAIT_READ)
        new_events |= UV_READABLE;
      if (status & MYSQL_WAIT_WRITE)
        new_events |= UV_WRITABLE;
      uv_poll_start(poll_handle, new_events, cb_poll);
    }

    static void cb_poll(uv_poll_t* handle, int status, int events) {
      HandleScope scope;
      Client* obj = (Client*) handle->data;
      assert(status == 0);

      int mysql_status = 0;
      if (events & UV_READABLE)
        mysql_status |= MYSQL_WAIT_READ;
      if (events & UV_WRITABLE)
        mysql_status |= MYSQL_WAIT_WRITE;
      /*if (events & UV_TIMEOUT)
        mysql_status |= MYSQL_WAIT_TIMEOUT;*/
      if (obj->mysql_sock) {
        // check for connection error
        int r = recv(obj->mysql_sock, conn_check_buf, 1, MSG_PEEK);
        if (r == 0 || (r == -1 && CHECK_CONNRESET)
            && obj->state == STATE_CONNECTED)
          return obj->emit_error("error", true, ERROR_HANGUP, STR_ERROR_HANGUP);
      }
      obj->do_work(mysql_status);
    }

    void emit(const char* eventName) {
      HandleScope scope;
      Local<Value> emit_argv[1] = {
        String::New(eventName)
      };
      TryCatch try_catch;
      Emit->Call(handle_, 1, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
    }

    void emit_error(const char* eventName, bool doClose = false,
                   unsigned int errNo = 0, const char* errMsg = NULL) {
      HandleScope scope;
      had_error = true;
      unsigned int errCode = mysql_errno(&mysql);
      if (errNo > 0)
        errCode = errNo;
      Local<Object> err =
          Exception::Error(
            String::New(errMsg ? errMsg : mysql_error(&mysql))
          )->ToObject();
      err->Set(code_symbol,
               Integer::NewFromUnsigned(errCode));
      Local<Value> emit_argv[2] = {
        String::New(eventName),
        err
      };
      TryCatch try_catch;
      Emit->Call(handle_, 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
      if (doClose || errCode == 2013 || errCode == ERROR_HANGUP)
        close();
    }

    void emit_done(my_ulonglong insert_id = 0, my_ulonglong affected_rows = 0,
                  my_ulonglong num_rows = 0) {
      HandleScope scope;
      Local<Object> info = Object::New();
      info->Set(insert_id_symbol, Number::New(insert_id));
      info->Set(affected_rows_symbol,
                Number::New(affected_rows == (my_ulonglong) - 1
                            ? 0
                            : affected_rows));
      info->Set(num_rows_symbol, Number::New(num_rows));
      Handle<Value> emit_argv[2] = {
        done_symbol,
        info
      };
      TryCatch try_catch;
      Emit->Call(handle_, 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
    }

    void emit_row() {
      HandleScope scope;
      MYSQL_FIELD* field;
      unsigned int f = 0, n_fields = mysql_num_fields(mysql_res),
                   i = 0, vlen;
      unsigned char *buf;
      uint16_t* new_buf;
      unsigned long* lengths = mysql_fetch_lengths(mysql_res);
      Handle<Value> field_value;
      Local<Object> row;
      if (query_use_array)
        row = Array::New(n_fields);
      else
        row = Object::New();
      for (; f<n_fields; ++f) {
        field = mysql_fetch_field_direct(mysql_res, f);
        if (mysql_row[f] == NULL)
          field_value = Null();
        else if (IS_BINARY(field)) {
          vlen = lengths[f];
          buf = (unsigned char*)(mysql_row[f]);
          new_buf = new uint16_t[vlen];
          for (i = 0; i < vlen; ++i)
            new_buf[i] = buf[i];
          field_value = String::New(new_buf, vlen);
        } else
          field_value = String::New(mysql_row[f], lengths[f]);
        if (query_use_array)
          row->Set(f, field_value);
        else
          row->Set(String::New(field->name, field->name_length), field_value);
      }
      Handle<Value> emit_argv[2] = {
        result_symbol,
        row
      };
      TryCatch try_catch;
      Emit->Call(handle_, 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
    }

    static Handle<Value> New(const Arguments& args) {
      HandleScope scope;

      if (!args.IsConstructCall()) {
        return ThrowException(Exception::TypeError(
          String::New("Use `new` to create instances of this object."))
        );
      }

      Client* obj = new Client();
      obj->Wrap(args.This());
      obj->Ref();

      if (Emit.IsEmpty())
        Emit = Persistent<Function>::New(Local<Function>::Cast(obj->handle_->Get(emit_symbol)));

      return args.This();
    }

    static Handle<Value> Escape(const Arguments& args) {
      HandleScope scope;
      Client* obj = ObjectWrap::Unwrap<Client>(args.This());

      if (obj->state < STATE_CONNECTED) {
        return ThrowException(Exception::Error(
          String::New("Not connected"))
        );
      } else if (args.Length() == 0 || !args[0]->IsString()) {
        return ThrowException(Exception::Error(
          String::New("You must supply a string"))
        );
      }
      String::Utf8Value arg_s(args[0]);
      char* newstr = obj->escape(*arg_s);
      Local<String> escaped_s = String::New(newstr);
      free(newstr);
      return scope.Close(escaped_s);
    }

    static Handle<Value> Connect(const Arguments& args) {
      HandleScope scope;
      Client* obj = ObjectWrap::Unwrap<Client>(args.This());

      if (obj->state != STATE_CLOSED) {
        return ThrowException(Exception::Error(
          String::New("Not ready to connect"))
        );
      }

      obj->init();

      Local<Object> cfg = args[0]->ToObject();
      Local<Value> user_v = cfg->Get(cfg_user_symbol);
      Local<Value> password_v = cfg->Get(cfg_pwd_symbol);
      Local<Value> ip_v = cfg->Get(cfg_host_symbol);
      Local<Value> port_v = cfg->Get(cfg_port_symbol);
      Local<Value> db_v = cfg->Get(cfg_db_symbol);
      //Local<Value> compress_v = cfg->Get(cfg_compress_symbol);
      //Local<Value> ssl_v = cfg->Get(cfg_ssl_symbol);

      if (!user_v->IsString() || user_v->ToString()->Length() == 0)
        obj->config.user = NULL;
      else {
        String::Utf8Value user_s(user_v);
        obj->config.user = strdup(*user_s);
      }
      if (!password_v->IsString() || password_v->ToString()->Length() == 0)
        obj->config.password = NULL;
      else {
        String::Utf8Value password_s(password_v);
        obj->config.password = strdup(*password_s);
      }
      if (!ip_v->IsString() || ip_v->ToString()->Length() == 0)
        obj->config.ip = NULL;
      else {
        String::Utf8Value ip_s(ip_v);
        obj->config.ip = strdup(*ip_s);
      }
      if (!port_v->IsUint32() || port_v->Uint32Value() == 0)
        obj->config.port = 3306;
      else
        obj->config.port = port_v->Uint32Value();

      if (db_v->IsString() && db_v->ToString()->Length() > 0) {
        String::Utf8Value db_s(db_v);
        obj->config.db = strdup(*db_s);
      }

      obj->connect();

      return Undefined();
    }

    static Handle<Value> AbortQuery(const Arguments& args) {
      HandleScope scope;
      Client* obj = ObjectWrap::Unwrap<Client>(args.This());
      obj->abort_query();
      return Undefined();
    }

    static Handle<Value> GetThreadID(const Arguments& args) {
      HandleScope scope;
      Client* obj = ObjectWrap::Unwrap<Client>(args.This());
      if (obj->state < STATE_CONNECTED) {
        return ThrowException(Exception::Error(
          String::New("Not connected"))
        );
      }
      return scope.Close(Integer::New(mysql_thread_id(&obj->mysql)));
    }

    static Handle<Value> Query(const Arguments& args) {
      HandleScope scope;
      Client* obj = ObjectWrap::Unwrap<Client>(args.This());
      if (obj->state != STATE_CONNECTED) {
        return ThrowException(Exception::Error(
          String::New("Not ready to query"))
        );
      }
      if (args.Length() == 0 || !args[0]->IsString()) {
        return ThrowException(Exception::Error(
          String::New("Query expected"))
        );
      }
      String::Utf8Value query(args[0]);
      obj->query(*query,
                 (args.Length() > 1 && args[1]->IsBoolean()
                  && args[1]->BooleanValue()));
      return Undefined();
    }

    static Handle<Value> Close(const Arguments& args) {
      HandleScope scope;
      Client* obj = ObjectWrap::Unwrap<Client>(args.This());
      if (obj->state == STATE_CLOSED) {
        return ThrowException(Exception::Error(
          String::New("Already closed"))
        );
      }
      obj->close();
      return Undefined();
    }

    static Handle<Value> IsMariaDB(const Arguments& args) {
      HandleScope scope;
      Client* obj = ObjectWrap::Unwrap<Client>(args.This());
      if (obj->state < STATE_CONNECTED) {
        return ThrowException(Exception::Error(
          String::New("Not connected"))
        );
      }
      return scope.Close(Boolean::New(mariadb_connection(&obj->mysql) == 1));
    }

    static void Initialize(Handle<Object> target) {
      HandleScope scope;

      Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
      Local<String> name = String::NewSymbol("Client");

      Client_constructor = Persistent<FunctionTemplate>::New(tpl);
      Client_constructor->InstanceTemplate()->SetInternalFieldCount(1);
      Client_constructor->SetClassName(name);

      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "connect", Connect);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "query", Query);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "abortQuery", AbortQuery);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "escape", Escape);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "end", Close);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "isMariaDB", IsMariaDB);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "threadId", GetThreadID);

      emit_symbol = NODE_PSYMBOL("emit");
      result_symbol = NODE_PSYMBOL("query.result");
      done_symbol = NODE_PSYMBOL("query.done");
      close_symbol = NODE_PSYMBOL("close");
      insert_id_symbol = NODE_PSYMBOL("insertId");
      affected_rows_symbol = NODE_PSYMBOL("affectedRows");
      num_rows_symbol = NODE_PSYMBOL("numRows");
      code_symbol = NODE_PSYMBOL("code");
      cfg_user_symbol = NODE_PSYMBOL("user");
      cfg_pwd_symbol = NODE_PSYMBOL("password");
      cfg_host_symbol = NODE_PSYMBOL("host");
      cfg_port_symbol = NODE_PSYMBOL("port");
      cfg_db_symbol = NODE_PSYMBOL("db");
      cfg_compress_symbol = NODE_PSYMBOL("compress");
      cfg_ssl_symbol = NODE_PSYMBOL("secure");

      target->Set(name, Client_constructor->GetFunction());
    }
};

static Handle<Value> Version(const Arguments& args) {
  HandleScope scope;
  return scope.Close(String::New(mysql_get_client_info()));
}

extern "C" {
  void init(Handle<Object> target) {
    HandleScope scope;
    Client::Initialize(target);
    target->Set(String::NewSymbol("version"),
                FunctionTemplate::New(Version)->GetFunction());
  }

  NODE_MODULE(sqlclient, init);
}