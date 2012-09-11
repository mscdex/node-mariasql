var lookup = require('dns').lookup,
    isIP = require('net').isIP,
    inherits = require('util').inherits,
    EventEmitter = require('events').EventEmitter;

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

function Client() {
  var self = this;
  this.threadId = undefined;
  this.connected = false;
  this._closeOnEmpty = false;
  this._queries = [];
  this._curResults = undefined;
  this._client = new addon.Client();
  // Client-level events
  this._client.on('connect', function() {
    self.connected = true;
    // use CONNECTION_ID() query instead of C library's mysql_thread_id()
    // because according to the MySQL docs, mysql_thread_id() does not work
    // correctly when the thread ID exceeds 32 bits
    self.query('SELECT CONNECTION_ID()', true, true)
        .on('result', function(r) {
          r.on('row', function(row) {
            self.threadId = row[0];
          });
        })
        .on('end', function() {
          self.emit('connect');
        });
  });
  this._client.on('conn.error', function(err) {
    self.connected = false;
    self._reset();
    self.emit('error', err);
  });
  this._client.on('close', function(had_err) {
    self._client.connected = false;
    self.threadId = undefined;
    self._reset();
    self.emit('close', had_err);
  });
  // Results-level events
  this._client.on('results.abort', function() {
    var r = self._curResults;
    self._curResults = undefined;
    r.emit('abort');
    self._processQueries();
  });
  this._client.on('results.error', function(err) {
    if (err.code === 2013 || err.code === 10001/*ERROR_HANGUP*/) {
      // connection closed by server
      self.emit('error', err);
    } else {
      var r = self._curResults;
      self._curResults = undefined;
      r.emit('error', err);
      self._processQueries();
    }
  });
  this._client.on('results.query', function() {
    self._curResults._curQuery = new Query(self._curResults);
    self._curResults.emit('result', self._curResults._curQuery);
  });
  this._client.on('results.done', function() {
    var r = self._curResults;
    self._curResults = undefined;
    r.emit('end');
    self._processQueries();
  });
  // Query-level events
  var fnQueryErr = function(err) {
    if (err.code === 2013 || err.code === 10001/*ERROR_HANGUP*/) {
      // connection closed by server
      self.emit('error', err);
    } else
      self._curResults._curQuery.emit('error', err);
  };
  this._client.on('query.error', fnQueryErr);
  this._client.on('query.row.error', fnQueryErr);
  this._client.on('query.abort', function() {
    self._curResults._curQuery.emit('abort');
  });
  this._client.on('query.row', function(res) {
    self._curResults._curQuery.emit('row', res);
  });
  this._client.on('query.done', function(info) {
    var q = self._curResults._curQuery;
    self._curResults._curQuery = undefined;
    q.emit('end', info);
  });
}
inherits(Client, EventEmitter);

Client.prototype._reset = function() {
  this._closeOnEmpty = false;
  // TODO: do not empty queries if using an auto-reconnect algorithm
  this._queries = [];
  this._curResults = undefined;
};

Client.prototype.isMariaDB = function() {
  return this._client.isMariaDB();
};

Client.prototype.connect = function(cfg) {
  var self = this;
  process.nextTick(function() {
    if (!isIP(cfg.host)) {
      lookup(cfg.host, function(err, address, family) {
        if (err)
          return self.emit('error', err);
        cfg = clone(cfg);
        cfg.host = address;
        self._client.connect(cfg);
      });
    } else
      self._client.connect(cfg);
  });
};

Client.prototype.end = function() {
  if (this._curQuery === undefined && !this._queries.length)
    this.destroy();
  else
    this._closeOnEmpty = true;
};

Client.prototype.destroy = function() {
  if (this.connected)
    this._client.end();
};

Client.prototype.query = function(query, useArray, promote) {
  var results = new Results(this, query, useArray);
  if (promote)
    this._queries.unshift(results);
  else
    this._queries.push(results);
  this._processQueries();
  return results;
};

Client.prototype.escape = function(str) {
  return this._client.escape(str);
};

Client.prototype._processQueries = function() {
  if (this._curResults === undefined && this.connected) {
    if (this._queries.length) {
      this._curResults = this._queries.shift();
      this._client.query(this._curResults._query, this._curResults._useArray);
    } else if (this._closeOnEmpty)
      this.destroy();
  }
};

Client.LIB_VERSION = addon.version();

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
    }
  }
  return this._aborted;
};

Client.Query = Query;
Client.Results = Results;
module.exports = Client;
