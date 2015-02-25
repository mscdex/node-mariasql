var EventEmitter = require('events').EventEmitter,
    inherits = require('util').inherits,
    lookup = require('dns').lookup,
    isIP = require('net').isIP,
    ReadableStream = require('stream').Readable;

var LRU = require('lru-cache');

var binding = require('../build/Release/sqlclient').Client;

var RE_PARAM = /(?:\?)|(?::(\d+|(?:[a-zA-Z][a-zA-Z0-9_]*)))/g,
    DQUOTE = 34,
    SQUOTE = 39,
    BSLASH = 92;

function Client(config) {
  if (!(this instanceof Client))
    return new Client(config);

  EventEmitter.call(this);

  this._handle = new binding({
    context: this,
    config: config,
    onconnect: this._onconnect,
    onerror: this._onerror,
    onidle: this._onidle,
    onresultinfo: this._onresultinfo,
    onrow: this._onrow,
    onresultend: this._onresultend,
    onping: this._onping,
    onclose: this._onclose,
  });
  if (typeof config === 'object' && config !== null)
    this._config = config;
  else
    this._config = {};
  this._req = undefined;
  this._queue = [];
  this._queryCache = undefined;
  this._handleClosing = false;
  this.connecting = false;
  this.connected = false;
  this.closing = false;
  this.threadId = undefined;

  // XXX: hack to get thread ID first before any other queries
  var self = this;
  this._firstQuery = {
    str: 'SELECT CONNECTION_ID()',
    cb: function(err, rows) {
      if (err) {
        self.emit('error', err);
        return;
      }
      self.threadId = rows[0][0];
      self.connecting = false;
      self.connected = true;
      self.emit('ready');
    },
    result: undefined,
    results: undefined,
    needMetadata: false,
    needColumns: false
  };
}
inherits(Client, EventEmitter);

Client.prototype._onconnect = function() {
  var queue = this._queue,
      firstQuery = this._firstQuery;
  if (queue.length === 0 || queue[0] !== firstQuery)
    queue.unshift(firstQuery);
  this._processQueue(true);
};
Client.prototype._onerror = function(err) {
  if (isDeadConn(err.code)) {
    this.connecting = false;
    this.connected = false;
    this.emit('error', err);
    this.close();
  } else {
    var req = this._req;
    if (req) {
      if (req.cb !== undefined) {
        var results = req.results;
        if (results === undefined)
          req.results = [err];
        else
          results.push(err);
      } else {
        var stream = req.stream;
        if (stream === undefined) {
          var emitter = req.emitter;
          stream = emitter._createStream();
          emitter.emit('result', stream);
          stream.emit('error', err);
          stream.push(null);
        } else {
          stream.emit('error', err);
          stream.push(null);
          req.stream = undefined;
        }
      }
    } else
      this.emit('error', err);
  }
};
Client.prototype._onidle = function() {
  var req = this._req;
  if (req) {
    // a query finished -- no more result sets
    this._queue.shift();
    this._req = undefined;
    var cb = req.cb;
    if (cb !== undefined) {
      var results = req.results;
      if (results.length === 1) {
        // single result set response

        var r = results[0];
        if (r instanceof Error)
          cb(r);
        else
          cb(undefined, r);
      } else {
        // multi-result set response

        // TODO: "undefined" here can be a bit misleading if 1 of several
        // results ended in error (can this even happen in reality?)
        cb(undefined, results);
      }
    } else
      req.emitter.emit('end');
  }
  this._processQueue(false);
};
Client.prototype._onresultinfo = function(cols, metadata) {
  var req = this._req;
  if (req.cb !== undefined) {
    if (req.needMetadata)
      req.metadata = createMetadata(metadata);
  } else {
    var emitter = req.emitter,
        stream = req.stream = emitter._createStream();
    if (req.needMetadata)
      stream.metadata = createMetadata(metadata);
    emitter.emit('result', stream);
  }
  if (req.needColumns)
    req.rowBuilder = createRowBuilder(cols);
};
Client.prototype._onrow = function(row) {
  var req = this._req,
      builder = req.rowBuilder;
  if (builder)
    row = builder(row);
  if (req.cb !== undefined) {
    var result = req.result;
    if (result === undefined)
      req.result = [row];
    else
      result.push(row);
  } else {
    var stream = req.stream;
    if (stream === undefined) {
      var emitter = req.emitter;
      stream = req.stream = emitter._createStream();
      emitter.emit('result', stream);
    }
    if (stream.push(row) === false) {
      this._handle.pause();
      stream._needResume = true;
    }
  }
};
Client.prototype._onresultend = function(numRows, affectedRows, insertId) {
  var req = this._req;
  if (req.cb !== undefined) {
    var results = req.results,
        result = req.result;
    if (result === undefined) {
      result = {
        numRows: numRows,
        affectedRows: affectedRows,
        insertId: insertId,
        metadata: req.metadata
      };
    } else {
      req.result = undefined;
      result.numRows = numRows;
      result.affectedRows = affectedRows;
      result.insertId = insertId;
      result.metadata = req.metadata;
    }
    req.metadata = undefined;
    if (results !== undefined)
      results.push(result);
    else
      req.results = [result];
  } else {
    var stream = req.stream;
    if (stream === undefined) {
      var emitter = req.emitter;
      stream = emitter._createStream();
      stream.numRows = numRows;
      stream.affectedRows = affectedRows;
      stream.insertId = insertId;
      emitter.emit('result', stream);
      stream.push(null);
    } else {
      stream.numRows = numRows;
      stream.affectedRows = affectedRows;
      stream.insertId = insertId;
      stream.push(null);
      req.stream = undefined;
    }
  }
};
Client.prototype._onping = function() {
  // TODO
};
Client.prototype._onclose = function() {
  this.connecting = false;
  this.connected = false;
  this.closing = false;
  this._handleClosing = false;
  this._req = undefined;
  this.emit('end');
  this.emit('close');
};

Client.prototype._processQueue = function(ignoreConnected) {
  var connected = this.connected;
  if (!ignoreConnected && !connected)
    return;
  var req = this._req;
  if (req)
    return;
  var queue = this._queue;
  if (queue.length) {
    req = this._req = queue[0];
    this._handle.query(req.str, req.needColumns, req.needMetadata);
  } else if (connected && this.closing && !this._handleClosing) {
    this._handleClosing = true;
    this._handle.close();
  }
};

Client.prototype.connect = function(config) {
  if (this.connecting || this.connected)
    return;

  if (typeof config === 'object' && config !== null)
    this._config = config;

  var cfg = this._config,
      self = this;
  if (typeof cfg !== 'object')
    throw new Error('Missing config');

  this.connecting = true;

  process.nextTick(function() {
    // doing a manual resolve prevents libmariadbclient from doing a blocking
    // DNS resolve
    if (!isIP(cfg.host)) {
      lookup(cfg.host, function(err, address, family) {
        if (err) {
          self.connecting = false;
          self.emit('error', err);
          return self.emit('close');
        }
        cfg = clone(cfg);
        cfg.host = address;
        self._handle.connect(cfg);
      });
    } else
      self._handle.connect(cfg);
  });
};

Client.prototype.isMariaDB = function() {
  return this._handle.isMariaDB();
};

Client.prototype.query = function(str, values, config, cb) {
  var req,
      ret;
  if (typeof str !== 'string')
    throw new Error('Missing query string');
  if (typeof values === 'function') {
    // query(str, cb)
    cb = values;
    values = config = undefined;
  } else if (typeof config === 'function') {
    // query(str, ___, cb)
    cb = config;
    if (typeof values === 'boolean') {
      config = values;
      values = undefined;
    } else
      config = undefined;
  } else if (typeof values === 'boolean') {
    // query(str, config)
    config = values;
    values = undefined;
  }

  if (Array.isArray(values) || (typeof values === 'object' && values !== null))
    query = this.prepare(str)(values);

  var needColumns = (!config ||
                     (typeof config === 'object'
                      && config !== null
                      && !config.useArray)),
      needMetadata = ((config && config.metadata === true)
                      || this._config.metadata === true);
  if (typeof cb === 'function') {
    // we are buffering all rows
    req = {
      cb: cb,
      result: undefined,
      results: undefined,
      metadata: undefined,

      str: str,
      needColumns: needColumns,
      needMetadata: needMetadata,
      rowBuilder: undefined
    };
  } else {
    // we are streaming all rows
    var hwm = (config && config.hwm) || this._config.streamHWM;
    req = {
      emitter: new ResultEmitter(this, hwm),
      stream: undefined,

      str: str,
      needColumns: needColumns,
      needMetadata: needMetadata,
      rowBuilder: undefined
    };
    ret = req.emitter;
  }
  this._queue.push(req);
  if (!this.connected)
    this.connect();
  else if (this._req === undefined)
    this._processQueue(false);
  return ret;
};

Client.prototype.close = function() {
  if (this.connected && !this.closing) {
    this.closing = true;
    if (this._req === undefined && this._queue.length) {
      this._handleClosing = true;
      this._handle.close();
    }
  }
};
Client.prototype.end = Client.prototype.close;

Client.prototype.escape = function(str) {
  if (!this.connected)
    throw new Error('Not connected');
  return this._handle.escape(str);
};

Client.prototype.prepare = function(query) {
  var cache = this._queryCache;
  var cqfn;
  if (cache && (cqfn = cache.get(query)))
    return cqfn;

  var ppos = RE_PARAM.exec(query),
      curpos = 0,
      start = 0,
      parts = [],
      inQuote = false,
      escape = false,
      tokens = [],
      qcnt = 0,
      qchr,
      chr,
      end,
      fn,
      i;

  if (ppos) {
    do {
      for (i = curpos, end = ppos.index; i < end; ++i) {
        chr = query.charCodeAt(i);
        if (chr === BSLASH)
          escape = !escape;
        else {
          if (escape) {
            escape = false;
            continue;
          }
          if (inQuote && chr === qchr) {
            if (query.charCodeAt(i + 1) === qchr) {
              // quote escaped via "" or ''
              ++i;
              continue;
            }
            inQuote = false;
          } else if (chr === DQUOTE || chr === SQUOTE) {
            inQuote = true;
            qchr = chr;
          }
        }
      }
      if (!inQuote) {
        parts.push(query.substring(start, end));
        tokens.push(ppos[0].length === 1 ? qcnt++ : ppos[1]);
        start = end + ppos[0].length;
      }
      curpos = end + ppos[0].length;
    } while (ppos = RE_PARAM.exec(query));

    if (tokens.length) {
      if (curpos < query.length)
        parts.push(query.substring(curpos));

      var self = this;      
      fn = function(values) {
        var ret = '', j, val;
        for (j = 0; j < tokens.length; ++j) {
          ret += parts[j];
          ret += self._format_value(values[tokens[j]]);
        }
        if (j < parts.length)
          ret += parts[j];
        return ret;
      };
      var cache = this._queryCache;
      cache && cache.set(query, fn);
      return fn;
    }
  }
  return function() { return query; };
};

Client.prototype._format_value = function (v) {
  if (Buffer.isBuffer(v))
    return "'" + Client.escape(v.toString('utf8')) + "'";
  else if (Array.isArray(v)) {
    var r = [];
    for (var i = 0, len = v.length; i < len; ++i)
      r.push(this._format_value(v[i]));
    return r.join(',');
  } else if (v != null)
    return "'" + Client.escape(v + '') + "'";

  return 'NULL';
};

Client.escape = binding.escape;

var QueryStreamDefaultOpts = { objectMode: true };
function ResultEmitter(client, hwm) {
  EventEmitter.call(this);
  if (typeof hwm === 'number')
    this._streamOpts = { objectMode: true, highWaterMark: hwm };
  else
    this._streamOpts = QueryStreamDefaultOpts;
  this._client = client;
}
inherits(ResultEmitter, EventEmitter);

ResultEmitter.prototype._createStream = function() {
  return new QueryStream(this._client, this._streamOpts);
};

function QueryStream(client, opts) {
  ReadableStream.call(this, opts);
  this._client = client;
  this._needResume = false;
  this.metadata = undefined;
  this.affectedRows = undefined;
  this.numRows = undefined;
  this.insertId = undefined;
}
inherits(QueryStream, ReadableStream);
QueryStream.prototype._read = function(n) {
  if (this._needResume) {
    this._needResume = false;
    this._client.resume();
  }
};

function clone(obj) {
  var ret = {};
  var keys = Object.keys(obj);
  var key;
  for (var i = 0; i < keys.length; ++i) {
    key = keys[i];
    ret[key] = obj[key];
  }
  return ret;
}

function isDeadConn(code) {
  return code === 2006 || code === 2013 || code === 2055;
}

function createRowBuilder(cols) {
  var fn = 'return {';
  for (var i = 0; i < cols.length; ++i)
    fn += JSON.stringify(cols[i]) + ': v[' + i + '],';
  return new Function('v', fn + '}');
}

function createMetadata(data) {
  var result = {},
      len = data.length;

  if (len > 0) {
    for (var i = 0, name; i < len; ++i) {
      name = data[i + 5];
      result[name] = {
        type: data[i++],
        charsetnr: data[i++],
        db: data[i++],
        table: data[i++],
        org_table: data[i++],
        name: name,
        org_name: data[++i]
      };
    }
    // TODO: need (fastest) way to make fast properties for metadata object
  }

  return result;
}

module.exports = Client;
