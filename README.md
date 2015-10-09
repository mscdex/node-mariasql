
Description
===========

A [node.js](http://nodejs.org/) binding to MariaDB's non-blocking (MySQL-compatible) client
library.

This binding is different from a vanilla libmysqlclient binding in that it
uses the non-blocking functions available in MariaDB's client library. As a result,
this binding does **_not_** use multiple threads to achieve non-blocking behavior.

Benchmarks comparing this module to the other node.js MySQL driver modules can be
found [here](http://mscdex.github.com/node-mysql-benchmarks).

[![Build Status](https://travis-ci.org/mscdex/node-mariasql.svg?branch=rewrite)](https://travis-ci.org/mscdex/node-mariasql)
[![Build status](https://ci.appveyor.com/api/projects/status/xx3pxdmpqept0uc2)](https://ci.appveyor.com/project/mscdex/node-mariasql)

Upgrading from v0.1.x? See a list of (breaking) changes [here](https://github.com/mscdex/node-mariasql/wiki/Upgrading-v0.1.x-to-v0.2.x).


Requirements
============

* [node.js](http://nodejs.org/) -- v0.10.0 or newer


Install
=======

    npm install mariasql


Examples
========

* Simple query:

```javascript
var Client = require('mariasql');

var c = new Client({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar'
});

c.query('SHOW DATABASES', function(err, rows) {
  if (err)
    throw err;
  console.dir(rows);
});

c.end();
```

* Get column metadata:

```javascript
var Client = require('mariasql');

var c = new Client({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar'
});

c.query('SHOW DATABASES', null, { metadata: true }, function(err, rows) {
  if (err)
    throw err;
  // `rows.info.metadata` contains the metadata
  console.dir(rows);
});

c.end();
```

* Use arrays (faster) instead of objects for rows:

```javascript
var Client = require('mariasql');

var c = new Client({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar'
});

c.query('SHOW DATABASES', null, { useArray: true }, function(err, rows) {
  if (err)
    throw err;
  console.dir(rows);
});

c.end();
```

* Use placeholders in a query

```javascript
var Client = require('mariasql');

var c = new Client({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar',
  db: 'mydb'
});

c.query('SELECT * FROM users WHERE id = :id AND name = :name',
        { id: 1337, name: 'Frylock' },
        function(err, rows) {
  if (err)
    throw err;
  console.dir(rows);
});

c.query('SELECT * FROM users WHERE id = ? AND name = ?',
        [ 1337, 'Frylock' ],
        function(err, rows) {
  if (err)
    throw err;
  console.dir(rows);
});

c.end();
```

* Stream rows

```javascript
var Client = require('mariasql');

var c = new Client({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar',
  db: 'mydb'
});

var query = c.query("SELECT * FROM users WHERE id > 1");
query.on('result', function(res) {
  // `res` is a streams2+ Readable object stream
  res.on('data', function(row) {
    console.dir(row);
  }).on('end', function() {
    console.log('Result set finished');
  });
}).on('end', function() {
  console.log('No more result sets!');
});

c.end();
```

* Explicitly generate a prepared query for later use

```javascript
var Client = require('mariasql');

var c = new Client({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar',
  db: 'mydb'
});

var prep = c.prepare('SELECT * FROM users WHERE id = :id AND name = :name');

c.query(prep({ id: 1337, name: 'Frylock' }), function(err, rows) {
  if (err)
    throw err;
  console.dir(rows);
});

c.end();
```


API
===

`require('mariasql')` returns a **_Client_** object


Client properties
-----------------

* **connected** - _boolean_ - `true` if the Client instance is currently connected to the server.

* **connecting** - _boolean_ - `true` if the Client instance is currently in the middle of connecting to the server.

* **threadId** - _string_ - If connected, this is the thread id of this connection on the server.


Client events
-------------

* **ready**() - Connection and authentication with the server was successful.

* **error**(< _Error_ >err) - An error occurred at the connection level.

* **end**() - The connection ended gracefully.

* **close**() - The connection has closed.


Client methods
--------------

* **(constructor)**() - Creates and returns a new Client instance.

* **connect**(< _object_ >config) - _(void)_ - Attempts a connection to a server using the information given in `config`:

    * **user** - _string_ - Username for authentication. **Default:** (\*nix: current login name, Windows: ???)

    * **password** - _string_ - Password for authentication. **Default:** (blank password)

    * **host** - _string_ - Hostname or IP address of the MySQL/MariaDB server. **Default:** "localhost"

    * **port** - _integer_ - Port number of the MySQL/MariaDB server. **Default:** 3306

    * **unixSocket** - _string_ - Path to a unix socket to connect to (host and port are ignored). **Default:** (none)

    * **protocol** - _string_ - Explicit connection method. Can be one of: `'tcp'`, `'socket'`, `'pipe'`, `'memory'`. Any other value uses the default behavior. **Default:** `'tcp'` if `host` or `port` are specified, `'socket'` if `unixSocket` is specified, otherwise default behavior is used.

    * **db** - _string_ - A database to automatically select after authentication. **Default:** (no db)

    * **keepQueries** - _boolean_ - Keep enqueued queries that haven't started executing, after the connection closes? (Only relevant if reusing Client instance) **Default:** false

    * **multiStatements** - _boolean_ - Allow multiple statements to be executed in a single "query" (e.g. `connection.query('SELECT 1; SELECT 2; SELECT 3')`) on this connection. **Default:** false

    * **connTimeout** - _integer_ - Number of seconds to wait for a connection to be made. **Default:** 10

    * **pingInterval** - _integer_ - Number of seconds between pings while idle. **Default:** 60

    * **secureAuth** - _boolean_ - Use password hashing available in MySQL 4.1.1+ when authenticating. **Default:** true

    * **compress** - _boolean_ - Use connection compression? **Default:** false

    * **ssl** - _mixed_ - If boolean true, defaults listed below and default ciphers will be used, otherwise it must be an object with any of the following valid properties: **Default:** false

        * **key** - _string_ - Path to a client private key file in PEM format (if the key requires a passphrase and libmysqlclient was built with yaSSL (bundled Windows libraries are), an error will occur). **Default:** (none)

        * **cert** - _string_ - Path to a client certificate key file in PEM format. **Default:** (none)

        * **ca** - _string_ - Path to a file in PEM format that contains a list of trusted certificate authorities. **Default:** (none)

        * **capath** - _string_ - Path to a directory containing certificate authority certificate files in PEM format. **Default:** (none)

        * **cipher** - _string_ - A colon-delimited list of ciphers to use when connecting. **Default:** "ECDHE-RSA-AES128-SHA256:AES128-GCM-SHA256:RC4:HIGH:!MD5:!aNULL:!EDH" (if cipher is set to anything other than false or non-empty string)

        * **rejectUnauthorized** - _boolean_ - If true, the connection will be rejected if the Common Name value does not match that of the host name. **Default:** false

    * **local_infile** - _boolean_ - If true, will set "local-infile" for the client. **Default:** (none)

        > **NOTE:** the server needs to have its own local-infile = 1 under the [mysql] and/or [mysqld] sections of my.cnf

    * **read_default_file** - _string_ - Provide a path to the my.cnf configuration file to be used by the client. Sets MYSQL_READ_DEFAULT_FILE option in the C client.  **Default:** (none)

        > **FROM MAN PAGE:** These options can be used to read a config file like /etc/my.cnf or ~/.my.cnf. By default MySQL's C client library doesn't use any config files unlike the client programs (mysql, mysqladmin, ...) that do, but outside of the C client library. Thus you need to explicitly request reading a config file...

    * **read_default_group** - _string_ - Provide the name of the group to be read in the my.cnf configuration file without the square brackets e.g. "client" for section [client] in my.cnf.  If not set but "read_default_file" is set, the client tries to read from these groups: [client] or [client-server] or [client-mariadb]. Sets MYSQL_READ_DEFAULT_GROUP option in the C client.  **Default:** (none)

    * **charset** - _string_ - The connection's charset.

    * **streamHWM** - _integer_ - A global `highWaterMark` to use for all result set streams for this connection. This value can also be supplied/overriden on a per-query basis.

* **query**(< _string_ >query[, < _mixed_ >values[, < _object_ >options]][, < _function_ >callback]) - _mixed_ - Enqueues the given `query` and returns a _Results_ object. `values` can be an object or array containing values to be used when replacing placeholders in `query` (see prepare()). If supplying `options` without `values`, you must pass `null` for `values`. If `callback` is supplied, all rows are buffered in memory and `callback` receives `(err, rows)` (`rows` also contains an `info` object containing information about the result set, including metadata if requested). Valid `options`:

    * **useArray** - _boolean_ - When `true`, arrays are used to store row values instead of an object keyed on column names. (Note: using arrays performs much faster)

    * **metadata** - _boolean_ - When `true`, column metadata is also retrieved and available for each result set.

    * **hwm** - _integer_ - This is the `highWaterMark` of result set streams. If you supply a `callback`, this option has no effect.

* **prepare**(< _string_ >query) - _function_ - Generates a re-usable function for `query` when it contains placeholders (can be simple `?` position-based or named `:foo_bar1` placeholders or any combination of the two). In the case that the function does contain placeholders, the generated function is cached per-connection if it is not already in the cache (currently the cache will hold at most **30** prepared queries). The returned function takes an object or array and returns the query with the placeholders replaced by the values in the object or array. **Note:** Every value is converted to a (utf8) string when filling the placeholders.

* **escape**(< _string_ >value) - _string_ - Escapes `value` for use in queries. **_This method requires a live connection_**.

* **isMariaDB**() - _boolean_ - Returns `true` if the remote server is MariaDB.

* **abort**([< _boolean_ >killConn][, < _function_ >callback]) - _(void)_ - If `killConn === true`, then the current connection is killed (via a `KILL xxxx` query on a separate, temporary connection). Otherwise, just the currently running query is killed (via a `KILL QUERY xxxx` query on a separate, temporary connection). When killing just the currently running query, this method will have no effect if the query has already finished but is merely in the process of transferring results from the server to the client.

* **lastInsertId**() - _string_ - Returns the last inserted auto-increment id. If you insert multiple rows in a single query, then this value will return the auto-increment id of the first row, not the last.

* **serverVersion**() - _string_ - Returns a string containing the server version.

* **end**() - _(void)_ - Closes the connection once all queries in the queue have been executed.

* **destroy**() - _(void)_ - Closes the connection immediately, even if there are other queries still in the queue.


Client static properties
------------------------

* Column flags (in metadata):

    * **Client.NOT_NULL_FLAG**: Field cannot be NULL
    * **Client.PRI_KEY_FLAG**: Field is part of a primary key
    * **Client.UNIQUE_KEY_FLAG**: Field is part of a unique key
    * **Client.MULTIPLE_KEY_FLAG**: Field is part of a nonunique key
    * **Client.BLOB_FLAG**: Field is a BLOB or TEXT (deprecated)
    * **Client.UNSIGNED_FLAG**: Field has the UNSIGNED attribute
    * **Client.ZEROFILL_FLAG**: Field has the ZEROFILL attribute
    * **Client.BINARY_FLAG**: Field has the BINARY attribute
    * **Client.ENUM_FLAG**: Field is an ENUM
    * **Client.AUTO_INCREMENT_FLAG**: Field has the AUTO_INCREMENT attribute
    * **Client.TIMESTAMP_FLAG**: Field is a TIMESTAMP (deprecated)
    * **Client.SET_FLAG**: Field is a SET
    * **Client.NO_DEFAULT_VALUE_FLAG**: Field has no default value
    * **Client.ON_UPDATE_NOW_FLAG**: Field is set to NOW on UPDATE
    * **Client.PART_KEY_FLAG**: Field is part of some key
    * **Client.NUM_FLAG**: Field is numeric


Client static methods
---------------------

* **escape**(< _string_ >value) - _string_ - Escapes `value` for use in queries. **_This method does not take into account character encodings_**.

* **version**() - _string_ - Returns a string containing the libmariadbclient version number.


Results events
--------------

* **result**(< _ResultSetStream_ >res) - `res` represents a single result set.

* **error**(< _Error_ >err) - An error occurred while processing this set of results (the 'end' event will not be emitted).

* **end**() - All queries in this result set finished _successfully_.


`ResultSetStream` is a standard streams2+ Readable object stream. Some things to note:

* `ResultSetStream` instances have an `info` property that contains result set-specific information, such as metadata, row count, number of affected rows, and last insert id. These values are populated and available at the `end` event.
