# SpinoDB

SpinoDB is an in-memory NoSQL database that is small and self-contained and with emphasis on **speed**. It is written in C++ and has bindings for both NodeJS and C using GObjects. Bindings for Vala, Python, Java, GJS (Gnome Javascript), Lua and so on are all automatically available through GObject Introspection. 


### When To Use It

##### Web Services

SpinoDB integrates nicely with Node & Express. By switching from an enterprise grade solution to SpinoDB, small/medium websites might notice improvements in website responsiveness and lower server CPU useage. 

Bots and online services that require fast document retrieval may find SpinoDB a more appropriate solution than a fully fledged enterprise system. 

SpinoDB may also be used as a fast cache to complement a traditional database.

##### Desktop Applications

Desktop applications can use SpinoDB as an alternative to creating an ad-hoc format for application specific data. 
- application settings
- data logging
- application specific data

##### Limitations

The practical limits are determined by 
- how much RAM your system has. It is an in memory database. 
- loading / saving time. If your system is reasonably spec'd you could budget ~100MB/s to load and save the database to a file.

As a rule of thumb, if the size of your data is greater than 50% of your available RAM, or if the save time is unmanageable, then this isn't for you. 

### Installation

## NodeJS 

    npm install spinodb

## GObject

    meson --prefix=/usr builddir
    ninja -C builddir
    cd builddir && sudo meson install

This will build and install the libary along with pkg-config, gir, vapi and typelib files.
    

### Design

SpinoDB stores data in two formats. 
* Collections
* Key/value store

A collection is roughly analogous to a table in an SQL database, except that instead of containing one dimensional rows, each entry in a collection contains a JSON document. The documents are not required to be consistent.

Key/value store is used to store and retrieve a value with some descriptive text (the key). This is a more elegant method of storing application settings than collections. 

The NodeJS and GObject bindings are almost identical except for these differences.
- The NodeJS bindings use camel case but GObject bindings use snake case. e.g. getCollection in NodeJS and get_collection for GObject bindings.
- The NodeJS bindings will convert objects to JSON strings. The GObject bindings strictly use strings to represent JSON documents. 


### Example

	const spino = require('spinodb');
    var db = new spino.Spino(); // create the db instance
    db.load('data.db'); // load from disk
    
    // get the users collection
    // if the collection doesn't exist, it is created
    var userColl = db.getCollection("users"); 
    
    // retrieve a user called Dave
    var result = userColl.findOne('{ name: "Dave" });
    console.log(result); 

    // update Dave's score to 50. if the score field does not exist, it is created. 
    userColl.update('{ name: "Dave" }', '{"score": 50}'); 


### Collections

db.getCollection(<collection_name>) will return a collection, or create one if it doesn't exist

    var col = db.getCollection("users");

db.addCollection(<collection_name>) will add a collection to the database.

    var col = db.addCollection("users");

db.dropCollection(<collection_name>) will remove a collection from the database.

    db.dropCollection("users"); //destroy all user data


### Adding Documents

append() will add a JSON document to a collection.

    col.append({
        name: "Dave",
        score: 50,
        subdoc: {
	        item: "An item in a subdocument"
	        }
	     array: [ 1, 2, 3, 4]
        });

append() will accept either a Javascript object or JSON string. The GObject bindings use strings only to represent documents.

### Find Queries

findOne() will retrieve exactly one document from the collection. The result is either a string of JSON data, or undefined if the query does not match any documents.

    result = collection.findOne(<query>);

find() will return a Cursor that can be used to iterate over the results.

    var cursor = collection.find(<query>);
    while(cursor.hasNext()) {
	var doc = cursor.next();
	// do something with the document
    } 
    
Creating an array from a cursor can be done like this
  
	var arr = cursor.toArray();

or as a one liner
	
	var arr = collection.find(<query>).toArray();

The array will contain all of the results as Javascript objects.


A cursor can also count the number of documents that it the query matches.

	collection.find(<query>).count();


An alternative to using cursors is to use the command execution interface to make SpinoDB collate the results into a string for you.

	var array = db.execute({
		cmd: "find",
		collection: "collectionName",
		query: <query>,
		limit: 100
	});

    

### Updating Documents

update() will merge a JSON document into existing documents that match the search query. Existing fields will be overwritten and fields that do not exist will be created.

    collection.update(<query>, <updated_document>);

It isn't possible to delete a field with update(). To remove fields from a document, you must use find() to get a copy of the document, drop() to remove the document and then append() to insert a new document with the required format.

### Deleting Documents

dropOne() will delete exactly one document from the collection that matches the search query.

    collection.dropOne(<query>);

drop() will drop all documents that match the query.

    collection.drop(<query>);


dropOlderThan() will drop all documents older than a timestamp.

	collection.dropOlderThan(<time value>);

e.g. dropping all documents more than 2 weeks old

	let now = new Date().getTime();
	let twoWeeksAgo = now - (1000*60*60*24*14);	//2 weeks in milliseconds
	let nDocumentsDropped = collection.dropOlderThan(twoWeeksAgo);

### Query Language

SpinoDB has a language for matching documents. Spino queries are **not** JSON. You cannot build a query as a javascript object and stringify it. You must build a string to create the query.

$eq - Equality operator will return documents if a field matches a value. 

    { name: {$eq: "Dave" }}
   
   If no operator is present then $eq is implied

    { name: "Dave" }

$ne - Not equal. Will return documents where the field does NOT match the value

    { name: {$ne: "Dave" }}
$gt - Greater than. Will only match documents if the field is a numeric value and is greater than the value

    {score: {$gt: 20}}
$lt - Less than. Will only match documents if the field is a numeric value and is less than the value

    {score: {$lt: 20}}

$in - Checks if a field contains one of many values. This example will return any document if the name field matches one of the specified names.

    {name: {$in: ["Dave", "Mike", "Alexis"]}}
$nin - Not in. Check if a field does not contain one of many values. This example will return any document that does not have one of the specified names. Results will exclude these names.

    {name: {$nin: ["Dave", "Mike", "Alexis"]}}
$exists - Checks if a field exists or not. If 'true' is passed as the parameter, then it will return documents that have the field. False will return documents that are missing the field.

    {name: {$exists: true}}
$type - Checks if a field is of a type. The type parameter can be either*number, string, bool, array, object*. This example will return all documents where the field 'name' is of type string.

    {name: {$type: string}}

$startsWith - will get documents with a string field that starts with the characters specified. Case sensitive.

	{name: {$startsWith: "D"}}

$regex - will match a string field against a regex query. The regex flavour is ECMAScript, not PCRE. ECMAScript does not allow for case insensitive searching, so SpinoDB has borrowed the "(?i)" modifier to do this. Any regex query that begins with (?i) will make the search case insensitive. 

	{name: {$regex: "^D.*"}}

	{name: {$regex: "(?i)d"}}

 
### Logical Expressions

$and - can be used to check multiple attributes of the document. All sub expressions must be true for the document to match.

    {$and: [{name: "Dave"}, {score: {$gt: 20}}]}
This example gets every document with the name Dave and a score greater than 20.

$or - any sub expression can be true for the document to match.

    {$or: [{name: "Dave"}, {score: {$lt: 20}}]}

This example gets documents with the name Dave or if the score is less than 20.

$not - inverts the result of an expression

    {$not: {name: "Dave"}}
Matches documents where the name is not Dave.

### Sub Object Field Names

Field names are the basis of query operations and are used to identify data in a JSON document. JSON documents can contain objects inside of objects.

    {
        name: "Dave",
        steamProfile: {
                steamId: "7656112598325978325",
                steamPic: "https://steamcommunity.com/profile/picture/7656114829873"
            }
    }
To query the field of a sub object, the following syntax can be used

    { steamProfile.steamId: "7656112598325978325" }


### Projections

Projections can be used to retrieve only selected fields of a document. Projections can only be applied to cursors. The projection is a JSON string that has the document fields to return as keys. The values must be 1. 

Example

    var cursor = collection.find("{score: {$gt: 20}}").setProjection("{\"name\": 1}");

This will create a cursor that return documents that only contain the names of players with a score greater than 20.

Projections are a white-list only. If a projection is applied, only the fields set to 1 in the projection JSON string will be retrieved from the document. Fields of sub-objects can be selected like this

    '{ "subobject": { "field": 1 }}'


### Limits

A limit can be set on the number of documents that a cursor can return. 

Example

    var cursor = collection.find("{ score: {$gt: 20}}").setLimit(10);

Projections and limits can be chained together like this

    var cursor = collection.find("<query>").setProjection("<projection>").setLimit(10);

### Cursor Scripting

The Cursors returned from find queries can execute Squirrel scripts to further refine search results. Cursor scripting can be used to
* order the search results
* format the query result into a particular format (e.g. CSV or XML)
* aggregation and process query results within SpinoDB
* write data to files

The benefits of using cursor scripts over processing results in 'native' code are
* Speed. Cursor scripts generally process results faster than Javascript and C/Vala. 
* Simplicity. Mostly to the benefit of C/Vala users, cursor scripts provide a very concise way of processing query results.  

Squirrel language documentation: 
http://www.squirrel-lang.org/squirreldoc/reference/language.html

Cursor scripts must contain two functions called 'result' and 'finalize'. 

The result function is called iteratively for each document that the query matches. The document is passed as a 'table' to the 'result' function. Once the cursor has no more matching documents, the 'finalize' function will be called. The finalize function does not take any parameters but it must return a result. The result 
is returned as a string to the application. If the result is a table or array, it will be stringified into JSON.


```
csv <- "";
function result(doc) {
    foreach(key, value in doc) {
        csv += value.tostring() + ",";
    }
    csv += "\n";
}

function finalize() {
    return csv;
}
```
The above example will dump all values in the documents to a CSV formatted string. 

Cursor scripting is a very powerful and flexible way to deal with the data, but it's much better and more efficient to use the query language and projections whenever possible, even just to whittle down the results. 

#### Running Scripts

```
var oldest_trains = db.get_collection("trains").
                    find("{}").
                    projection("{\"name\": 1, \"year\": 1}").
		    run_script("""

trains <- [];
function result(r) {
    if(trains.len() < 5) {
        trains.append(r);
        trains.sort(@(a,b) a.year <=> b.year);
    }
    else {
        if(r.year < trains[4].year) {
            trains[4] = r;
            trains.sort(@(a,b) a.year <=> b.year);
	}
    }
}

function finalize() {
    return trains;
}
""");
```
This Vala example starts by retrieving every document in the 'trains' collection and setting a projection to only retrieve the 'name' and 'year' fields. The script then keeps a track of only the 5 oldest trains and sorts them by the 'year' field. The returned value of the finalize function, which is an ordered list of the 5 oldest trains, is stringified and assigned to the 'oldest_trains' variable. 
 
#### Notes on script performance

Cursor scripts are generally more efficient for processing large numbers of results than repeatedly calling cursor.next() in your application. With the 'solarsystem' test case, the cursor script executes about 30% faster than equivalent NodeJS javascript and 65% faster than equivalent Vala. Cursor scripts are fast because the documents processed by cursor scripts are not converted to and from strings. They are converted from Spino's internal DOM memory format directly to a table in the Squirrel VM which is a fairly efficient operation. On the other hand, processing results by calling cursor.next() will make Spino convert the document into a string. Most likely, the application will parse the document back into an object/DOM. 

Cursor scripts 
```Spino DOM -> Squirrel table```

Processing cursor results in 'native' language
```Spino DOM -> stringified -> parsed -> 'native' dom/table```

The Json-Glib parser seems particularly slow and is the reason why Vala (a compiled language) runs slower than Javascript when processing query results. 


### _id Field

When documents are first added to the database, they are given a unique ID. The ID encodes the document creation time which is used to optimize searching operations. For best performance, use the ByID functions.

	updateById(<id_string>, <newDocument>);

	findOneById(<id_string>);

	dropById(<id_string>);


### Indexing

Collections can be indexed. Indexing yields huge performance increases because the database can search for results far more efficiently. Note that indexed fields are not saved to disk and must be created in the application each time it runs.

col.createIndex(<field_name>);

	col.createIndex("steamProfile.steamId");




### Key/Value Store

The key/value interface is very simple. Values can be stored using the following functions.

```
db.setIntValue(key name, value)
db.setUintValue(key name, value)
db.setDoubleValue(key name, value)
db.setStringValue(key name, string)

db.getIntValue(key name)
db.getUintValue(key name)
db.getDoubleValue(key name)
db.getStringValue(key name)

db.hasKey(key name)
```



### Command Execution

A database administrator will, from time to time, have a requirement to run arbitrary queries on the data. SpinoDB does not follow the usual client-server model that would allow an administrator to simply login and run queries.

As an alternative, SpinoDB offers a command execution interface. This simplifies the creation of some kind of remote access because all the application needs to do is pass JSON objects or strings to the db.execute() function. A web server might have a secure URL end point that can be used to run queries with POSTman. 

Commands can be either JSON strings or Javascript objects. 

	var resultStr = db.execute(<string>);

	var resultObj = db.execute(<object>);


Examples:

	var result = db.execute({
		cmd: "createIndex",
		collection: "testCollection",
		field: "number"
	});

	var document = db.execute({
		cmd: "findById",
		collection: "testCollection",
		id: idString
	});

	var documentArray = db.execute({
		cmd: "find",
		collection: "testCollection",
		query: "{$and: [{number: {$gt: 10}}, {number: {$lt: 20}}]}"
	});

	var result = db.execute({
		cmd: "append",
		collection: "testCollection",
		document: JSON.stringify({
			number: 10,
			text: "a test object"
		})
	});

	var result = db.execute({
		cmd: "dropOne",
		collection: "testCollection",
		query: "{number: 10}"
	});

	var result = db.execute({
		cmd: "drop",
		collection: "testCollection",
		query: "{number: {$gt: 10}}"
	});

	var result = db.execute({
		cmd: "save",
		path: "data.db"
	});

The db.load() function does not have a corresponding command because loading should only occur once at start up. There should be no need to load from disk again even durnig long running execution.
	
### Performance Tuning

When SpinoDB executes a search, it goes through 3 stages
1. it will look up a cache of previously conducted queries and return a result from that. the cache is purged every time the collection is modified (appended, updated or a document dropped).
2. if the query is a basic comparison to an indexed field, it will conduct a binary search on the index. this operation is very fast, typically under 50us for a findOne()
3. finally, it will execute the query on every document. this is done linearly from the first document to the last. typically, might take a millisecond, but results vary. absolute worst case scenarious might take hundreds of milliseconds. 

* use the id field with the ById functions whenever possible. performing operations by id is by far the fastest.
* make sure you create indexes for fields you will be using to search for documents
* use findOne over find if you only expect to get 1 result. 

Prefer drop() over a series of calls to dropOne(). drop() can be used delete many documents but will only reconstruct the index once. 

If you have a high rate of data passing through a collection of millions of documents, drop time potentially could become a performance bottleneck. One strategy might be adding a field called pendingDelete to your documents that are ready for deletion. A search query to exclude 'deleted' documents might look like this.

	var cursor = col.find('{$and: [{name: "Dave"}, {pendingDelete: {$exists: false}}]}');

Then at some convenient moment, call 
	
    col.drop("{pendingDelete: {$exists: true}}");
    
Only bother with this is unless drop time becomes problematic.

dropOlderThan() is very fast. Delete old documents using this where possible.


### Comparison To LokiJS

LokiJS is also a NoSQL database, however it has the advantage of being a pure javascript solution and can run in browsers. 

##### Inserting 1 Million Documents (1x index field)

| LokiJS | SpinoDB |
|:-------|:--------|
| 1148ms |  2100ms |


LokiJS is about 2x faster at inserting new documents. This is most likely because SpinoDB requires objects to be stringified and reparsed . .. we working on this.

##### findOne by an indexed field

| LokiJS | SpinoDB |
|--------|---------|
| 0.459ms|0.049ms  |


##### drop/remove by an indexed field

| LokiJS  | SpinoDB |
|---------|---------|
| 2075ms  | 404ms   |


### Persistence And Journalling

The data can be loaded and stored to a file.
db.load(<path_to_file>);

    db.load("data.db");

db.save(<path_to_file>);

    db.save("data.db");
Note that these functions are synchronous / blocking functions. Once your data exceeds 100+MB you may start to notice lags during saves. 

For reference, SpinoDB will parse/stringify about 100MB per second from file on my PC with a mediocre SATA SSD, . 

When journalling is enabled, Spino will record every change to the data to a journal file. In the case that your application crashes (or PC loses power or something), the journal file can be 'replayed' or consolidated with the database file to restore the unsaved data. 

    db.enableJournal("journal.db");

    db.consolidate("data.db");

The normal process for using journalling is
1. Load the database file 
```db.load("data.db");```

2. Enable journalling and set the journal file path 
```db.enableJournal("journal");```

3. Consolidate the journal. This will 'replay' the journal and save the data to the specified file.
```db.consolidate("data.db");```
    

From here, your application probably only needs to explicitly call 'save' periodically, perhaps every 15mins or once per hour. Note that 'save' will also clear the journal. 

