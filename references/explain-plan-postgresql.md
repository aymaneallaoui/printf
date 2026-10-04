<!-- Source: https://use-the-index-luke.com/sql/explain-plan/postgresql — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# PostgreSQL

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/postgresql</sub>

The methods described in this section apply to PostgreSQL 12 and later.

## Contents

1. *[Getting](#getting-an-execution-plan)*
2. *[Operations](#operations)*
3. *[Access vs. filter predicates](#distinguishing-access-and-filter-predicates)*


## Getting an Execution Plan

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/postgresql/getting-an-execution-plan</sub>

A PostgreSQL execution plan is fetched by putting the `explain` command in front of an SQL statement. There is, however, one important limitation: SQL statements with [bind parameters](where-clause-bind-parameters.md) (e.g., `$1`, `$2, etc.)` cannot be explained this way—they need to be prepared first:

```
PREPARE stmt(int) AS SELECT $1
```

Note that PostgreSQL uses “`$n`” for bind parameters. Your database abstraction layer might hide this so you can use question marks as defined by the SQL standard.

The execution of the prepared statement can be explained:

```
EXPLAIN EXECUTE stmt(1)
```

Since PostgreSQL 9.2 the creation of the execution plan is postponed until execution and thus considers the actual values for the bind parameters. To obtain an execution plan that does not consider the actual values of the bind parameters, PostgreSQL 16 introduced [the `generic_plan` option of `explain`](https://www.postgresql.org/docs/current/sql-explain.html#id-1.9.3.148.8).

> **Note:**
>
> Statements without bind parameters can be explained directly:
>
> ```
> EXPLAIN SELECT 1
> ```
>
> In this case, the optimizer has always considered the actual values during query planning.

The explain plan output is as follows:

```
                QUERY PLAN
------------------------------------------
 Result  (cost=0.00..0.01 rows=1 width=0)
```

The output has similar information as the Oracle execution plans shown throughout the book: the operation name (“Result”), the related cost, the row count estimate, and the expected row width.

Note that PostgreSQL shows two cost values. The first is the cost for the startup, the second is the total cost for the execution if all rows are retrieved. The Oracle database’s execution plan only shows the second value.

The PostgreSQL `explain` command has many options of which `analyze`, `buffers` and `settings` are the most helpful ones.

Enabling the `analyze` option means that the execution plan is not only created, but also executed. That implies that the side effects of running the statement being explained will take place. Such as deleting rows when explain a `delete` statement. You can enclose `explain` in a transaction and perform a rollback afterwards if you don’t want such side effects to persist.

> **Warning:**
>
> `explain analyze` executes the explained statement, even if the statement is an `insert`, `update` or `delete`.

Running the statement allows collecting run time benchmarks like the time and the actual number of rows produced by each operation. The option `buffers` also counts the number of database blocks being accessed.

Finally, the `settings` option also shows settings that differ from their default.

```
BEGIN
```

```
EXPLAIN (ANALYZE, BUFFERS, SETTINGS)
EXECUTE stmt(1)
```

```
                   QUERY PLAN
--------------------------------------------------
 Result  (cost=0.00..0.01 rows=1 width=4)
         (actual time=0.001..0.002 rows=1 loops=1)
 Settings: random_page_cost = '1.1'
 Planning Time: 0.032 ms
 Execution Time: 0.078 ms
```

```
ROLLBACK
```

Note that the plan is formatted for a better fit on the page. PostgreSQL prints the “actual” values on the same line as the estimated values.

The row count is the only value that is shown in both parts—in the estimated and in the actual figures. That allows you to quickly find erroneous cardinality estimates.

Last but not least, prepared statements must be closed again:

```
DEALLOCATE stmt
```

> **Tip:**
>
> [Bind-Variables](where-clause-bind-parameters.md)
>
> [Avoid Smart Logic for Conditional `WHERE` clauses](where-clause-obfuscation.md#smart-logic)
>
> Article “[Planning for Re-Use](https://use-the-index-luke.com/blog/2011-07-16/planning-for-reuse)“


## Operations

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/postgresql/operations</sub>

### Index and Table Access

Seq Scan
:   The `Seq Scan` operation scans the entire relation (table) as stored on disk (like `TABLE ACCESS FULL`).

Index Scan
:   The `Index Scan` performs a B-tree traversal, walks through the leaf nodes to find all matching entries, and fetches the corresponding table data. It is like an `INDEX RANGE SCAN` followed by a `TABLE ACCESS BY INDEX ROWID` operation. See also [Chapter 1, “*Anatomy of an SQL Index*”](anatomy.md).

    The so-called index filter predicates often cause performance problems for an `Index Scan`. The [next section](#distinguishing-access-and-filter-predicates) explains how to identify them.

Index Only Scan
:   The `Index Only Scan` performs a B-tree traversal and walks through the leaf nodes to find all matching entries. There is no table access needed because the index has all columns to satisfy the query (exception: MVCC visibility information). See also [“*Index-Only Scan: Avoiding Table Access*”](clustering.md#index-only-scan-avoiding-table-access).

Bitmap Index Scan / Bitmap Heap Scan / Recheck Cond
:   Tom Lane’s [post to the PostgreSQL performance mailing list](https://www.postgresql.org/message-id/12553.1135634231@sss.pgh.pa.us) is very clear and concise.

    > A plain `Index Scan` fetches one tuple-pointer at a time from the index, and immediately visits that tuple in the table. A bitmap scan fetches all the tuple-pointers from the index in one go, sorts them using an in-memory “bitmap” data structure, and then visits the table tuples in physical tuple-location order.
    >
    > — [Tom Lane](https://www.postgresql.org/message-id/12553.1135634231@sss.pgh.pa.us)

### Join Operations

Generally join operations process only two tables at a time. In case a query has more joins, they are executed sequentially: first two tables, then the intermediate result with the next table. In the context of joins, the term “table” could therefore also mean “intermediate result”.

Nested Loops
:   Joins two tables by fetching the result from one table and querying the other table for each row from the first. See also [“*Nested Loops*”](join.md#nested-loops).

Hash Join / Hash
:   The hash join loads the candidate records from one side of the join into a hash table (marked with `Hash` in the plan) which is then probed for each record from the other side of the join. See also [“*Hash Join*”](join.md#hash-join).

Merge Join
:   The (sort) merge join combines two sorted lists like a zipper. Both sides of the join must be presorted. See also [“*Sort Merge*”](join.md#sort-merge).

### Sorting and Grouping

Sort / Sort Key
:   Sorts the set on the columns mentioned in `Sort Key`. The `Sort` operation needs large amounts of memory to materialize the intermediate result (not pipelined). See also [“*Indexing Order By*”](sorting-grouping.md#indexing-order-by).

GroupAggregate
:   Aggregates a presorted set according to the `group by` clause. This operation does not buffer large amounts of data (pipelined). See also [“*Indexing Group By*”](sorting-grouping.md#indexing-group-by).

HashAggregate
:   Uses a temporary hash table to group records. The `HashAggregate` operation does not require a presorted data set, instead it uses large amounts of memory to materialize the intermediate result (not pipelined). The output is not ordered in any meaningful way. See also [“*Indexing Group By*”](sorting-grouping.md#indexing-group-by).

### Top-N Queries

Limit
:   Aborts the underlying operations when the desired number of rows has been fetched. See also [“*Querying Top-N Rows*”](partial-results.md#querying-top-n-rows).

    The efficiency of the top-N query depends on the execution mode of the underlying operations. It is very inefficient when aborting non-pipelined operations such as `Sort`.

WindowAgg
:   Indicates the use of window functions. From PostgreSQL 15 onwards “Run Condition” indicates a possible Top-N termination. See also [“*Using Window Functions for Efficient Pagination*”](partial-results.md#using-window-functions-for-efficient-pagination).


## Distinguishing Access and Filter-Predicates

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/postgresql/filter-predicates</sub>

The PostgreSQL database uses three different methods to apply `where` clauses (predicates):

Access Predicate (“Index Cond”)
:   The access predicates express the start and stop conditions of the [leaf node traversal](anatomy.md#the-index-leaf-nodes).

Index Filter Predicate (“Index Cond”)
:   Index filter predicates are applied during the leaf node traversal only. They do not contribute to the start and stop conditions and do not narrow the scanned range.

Table level filter predicate (“Filter”)
:   Predicates on columns that are not part of the index are evaluated on the table level. For that to happen, the database must load the row from the heap table first.

> **Note:**
>
> Index filter predicates give a false sense of safety; even though an index is used, the performance degrades rapidly on a growing data volume or system load.

PostgreSQL execution plans do not show index access and filter predicates separately—both show up as “Index Cond”. That means the execution plan must be compared to the index definition to differentiate access predicates from index filter predicates.

> **Note:**
>
> The PostgreSQL explain plan does not provide enough information for finding index filter predicates.

The predicates shown as “Filter” are always table level filter predicates—even when shown for an `Index Scan` operation.

Consider the following example, which originally appeared in the “[Performance and Scalability”](testing-scalability.md#performance-impacts-of-data-volume) chapter([`create` & `insert` script](example-schema-postgresql.md#postgresql-example-scripts-for-testing-and-scalability)):

```
CREATE TABLE scale_data (
   section NUMERIC NOT NULL,
   id1     NUMERIC NOT NULL,
   id2     NUMERIC NOT NULL
)
```

```
CREATE INDEX scale_data_key ON scale_data(section, id1)
```

The following `select` filters on the `ID2` column, which is not included in the index:

```
PREPARE stmt(int) AS SELECT count(*)
                       FROM scale_data
                      WHERE section = 1
                        AND id2 = $1
```

```
EXPLAIN EXECUTE stmt(1)
```

The `ID2` predicate shows up as “`Filter`” below the `Index Scan` operation. This is because PostgreSQL performs the table access as part of the `Index Scan` operation. In other words, the `TABLE ACCESS BY INDEX ROWID` operation of the Oracle database is hidden within PostgreSQL’s `Index Scan` operation. It is therefore possible that a `Index Scan` filters on columns that are not included in the index.

> **Important:**
>
> The PostgreSQL `Filter` predicates are table level filter predicates—even when shown for an `Index Scan`.

When we add the index from the “[Performance and Scalability”](testing-scalability.md#performance-impacts-of-data-volume) chapter, we can see that all columns show up as “Index Cond”—regardless of whether they are access or filter predicates.

```
CREATE INDEX scale_slow ON scale_data (section, id1, id2)
```

The execution plan with the new index does not show any filter conditions:

```
                      QUERY PLAN
------------------------------------------------------
Aggregate  (cost=14215.98..14215.99 rows=1 width=0)
  Output: count(*)
  -> Index Scan using scale_slow on scale_data
     (cost=0.00..14208.51 rows=2989 width=0)
     Index Cond: (section = 1::numeric AND id2 = ($1)::numeric)
```

Please note that the condition on `ID2` cannot narrow the leaf node traversal because the index has the `ID1` column before `ID2`. That means, the `Index Scan` will scan the entire range for the condition `SECTION=1::numeric` and apply the filter `ID2=($1)::numeric` on each row that fulfills the clause on `SECTION`.

> **Tip:**
>
> - The section [“*Greater, Less and `BETWEEN`*”](where-clause-searching-for-ranges.md#greater-less-and-between) explains the difference between access and index filter predicates by example.
> - [Chapter 3, “*Performance and Scalability*”](testing-scalability.md), demonstrates the performance difference access and index filter predicates make.
