
Description
===========

A [node.js](http://nodejs.org/) binding to MariaDB's non-blocking (MySQL-compatible) client
library.

This binding is different from other vanilla libmysqlclient bindings in that it
uses the non-blocking functions available in MariaDB's client library.

Therefore, this binding does **_not_** use multiple threads to achieve non-blocking
behavior.

This binding is currently tested on Windows and Linux.


Requirements
============

* [node.js](http://nodejs.org/) -- v0.8.0 or newer

* \*nix users need to install the libmariadbclient-dev package (must be 5.5.21+).
  Linux users can get this from [the MariaDB repositories](http://downloads.mariadb.org/MariaDB/repositories/).


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

c.on('connect', function() {
   console.log('Client connected');
 })
 .on('error', function(err) {
   console.log('Client error: ' + err);
 })
 .on('close', function(had_err) {
   console.log('Client closed');
 });

c.query("SHOW DATABASES")
 .on('result', function(result) {
   console.log('Query result: ' + inspect(result));
 })
 .on('error', function(err) {
   console.log('Query error: ' + inspect(err));
 })
 .on('end', function(info) {
   console.log('Query finished successfully');
   c.end();
 });

c.connect({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar'
});
```


API
===

`require('mariasql')` returns a **_Client_** object

Client properties
-----------------

* **connected** - <_boolean_> - Set to true if the Client is currently connected.

* **threadId** - <_integer_> - If connected, this is the thread id of this connection on the server.


Client events
-------------

* **connect**() - Connection and authentication with the server was successful.

* **error**(<_Error_>err) - An error occurred at the connection level.

* **close**(<_boolean_>hadError) - The connection was closed. `hadError` is set to true if this was due to a connection-level error.


Client methods
--------------

* **(constructor)**() - Creates and returns a new Client instance.

* **connect**(<_object_>config) - _(void)_ - Attempts a connection to a server using the information given in `config`:

    * **user** - <_string_> - Username for authentication. **Default:** (\*nix: current login name, Windows: ???)

    * **password** - <_string_> - Password for authentication. **Default:** (blank password)

    * **host** - <_string_> - Hostname or IP address of the MySQL/MariaDB server. **Default:** "localhost"

    * **port** - <_integer_> - Port number of the MySQL/MariaDB server. **Default:** 3306

    * **db** - <_string_> - A database to automatically select after authentication **Default:** (no db)

* **query**(<_string_>query[, <_boolean_>useArray=false]) - <_Query_> - Enqueues the given `query` and returns a _Query_ instance. If `useArray` is set to true, then an array of field values are returned instead of an object of fieldName=>fieldValue pairs.

* **escape**(<_string_>value) - <_string_> - Escapes `value` for use in queries. **_This method requires a live connection_**.

* **end**() - _(void)_ - Closes the connection once all queries in the queue have been executed.

* **destroy**() - _(void)_ - Closes the connection immediately, even if there are other queries still in the queue.

* **isMariaDB**() - <_boolean_> - Returns true if the remote server is MariaDB.


Query events
------------

* **result**(<_mixed_>res) - `res` is either an object of fieldName=>fieldValue pairs **or** just an array of the field values (in either case, JavaScript nulls are used for MySQL NULLs), depending on how query() was called.

* **abort**() - The query was aborted (the 'end' event will not be emitted) by way of query.abort().

* **error**(<_Error_>err) - An error occurred while executing this query (the 'end' event will not be emitted).

* **end**(<_object_>info) - The query finished _successfully_. `info` contains statistics such as 'affectedRows', 'insertId', and 'numRows.'


Query methods
-------------

* **abort**() - _(void)_ - Aborts query immediately if the query is either currently queued or is (about to start) returning rows (if applicable). If the query is still being processed on the server side, the query will not be aborted until the server finishes processing it (i.e. when rows are about to be returned for SELECT queries). In any case, you can always kill the currently running query early by executing "KILL QUERY _client_threadId_" from a separate connection.


TODO
====

* Multiple statement (e.g. "SELECT * FROM foo; SELECT * FROM bar" vs. "SELECT * FROM foo") support

* Compression

* SSL encrypted connections

* Auto-reconnect algorithm(s) ?

* Method to change character set

* Possibly some other stuff I'm not aware of at the moment
