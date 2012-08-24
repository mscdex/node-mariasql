
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

c.on('error', function(err) {
  console.log('Client error: ' + err);
});

c.on('close', function(had_err) {
  console.log('Closed');
});

c.query("SHOW DATABASES")
 .on('result', function(result) {
   console.log('Query result: ' + inspect(result));
 })
 .on('error', function(err) {
   console.log('Query error: ' + inspect(err));
 })
 .on('end', function() {
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

require('mariasql') returns a **_Client_** object

Client events
-------------

* **connect**() - A connection to the server was successful.

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

* **query**(<_string_>query) - <_Query_> - Enqueues the given `query` and returns a _Query_ instance.

* **escape**(<_string_>value) - Escapes `value` for use in queries. **_This method requires a live connection_**.

* **end**() - Closes the connection once all queries in the queue have been executed.

* **destroy**() - Closes the connection immediately, even if there are other queries still in the queue.


Query events
------------

* **result**(<_object_>res) - `res` is an object of fieldName=>value pairs, where value is a string.

* **end**(<_object_>info) - The query finished successfully. `info` contains statistics such as 'affectedRows', 'insertId', and 'numRows.'

* **abort**() - The query was aborted (the 'end' event will not be emitted) by way of query.abort().

* **error**(<_Error_>err) - An error occurred while executing this query (the 'end' event will not be emitted).


Query methods
-------------

* **abort**() - Aborts the query if possible. This currently will not work for "blocking queries" (e.g. "SELECT SLEEP(10000)") that are not generating network traffic (e.g. returning results).


TODO
====

* Multiple statement (e.g. "SELECT * FROM foo; SELECT * FROM bar" vs. "SELECT * FROM foo") support

* Compression

* SSL encrypted connections

* Auto-reconnect algorithm(s) ?

* Method to change character set

* Possibly some other stuff I'm not aware of at the moment
