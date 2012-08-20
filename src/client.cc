#include <node.h>
#include <stdlib.h>
#include <mysql/mysql.h>

using namespace node;
using namespace v8;

static Persistent<FunctionTemplate> Client_constructor;
static Persistent<String> emit_symbol;

const int STATE_CLOSE = -2,
          STATE_CLOSED = -1,
          STATE_CONNECT = 0,
          STATE_CONNECTING = 1,
          STATE_CONNECTED = 2,
          STATE_QUERY = 3,
          STATE_QUERYING = 4,
          STATE_QUERIED = 5,
          STATE_ROWSTREAM = 6,
          STATE_ROWSTREAMING = 7,
          STATE_ROWSTREAMED = 8;

struct sql_config {
  char* user;
  char* password;
  char* ip;
  char* db;
  unsigned int port;
  bool compress;
};

#include <stdio.h>
#define DEBUG(s) fprintf(stderr, "BINDING: " s "\n")

class Client : public ObjectWrap {
  public:
    uv_poll_t poll_handle;
    MYSQL mysql, *mysql_ret;
    MYSQL_RES *mysql_res;
    MYSQL_ROW mysql_row;
    int mysql_qerr;
    char* cur_query;
    sql_config config;
    bool hadError;
    int state;

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
      hadError = false;
      poll_handle.type = UV_UNKNOWN_HANDLE;

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
        if (config.user) {
          free(config.user);
          config.user = NULL;
        }
        if (config.password) {
          free(config.password);
          config.password = NULL;
        }
        if (config.ip) {
          free(config.ip);
          config.ip = NULL;
        }
        if (config.db) {
          free(config.db);
          config.db = NULL;
        }
        if (cur_query) {
          free(cur_query);
          cur_query = NULL;
        }
        
        state = STATE_CLOSE;
        doWork();
      }
    }

    char* escape(const char* str) {
      unsigned int str_len = strlen(str);
      char* dest = (char*) malloc(str_len * 2 + 1);
      mysql_real_escape_string(&mysql, dest, str, str_len);
      return dest;
    }

    void query(const char* qry) {
      if (state == STATE_CONNECTED) {
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
//DEBUG("STATE_CONNECT");
            status = mysql_real_connect_start(&mysql_ret, &mysql,
                                              config.ip,
                                              config.user,
                                              config.password,
                                              config.db,
                                              config.port, NULL, 0);
            uv_poll_init_socket(uv_default_loop(), &poll_handle,
                                mysql_get_socket(&mysql));
            poll_handle.data = this;
            if (status) {
              state = STATE_CONNECTING;
              done = true;
            } else {
              state = STATE_CONNECTED;
              emit("connect");
            }
            break;
          case STATE_CONNECTING:
//DEBUG("STATE_CONNECTING");
            status = mysql_real_connect_cont(&mysql_ret, &mysql, event);
            if (status)
              done = true;
            else {
              if (!mysql_ret)
                return emitError("conn", true);
              state = STATE_CONNECTED;
              emit("connect");
            }
            break;
          case STATE_CONNECTED:
//DEBUG("STATE_CONNECTED");
            done = true;
            break;
          case STATE_QUERY:
//DEBUG("STATE_QUERY");
            status = mysql_real_query_start(&mysql_qerr, &mysql, cur_query,
                                            strlen(cur_query));
            if (status) {
              state = STATE_QUERYING;
              done = true;
            } else
              state = STATE_QUERIED;
            break;
          case STATE_QUERYING:
//DEBUG("STATE_QUERYING");
            status = mysql_real_query_cont(&mysql_qerr, &mysql,
                                           mysql_status(event));
            if (status)
              done = true;
            else {
              if (cur_query) {
                free(cur_query);
                cur_query = NULL;
              }
              if (mysql_qerr)
                return emitError("query");
              state = STATE_QUERIED;
            }
            break;
          case STATE_QUERIED:
//DEBUG("STATE_QUERIED");
            mysql_res = mysql_use_result(&mysql);
            if (!mysql_res) {
              if (mysql_errno(&mysql))
                return emitError("query");
              state = STATE_CONNECTED;
              emit("done");
            } else
              state = STATE_ROWSTREAM;
            break;
          case STATE_ROWSTREAM:
//DEBUG("STATE_ROWSTREAM");
            status = mysql_fetch_row_start(&mysql_row, mysql_res);
            if (status) {
              done = true;
              state = STATE_ROWSTREAMING;
            } else
              state = STATE_ROWSTREAMED;
            break;
          case STATE_ROWSTREAMING:
//DEBUG("STATE_ROWSTREAMING");
            status = mysql_fetch_row_cont(&mysql_row, mysql_res,
                                          mysql_status(event));
            if (status)
              done = true;
            else
              state = STATE_ROWSTREAMED;
            break;
          case STATE_ROWSTREAMED:
//DEBUG("STATE_ROWSTREAMED");
            if (mysql_row) {
              state = STATE_ROWSTREAM;
              emitRow();
            } else {
              if (mysql_errno(&mysql)) {
                mysql_free_result(mysql_res);
                return emitError("result");
              } else {
                // no more rows
                mysql_free_result(mysql_res);
                state = STATE_CONNECTED;
                emit("done");
              }
            }
            break;
          case STATE_CLOSE:
//DEBUG("STATE_CLOSE");
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

    void emitError(const char* when, bool doClose = false) {
      HandleScope scope;
      hadError = true;
      Local<Function> Emit = Local<Function>::Cast(handle_->Get(emit_symbol));
      Local<Value> err = Exception::Error(String::New(mysql_error(&mysql)));
      Local<Object> err_obj = err->ToObject();
      err_obj->Set(String::New("code"),
               Integer::NewFromUnsigned(mysql_errno(&mysql)));
      err_obj->Set(String::New("when"), String::New(when));
      Local<Value> emit_argv[2] = {
        String::New("error"),
        err
      };
      TryCatch try_catch;
      Emit->Call(handle_, 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
      if (doClose)
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

    void emitRow() {
      HandleScope scope;
      unsigned int i = 0, len = mysql_num_fields(mysql_res);
      char* field;
      Local<Function> Emit = Local<Function>::Cast(handle_->Get(emit_symbol));
      Local<Object> row = Object::New();
      for (; i<len; ++i) {
        field = mysql_fetch_field_direct(mysql_res, i)->name;
        row->Set(String::New(field), (mysql_row[i]
                                      ? String::New(mysql_row[i])
                                      : Null()));
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

    static void Initialize(Handle<Object> target) {
      HandleScope scope;

      Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
      Local<String> name = String::NewSymbol("Client");

      Client_constructor = Persistent<FunctionTemplate>::New(tpl);
      Client_constructor->InstanceTemplate()->SetInternalFieldCount(1);
      Client_constructor->SetClassName(name);

      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "connect", Connect);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "query", Query);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "escape", Escape);
      NODE_SET_PROTOTYPE_METHOD(Client_constructor, "end", Close);

      emit_symbol = NODE_PSYMBOL("emit");
      target->Set(name, Client_constructor->GetFunction());
    }
};

static Handle<Value> Version(const Arguments& args) {
  HandleScope scope;
  unsigned long client_ver = mysql_get_client_version();
  char major = (client_ver >> 16) & 0xFF,
       release = (client_ver >> 8) & 0xFF,
       rel_ver = client_ver & 0xFF;
  int slen = (major < 10 ? 1 : 2)
             + (release < 10 ? 1 : 2)
             + (rel_ver < 10 ? 1 : 2);
  char* ver = (char*) malloc(slen + 3);
  sprintf(ver, "%u.%u.%u", major, release, rel_ver);
  Local<String> ver_str = String::New(ver);
  free(ver);
  return scope.Close(ver_str);
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