<!-- Source: https://use-the-index-luke.com/sql/explain-plan/oracle — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Oracle Database

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/oracle</sub>

Most development environments (IDEs) can very easily show an execution plan but use very different ways to format them on the screen and are sometimes incomplete or even wrong. The method described in this section delivers an correct execution plan in the format used in the book.

## Contents

1. *[Getting](#getting-an-execution-plan)*
2. *[Operations](#operations)*
3. *[Access vs. filter predicates](#distinguishing-access-and-filter-predicates)*


## Getting an Execution Plan

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/oracle/getting-an-execution-plan</sub>

Viewing an execution plan, including actual measurements, in the Oracle database involves three steps:

1. Activation of the measurements (optional)
2. Executing the SQL statement
3. Fetching the execution plan

### Activation of the Measurements

To get all run-time statistics, such as the time for each operation, the collecting of these values must be activated first. This can be done in the respective statement by adding the hint `/*+ GATHER_PLAN_STATISTICS */` or once in the session so that it affects all following executions.

```
alter session set statistics_level = 'ALL'
```

### Executing the SQL Statement

Running the statement causes the execution plan to be cached (SQL area). If you activated the collection of run-time statistics, they will be added there as well.

```
select * from dual
```

### Fetching the Execution Plan

The package `DBMS_XPLAN` can show execution plans from the SQL area. The following example shows how to display the last execution plan that was executed in the current database session:

```
select * from table(dbms_xplan.display_cursor(null, null,
                                  'LAST ALLSTATS +COST'))
```

The query will display the execution plan as shown in the book:

```
---------------------------------------------------------------
| Operation         | Name | E-Rows | Cost | A-Rows | A-Time |.
---------------------------------------------------------------
| SELECT STATEMENT  |      |        |    2 |      1 |  00.01 |.
|  TABLE ACCESS FULL| DUAL |      1 |    2 |      1 |  00.01 |.
---------------------------------------------------------------
```

The execution plans shown here were edited for brevity.


## Operations

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/oracle/operations</sub>

My personal most favorite resource for execution plan operations is [Julian Dyke’s listing](http://www.juliandyke.com/Optimisation/Operations/Operations.php)—however, that is from a different point of view.

### Index and Table Access

INDEX UNIQUE SCAN
:   The `INDEX UNIQUE SCAN` performs the B-tree traversal only. The database uses this operation if a unique constraint ensures that the search criteria will match no more than one entry. See also [Chapter 1, “*Anatomy of an SQL Index*”](anatomy.md).

INDEX RANGE SCAN
:   The `INDEX RANGE SCAN` performs the B-tree traversal *and* follows the leaf node chain to find all matching entries. See also [Chapter 1, “*Anatomy of an SQL Index*”](anatomy.md).

    The so-called index filter predicates often cause performance problems for an `INDEX RANGE SCAN`. The [next section](#distinguishing-access-and-filter-predicates) explains how to identify them.

INDEX FULL SCAN
:   Reads the entire index—all rows—in index order. Depending on various system statistics, the database might perform this operation if it needs all rows in index order—e.g., because of a corresponding `order by` clause. Instead, the optimizer might also use an `INDEX FAST FULL SCAN` and perform an additional sort operation. See [Chapter 6, “*Sorting and Grouping*”](sorting-grouping.md).

INDEX FAST FULL SCAN
:   Reads the entire index—all rows—as stored on the disk. This operation is typically performed instead of a full table scan if all required columns are available in the index. Similar to `TABLE ACCESS FULL`, the `INDEX FAST FULL SCAN` can benefit from multi-block read operations. See [Chapter 5, “*Clustering Data: The Second Power of Indexing*”](clustering.md).

TABLE ACCESS BY INDEX ROWID
:   Retrieves a row from the table using the `ROWID` retrieved from the preceding index lookup. See also [Chapter 1, “*Anatomy of an SQL Index*”](anatomy.md).

TABLE ACCESS FULL
:   This is also known as full table scan. Reads the entire table—all rows and columns—as stored on the disk. Although multi-block read operations improve the speed of a full table scan considerably, it is still one of the most expensive operations. Besides high IO rates, a full table scan must inspect all table rows so it can also consume a considerable amount of CPU time. See also [“*Full Table Scan*”](where-clause-the-equals-operator.md#concatenated-indexes).

### Joins

Generally join operations process only two tables at a time. In case a query has more joins, they are executed sequentially: first two tables, then the intermediate result with the next table. In the context of joins, the term “table” could therefore also mean “intermediate result”.

NESTED LOOPS JOIN
:   Joins two tables by fetching the result from one table and querying the other table for each row from the first. See also [“*Nested Loops*”](join.md#nested-loops).

HASH JOIN
:   The hash join loads the candidate records from one side of the join into a hash table that is then probed for each row from the other side of the join. See also [“*Hash Join*”](join.md#hash-join).

MERGE JOIN
:   The merge join combines two sorted lists like a zipper. Both sides of the join must be presorted. See also [“*Sort Merge*”](join.md#sort-merge).

### Sorting and Grouping

SORT ORDER BY
:   Sorts the result according to the `order by` clause. This operation needs large amounts of memory to materialize the intermediate result (not pipelined). See also [“*Indexing Order By*”](sorting-grouping.md#indexing-order-by).

SORT ORDER BY STOPKEY
:   Sorts a subset of the result according to the `order by` clause. Used for top-N queries if pipelined execution is not possible. See also [“*Querying Top-N Rows*”](partial-results.md#querying-top-n-rows).

SORT GROUP BY
:   Sorts the result set on the `group by` columns and aggregates the sorted result in a second step. This operation needs large amounts of memory to materialize the intermediate result set (not pipelined). See also [“*Indexing Group By*”](sorting-grouping.md#indexing-group-by).

SORT GROUP BY NOSORT
:   Aggregates a presorted set according the `group by` clause. This operation does not buffer the intermediate result: it is executed in a pipelined manner. See also [“*Indexing Group By*”](sorting-grouping.md#indexing-group-by).

HASH GROUP BY
:   Groups the result using a hash table. This operation needs large amounts of memory to materialize the intermediate result set (not pipelined). The output is not ordered in any meaningful way. See also [“*Indexing Group By*”](sorting-grouping.md#indexing-group-by).

### Top-N Queries

The efficiency of top-N queries depends on the execution mode of the underlying operations. They are very inefficient when aborting non-pipelined operations such as `SORT ORDER BY`.

COUNT STOPKEY
:   Aborts the underlying operations when the desired number of rows was fetched. See also [*Querying Top-N Rows*](partial-results.md#querying-top-n-rows).

WINDOW NOSORT STOPKEY
:   Uses a window function (`over` clause) to abort the execution when the desired number of rows was fetched. See also [“*Using Window Functions for Efficient Pagination*”](partial-results.md#using-window-functions-for-efficient-pagination).


## Distinguishing Access and Filter-Predicates

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/oracle/filter-predicates</sub>

The Oracle database uses three different methods to apply `where` clauses (predicates):

Access predicate (“access”)
:   The access predicates express the start and stop conditions of the [leaf node traversal](anatomy.md#the-index-leaf-nodes).

Index filter predicate (“filter” for index operations)
:   Index filter predicates are applied during the leaf node traversal only. They do not contribute to the start and stop conditions and do not narrow the scanned range.

Table level filter predicate (“filter” for table operations)
:   Predicates on columns that are not part of the index are evaluated on table level. For that to happen, the database must load the row from the table first.

> **Note:**
>
> Index filter predicates give a false sense of safety; even though an index is used, the performance degrades rapidly on a growing data volume or system load.

Execution plans that were created using the `DBMS_XPLAN` utility (see [“*Getting an Execution Plan*”](#getting-an-execution-plan)), show the index usage in the “Predicate Information” section below the tabular execution plan:

```
------------------------------------------------------
| Id | Operation         | Name       | Rows  | Cost |
------------------------------------------------------
|  0 | SELECT STATEMENT  |            |     1 | 1445 |
|  1 |  SORT AGGREGATE   |            |     1 |      |
|* 2 |   INDEX RANGE SCAN| SCALE_SLOW |  4485 | 1445 |
------------------------------------------------------

Predicate Information (identified by operation id):
   2 - access("SECTION"=:A AND "ID2"=:B)
       filter("ID2"=:B)
```

The numbering of the predicate information refers to the “Id” column of the execution plan. There, the database also shows an asterisk to mark operations that have predicate information.

This example, taken from the chapter “[Performance and Scalability](testing-scalability.md#performance-impacts-of-data-volume)”, shows an `INDEX RANGE SCAN` that has access and filter predicates. The Oracle database has the peculiarity of also showing some filter predicate as access predicates—e.g., `ID2=:B` in the execution plan above.

> **Important:**
>
> If a condition shows up as filter predicate, it is a filter predicate—it does not matter if it is also shown as access predicate.

This means that the `INDEX RANGE SCAN` scans the entire range for the condition `"SECTION"=:A` and applies the filter `"ID2"=:B` on each row.

Filter predicates on table level are shown for the respective table access such as `TABLE ACCESS BY INDEX ROWID` or `TABLE ACCESS FULL`.

Please note that different tools display the predicate information differently. Oracle SQL Developer, for example, shows the predicate information below the respective operation.

*[Figure A.1 Access and Filter Predicates in Oracle SQL Developer — image: https://use-the-index-luke.com/static/sqldeveloper_access_filter_predicates.6GQmGBUE.png]*

Some tools don’t show the predicate information at all. Remember that you can always fall back to `DBMS_XPLAN` as explained in “[Getting an Execution Plan](#getting-an-execution-plan)”.

> **Tip:**
>
> - The section [“*Greater, Less and `BETWEEN`*”](where-clause-searching-for-ranges.md#greater-less-and-between) explains the difference between access and index filter predicates by example.
> - [Chapter 3, “*Performance and Scalability*”](testing-scalability.md), demonstrates the performance difference access and index filter predicates make.
