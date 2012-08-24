#include <node.h>
#include <node_buffer.h>
#include <stdlib.h>
#include <stdio.h>
#include <mysql/mysql.h>

using namespace node;
using namespace v8;

static Persistent<FunctionTemplate> Client_constructor;
static Persistent<String> emit_symbol;

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
          STATE_KILL = 12,
          STATE_KILLING = 13,
          STATE_KILLED = 14,
          STATE_RESULTERR = 15,
          STATE_QUERYABORTED = 16;

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
char* connChk = (char*) malloc(1);

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
    uv_poll_t poll_handle;
    uv_os_sock_t mysql_sock;
    MYSQL mysql, *mysql_ret;
    MYSQL_RES *mysql_res;
    MYSQL_ROW mysql_row;
    int mysql_qerr;
    char* cur_query;
    sql_config config;
    bool hadError, aborting;
    int state, deferredState;

    Client() {
      state = STATE_CLOSED;
    }

    ~Client() {
      close();
    }

    void init() {
      config.user = NULL;
      config.password = NULL;
      config.ip = NULL;
      config.db = NULL;
      cur_query = NULL;
      hadError = aborting = false;
      deferredState = STATE_NULL;
      poll_handle.type = UV_UNKNOWN_HANDLE;

      mysql_sock = NULL;
      mysql_init(&mysql);
      mysql_options(&mysql, MYSQL_OPT_NONBLOCK, 0);
    }

    void connect() {
      if (state == STATE_CLOSED) {
        state = STATE_CONNECT;
        doWork();
      }
    }

    void close() {
      if (state != STATE_CLOSED) {
        FREE(config.user);
        FREE(config.password);
        FREE(config.ip);
        FREE(config.db);
        FREE(cur_query);
        uv_poll_stop(&poll_handle);
        if (mysql_errno(&mysql) == 2013) {
          mysql_close(&mysql);
          state = STATE_CLOSED;
          uv_close((uv_handle_t*) &poll_handle, cbClose);
        } else {
          state = STATE_CLOSE;
          doWork();
        }
      }
    }

    char* escape(const char* str) {
      unsigned int str_len = strlen(str);
      char* dest = (char*) malloc(str_len * 2 + 1);
      mysql_real_escape_string(&mysql, dest, str, str_len);
      return dest;
    }

    void abortQuery() {
      if (state >= STATE_QUERY && state <= STATE_ROWSTREAMED) {
        FREE(cur_query);
        aborting = true;
      }
    }

    void query(const char* qry) {
      if (state == STATE_CONNECTED) {
        FREE(cur_query);
        cur_query = strdup(qry);
        state = STATE_QUERY;
        doWork();
      }
    }

    void doWork(int event = 0) {
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
            uv_poll_init_socket(uv_default_loop(), &poll_handle,
                                mysql_sock);
            uv_poll_start(&poll_handle, UV_READABLE, cbPoll);
            poll_handle.data = this;
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
                return emitError("conn", true);
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
              return emit("queryAbort");
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
            if (aborting) {
              FREE(cur_query);
              aborting = false;
              state = STATE_KILL;
              deferredState = STATE_QUERYABORTED;
            } else {
              status = mysql_real_query_cont(&mysql_qerr, &mysql,
                                             mysql_status(event));
              if (status)
                done = true;
              else {
                FREE(cur_query);
                if (mysql_qerr) {
                  state = STATE_CONNECTED;
                  return emitError("query");
                }
                state = STATE_QUERIED;
              }
            }
            break;
          case STATE_QUERIED:
            if (aborting) {
              aborting = false;
              state = STATE_CONNECTED;
              return emit("queryAbort");
            }
            mysql_res = mysql_use_result(&mysql);
            if (!mysql_res) {
              if (mysql_errno(&mysql)) {
                state = STATE_CONNECTED;
                return emitError("query");
              }
              my_ulonglong insert_id = mysql_insert_id(&mysql),
                           affected_rows = mysql_affected_rows(&mysql);
              state = STATE_CONNECTED;
              return emitDone(insert_id, affected_rows);
            } else
              state = STATE_ROWSTREAM;
            break;
          case STATE_ROWSTREAM:
            if (aborting) {
              aborting = false;
              state = STATE_RESULTFREE;
              deferredState = STATE_QUERYABORTED;
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
              deferredState = STATE_QUERYABORTED;
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
              deferredState = STATE_QUERYABORTED;
              done = true;
            } else if (mysql_row) {
              state = STATE_ROWSTREAM;
              emitRow();
            } else {
              if (mysql_errno(&mysql)) {
                deferredState = STATE_RESULTERR;
                state = STATE_RESULTFREE;
              } else {
                // no more rows
                my_ulonglong insert_id = mysql_insert_id(&mysql),
                             affected_rows = mysql_affected_rows(&mysql),
                             num_rows = mysql_num_rows(mysql_res);
                mysql_free_result(mysql_res);
                state = STATE_CONNECTED;
                return emitDone(insert_id, affected_rows, num_rows);
              }
            }
            break;
          case STATE_KILL:
            char killquery[128];
            sprintf(killquery, "KILL QUERY %u", mysql_thread_id(&mysql));
            status = mysql_real_query_start(&mysql_qerr, &mysql, killquery,
                                            strlen(killquery));
            if (status) {
              state = STATE_KILLING;
              done = true;
            } else
              state = STATE_KILLED;
            break;
          case STATE_KILLING:
            status = mysql_real_query_cont(&mysql_qerr, &mysql,
                                           mysql_status(event));
            if (status)
              done = true;
            else
              state = STATE_KILLED;
          case STATE_KILLED:
            if (deferredState == STATE_NULL)
              return emit("queryAbort");
            else {
              state = deferredState;
              deferredState = STATE_NULL;
            }
            break;
          case STATE_RESULTERR:
            state = STATE_CONNECTED;
            return emitError("result");
          case STATE_QUERYABORTED:
            state = STATE_CONNECTED;
            return emit("queryAbort");
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
            if (deferredState == STATE_NULL)
              return emit("queryAbort");
            else {
              state = deferredState;
              deferredState = STATE_NULL;
            }
            break;
          case STATE_CLOSE:
            mysql_close(&mysql);
            state = STATE_CLOSED;
            uv_close((uv_handle_t*) &poll_handle, cbClose);
            return;
          case STATE_CLOSED:
            return;
        }
      }
      if (status & MYSQL_WAIT_READ)
        new_events |= UV_READABLE;
      if (status & MYSQL_WAIT_WRITE)
        new_events |= UV_WRITABLE;
      uv_poll_start(&poll_handle, new_events, cbPoll);
    }

    void emitError(const char* when, bool doClose = false,
                   unsigned int errNo = 0, const char* errMsg = NULL) {
      HandleScope scope;
      hadError = true;
      Local<Function> Emit = Local<Function>::Cast(handle_->Get(emit_symbol));
      unsigned int errCode = mysql_errno(&mysql);
      if (errNo > 0)
        errCode = errNo;
        Local<Value> err =
          Exception::Error(String::New(errMsg ? errMsg : mysql_error(&mysql)));
      Local<Object> err_obj = err->ToObject();
      err_obj->Set(String::New("code"),
                   Integer::NewFromUnsigned(errCode));
      err_obj->Set(String::New("when"), String::New(when));
      Local<Value> emit_argv[2] = {
        String::New("error"),
        err
      };
      TryCatch try_catch;
      Emit->Call(handle_, 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
      if (doClose || errCode == 2013 || errCode == ERROR_HANGUP)
        close();
    }

    void emit(const char* eventName) {
      HandleScope scope;
      Local<Function> Emit = Local<Function>::Cast(handle_->Get(emit_symbol));
      Local<Value> emit_argv[1] = {
        String::New(eventName)
      };
      TryCatch try_catch;
      Emit->Call(handle_, 1, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
    }

    void emitDone(my_ulonglong insert_id, my_ulonglong affected_rows,
                  my_ulonglong num_rows = 0) {
      HandleScope scope;
      Local<Function> Emit = Local<Function>::Cast(handle_->Get(emit_symbol));
      Local<Object> info = Object::New();
      info->Set(String::New("insertId"), Number::New(insert_id));
      info->Set(String::New("affectedRows"),
                Number::New(affected_rows == (my_ulonglong)-1
                            ? 0
                            : affected_rows));
      info->Set(String::New("numRows"), Number::New(num_rows));
      Local<Value> emit_argv[2] = {
        String::New("done"),
        info
      };
      TryCatch try_catch;
      Emit->Call(handle_, 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
      
    }

    void emitRow() {
      HandleScope scope;
      MYSQL_FIELD* field;
      unsigned int i = 0, len = mysql_num_fields(mysql_res)/*,
                   j = 0, vlen*/;
      unsigned long* lengths = mysql_fetch_lengths(mysql_res);
      Local<Function> Emit = Local<Function>::Cast(handle_->Get(emit_symbol));
      Local<Object> row = Object::New();
      for (; i<len; ++i) {
        field = mysql_fetch_field_direct(mysql_res, i);
        if (mysql_row[i] == NULL)
          row->Set(String::New(field->name, field->name_length), Null());
        else {
          if (IS_BINARY(field)) {
            /*vlen = lengths[i];
            uint16_t* newbuf = new uint16_t[vlen];
            for (j = 0; j < vlen; ++j)
              newbuf[j] = (uint16_t) mysql_row[i][j];
            row->Set(String::New(field->name, field->name_length),
                     String::New(newbuf, vlen));
            */
            row->Set(String::New(field->name, field->name_length),
                     Local<Value>::New(
                      Buffer::New((char*)mysql_row[i], lengths[i])->handle_
                     ));
          } else {
            row->Set(String::New(field->name, field->name_length),
                     String::New(mysql_row[i], lengths[i]));
          }
        }
      }
      Local<Value> emit_argv[2] = {
        String::New("result"),
        row
      };
      TryCatch try_catch;
      Emit->Call(handle_, 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
    }

    static void cbPoll(uv_poll_t* handle, int status, int events) {
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
        int r = recv(obj->mysql_sock, connChk, 1, MSG_PEEK);
        if (r == 0 || (r == -1 && CHECK_CONNRESET)
            && obj->state == STATE_CONNECTED)
          return obj->emitError("conn", true, ERROR_HANGUP, STR_ERROR_HANGUP);
      }
      obj->doWork(mysql_status);
    }

    static void cbClose(uv_handle_t* handle) {
      HandleScope scope;
      Client* obj = (Client*) handle->data;
      Local<Function> Emit = Local<Function>::Cast(obj->handle_->Get(emit_symbol));
      TryCatch try_catch;
      Local<Value> emit_argv[2] = {
        String::New("close"),
        Local<Boolean>::New(Boolean::New(obj->hadError))
      };
      Emit->Call(obj->handle_, 2, emit_argv);
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
      Local<Value> user_v = cfg->Get(String::New("user"));
      Local<Value> password_v = cfg->Get(String::New("password"));
      Local<Value> ip_v = cfg->Get(String::New("host"));
      Local<Value> port_v = cfg->Get(String::New("port"));
      Local<Value> db_v = cfg->Get(String::New("db"));
      Local<Value> compress_v = cfg->Get(String::New("compress"));
      Local<Value> ssl_v = cfg->Get(String::New("secure"));

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
      obj->abortQuery();
      return Undefined();
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
      obj->query(*query);
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
      return scope.Close(Boolean::New(mariadb_connection(&obj->mysql)));
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

      emit_symbol = NODE_PSYMBOL("emit");
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