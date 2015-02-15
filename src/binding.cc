#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <mysql.h>
#include <nan.h>

using namespace node;
using namespace v8;

static Persistent<FunctionTemplate> Client_constructor;

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
          STATE_NEXTQUERY = 9,
          STATE_NEXTQUERYING = 10,
          STATE_RESULTFREE = 11,
          STATE_RESULTFREEING = 12,
          STATE_RESULTFREED = 13,
          STATE_ROWERR = 14,
          STATE_QUERYERR = 15,
          STATE_ABORT = 16;

#define STATE_TEXT(s)                               \
    (s == STATE_NULL ? "NULL" :                     \
     s == STATE_CLOSE ? "CLOSE" :                   \
     s == STATE_CLOSED ? "CLOSED" :                 \
     s == STATE_CONNECT ? "CONNECT" :               \
     s == STATE_CONNECTING ? "CONNECTING" :         \
     s == STATE_CONNECTED ? "CONNECTED" :           \
     s == STATE_QUERY ? "QUERY" :                   \
     s == STATE_QUERYING ? "QUERYING" :             \
     s == STATE_QUERIED ? "QUERIED" :               \
     s == STATE_ROWSTREAM ? "ROWSTREAM" :           \
     s == STATE_ROWSTREAMING ? "ROWSTREAMING" :     \
     s == STATE_ROWSTREAMED ? "ROWSTREAMED" :       \
     s == STATE_NEXTQUERY ? "NEXTQUERY" :           \
     s == STATE_NEXTQUERYING ? "NEXTQUERYING" :     \
     s == STATE_RESULTFREE ? "RESULTFREE" :         \
     s == STATE_RESULTFREEING ? "RESULTFREEING" :   \
     s == STATE_RESULTFREED ? "RESULTFREED" :       \
     s == STATE_ROWERR ? "ROWERR" :                 \
     s == STATE_QUERYERR ? "QUERYERR" :             \
     s == STATE_ABORT ? "ABORT" : ""                \
    )


enum abort_t { ABORT_NONE, ABORT_QUERY, ABORT_RESULTS };

struct sql_config {
  char *user;
  char *password;
  char *ip;
  char *db;
  char *socket;
  unsigned int port;
  unsigned long client_opts;
  bool metadata;

  // ssl
  char *ssl_key;
  char *ssl_cert;
  char *ssl_ca;
  char *ssl_capath;
  char *ssl_cipher;
  char *charset;
};

struct sql_query {
  MYSQL_RES *result;
  MYSQL_ROW row;
  int err;
  char *str;
  bool use_array;
  abort_t abort;
};

// used with recv peek to check for disconnection during idle,
// long-running query, etc.
char *conn_check_buf = (char*) malloc(1);

const my_bool MY_BOOL_TRUE = 1,
              MY_BOOL_FALSE = 0;


#define INTERNAL_1_UV_VERSION 1

#if UV_VERSION_MAJOR < INTERNAL_1_UV_VERSION
  NAN_INLINE
    bool isPollErr(int status){
      return status != 0 && uv_last_error(uv_default_loop()).code == UV_EBADF;
    }
#else
  NAN_INLINE
    bool isPollErr(int status){
      return status != 0 && status == UV_EBADF;
    }
#endif
             

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

#define DEFAULT_CIPHER "ECDHE-RSA-AES128-SHA256:AES128-GCM-SHA256:RC4:HIGH:!MD5:!aNULL:!EDH"

#define ERROR_HANGUP 10001
#define STR_ERROR_HANGUP "Disconnected from the server"

#define FREE(p) if (p) { free(p); p = NULL; }

#define IS_BINARY(f) ((f.flags & BINARY_FLAG) &&             \
                      ((f.type == MYSQL_TYPE_TINY_BLOB)   || \
                       (f.type == MYSQL_TYPE_MEDIUM_BLOB) || \
                       (f.type == MYSQL_TYPE_BLOB)        || \
                       (f.type == MYSQL_TYPE_LONG_BLOB)   || \
                       (f.type == MYSQL_TYPE_STRING)      || \
                       (f.type == MYSQL_TYPE_VAR_STRING)))

class Client : public ObjectWrap {
  public:
    NanCallback *Emit;
    uv_poll_t *poll_handle;
    uv_os_sock_t mysql_sock;
    MYSQL mysql, *mysql_ret;
    sql_query cur_query;
    sql_config config;
    bool had_error, destructing;
    int state, deferred_state;

    Client() {
      state = STATE_CLOSED;
    }

    ~Client() {
      destructing = true;
      close();
      delete Emit;
    }

    void init() {
      config.user = NULL;
      config.password = NULL;
      config.ip = NULL;
      config.db = NULL;
      config.socket = NULL;
      config.client_opts = CLIENT_MULTI_RESULTS | CLIENT_REMEMBER_OPTIONS;
      config.metadata = false;
      config.ssl_key = NULL;
      config.ssl_cert = NULL;
      config.ssl_ca = NULL;
      config.ssl_capath = NULL;
      config.ssl_cipher = NULL;
      config.charset = NULL;
      had_error = destructing = false;
      deferred_state = STATE_NULL;
      poll_handle = NULL;

      cur_query.result = NULL;
      cur_query.err = 0;
      cur_query.str = NULL;
      cur_query.use_array = false;
      cur_query.abort = ABORT_NONE;

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
      FREE(config.socket);
      FREE(config.db);
      FREE(config.ssl_key);
      FREE(config.ssl_cert);
      FREE(config.ssl_ca);
      FREE(config.ssl_capath);
      FREE(config.ssl_cipher);
      FREE(config.charset);

      FREE(cur_query.result);
      cur_query.err = 0;
      FREE(cur_query.str);
      cur_query.use_array = false;
      cur_query.abort = ABORT_NONE;

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

    static void cb_close(uv_handle_t *handle) {
      NanScope();

      Client *obj = (Client*) handle->data;

      TryCatch try_catch;
      Handle<Value> emit_argv[2] = {
        NanNew<String>("close"),
        NanNew<Boolean>(obj->had_error)
      };
      obj->Emit->Call(NanObjectWrapHandle(obj), 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
      FREE(obj->poll_handle);
    }

    unsigned long escape(const char *source, unsigned long source_len,
                         char* dest) {
      return mysql_real_escape_string(&mysql, dest, source, source_len);
    }

    void abort_query(abort_t kind) {
      if (state >= STATE_CONNECTED) {
        FREE(cur_query.str);
        if (cur_query.abort < kind)
          cur_query.abort = kind;
      }
    }

    void query(const char *qry, bool use_array = false) {
      if (state == STATE_CONNECTED) {
        FREE(cur_query.str);
        cur_query.str = strdup(qry);
        cur_query.use_array = use_array;
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
            status = mysql_real_connect_start(&mysql_ret,
                                              &mysql,
                                              config.ip,
                                              config.user,
                                              config.password,
                                              config.db,
                                              config.port,
                                              config.socket,
                                              config.client_opts);
            if (!mysql_ret && mysql_errno(&mysql) > 0)
              return emit_error(NanNew<String>("conn.error"), true);

            mysql_sock = mysql_get_socket(&mysql);

            poll_handle = (uv_poll_t*) malloc(sizeof(uv_poll_t));
            uv_poll_init_socket(uv_default_loop(),
                                poll_handle,
                                mysql_sock);
            uv_poll_start(poll_handle, UV_READABLE, cb_poll);
            poll_handle->data = this;

            if (status) {
              done = true;
              state = STATE_CONNECTING;
            } else {
              state = STATE_CONNECTED;
              return emit(NanNew<String>("connect"));
            }
            break;
          case STATE_CONNECTING:
            status = mysql_real_connect_cont(&mysql_ret, &mysql, event);

            if (status)
              done = true;
            else {
              if (!mysql_ret)
                return emit_error(NanNew<String>("conn.error"), true);
              state = STATE_CONNECTED;
              return emit(NanNew<String>("connect"));
            }
            break;
          case STATE_CONNECTED:
            had_error = false;
            return;
          case STATE_QUERY:
            if (cur_query.abort) {
              FREE(cur_query.str);
              state = STATE_ABORT;
            } else {
              had_error = false;
              status = mysql_real_query_start(&cur_query.err,
                                              &mysql,
                                              cur_query.str,
                                              strlen(cur_query.str));
              if (status) {
                state = STATE_QUERYING;
                done = true;
              } else {
                if (!cur_query.err
                    || (cur_query.err && mysql_errno(&mysql) != 2013))
                  emit(NanNew<String>("results.query"));
                FREE(cur_query.str);
                if (cur_query.err) {
                  state = STATE_NEXTQUERY;
                  emit_error(NanNew<String>("query.error"));
                } else
                  state = STATE_QUERIED;
              }
            }
            break;
          case STATE_QUERYING:
            status = mysql_real_query_cont(&cur_query.err,
                                           &mysql,
                                           mysql_status(event));
            if (status)
              done = true;
            else {
              if (!cur_query.err
                  || (cur_query.err && mysql_errno(&mysql) != 2013))
                emit(NanNew<String>("results.query"));
              FREE(cur_query.str);
              if (cur_query.err) {
                state = STATE_NEXTQUERY;
                emit_error(NanNew<String>("query.error"));
              } else
                state = STATE_QUERIED;
            }
            break;
          case STATE_QUERIED:
            cur_query.result = mysql_use_result(&mysql);
            if (!cur_query.result) {
              state = STATE_NEXTQUERY;
              if (mysql_errno(&mysql) && !cur_query.abort)
                emit_error(NanNew<String>("query.error"));
              else if (!cur_query.abort) {
                my_ulonglong insert_id = mysql_insert_id(&mysql),
                             affected_rows = mysql_affected_rows(&mysql);
                emit_done(insert_id, affected_rows);
              }
            } else
              state = STATE_ROWSTREAM;
            break;
          case STATE_ROWSTREAM:
            if (cur_query.abort)
              state = STATE_ABORT;
            else {
              status = mysql_fetch_row_start(&cur_query.row, cur_query.result);
              if (status) {
                done = true;
                state = STATE_ROWSTREAMING;
              } else
                state = STATE_ROWSTREAMED;
            }
            break;
          case STATE_ROWSTREAMING:
            status = mysql_fetch_row_cont(&cur_query.row,
                                          cur_query.result,
                                          mysql_status(event));
            if (status)
              done = true;
            else
              state = STATE_ROWSTREAMED;
            break;
          case STATE_ROWSTREAMED:
            if (cur_query.abort)
              state = STATE_ABORT;
            else if (cur_query.row) {
              state = STATE_ROWSTREAM;
              emit_row();
            } else {
              if (mysql_errno(&mysql)) {
                state = STATE_RESULTFREE;
                deferred_state = STATE_ROWERR;
              } else {
                // no more rows
                my_ulonglong insert_id = mysql_insert_id(&mysql),
                             affected_rows = mysql_affected_rows(&mysql),
                             num_rows = mysql_num_rows(cur_query.result);
                mysql_free_result(cur_query.result);
                cur_query.result = NULL;
                state = STATE_NEXTQUERY;
                emit_done(insert_id, affected_rows, num_rows);
              }
            }
            break;
          case STATE_NEXTQUERY:
            if (!mysql_more_results(&mysql)) {
              state = STATE_CONNECTED;
              if (cur_query.abort == ABORT_RESULTS)
                emit(NanNew<String>("results.abort"));
              else
                emit(NanNew<String>("results.done"));
              if (cur_query.abort)
                cur_query.abort = ABORT_NONE;
              return;
            } else {
              had_error = false;
              status = mysql_next_result_start(&cur_query.err, &mysql);
              if (status) {
                state = STATE_NEXTQUERYING;
                done = true;
              } else {
                if (!cur_query.abort
                    && (!cur_query.err
                        || (cur_query.err && mysql_errno(&mysql) != 2013)))
                  emit(NanNew<String>("results.query"));
                if (cur_query.err) {
                  state = STATE_RESULTFREE;
                  deferred_state = STATE_ROWERR;
                } else
                  state = STATE_QUERIED;
              }
            }
            break;
          case STATE_NEXTQUERYING:
            status = mysql_next_result_cont(&cur_query.err,
                                            &mysql,
                                            mysql_status(event));
            if (status)
              done = true;
            else {
                if (!cur_query.abort
                    && (!cur_query.err
                        || (cur_query.err && mysql_errno(&mysql) != 2013)))
                emit(NanNew<String>("results.query"));
              if (cur_query.err) {
                state = STATE_RESULTFREE;
                deferred_state = STATE_ROWERR;
              } else
                state = STATE_QUERIED;
            }
            break;
          case STATE_ABORT:
            if (cur_query.result) {
              state = STATE_RESULTFREE;
              deferred_state = STATE_ABORT;
            } else {
              state = STATE_NEXTQUERY;
              if (cur_query.abort == ABORT_QUERY) {
                cur_query.abort = ABORT_NONE;
                emit(NanNew<String>("query.abort"));
              }
            }
            break;
          case STATE_QUERYERR:
            state = STATE_NEXTQUERY;
            if (!cur_query.abort)
              emit_error(NanNew<String>("query.error"));
            break;
          case STATE_ROWERR:
            state = STATE_NEXTQUERY;
            if (!cur_query.abort)
              emit_error(NanNew<String>("query.row.error"));
            break;
          case STATE_RESULTFREE:
            if (!cur_query.result)
              state = STATE_RESULTFREED;
            else {
              status = mysql_free_result_start(cur_query.result);
              if (status) {
                state = STATE_RESULTFREEING;
                done = true;
              } else
                state = STATE_RESULTFREED;
            }
            break;
          case STATE_RESULTFREEING:
            status = mysql_free_result_cont(cur_query.result,
                                            mysql_status(event));
            if (status)
              done = true;
            else
              state = STATE_RESULTFREED;
            break;
          case STATE_RESULTFREED:
            state = STATE_CONNECTED;
            cur_query.result = NULL;
            if (deferred_state != STATE_NULL) {
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

    static void cb_poll(uv_poll_t *handle, int status, int events) {
      NanScope();

      Client *obj = (Client*) handle->data;

      // for some reason no MySQL error is set when it cannot connect on *nix,
      // so we check for the invalid FD here ...
      if ( (isPollErr(status)) && obj->state == STATE_CONNECTING) {
        std::string errmsg("Can't connect to MySQL server on '");
        if (obj->config.socket)
          errmsg += obj->config.socket;
        else
          errmsg += obj->config.ip;
        errmsg += "' (0)";
        obj->emit_error(NanNew<String>("conn.error"), true, 2003, errmsg.c_str());
        return;
      }
      assert(status == 0);

      int mysql_status = 0;
      if (events & UV_READABLE)
        mysql_status |= MYSQL_WAIT_READ;
      if (events & UV_WRITABLE)
        mysql_status |= MYSQL_WAIT_WRITE;
      if (obj->mysql_sock) {
        // check for connection error
        int r = recv(obj->mysql_sock, conn_check_buf, 1, MSG_PEEK);
        if ((r == 0 || (r == -1 && CHECK_CONNRESET))
            && obj->state == STATE_CONNECTED) {
          return obj->emit_error(NanNew<String>("conn.error"), true, ERROR_HANGUP,
                                 STR_ERROR_HANGUP);
        }
      }
      obj->do_work(mysql_status);
    }

    void emit(Local<String>eventName) {
      NanScope();

      Handle<Value> emit_argv[1] = { eventName };
      TryCatch try_catch;
      Emit->Call(NanObjectWrapHandle(this), 1, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
    }

    void emit_error(Local<String>eventName, bool doClose = false,
                   unsigned int errNo = 0, const char *errMsg = NULL) {
      NanScope();

      had_error = true;
      unsigned int errCode = mysql_errno(&mysql);
      if (errNo > 0)
        errCode = errNo;
      Local<Object> err =
          Exception::Error(
            NanNew<String>(errMsg ? errMsg : mysql_error(&mysql))
          )->ToObject();
      err->Set(NanNew<String>("code"), NanNew<Integer, uint32_t>(errCode));
      Handle<Value> emit_argv[2] = { eventName, err };
      TryCatch try_catch;
      Emit->Call(NanObjectWrapHandle(this), 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
      if (doClose || errCode == 2013 || errCode == ERROR_HANGUP)
        close();
    }

    void emit_done(my_ulonglong insert_id = 0, my_ulonglong affected_rows = 0,
                  my_ulonglong num_rows = 0) {
      NanScope();

      Local<Object> info = NanNew<Object>();
      info->Set(NanNew<String>("insertId"), NanNew<Number>(insert_id));
      info->Set(NanNew<String>("affectedRows"),
                NanNew<Number>(affected_rows == (my_ulonglong) - 1
                                ? 0
                                : affected_rows));
      info->Set(NanNew<String>("numRows"), NanNew<Number>(num_rows));
      Handle<Value> emit_argv[2] = { NanNew<String>("query.done"), info };
      TryCatch try_catch;
      Emit->Call(NanObjectWrapHandle(this), 2, emit_argv);
      if (try_catch.HasCaught())
        FatalException(try_catch);
    }

    void emit_row() {
      NanScope();

      MYSQL_FIELD field, *fields;
      unsigned int f = 0, n_fields = mysql_num_fields(cur_query.result),
                   i = 0, vlen;
      unsigned char *buf;
      uint16_t *new_buf;
      unsigned long *lengths;
      Handle<Value> field_value;
      Local<Object> row, metadata, types, charsetNrs, dbs, tables, orgTables, names, orgNames;
      Local<String> fieldName;

      if (n_fields == 0)
        return;
      lengths = mysql_fetch_lengths(cur_query.result);
      fields = mysql_fetch_fields(cur_query.result);
      if (cur_query.use_array) {
        row = NanNew<Array>(n_fields);
        if (config.metadata) {
          types = NanNew<Array>(n_fields);
          charsetNrs = NanNew<Array>(n_fields);
          dbs = NanNew<Array>(n_fields);
          tables = NanNew<Array>(n_fields);
          orgTables = NanNew<Array>(n_fields);
          names = NanNew<Array>(n_fields);
          orgNames = NanNew<Array>(n_fields);
        }
      }
      else {
        row = NanNew<Object>();
        if (config.metadata) {
          types = NanNew<Object>();
          charsetNrs = NanNew<Object>();
          dbs = NanNew<Object>();
          tables = NanNew<Object>();
          orgTables = NanNew<Object>();
          names = NanNew<Object>();
          orgNames = NanNew<Object>();
        }
      }
      for (f = 0; f < n_fields; ++f) {
        field = fields[f];

        if (cur_query.row[f] == NULL)
          field_value = NanNull();
        else if (IS_BINARY(fields[f])) {
          vlen = lengths[f];
          buf = (unsigned char*)(cur_query.row[f]);
          new_buf = new uint16_t[vlen];
          for (i = 0; i < vlen; ++i)
            new_buf[i] = buf[i];
          field_value = NanNew<String>(new_buf, vlen);
          delete[] new_buf;
        } else
          field_value = NanNew<String>(cur_query.row[f], lengths[f]);

        if (cur_query.use_array)
          row->Set(f, field_value);
        else{
          fieldName = NanNew(field.name, field.name_length);
          row->Set(fieldName, field_value);
        }

        if (config.metadata) {
          if (cur_query.use_array) {
            types->Set(f, NanNew<String>(FieldTypeToString(field.type)));
            charsetNrs->Set(f, NanNew<Integer, uint32_t>(field.charsetnr));            
            dbs->Set(f, NanNew<String>(field.db));
            tables->Set(f, NanNew<String>(field.table));
            orgTables->Set(f, NanNew<String>(field.org_table));
            names->Set(f, NanNew<String>(field.name));
            orgNames->Set(f, NanNew<String>(field.org_name));
          }
          else {
            types->Set(fieldName, NanNew<String>(FieldTypeToString(field.type)));
            charsetNrs->Set(fieldName, NanNew<Integer, uint32_t>(field.charsetnr));            
            dbs->Set(fieldName, NanNew<String>(field.db));
            tables->Set(fieldName, NanNew<String>(field.table));
            orgTables->Set(fieldName, NanNew<String>(field.org_table));
            names->Set(fieldName, NanNew<String>(field.name));
            orgNames->Set(fieldName, NanNew<String>(field.org_name));
          }
        }
      }

      TryCatch try_catch;
      if (config.metadata) {
        metadata = NanNew<Object>();
        metadata->Set(NanNew<String>("types"), types);
        metadata->Set(NanNew<String>("charsetNrs"), charsetNrs);
        metadata->Set(NanNew<String>("dbs"), dbs);
        metadata->Set(NanNew<String>("tables"), tables);
        metadata->Set(NanNew<String>("orgTables"), orgTables);
        metadata->Set(NanNew<String>("names"), names);
        metadata->Set(NanNew<String>("orgNames"), orgNames);

        Handle<Value> emit_argv[3] = { NanNew<String>("query.row"), row, metadata };
        Emit->Call(NanObjectWrapHandle(this), 3, emit_argv);
      } else {
        Handle<Value> emit_argv[2] = { NanNew<String>("query.row"), row };
        Emit->Call(NanObjectWrapHandle(this), 2, emit_argv);
      }

      if (try_catch.HasCaught())
        FatalException(try_catch);
    }

    inline const char* FieldTypeToString(enum_field_types v)
    {
        // http://dev.mysql.com/doc/refman/5.7/en/c-api-data-structures.html
        switch (v)
        {
            case MYSQL_TYPE_TINY:       return "TINYINT";
            case MYSQL_TYPE_SHORT:      return "SMALLINT";
            case MYSQL_TYPE_LONG:       return "INTEGER";
            case MYSQL_TYPE_INT24:      return "MEDIUMINT";
            case MYSQL_TYPE_LONGLONG:   return "BIGINT";
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_NEWDECIMAL: return "DECIMAL";
            case MYSQL_TYPE_FLOAT:      return "FLOAT";
            case MYSQL_TYPE_DOUBLE:     return "DOUBLE";
            case MYSQL_TYPE_BIT:        return "BIT";
            case MYSQL_TYPE_TIMESTAMP:  return "TIMESTAMP";
            case MYSQL_TYPE_DATE:
            case MYSQL_TYPE_NEWDATE:    return "DATE";
            case MYSQL_TYPE_TIME:       return "TIME";
            case MYSQL_TYPE_DATETIME:   return "DATETIME";
            case MYSQL_TYPE_YEAR:       return "YEAR";
            case MYSQL_TYPE_STRING:     return "CHAR";
            case MYSQL_TYPE_VAR_STRING:
            case MYSQL_TYPE_VARCHAR:    return "VARCHAR";
            case MYSQL_TYPE_BLOB:       return "BLOB";
            case MYSQL_TYPE_TINY_BLOB:  return "TINYBLOB";
            case MYSQL_TYPE_MEDIUM_BLOB:return "MEDIUMBLOB";
            case MYSQL_TYPE_LONG_BLOB:  return "LONGBLOB";
            case MYSQL_TYPE_SET:        return "SET";
            case MYSQL_TYPE_ENUM:       return "ENUM";
            case MYSQL_TYPE_GEOMETRY:   return "GEOMETRY";
            case MYSQL_TYPE_NULL:       return "NULL";
            default:                    return "[Unknown field type]";
        }
    }

    static NAN_METHOD(New) {
      NanScope();

      if (!args.IsConstructCall()) {
        return NanThrowError("Use `new` to create instances of this object.");
      }

      Client *obj = new Client();
      obj->Wrap(args.This());
      obj->Ref();


      obj->Emit = new NanCallback( (NanObjectWrapHandle(obj)->Get(NanNew<String>("emit"))).As<Function>() );

      NanReturnThis();
    }

    static NAN_METHOD(Escape) {
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      NanScope();

      if (obj->state < STATE_CONNECTED) {
        return NanThrowError("Not connected");
      } else if (args.Length() == 0 || !args[0]->IsString()) {
        return NanThrowTypeError("You must supply a string");
      }

      String::Utf8Value arg_v(args[0]);
      unsigned long arg_len = arg_v.length();
      char *result = (char*) malloc(arg_len * 2 + 1);
      unsigned long result_len = obj->escape((char*)*arg_v, arg_len, result);
      Local<String> escaped_s = NanNew<String>(result, result_len);
      free(result);

      NanReturnValue(escaped_s);
    }

    static NAN_METHOD(Connect) {
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      NanScope();

      if (obj->state != STATE_CLOSED) {
        return NanThrowError("Not ready to connect");
      } else if (!args[0]->IsObject()) {
        return NanThrowTypeError("Missing configuration object");
      }

      obj->init();

      Local<Object> cfg = args[0]->ToObject();
      Local<Value> user_v = cfg->Get(NanNew<String>("user"));
      Local<Value> password_v = cfg->Get(NanNew<String>("password"));
      Local<Value> ip_v = cfg->Get(NanNew<String>("host"));
      Local<Value> port_v = cfg->Get(NanNew<String>("port"));
      Local<Value> socket_v = cfg->Get(NanNew<String>("unixSocket"));
      Local<Value> db_v = cfg->Get(NanNew<String>("db"));
      Local<Value> timeout_v = cfg->Get(NanNew<String>("connTimeout"));
      Local<Value> secauth_v = cfg->Get(NanNew<String>("secureAuth"));
      Local<Value> multi_v = cfg->Get(NanNew<String>("multiStatements"));
      Local<Value> compress_v = cfg->Get(NanNew<String>("compress"));
      Local<Value> ssl_v = cfg->Get(NanNew<String>("ssl"));
      Local<Value> metadata_v = cfg->Get(NanNew<String>("metadata"));
      Local<Value> local_infile_v = cfg->Get(NanNew<String>("local_infile"));
      Local<Value> default_file_v = cfg->Get(NanNew<String>("read_default_file"));
      Local<Value> default_group_v = cfg->Get(NanNew<String>("read_default_group"));
      Local<Value> charset_v = cfg->Get(NanNew<String>("charset"));
      
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

      if (!socket_v->IsString() || socket_v->ToString()->Length() == 0)
        obj->config.socket = NULL;
      else {
        String::Utf8Value socket_s(socket_v);
        obj->config.socket = strdup(*socket_s);
      }

      if (!port_v->IsUint32() || port_v->Uint32Value() == 0)
        obj->config.port = 3306;
      else
        obj->config.port = port_v->Uint32Value();

      if (db_v->IsString() && db_v->ToString()->Length() > 0) {
        String::Utf8Value db_s(db_v);
        obj->config.db = strdup(*db_s);
      }

      unsigned int timeout = 10;
      if (timeout_v->IsUint32() && timeout_v->Uint32Value() > 0)
        timeout = timeout_v->Uint32Value();
      mysql_options(&obj->mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

      if (local_infile_v->IsBoolean() && local_infile_v->BooleanValue()){
        mysql_options(&obj->mysql, MYSQL_OPT_LOCAL_INFILE, &MY_BOOL_TRUE);
      }

      if (default_file_v->IsString() && default_file_v->ToString()->Length() > 0) {
        mysql_options(&obj->mysql, MYSQL_READ_DEFAULT_FILE, *String::Utf8Value(default_file_v));
      } 

      if (default_group_v->IsString() && default_group_v->ToString()->Length() > 0) {
        mysql_options(&obj->mysql, MYSQL_READ_DEFAULT_GROUP, *String::Utf8Value(default_group_v));
      }  

      if (!secauth_v->IsBoolean()
          || (secauth_v->IsBoolean() && secauth_v->BooleanValue()))
        mysql_options(&obj->mysql, MYSQL_SECURE_AUTH, &MY_BOOL_TRUE);
      else
        mysql_options(&obj->mysql, MYSQL_SECURE_AUTH, &MY_BOOL_FALSE);

      if (multi_v->IsBoolean() && multi_v->BooleanValue())
        obj->config.client_opts |= CLIENT_MULTI_STATEMENTS;

      if (compress_v->IsBoolean() && compress_v->BooleanValue())
        mysql_options(&obj->mysql, MYSQL_OPT_COMPRESS, 0);

      if (metadata_v->IsBoolean() && metadata_v->BooleanValue())
        obj->config.metadata = true;

      if (ssl_v->IsObject() || ssl_v->IsBoolean()) {
        if (ssl_v->IsBoolean() && ssl_v->BooleanValue())
          obj->config.ssl_cipher = DEFAULT_CIPHER;
        if (ssl_v->IsObject()) {
          Local<Object> ssl_opts = ssl_v->ToObject();
          Local<Value> key = ssl_opts->Get(NanNew<String>("key"));
          Local<Value> cert = ssl_opts->Get(NanNew<String>("cert"));
          Local<Value> ca = ssl_opts->Get(NanNew<String>("ca"));
          Local<Value> capath = ssl_opts->Get(NanNew<String>("capath"));
          Local<Value> cipher = ssl_opts->Get(NanNew<String>("cipher"));
          Local<Value> reject = ssl_opts->Get(NanNew<String>("rejectUnauthorized"));

          if (key->IsString() && key->ToString()->Length() > 0)
            obj->config.ssl_key = strdup(*(String::Utf8Value(key)));
          if (cert->IsString() && cert->ToString()->Length() > 0)
            obj->config.ssl_cert = strdup(*(String::Utf8Value(cert)));
          if (ca->IsString() && ca->ToString()->Length() > 0)
            obj->config.ssl_ca = strdup(*(String::Utf8Value(ca)));
          if (capath->IsString() && capath->ToString()->Length() > 0)
            obj->config.ssl_capath = strdup(*(String::Utf8Value(capath)));
          if (cipher->IsString() && cipher->ToString()->Length() > 0)
            obj->config.ssl_cipher = strdup(*(String::Utf8Value(cipher)));
          else if (!(cipher->IsBoolean() && !cipher->BooleanValue()))
            obj->config.ssl_cipher = DEFAULT_CIPHER;

          mysql_options(&obj->mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                        (reject->IsBoolean() && reject->BooleanValue()
                         ? &MY_BOOL_TRUE
                         : &MY_BOOL_FALSE));
        } else {
          mysql_options(&obj->mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                        &MY_BOOL_FALSE);
        }

        mysql_ssl_set(&obj->mysql,
                      obj->config.ssl_key,
                      obj->config.ssl_cert,
                      obj->config.ssl_ca,
                      obj->config.ssl_capath,
                      obj->config.ssl_cipher);
      }
      if (charset_v->IsString() && charset_v->ToString()->Length() > 0) {
        obj->config.charset = strdup(*(String::Utf8Value(charset_v)));
        mysql_options(&obj->mysql, MYSQL_SET_CHARSET_NAME, obj->config.charset); 
      }
      
      obj->connect();

      NanReturnUndefined();
    }

    static NAN_METHOD(AbortQuery) {
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      NanScope();

      if (args.Length() == 0 || !args[0]->IsUint32()) {
        return NanThrowTypeError("Missing abort level");
      }
      obj->abort_query((abort_t)args[0]->Uint32Value());
      
      NanReturnUndefined();
    }

    static NAN_METHOD(Query) {
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      NanScope();

      if (obj->state != STATE_CONNECTED) {
        return NanThrowError("Not ready to query");
      }
      if (args.Length() == 0 || !args[0]->IsString()) {
        return NanThrowTypeError("Query expected");
      }
      String::Utf8Value query(args[0]);
      obj->query(*query,
                 (args.Length() > 1 && args[1]->IsBoolean()
                  && args[1]->BooleanValue()));
      
      NanReturnUndefined();
    }

    static NAN_METHOD(Close) {
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      NanScope();

      if (obj->state == STATE_CLOSED) {
        return NanThrowError("Already closed");
      }
      obj->close();
      obj->Unref();

      NanReturnUndefined();
    }

    static NAN_METHOD(IsMariaDB) {
      Client *obj = ObjectWrap::Unwrap<Client>(args.This());

      NanScope();

      if (obj->state < STATE_CONNECTED) {
        return NanThrowError("Not connected");
      }
      
      NanReturnValue( NanNew<Boolean>(mariadb_connection(&obj->mysql) == 1) );
    }

    static void Initialize(Handle<Object> target) {
      NanScope();

      Local<String> name = NanNew<String>("Client");

      Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
      tpl->InstanceTemplate()->SetInternalFieldCount(1);
      tpl->SetClassName(name);

      NanAssignPersistent(Client_constructor, tpl);

      NODE_SET_PROTOTYPE_METHOD(tpl, "connect", Connect);
      NODE_SET_PROTOTYPE_METHOD(tpl, "query", Query);
      NODE_SET_PROTOTYPE_METHOD(tpl, "abortQuery", AbortQuery);
      NODE_SET_PROTOTYPE_METHOD(tpl, "escape", Escape);
      NODE_SET_PROTOTYPE_METHOD(tpl, "end", Close);
      NODE_SET_PROTOTYPE_METHOD(tpl, "isMariaDB", IsMariaDB);

      // Make it visible in JavaScript land
      target->Set(name, tpl->GetFunction());
    }
};

static NAN_METHOD(Escape) {
  NanScope();

  if (args.Length() == 0 || !args[0]->IsString()) {
    return NanThrowTypeError("You must supply a string");
  }

  String::Utf8Value arg_v(args[0]);
  unsigned long arg_len = arg_v.length();
  char *result = (char*) malloc(arg_len * 2 + 1);
  unsigned long result_len = 
        mysql_escape_string_ex(result, (char*)*arg_v, arg_len, "utf8");
  Local<String> escaped_s = NanNew<String>(result, result_len);
  free(result);

  NanReturnValue(escaped_s);
}

static NAN_METHOD(Version) {
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
