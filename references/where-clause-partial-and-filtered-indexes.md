<!-- Source: https://use-the-index-luke.com/sql/where-clause/partial-and-filtered-indexes — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Partial Indexes

<sub>Source: https://use-the-index-luke.com/sql/where-clause/partial-and-filtered-indexes</sub>

So far we have only discussed which *columns* to add to an index. With *partial* (PostgreSQL) or *filtered* (SQL Server) indexes you can also specify the *rows* that are indexed.

> **Caution:**
>
> The Oracle database has a unique approach to partial indexing. The [next section](where-clause-null.md) explains it while building upon this section.
>
> Db2 (LUW) does not support partial indexes, but the can be [emulated like in the Oracle database](https://use-the-index-luke.com/blog/2014-11/seven-surprising-findings-about-DB2#blog-db2intro-partialidx) when using the `EXCLUDE NULL KEYS` feature.

A partial index is useful for commonly used `where` conditions that use constant values—like the status code in the following example:

```
SELECT message
  FROM messages
 WHERE processed = 'N'
   AND receiver  = ?
```

Queries like this are very common in queuing systems. The query fetches all unprocessed messages for a specific recipient. Messages that were already processed are rarely needed. If they are needed, they are usually accessed by a more specific criteria like the primary key.

We can optimize this query with a two-column index. Considering this query only, the column order does not matter because there is no range condition.

```
CREATE INDEX messages_todo
          ON messages (receiver, processed)
```

The index fulfills its purpose, but it includes many rows that are never searched, namely all the messages that were already processed. Due to the logarithmic scalability the index nevertheless makes the query very fast even though it wastes a lot of disk space.

With partial indexing you can limit the index to include only the unprocessed messages. The syntax for this is surprisingly simple: a `where` clause.

```
CREATE INDEX messages_todo
          ON messages (receiver)
       WHERE processed = 'N'
```

The index only contains the rows that satisfy the `where` clause. In this particular case, we can even remove the `PROCESSED` column because it is always `'N'` anyway. That means the index reduces its size in two dimensions: vertically, because it contains fewer rows; horizontally, due to the removed column.

The index is therefore very small. For a queue, it can even mean that the index size remains unchanged although the table grows without bounds. The index does not contain all messages, just the unprocessed ones.

The `where` clause of a partial index can become arbitrarily complex. The only fundamental limitation is about functions: you can only use deterministic functions as is the case everywhere in an index definition. SQL Server has, however, more restrictive rules and [neither allow functions nor the `OR` operator in index predicates](https://learn.microsoft.com/en-us/sql/t-sql/statements/create-index-transact-sql?view=sql-server-ver16).

A database can use a partial index whenever the `where` clause appears in a query.

> **Think About It:**
>
> What peculiarity has the smallest possible index for the following query:
>
> ```
> SELECT message
>   FROM messages
>  WHERE processed = 'N'
> ```
