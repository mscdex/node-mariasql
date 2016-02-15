var EventEmitter = require('events').EventEmitter;
var inherits = require('util').inherits;
var lookup = require('dns').lookup;
var isIP = require('net').isIP;
var ReadableStream = require('stream').Readable;

var LRU = require('lru-cache');

var addon;
try {
  addon = require('../build/Release/sqlclient');
} catch (ex) {
  addon = require('../build/Debug/sqlclient');
}

var binding = addon.ClientBinding;

var RE_PARAM = /(?:\?)|(?::(\d+|(?:[a-zA-Z][a-zA-Z0-9_]*)))/g;
var DQUOTE = 34;
var SQUOTE = 39;
var BSLASH = 92;

var EMPTY_LRU_FN = function(key, value) {};

Client.escape = addon.escape;
Client.version = addon.version;

function Client(config) {
  if (!(this instanceof Client))
    return new Client(config);

  EventEmitter.call(this);

  this._handle = null;

  if (typeof config === 'object' && config !== null)
    this._config = config;
  else
    this._config = {};

  var queryCache;
  var ncache = 30;
  if (typeof this._config.queryCache === 'number')
    ncache = this._config.queryCache;
  else if (typeof this._config.queryCache === 'object')
    queryCache = this._config.queryCache; // Assume lru-cache instance
  if (this._config.queryCache !== false && !queryCache)
    queryCache = new LRU({ max: ncache, dispose: EMPTY_LRU_FN });

  this._req = undefined;
  this._queue = [];
  this._queryCache = queryCache;
  this._handleClosing = false;
  this._tmrInactive = undefined;
  this._tmrPingWaitRes = undefined;
  this.connecting = false;
  this.connected = false;
  this.closing = false;
  this.threadId = undefined;

  if (this._config.threadId === false)
    return;

  // XXX: hack to get thread ID first before any other queries
  var self = this;
  this._firstQuery = {
    str: 'SELECT CONNECTION_ID()',
    cb: function(err, rows) {
      if (err) {
        self.emit('error', err);
        self.close(true);
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

Client.prototype._initHandle = function() {
  if (this._handle !== null) {
    return;
  }
  this._handle = new binding({
    context: this,
    config: this._config,
    onconnect: this._onconnect,
    onerror: this._onerror,
    onidle: this._onidle,
    onresultinfo: this._onresultinfo,
    onrow: this._onrow,
    onresultend: this._onresultend,
    onping: this._onping,
    onclose: this._onclose,
  });
}

Client.prototype.connect = function(config, cb) {
  if (typeof config === 'function') {
    cb = config;
    config = undefined;
  }

  this._initHandle();

  if (this.connecting) {
    if (typeof cb === 'function')
      this.once('ready', cb);
    return;
  } else if (this.connected) {
    if (typeof cb === 'function')
      process.nextTick(cb);
    return;
  }

  if (typeof cb === 'function')
    this.once('ready', cb);

  if (typeof config === 'object' && config !== null)
    this._config = config;

  var cfg = this._config;
  var self = this;
  if (typeof cfg !== 'object')
    throw new Error('Missing config');

  this.connecting = true;

  setImmediate(function() {
    // Doing a manual resolve prevents libmariadbclient from doing a blocking
    // DNS resolve
    self._initHandle();
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

Client.prototype.query = function(str, values, config, cb) {
  var req;
  var ret;
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
    str = this.prepare(str)(values);

  var needColumns = (!config ||
                     (typeof config === 'object'
                      && config !== null
                      && !config.useArray));
  var needMetadata = ((config && config.metadata === true)
                      || this._config.metadata === true);
  if (typeof cb === 'function') {
    // We are buffering all rows
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
    // We are streaming all rows
    var hwm = (config && config.hwm) || this._config.streamHWM;
    req = {
      emitter: new ResultEmitter(this._handle, hwm),
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
  else if (this._req === undefined) {
    var self = this;
    process.nextTick(function() {
      self._processQueue(false);
    });
  }

  return ret;
};

Client.prototype.close = function(force) {
  if (!this.closing && (this.connected || this.connecting)) {
    this.closing = true;
    if (force || (this._req === undefined && this._queue.length === 0)) {
      this._handleClosing = true;
      this._handle.close();
    }
  }
};
Client.prototype.destroy = function() {
  this.close(true);
};
Client.prototype.end = Client.prototype.close;

Client.prototype.abort = function(killConn, cb) {
  // TODO: support MariaDB-specific kill options

  if (!this._req)
    return;
  if (typeof this.threadId !== 'string' || !this.threadId.length)
    throw new Error('Cannot abort: no thread id');

  if (typeof killConn === 'function') {
    cb = killConn;
    killConn = false;
  }

  var kind = (killConn ? 'CONNECTION' : 'QUERY');
  var querystr = 'KILL ' + kind + ' ' + this.threadId;
  var calledBack = false;

  var abortClient = new Client(this._config);
  abortClient._firstQuery = null;
  abortClient.on('error', handler);
  abortClient.connect(function() {
    abortClient.query(querystr, handler);
    abortClient.end();
  });
  function handler(err) {
    if (calledBack)
      return;
    calledBack = true;
    cb && cb(err);
  }
};

Client.prototype.isMariaDB = function() {
  this._initHandle();
  return this._handle.isMariaDB();
};

Client.prototype.lastInsertId = function() {
  this._initHandle();
  return this._handle.lastInsertId();
};

Client.prototype.escape = function(str) {
  this._initHandle();
  return this._handle.escape(str);
};

Client.prototype.serverVersion = function() {
  this._initHandle();
  return this._handle.serverVersion();
};

Client.prototype.prepare = function(query) {
  var cache = this._queryCache;
  var cqfn;
  if (cache && (cqfn = cache.get(query)))
    return cqfn;

  var ppos = RE_PARAM.exec(query);
  var curpos = 0;
  var start = 0;
  var parts = [];
  var wasInQuote = false;
  var inQuote = false;
  var escape = false;
  var tokens = [];
  var qcnt = 0;
  var qchr;
  var chr;
  var end;
  var fn;
  var i;

  if (ppos) {
    do {
      wasInQuote = inQuote;
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
              // Quote escaped via "" or ''
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
        parts.push(query.slice(start, end));
        if (wasInQuote)
          tokens.push(null);
        tokens.push(ppos[0].length === 1 ? qcnt++ : ppos[1]);
        start = end + ppos[0].length;
      }

      curpos = end + ppos[0].length;
    } while (ppos = RE_PARAM.exec(query));

    if (tokens.length) {
      if (end < query.length)
        parts.push(query.slice(start));
      var self = this;
      fn = function(values) {
        var ret = '';
        for (var j = 0, t = 0; j < tokens.length; ++j) {
          ret += parts[j];
          if (tokens[j] === null)
            continue;
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

Client.prototype._onconnect = function() {
  var queue = this._queue;
  if (queue.length === 0 || queue[0] !== this._firstQuery) {
    if (this._firstQuery)
      queue.unshift(this._firstQuery);
    else {
      this.connecting = false;
      this.connected = true;
      this.emit('ready');
    }
  }
  this._processQueue(true);
};

Client.prototype._onerror = function(err) {
  if (isDeadConn(err.code)) {
    this.connecting = this.connected = false;
    this.emit('error', err);
    this._onclose(err);
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
          stream.read(0);
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
    // A query finished -- no more result sets
    this._queue.shift();
    this._req = undefined;
    var cb = req.cb;
    if (cb !== undefined) {
      var results = req.results;
      if (results.length === 1) {
        // Single result set response

        var r = results[0];
        if (r instanceof Error)
          cb(r);
        else
          cb(null, r);
      } else {
        // Multi-result set response

        // TODO: "null" here can be a bit misleading if 1 of several
        // results ended in error (can this even happen in reality?)
        cb(null, results);
      }
    } else {
      // Signal to the emitter that when the last QueryStream ends, that it's ok
      // to emit 'end' as well ...
      req.emitter._done = true;
      if (!req.emitter._waitingForEnd)
        req.emitter._complete(req.stream);
    }
  }
  this._processQueue(false);
};

Client.prototype._onresultinfo = function(cols, metadata) {
  var req = this._req;
  if (req.cb !== undefined) {
    if (req.needMetadata)
      req.metadata = createMetadata(metadata);
  } else {
    var emitter = req.emitter;
    var stream = req.stream = emitter._createStream();
    if (req.needMetadata)
      stream.info.metadata = createMetadata(metadata);
    emitter.emit('result', stream);
  }
  if (req.needColumns)
    req.rowBuilder = createRowBuilder(cols);
};

Client.prototype._onrow = function(row) {
  var req = this._req;
  var builder = req.rowBuilder;
  if (req.cb !== undefined) {
    if (builder) {
      for (var i = 0; i < row.length; ++i)
        row[i] = builder(row[i]);
    }
    req.result = row;
  } else {
    if (builder)
      row = builder(row);
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
    var results = req.results;
    var result = req.result;
    if (result === undefined) {
      result = {
        info: {
          numRows: numRows,
          affectedRows: affectedRows,
          insertId: insertId,
          metadata: req.metadata
        }
      };
    } else {
      req.result = undefined;
      result.info = {
        numRows: numRows,
        affectedRows: affectedRows,
        insertId: insertId,
        metadata: req.metadata
      };
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
      stream.info.numRows = numRows;
      stream.info.affectedRows = affectedRows;
      stream.info.insertId = insertId;
      emitter.emit('result', stream);
      stream.push(null);
      stream.read(0);
    } else {
      stream.info.numRows = numRows;
      stream.info.affectedRows = affectedRows;
      stream.info.insertId = insertId;
      stream.push(null);
      req.stream = undefined;
    }
  }
};

Client.prototype._onping = function() {
  clearTimeout(this._tmrPingWaitRes);
  this._tmrPingWaitRes = undefined;
  this._tmrInactive = undefined;
  if (this._queue.length === 0)
    this._ping();
};

Client.prototype._onclose = function(err) {
  var self = this;
  clearTimeout(this._tmrInactive);
  clearTimeout(this._tmrPingWaitRes);
  this._tmrInactive = undefined;
  this._tmrPingWaitRes = undefined;
  this.connecting = false;
  this.connected = false;
  this.closing = false;
  this._handleClosing = false;

  var keepQueries = this._config.keepQueries;
  if (keepQueries === false || keepQueries === undefined) {
    if (this._req !== undefined)
      this._queue.unshift(this._req);
    cleanupReqs(this._queue, err);
    this._queue = [];
  } else if (this._req !== undefined) {
    // No easy way to "recover" the current request, so just remove it and clean
    // it up
    cleanupReqs([this._queue.shift()], err);
  }
  this._req = undefined;
  if (!err)
    this.emit('end');
  this.emit('close');
  // Allow addon handle to be garbage collected since we are no longer connected
  // See: https://github.com/mscdex/node-mariasql/pull/130
  //      https://github.com/mscdex/node-mariasql/pull/133
  process.nextTick(function() {
    if (!self.connecting
        && !self.connected
        && !self.closing
        && !self._handleClosing) {
      self._handle = null;
    }
  });
};

Client.prototype._processQueue = function(ignoreConnected) {
  var connected = this.connected;
  if (!ignoreConnected && !connected)
    return;
  var req = this._req;
  if (req)
    return;
  var queue = this._queue;
  if (queue.length > 0) {
    // Allow an outstanding ping request to finish first
    if (this._tmrPingWaitRes !== undefined)
      return;
    clearTimeout(this._tmrInactive);
    this._tmrInactive = undefined;

    req = this._req = queue[0];
    this._handle.query(req.str,
                       req.needColumns,
                       req.needMetadata,
                       req.cb !== undefined);
  } else if (connected) {
    if (this.closing && !this._handleClosing) {
      this._handleClosing = true;
      this._handle.close();
    } else if (!this.closing)
      this._ping();
  }
};

function pingNoAnswer(self) {
  self.emit('error', new Error('Ping response lost'));
  self.close(true);
}
function pingCb(self) {
  self._tmrPingWaitRes = setTimeout(pingNoAnswer,
                                    self._config.pingWaitRes,
                                    self);
  self._handle.ping();
}
Client.prototype._ping = function() {
  if (this._tmrInactive === undefined
      && typeof this._config.pingInactive === 'number'
      && typeof this._config.pingWaitRes === 'number'
      && this._config.pingInactive > 0
      && this._config.pingWaitRes > 0) {
    this._tmrInactive = setTimeout(pingCb, this._config.pingInactive, this);
  }
};

Client.prototype._format_value = function(v) {
  if (Buffer.isBuffer(v))
    return "'" + Client.escape(v.toString('utf8')) + "'";
  else if (Array.isArray(v)) {
    var r = [];
    for (var i = 0, len = v.length; i < len; ++i)
      r.push(this._format_value(v[i]));
    return r.join(',');
  } else if (v !== null && v !== undefined)
    return "'" + Client.escape(v + '') + "'";

  return 'NULL';
};



var QueryStreamDefaultOpts = { objectMode: true };
function ResultEmitter(handle, hwm) {
  EventEmitter.call(this);
  if (typeof hwm === 'number')
    this._streamOpts = { objectMode: true, highWaterMark: hwm };
  else
    this._streamOpts = QueryStreamDefaultOpts;
  this._handle = handle;
  this._done = false;
  this._waitingForEnd = false;
}
inherits(ResultEmitter, EventEmitter);

ResultEmitter.prototype._createStream = function() {
  var qs = new QueryStream(this._handle, this._streamOpts);
  var self = this;
  this._waitingForEnd = true;
  qs.on('end', function() {
    self._waitingForEnd = false;
    self._complete(qs);
  });
  return qs;
};

ResultEmitter.prototype._complete = function(qs) {
  if (this._done) {
    if (qs && EventEmitter.listenerCount(qs, 'end') > 1) {
      // We emit 'end' on the next tick to ensure proper event ordering
      // (otherwise a user's QueryStream 'end' listener will get called
      // *after* the ResultEmitter's 'end' because we add our 'end' listener
      // first)
      var self = this;
      process.nextTick(function() {
        self.emit('end');
      });
    } else
      this.emit('end');
  }
};

function QueryStream(handle, opts) {
  ReadableStream.call(this, opts);
  this._handle = handle;
  this._needResume = false;
  this.info = {
    numRows: undefined,
    affectedRows: undefined,
    insertId: undefined,
    metadata: undefined
  };
}
inherits(QueryStream, ReadableStream);
QueryStream.prototype._read = function(n) {
  if (this._needResume) {
    this._needResume = false;
    this._handle.resume();
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

function cleanupReqs(queue, err) {
  var len = queue.length;
  if (!err) {
    err = new Error('Connection closed early');
    err.code = -1;
  }
  for (var i = 0, req; i < len; ++i) {
    req = queue[i];
    if (req.cb !== undefined)
      req.cb(err);
    else {
      req.emitter._done = true;
      var stream = req.stream;
      if (stream && stream.readable) {
        stream.emit('error', err);
        stream.push(null);
      }
    }
  }
}

function isDeadConn(code) {
  return (code === 2006 || code === 2013 || code === 2055);
}

function createRowBuilder(cols) {
  var fn = 'return {';
  for (var i = 0; i < cols.length; ++i)
    fn += JSON.stringify(cols[i]) + ': v[' + i + '],';
  return new Function('v', fn + '}');
}

function createMetadata(data) {
  var result = {};
  var len = data.length;

  if (len > 0) {
    for (var i = 0, name; i < len;) {
      name = data[i++];
      result[name] = {
        org_name: data[i++],
        type: data[i++],
        flags: data[i++],
        charsetnr: data[i++],
        db: data[i++],
        table: data[i++],
        org_table: data[i++]
      };
    }
    // TODO: need (fastest) way to make fast properties for metadata object
  }

  return result;
}

// Field cannot be NULL
Client.NOT_NULL_FLAG = 1;
// Field is part of a primary key
Client.PRI_KEY_FLAG = 2;
// Field is part of a unique key
Client.UNIQUE_KEY_FLAG = 4;
// Field is part of a nonunique key
Client.MULTIPLE_KEY_FLAG = 8;
// Field is a BLOB or TEXT (deprecated)
Client.BLOB_FLAG = 16;
// Field has the UNSIGNED attribute
Client.UNSIGNED_FLAG = 32;
// Field has the ZEROFILL attribute
Client.ZEROFILL_FLAG = 64;
// Field has the BINARY attribute
Client.BINARY_FLAG = 128;
// Field is an ENUM
Client.ENUM_FLAG = 256;
// Field has the AUTO_INCREMENT attribute
Client.AUTO_INCREMENT_FLAG = 512;
// Field is a TIMESTAMP (deprecated)
Client.TIMESTAMP_FLAG = 1024;
// Field is a SET
Client.SET_FLAG = 2048;
// Field has no default value
Client.NO_DEFAULT_VALUE_FLAG = 4096;
// Field is set to NOW on UPDATE
Client.ON_UPDATE_NOW_FLAG = 8192;
// Field is part of some key
Client.PART_KEY_FLAG = 16384;
// Field is numeric
Client.NUM_FLAG = 32768;

module.exports = Client;
