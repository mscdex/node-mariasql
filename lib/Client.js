var lookup = require('dns').lookup,
    isIP = require('net').isIP,
    inherits = require('util').inherits,
    EventEmitter = require('events').EventEmitter;

var LRU = require('lru-cache');

var addon = require('../build/Release/sqlclient');

addon.Client.prototype.__proto__ = EventEmitter.prototype;

function clone(o) {
  var n = Object.create(Object.getPrototypeOf(o)),
      props = Object.getOwnPropertyNames(o),
      pName, p;
  for (p in props) {
    pName = props[p];
    Object.defineProperty(n, pName, Object.getOwnPropertyDescriptor(o, pName));
  }
  return n;
}

var ABORT_QUERY = 1,
    ABORT_RESULTS = 2;

var EMPTY_LRU_FN = function(key, value) {};

// for prepare()
var RE_PARAM = /(?:\?)|(?::(\d+|(?:[a-zA-Z][a-zA-Z0-9_]*)))/g,
    DQUOTE = 34,
    SQUOTE = 39,
    BSLASH = 92;

function Client() {
  var self = this;
  this.threadId = undefined;
  this.connected = false;
  this.connecting = false;
  this.pingInterval = 60000;
  this._keepQueries = undefined;
  this._pinger = undefined;
  this._queryCache = undefined;
  this._closeOnEmpty = false;
  this._reusableAfterClose = false;
  this._queries = [];
  this._curResults = undefined;
  this._client = new addon.Client();
  // Client-level events
  this._client.on('connect', function() {
    self.connected = true;
    self.connecting = false;
    // use CONNECTION_ID() query instead of C library's mysql_thread_id()
    // because according to the MySQL docs, mysql_thread_id() does not work
    // correctly when the thread ID exceeds 32 bits
    self.query('SELECT CONNECTION_ID()', true, true)
        .on('result', function(r) {
          r.on('row', function(row) {
            self.threadId = row[0];
          });
        })
        // caller has no way to attach an error listener to avoid crash
        .on('error', function() {})
        .on('end', function() {
          self.emit('connect');
        });
    // doing everything in utf8 greatly simplifies things --
    // end users can use iconv/iconv-lite if they need to convert to something
    // else
    self.query("SET NAMES 'utf8'", true, true).on('error', function() {});
  });
  this._client.on('conn.error', function(err) {
    // 'close' event will be fired immediately
    // so don't set connected = connecting = false here
    // another reason not to set false here is to make sure when connected == connecting == false
    // either connect() never called or 'close' event already fired
    // and _client.connect() already scheduled by process.nextTick will see destroyed == true
    self.emit('error', err);
  });
  this._client.on('close', function(had_err) {
    self.connected = false;
    self.connecting = false;
    self.threadId = undefined;
    self._reset();
    if (!self._reusableAfterClose) {
      self.destroyed = true;
      self._client.removeAllListeners();
    }
    self.emit('close', had_err);
  });
  // Results-level events
  this._client.on('results.abort', function() {
    var r = self._curResults;
    self._curResults = undefined;
    r && r.emit('abort');
    self._processQueries();
  });
  this._client.on('results.error', function(err) {
    if (err.code === 2013 || err.code === 10001/*ERROR_HANGUP*/) {
      // connection closed by server
      self.emit('error', err);
    } else {
      var r = self._curResults;
      self._curResults = undefined;
      r && r.emit('error', err);
      self._processQueries();
    }
  });
  this._client.on('results.query', function() {
    if (self._curResults) {
      self._curResults._curQuery = new Query(self._curResults);
      self._curResults.emit('result', self._curResults._curQuery);
    }
  });
  this._client.on('results.done', function() {
    var r = self._curResults;
    self._curResults = undefined;
    r && r.emit('end');
    self._processQueries();
  });
  // Query-level events
  var fnQueryErr = function(err) {
    if (err.code === 2013 || err.code === 10001/*ERROR_HANGUP*/) {
      // connection closed by server
      self.emit('error', err);
    } else {
      if (!self._curResults)
        return;
      if (self._curResults._curQuery.listeners('error').length > 0)
        self._curResults._curQuery.emit('error', err);
      else
        self._curResults.emit('error', err, self._curResults._curQuery);
    }
  };
  this._client.on('query.error', fnQueryErr);
  this._client.on('query.row.error', fnQueryErr);
  this._client.on('query.abort', function() {
    if (self._curResults)
      self._curResults._curQuery.emit('abort');
  });
  this._client.on('query.row', function(res, metadata) {
    if (self._curResults)
      self._curResults._curQuery.emit('row', res, metadata);
  });
  this._client.on('query.done', function(info) {
    if (self._curResults) {
      var q = self._curResults._curQuery;
      self._curResults._curQuery = undefined;
      q.emit('end', info);
    }
  });
}
inherits(Client, EventEmitter);

Client.prototype.isMariaDB = function() {
  return this._client.isMariaDB();
};

Client.prototype.connect = function(cfg) {
  if (this.connected || this.connecting || this.destroyed)
    return;

  var self = this;
  var ncache = 30, queryCache;
  if (typeof cfg.queryCache === 'number')
    ncache = cfg.queryCache;
  else if (typeof cfg.queryCache === 'object')
    queryCache = cfg.queryCache; // assume lru-cache instance

  if (cfg.queryCache !== false && !queryCache)
    queryCache = LRU({ max: ncache, dispose: EMPTY_LRU_FN });

  this._queryCache = queryCache;

  this._keepQueries = cfg.keepQueries;
  this._reusableAfterClose = cfg.reusableAfterClose

  process.nextTick(function() {
    if (!isIP(cfg.host)) {
      lookup(cfg.host, function(err, address, family) {
        if (err)
          return self.emit('error', err);
        cfg = clone(cfg);
        cfg.host = address;
        if (cfg.pingInterval)
          self.pingInterval = cfg.pingInterval * 1000;
        if (!self.destroyed) {
          self.connecting = true;
          self._client.connect(cfg);
        }
      });
    } else {
      if (cfg.pingInterval)
        self.pingInterval = cfg.pingInterval * 1000;
      if (!self.destroyed) {
        self.connecting = true;
        self._client.connect(cfg);
      }
    }
  });
};

Client.prototype.end = function() {
  if (this._curQuery === undefined && !this._queries.length)
    this.destroy();
  else {
    this._reusableAfterClose = false;
    this._closeOnEmpty = true;
  }
};

Client.prototype.destroy = function() {
  if (this.connected || this.connecting) {
    this._reusableAfterClose = false;
    this._client.end();
  } else {
    this.destroyed = true;
    this._client.removeAllListeners();
  }
};

Client.prototype.query = function(query, values, useArray, promote) {
  var results, self = this;

  if (Array.isArray(values) || typeof values === 'object')
    query = this.prepare(query)(values);
  else {
    promote = useArray;
    useArray = values;
  }

  results = new Results(this, query, useArray);
  if (promote)
    this._queries.unshift(results);
  else
    this._queries.push(results);

  // queries can actually start emitting results before the results object is
  // returned here, so we defer processing until the next tick
  process.nextTick(function() { self._processQueries(); });

  return results;
};

Client.prototype.escape = function(str) {
  return this._client.escape(str);
};

Client.prototype.prepare = function(query) {
  var cqfn;
  if (this._queryCache && (cqfn = this._queryCache.get(query)))
    return cqfn;

  var ppos = RE_PARAM.exec(query), curpos = 0, start = 0, end, parts = [],
      i, chr, inQuote = false, escape = false, qchr, tokens = [], qcnt = 0, fn;

  if (ppos) {
    do {
      for (i=curpos,end=ppos.index; i<end; ++i) {
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
        var ret = '', j, len, val;
        for (j=0,len=tokens.length; j<len; ++j) {
          ret += parts[j];
          ret += self._format_value(values[tokens[j]]);
        }
        if (j < parts.length)
          ret += parts[j];
        return ret;
      };
      this._queryCache && this._queryCache.set(query, fn);
      return fn;
    }
  }
  return function() { return query; };
};

Client.prototype._format_value = function (v) {
  if (Buffer.isBuffer(v)) return "'" + addon.escape(v.toString('utf8')) + "'";
  else if (Array.isArray(v)) {
    var r = [];
    for (var i = 0; i < v.length; i++) r.push(this._format_value(v[i]));
    return r.join(',');
  }
  else if (v !== null) return "'" + addon.escape(v + '') + "'";
  else return 'NULL';
};

Client.prototype._reset = function() {
  clearTimeout(this._pinger);
  this._closeOnEmpty = false;
  if (!this._keepQueries)
    this._queries = [];
  else
    // only conn.error and close event cause _reset(),
    // neither abort nor end event has been fired for _curResults
    // so keep _curResults for future error handling or replay after auto-reconnect
    if (this._curResults)
      this._queries.unshift(this._curResults);
  this._curResults = undefined;
};

Client.prototype._processQueries = function() {
  if (this._curResults === undefined && this.connected) {
    clearTimeout(this._pinger);
    if (this._queries.length) {
      this._curResults = this._queries.shift();
      this._client.query(this._curResults._query, this._curResults._useArray);
    } else if (this._closeOnEmpty)
      this.destroy();
    else {
      var self = this;
      this._pinger = setTimeout(function ping() {
        self.query('DO 0', true, true).on('error', function() {});
      }, this.pingInterval);
    }
  }
};

function Query(parent) {
  this._removed = false;
  this._parent = parent;
}
inherits(Query, EventEmitter);
Query.prototype.abort = function(active) {
  if (!this._removed) {
    // if active perform "KILL QUERY n" on separate connection
    this._removed = true;
    this._parent.client._client.abortQuery(ABORT_QUERY);
  }
  return this._removed;
};

function Results(client, query, useArray) {
  this.client = client;
  this._curQuery = undefined;
  this._query = query;
  this._useArray = useArray;
  this._aborted = false;
}
inherits(Results, EventEmitter);
Results.prototype.abort = function(active) {
  if (!this._aborted) {
    // if active perform "KILL QUERY n" on separate connection
    this._aborted = true;
    var i;
    if (this === this.client._curResults) {
      // abort immediately if we're the currently running query
      this.client._client.abortQuery(ABORT_RESULTS);
    } else if ((i = this.client._queries.indexOf(this)) > -1) {
      // remove from the queue if the query hasn't started executing yet
      this.client._queries.splice(i, 1);
      this.emit('abort');
    }
  }
  return this._aborted;
};

Client.LIB_VERSION = addon.version();
Client.escape = addon.escape;
module.exports = Client;
