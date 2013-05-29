
Description
===========

A [node.js](http://nodejs.org/) binding to MariaDB's non-blocking (MySQL-compatible) client
library.

This binding is different from other vanilla libmysqlclient bindings in that it
uses the non-blocking functions available in MariaDB's client library.

Therefore, this binding does **_not_** use multiple threads to achieve non-blocking
behavior.

This binding has been tested on Windows, Linux, and Mac (OSX 10.6).

Benchmarks comparing this module to the other node.js MySQL driver modules can be
found [here](http://mscdex.github.com/node-mysql-benchmarks).


Requirements
============

* [node.js](http://nodejs.org/) -- v0.8.0 or newer


Install
============

    npm install mariasql


Examples
========

* Simple query to retrieve a list of all databases:

```javascript
var inspect = require('util').inspect;
var Client = require('mariasql');

var c = new Client();
c.connect({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar'
});

c.on('connect', function() {
   console.log('Client connected');
 })
 .on('error', function(err) {
   console.log('Client error: ' + err);
 })
 .on('close', function(hadError) {
   console.log('Client closed');
 });

c.query('SHOW DATABASES')
 .on('result', function(res) {
   res.on('row', function(row) {
     console.log('Result row: ' + inspect(row));
   })
   .on('error', function(err) {
     console.log('Result error: ' + inspect(err));
   })
   .on('end', function(info) {
     console.log('Result finished successfully');
   });
 })
 .on('end', function() {
   console.log('Done with all results');
 });

c.end();
```

* Use placeholders in a query

```javascript
var inspect = require('util').inspect;
var Client = require('mariasql');

var c = new Client();
c.connect({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar',
  db: 'mydb'
});

c.on('connect', function() {
   console.log('Client connected');
 })
 .on('error', function(err) {
   console.log('Client error: ' + err);
 })
 .on('close', function(hadError) {
   console.log('Client closed');
 });

c.query('SELECT * FROM users WHERE id = :id AND name = :name',
        { id: 1337, name: 'Frylock' })
 .on('result', function(res) {
   res.on('row', function(row) {
     console.log('Result row: ' + inspect(row));
   })
   .on('error', function(err) {
     console.log('Result error: ' + inspect(err));
   })
   .on('end', function(info) {
     console.log('Result finished successfully');
   });
 })
 .on('end', function() {
   console.log('Done with all results');
 });

c.query('SELECT * FROM users WHERE id = ? AND name = ?',
        [ 1337, 'Frylock' ])
 .on('result', function(res) {
   res.on('row', function(row) {
     console.log('Result row: ' + inspect(row));
   })
   .on('error', function(err) {
     console.log('Result error: ' + inspect(err));
   })
   .on('end', function(info) {
     console.log('Result finished successfully');
   });
 })
 .on('end', function() {
   console.log('Done with all results');
 });

c.end();
```

* Explicitly generate a prepared query for later use

```javascript
var inspect = require('util').inspect;
var Client = require('mariasql');

var c = new Client();
c.connect({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar',
  db: 'mydb'
});

c.on('connect', function() {
   console.log('Client connected');
 })
 .on('error', function(err) {
   console.log('Client error: ' + err);
 })
 .on('close', function(hadError) {
   console.log('Client closed');
 });

var pq = c.prepare('SELECT * FROM users WHERE id = :id AND name = :name');

c.query(pq({ id: 1337, name: 'Frylock' }))
 .on('result', function(res) {
   res.on('row', function(row) {
     console.log('Result row: ' + inspect(row));
   })
   .on('error', function(err) {
     console.log('Result error: ' + inspect(err));
   })
   .on('end', function(info) {
     console.log('Result finished successfully');
   });
 })
 .on('end', function() {
   console.log('Done with all results');
 });

c.end();
```

* Abort the second query when doing multiple statements

```javascript
var inspect = require('util').inspect;
var Client = require('mariasql');

var c = new Client(), qcnt = 0;
c.connect({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar',
  multiStatements: true
});

c.on('connect', function() {
   console.log('Client connected');
 })
 .on('error', function(err) {
   console.log('Client error: ' + err);
 })
 .on('close', function(hadError) {
   console.log('Client closed');
 });

c.query('SELECT "first query"; SELECT "second query"; SELECT "third query"', true)
 .on('result', function(res) {
   if (++qcnt === 2)
     res.abort();
   res.on('row', function(row) {
     console.log('Query #' + (qcnt) + ' row: ' + inspect(row));
   })
   .on('error', function(err) {
     console.log('Query #' + (qcnt) + ' error: ' + inspect(err));
   })
   .on('abort', function() {
     console.log('Query #' + (qcnt) + ' was aborted');
   })
   .on('end', function(info) {
     console.log('Query #' + (qcnt) + ' finished successfully');
   });
 })
 .on('end', function() {
   console.log('Done with all queries');
 });

c.end();

/* output:
    Client connected
    Query #1 row: [ 'first query' ]
    Query #1 finished successfully
    Query #2 was aborted
    Query #3 row: [ 'third query' ]
    Query #3 finished successfully
    Done with all queries
    Client closed
 */
```


API
===

`require('mariasql')` returns a **_Client_** object

Client properties
-----------------

* **connected** - < _boolean_ > - Set to true if the Client is currently connected.

* **threadId** - < _string_ > - If connected, this is the thread id of this connection on the server.


Client events
-------------

* **connect**() - Connection and authentication with the server was successful.

* **error**(< _Error_ >err) - An error occurred at the connection level.

* **close**(< _boolean_ >hadError) - The connection was closed. `hadError` is set to true if this was due to a connection-level error.


Client methods
--------------

* **(constructor)**() - Creates and returns a new Client instance.

* **connect**(< _object_ >config) - _(void)_ - Attempts a connection to a server using the information given in `config`:

    * **user** - < _string_ > - Username for authentication. **Default:** (\*nix: current login name, Windows: ???)

    * **password** - < _string_ > - Password for authentication. **Default:** (blank password)

    * **host** - < _string_ > - Hostname or IP address of the MySQL/MariaDB server. **Default:** "localhost"

    * **port** - < _integer_ > - Port number of the MySQL/MariaDB server. **Default:** 3306

    * **unixSocket** - < _string_ > - Path to a unix socket to connect to (host and port are ignored). **Default:** (none)

    * **db** - < _string_ > - A database to automatically select after authentication. **Default:** (no db)

    * **keepQueries** - < _boolean_ > - Keep query queue when connection closes? **Default:** false

    * **multiStatements** - < _boolean_ > - Allow multiple statements to be executed in a single "query" (e.g. `connection.query('SELECT 1; SELECT 2; SELECT 3')`) on this connection. **Default:** false

    * **connTimeout** - < _integer_ > - Number of seconds to wait for a connection to be made. **Default:** 10

    * **pingInterval** - < _integer_ > - Number of seconds between pings while idle. **Default:** 60

    * **secureAuth** - < _boolean_ > - Use password hashing available in MySQL 4.1.1+ when authenticating. **Default:** true

    * **compress** - < _boolean_ > - Use connection compression? **Default:** false

    * **ssl** - < _mixed_ > - If boolean true, defaults listed below and default ciphers will be used, otherwise it must be an object with any of the following valid properties: **Default:** false

        * **key** - < _string_ > - Path to a client private key file in PEM format (if the key requires a passphrase and libmysqlclient was built with yaSSL (bundled Windows libraries are), an error will occur). **Default:** (none)

        * **cert** - < _string_ > - Path to a client certificate key file in PEM format. **Default:** (none)

        * **ca** - < _string_ > - Path to a file in PEM format that contains a list of trusted certificate authorities. **Default:** (none)

        * **capath** - < _string_ > - Path to a directory containing certificate authority certificate files in PEM format. **Default:** (none)

        * **cipher** - < _string_ > - A colon-delimited list of ciphers to use when connecting. **Default:** "ECDHE-RSA-AES128-SHA256:AES128-GCM-SHA256:RC4:HIGH:!MD5:!aNULL:!EDH" (if cipher is set to anything other than false or non-empty string)

        * **rejectUnauthorized** - < _boolean_ > - If true, the connection will be rejected if the Common Name value does not match that of the host name. **Default:** false

* **query**(< _string_ >query[, < _mixed_ >values[, < _boolean_ >useArray=false]]) - < _Results_ > - Enqueues the given `query` and returns a _Results_ object. `values` can be an object or array containing values to be used when replacing placeholders in `query` (see prepare()). If `useArray` is set to true, then an array of field values are returned instead of an object of fieldName=>fieldValue pairs. (Note: using arrays performs much faster)

* **prepare**(< _string_ >query) - < _function_ > - Generates a re-usable function for `query` when it contains placeholders (can be simple `?` position-based or named `:foo_bar1` placeholders or any combination of the two). In the case that the function does contain placeholders, the generated function is cached per-connection if it is not already in the cache (currently the cache will hold at most **30** prepared queries). The returned function takes an object or array and returns the query with the placeholders replaced by the values in the object or array. **Note:** Every value is converted to a (utf8) string when filling the placeholders.

* **escape**(< _string_ >value) - < _string_ > - Escapes `value` for use in queries. **_This method requires a live connection_**.

* **end**() - _(void)_ - Closes the connection once all queries in the queue have been executed.

* **destroy**() - _(void)_ - Closes the connection immediately, even if there are other queries still in the queue.

* **isMariaDB**() - < _boolean_ > - Returns true if the remote server is MariaDB.


Client static methods
---------------------

* **escape**(< _string_ >value) - < _string_ > - Escapes `value` for use in queries. **_This method does not take into account character encodings_**.


Results events
--------------

* **result**(< _Query_ >res) - `res` represents the result of a single query.

* **abort**() - The results were aborted (the 'end' event will not be emitted) by way of results.abort().

* **error**(< _Error_ >err) - An error occurred while processing this set of results (the 'end' event will not be emitted).

* **end**() - All queries in this result set finished _successfully_.


Results methods
---------------

* **abort**() - _(void)_ - Aborts any remaining queries (if multiple statements are used) immediately if the queries are either currently queued or are (about to start) returning rows (if applicable). This is a passive abort, so if the current query is still being processed on the server side, the queries will not be aborted until the server finishes processing the current query (i.e. when rows are about to be returned for SELECT queries). In any case, you can always kill the currently running query early by executing "KILL QUERY _client_threadId_" from a separate connection (note: this query can be dangerous if modifying information without transactions).


Query events
------------

* **row**(< _mixed_ >row) - `row` is either an object of fieldName=>fieldValue pairs **or** just an array of the field values (in either case, JavaScript nulls are used for MySQL NULLs), depending on how query() was called.

* **abort**() - The query was aborted (the 'end' event will not be emitted) by way of query.abort().

* **error**(< _Error_ >err) - An error occurred while executing this query (the 'end' event will not be emitted).

* **end**(< _object_ >info) - The query finished _successfully_. `info` contains statistics such as 'affectedRows', 'insertId', and 'numRows.'


Query methods
-------------

* **abort**() - _(void)_ - Aborts query immediately if the query is either currently queued or is (about to start) returning rows (if applicable). If the query is still being processed on the server side, the query will not be aborted until the server finishes processing it (i.e. when rows are about to be returned for SELECT queries). In any case, you can always kill the currently running query early by executing "KILL QUERY _client_threadId_" from a separate connection (note: this query is dangerous if modifying information without transactions).


TODO
====

* Auto-reconnect algorithm(s) ?

* API to execute "KILL QUERY _client_threadId_" easily

* Possibly some other stuff I'm not aware of at the moment
