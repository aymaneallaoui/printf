<!-- Source: https://use-the-index-luke.com/sql/where-clause/the-equals-operator — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# The Equality Operator

<sub>Source: https://use-the-index-luke.com/sql/where-clause/the-equals-operator</sub>

The equality operator is both the most trivial and the most frequently used SQL operator. Indexing mistakes that affect performance are still very common and `where` clauses that combine multiple conditions are particularly vulnerable.

This section shows how to verify index usage and explains how concatenated indexes can optimize combined conditions. To aid understanding, we will analyze a slow query to see the real world impact of the causes explained in [Chapter 1](anatomy.md).

## Contents

1. *[Primary Keys](#primary-keys)* — Verifying index usage
2. *[Concatenated Keys](#concatenated-indexes)* — Multi-column indexes
3. *[Slow Indexes, Part II](#slow-indexes-part-ii)* — The first ingredient, revisited


## Primary Keys

<sub>Source: https://use-the-index-luke.com/sql/where-clause/the-equals-operator/primary-keys</sub>

We start with the simplest yet most common `where` clause: the primary key lookup. For the examples throughout this chapter we use the `EMPLOYEES` table defined as follows:

```
CREATE TABLE employees (
   employee_id   NUMBER        NOT NULL,
   first_name    VARCHAR(1000) NOT NULL,
   last_name     VARCHAR(1000) NOT NULL,
   date_of_birth DATE          NOT NULL,
   phone_number  VARCHAR(1000) NOT NULL,
   CONSTRAINT employees_pk PRIMARY KEY (employee_id)
)
```

The database automatically creates an index for the primary key. That means there is an index on the `EMPLOYEE_ID` column, even though there is no `create index` statement.

> **Tip:**
>
> [Appendix C*Example Schema*](example-schema.md) contains scripts to populate the `EMPLOYEES` table with sample data. You can use it to test the examples in your own environment.
>
> To follow the text, it is enough to know that the table contains 1000 rows.

The following query uses the primary key to retrieve an employee’s name:

```
SELECT first_name, last_name
  FROM employees
 WHERE employee_id = 123
```

The `where` clause cannot match multiple rows because the primary key constraint ensures uniqueness of the `EMPLOYEE_ID` values. The database does not need to follow the index leaf nodes—it is enough to traverse the index tree. We can use the so-called *execution plan* for verification:

#### Db2 (LUW)

The following execution plan was gathered with the [`last_explained` view](explain-plan-db2.md#getting-an-execution-plan) available from the [appendix](explain-plan-db2.md#getting-an-execution-plan).

```
Explain Plan
-------------------------------------------------------
ID | Operation             |                Rows | Cost
 1 | RETURN                |                     |   13
 2 |  FETCH EMPLOYEES      |    1 of 1 (100.00%) |   13
 3 |   IXSCAN EMPLOYEES_PK | 1 of 1000 (   .10%) |    6

Predicate Information
 3 - START (Q1.EMPLOYEE_ID = +00123.)
      STOP (Q1.EMPLOYEE_ID = +00123.)
```

The Operation `IXSCAN` is similar to Oracle’s `INDEX [RANGE|UNIQUE] SCAN`. From this output, we cannot decided if it is a unique or range scan. The `FETCH` operation corresponds to Oracle’s `TABLE ACCESS BY INDEX ROWID`.

#### MySQL

```
+----+-----------+-------+---------+---------+------+-------+
| id | table     | type  | key     | key_len | rows | Extra |
+----+-----------+-------+---------+---------+------+-------+
|  1 | employees | const | PRIMARY | 5       |    1 |       |
+----+-----------+-------+---------+---------+------+-------+
```

Type `const` is MySQL’s equivalent of Oracle’s `INDEX UNIQUE SCAN`.

#### Oracle

```
---------------------------------------------------------------
|Id |Operation                   | Name         | Rows | Cost |
---------------------------------------------------------------
| 0 |SELECT STATEMENT            |              |    1 |    2 |
| 1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES    |    1 |    2 |
|*2 |  INDEX UNIQUE SCAN         | EMPLOYEES_PK |    1 |    1 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("EMPLOYEE_ID"=123)
```

#### PostgreSQL

```
                QUERY PLAN
-------------------------------------------
 Index Scan using employees_pk on employees
   (cost=0.00..8.27 rows=1 width=14)
   Index Cond: (employee_id = 123::numeric)
```

The PostgreSQL operation `Index Scan` combines the `INDEX [UNIQUE/RANGE] SCAN` and `TABLE ACCES BY INDEX ROWID` operations from the Oracle Database. It is not visible from the execution plan if the index access might potentially return more than one row.

#### SQL Server

```
|--Nested Loops(Inner Join)
   |--Index Seek(OBJECT:employees_pk,
   |               SEEK:employees.employee_id=@1
   |            ORDERED FORWARD)
   |--RID Lookup(OBJECT:employees,
                   SEEK:Bmk1000=Bmk1000
                 LOOKUP ORDERED FORWARD)
```

The SQL Server operation `INDEX SEEK` and `RID Lookup` correspond to Oracle’s `INDEX RANGE SCAN` and `TABLE ACCESS BY ROWID` respectively. Unlike the Oracle Database, SQL Server explicitly shows the `Nested Loops` join to combine the index and table data.

The Oracle execution plan shows an `INDEX UNIQUE SCAN`—the operation that only traverses the index tree. It fully utilizes the logarithmic scalability of the index to find the entry very quickly—almost independent of the table size.

> **Tip:**
>
> The *execution plan* (sometimes *explain plan* or *query plan*) shows the steps the database takes to execute an SQL statement. [Appendix A](explain-plan.md) explains how to retrieve and read execution plans with other databases.

After accessing the index, the database must do one more step to fetch the queried data (`FIRST_NAME`, `LAST_NAME`) from the table storage: the `TABLE ACCESS BY INDEX ROWID` operation. This operation can become a performance bottleneck—as explained in [“*Slow Indexes, Part I*”](anatomy.md#slow-indexes-part-i)—but there is no such risk in connection with an `INDEX UNIQUE SCAN`. This operation cannot deliver more than one entry so it cannot trigger more than one table access. That means that the ingredients of a slow query are not present with an `INDEX UNIQUE SCAN`.

> **Sidebar — Primary Keys without Unique Index**
>
> A primary key does not necessarily need a unique index—you can use a non-unique index as well. In that case the Oracle database does not use an `INDEX UNIQUE SCAN` but instead the `INDEX RANGE SCAN` operation. Nonetheless, the constraint still maintains the uniqueness of keys so that the index lookup delivers at most one entry.
>
> One of the reasons for using non-unique indexes for a primary keys are *deferrable constraints*. As opposed to regular constraints, which are validated during statement execution, the database postpones the validation of deferrable constraints until the transaction is committed. Deferred constraints are required for inserting data into tables with circular dependencies.


## Concatenated Indexes

<sub>Source: https://use-the-index-luke.com/sql/where-clause/the-equals-operator/concatenated-keys</sub>

Even though the database creates the index for the primary key automatically, there is still room for manual refinements if the key consists of multiple columns. In that case the database creates an index on all primary key columns—a so-called *concatenated* index (also known as *multi-column*, *composite* or *combined* index). Note that the column order of a concatenated index has great impact on its usability so it must be chosen carefully.

For the sake of demonstration, let’s assume there is a company merger. The employees of the other company are added to our `EMPLOYEES` table so it becomes ten times as large. There is only one problem: the `EMPLOYEE_ID` is not unique across both companies. We need to extend the primary key by an extra identifier—e.g., a subsidiary ID. Thus the new primary key has two columns: the `EMPLOYEE_ID` as before and the `SUBSIDIARY_ID` to reestablish uniqueness.

The index for the new primary key is therefore defined in the following way:

```
CREATE UNIQUE INDEX employees_pk
    ON employees (employee_id, subsidiary_id)
```

A query for a particular employee has to take the full primary key into account—that is, the `SUBSIDIARY_ID` column also has to be used:

```
SELECT first_name, last_name
  FROM employees
 WHERE employee_id   = 123
   AND subsidiary_id = 30
```

Whenever a query uses the complete primary key, the database can use an `INDEX UNIQUE SCAN`—no matter how many columns the index has. But what happens when using only one of the key columns, for example, when searching all employees of a subsidiary?

```
SELECT first_name, last_name
  FROM employees
 WHERE subsidiary_id = 20
```

The execution plan reveals that the database does not use the index. Instead it performs a `TABLE ACCESS FULL`. As a result the database reads the entire table and evaluates every row against the `where` clause. The execution time grows with the table size: if the table grows tenfold, the `TABLE ACCESS FULL` takes ten times as long. The danger of this operation is that it is often fast enough in a small development environment, but it causes serious performance problems in production.

> **Sidebar — Full Table Scan**
>
> The operation `TABLE ACCESS FULL`, also known as *full table scan*, can be the most efficient operation in some cases anyway, in particular when retrieving a large part of the table.
>
> This is partly due to the overhead for the index lookup itself, which does not happen for a `TABLE ACCESS FULL` operation. This is mostly because an index lookup reads one block after the other as the database does not know which block to read next until the current block has been processed. A `FULL TABLE SCAN` must get the entire table anyway so that the database can read larger chunks at a time (*multi block read*). Although the database reads more data, it might need to execute fewer read operations.

The database does not use the index because it cannot use single columns from a concatenated index arbitrarily. A closer look at the index structure makes this clear.

A concatenated index is just a B-tree index like any other that keeps the indexed data in a sorted list. The database considers each column according to its position in the index definition to sort the index entries. The first column is the primary sort criterion and the second column determines the order only if two entries have the same value in the first column and so on.

> **Important:**
>
> A concatenated index is *one index across multiple columns*.

The ordering of a two-column index is therefore like the ordering of a telephone directory: it is first sorted by surname, then by first name. That means that a two-column index does not support searching on the second column alone; that would be like searching a telephone directory by first name.

*[Figure 2.1 Concatenated Index — diagram, see https://use-the-index-luke.com/sql/where-clause/the-equals-operator/concatenated-keys]*

The index excerpt in Figure 2.1 shows that the entries for subsidiary 20 are not stored next to each other. It is also apparent that there are no entries with `SUBSIDIARY_ID = 20` in the tree, although they exist in the leaf nodes. The tree is therefore useless for this query.

> **Tip:**
>
> Visualizing an index helps in understanding what queries the index supports. You can query the database to retrieve the entries in index order (SQL:2008 syntax, see [syntax of top-n queries](partial-results.md#querying-top-n-rows) for proprietary solutions using `LIMIT`, `TOP` or `ROWNUM`):
>
> ```
> SELECT <INDEX COLUMN LIST>
>   FROM <TABLE>
>  ORDER BY <INDEX COLUMN LIST>
>  FETCH FIRST 100 ROWS ONLY
> ```
>
> If you put the index definition and table name into the query, you will get a sample from the index. Ask yourself if the requested rows are clustered in a central place. If not, the index tree cannot help find that place.

We could, of course, add another index on `SUBSIDIARY_ID` to improve query speed. There is however a better solution—at least if we assume that searching on `EMPLOYEE_ID` alone does not make sense.

We can take advantage of the fact that the first index column is always usable for searching. Again, it is like a telephone directory: you don’t need to know the first name to search by last name. The trick is to reverse the index column order so that the `SUBSIDIARY_ID` is in the first position:

```
CREATE UNIQUE INDEX EMPLOYEES_PK
    ON EMPLOYEES (SUBSIDIARY_ID, EMPLOYEE_ID)
```

Both columns together are still unique so queries with the full primary key can still use an `INDEX UNIQUE SCAN` but the sequence of index entries is entirely different. The `SUBSIDIARY_ID` has become the primary sort criterion. That means that all entries for a subsidiary are in the index consecutively so the database can use the B-tree to find their location.

> **Important:**
>
> The most important consideration when defining a concatenated index is how to choose the column order so it can be used as often as possible.

The execution plan confirms that the database uses the “reversed” index. The `SUBSIDIARY_ID` alone is not unique anymore so the database must follow the leaf nodes in order to find all matching entries: it is therefore using the `INDEX RANGE SCAN` operation.

#### Db2 (LUW)

```
Explain Plan
-------------------------------------------------------------
ID | Operation               |                    Rows | Cost
 1 | RETURN                  |                         |  128
 2 |  FETCH EMPLOYEES        |  1195 of 1195 (100.00%) |  128
 3 |   RIDSCN                |  1195 of 1195 (100.00%) |   43
 4 |    SORT (UNIQUE)        |  1195 of 1195 (100.00%) |   43
 5 |     IXSCAN EMPLOYEES_PK | 1195 of 10000 ( 11.95%) |   43

Predicate Information
 2 - SARG (Q1.SUBSIDIARY_ID = +00002.)
 5 - START (Q1.SUBSIDIARY_ID = +00002.)
      STOP (Q1.SUBSIDIARY_ID = +00002.)
```

This execution plan looks more complex than the execution plan that was using the index before. They key operations are still there, however: the `IXSCAN` representing the index range scan and the `FETCH` for the table access. In between these operations there is an unexpected `SORT` and `RIDSCN` operation: the `SORT` operation sorts the entries fetched from the index according to the rows physical storage location in the heap table. The `RIDSCAN` then prefetches all the affected database pages (collapsing multiple adjacent blocks into a single IO operation).

#### MySQL

```
+----+-----------+------+---------+---------+------+-------+
| id | table     | type | key     | key_len | rows | Extra |
+----+-----------+------+---------+---------+------+-------+
|  1 | employees | ref  | PRIMARY | 5       |  123 |       |
+----+-----------+------+---------+---------+------+-------+
```

The MySQL access type `ref` is the equivalent of `INDEX RANGE SCAN` in the Oracle database.

#### Oracle

```
---------------------------------------------------------------
|Id |Operation                   | Name         | Rows | Cost |
---------------------------------------------------------------
| 0 |SELECT STATEMENT            |              |  106 |   75 |
| 1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES    |  106 |   75 |
|*2 |  INDEX RANGE SCAN          | EMPLOYEES_PK |  106 |    2 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("SUBSIDIARY_ID"=20)
```

#### PostgreSQL

```
                 QUERY PLAN
----------------------------------------------
 Bitmap Heap Scan on employees
 (cost=24.63..1529.17 rows=1080 width=13)
   Recheck Cond: (subsidiary_id = 2::numeric)
   -> Bitmap Index Scan on employees_pk
      (cost=0.00..24.36 rows=1080 width=0)
      Index Cond: (subsidiary_id = 2::numeric)
```

The PostgreSQL database uses two operations in this case: a `Bitmap Index Scan` followed by a `Bitmap Heap Scan`. They roughly correspond to Oracle’s `INDEX RANGE SCAN` and `TABLE ACCESS BY INDEX ROWID` with one important difference: it first fetches all results from the index (`Bitmap Index Scan`), then sorts the rows according to the physical storage location of the rows in the heap table and than fetches all rows from the table (`Bitmap Heap Scan`). This method reduces the number of random access IOs on the table.

#### SQL Server

```
|--Nested Loops(Inner Join)
   |--Index Seek(OBJECT:employees_pk,
   |               SEEK:subsidiary_id=20
   |            ORDERED FORWARD)
   |--RID Lookup(OBJECT:employees,
                   SEEK:Bmk1000=Bmk1000
                 LOOKUP ORDERED FORWARD)
```

In general, a database can use a concatenated index when searching with the leading (leftmost) columns. An index with three columns can be used when searching for the first column, when searching with the first two columns together, and when searching using all columns.

Even though the two-index solution delivers very good `select` performance as well, the single-index solution is preferable. It not only saves storage space, but also the maintenance overhead for the second index. The fewer indexes a table has, the better the `insert`, `delete` and `update` performance.

To define an optimal index you must understand more than just how indexes work—you must also know how the application queries the data. This means you have to know the column combinations that appear in the `where` clause.

Defining an optimal index is therefore very difficult for external consultants because they don’t have an overview of the application’s access paths. Consultants can usually consider one query only. They do not exploit the extra benefit the index could bring for other queries. Database administrators are in a similar position as they might know the database schema but do not have deep insight into the access paths.

The only place where the technical database knowledge meets the functional knowledge of the business domain is the development department. Developers have a feeling for the data and know the access path. They can properly index to get the best benefit for the overall application without much effort.


## Slow Indexes, Part II

<sub>Source: https://use-the-index-luke.com/sql/where-clause/the-equals-operator/slow-indexes-part-ii</sub>

The [previous section](#concatenated-indexes) explained how to gain additional benefits from an existing index by changing its column order, but the example considered only two SQL statements. Changing an index, however, may affect all queries on the indexed table. This section explains the way databases pick an index and demonstrates the possible side effects when changing existing indexes.

The adopted `EMPLOYEES_PK` index improves the performance of all queries that search by subsidiary only. It is however usable for all queries that search by `SUBSIDIARY_ID`—regardless of whether there are any additional search criteria. That means the index becomes usable for queries that used to use another index with another part of the `where` clause. In that case, if there are multiple access paths available it is the optimizer’s job to choose the best one.

> **Sidebar — The Query Optimizer**
>
> The query optimizer, or query planner, is the database component that transforms an SQL statement into an execution plan. This process is also called *compiling* or *parsing*. There are two distinct optimizer types.
>
> *Cost-based optimizers* (CBO) generate many execution plan variations and calculate a *cost* value for each plan. The cost calculation is based on the operations in use and the estimated row numbers. In the end the cost value serves as the benchmark for picking the “best” execution plan.
>
> *Rule-based optimizers* (RBO) generate the execution plan using a hard-coded rule set. Rule based optimizers are less flexible and are seldom used today.

Changing an index might have unpleasant side effects as well. In our example, it is the internal telephone directory application that has become very slow since the merger. The first analysis identified the following query as the cause for the slowdown:

```
SELECT first_name, last_name, subsidiary_id, phone_number
  FROM employees
 WHERE last_name  = 'WINAND'
   AND subsidiary_id = 30
```

The execution plan is:

### Example 2.1 Execution Plan with Revised Primary Key Index

```
---------------------------------------------------------------
|Id |Operation                   | Name         | Rows | Cost |
---------------------------------------------------------------
| 0 |SELECT STATEMENT            |              |    1 |   30 |
|*1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES    |    1 |   30 |
|*2 |  INDEX RANGE SCAN          | EMPLOYEES_PK |   40 |    2 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
  1 - filter("LAST_NAME"='WINAND')
  2 - access("SUBSIDIARY_ID"=30)
```

The execution plan uses an index and has an overall cost value of 30. So far, so good. It is however suspicious that it uses the index we just changed—that is enough reason to suspect that our index change caused the performance problem, especially when bearing the old index definition in mind—it started with the `EMPLOYEE_ID` column which is not part of the `where` clause at all. The query could not use that index before.

For further analysis, it would be nice to compare the execution plan before and after the change. To get the original execution plan, we could just deploy the old index definition again, however most databases offer a simpler method to prevent using an index for a specific query. The following example uses an Oracle *optimizer hint* for that purpose.

```
SELECT /*+ NO_INDEX(EMPLOYEES EMPLOYEES_PK) */
       first_name, last_name, subsidiary_id, phone_number
  FROM employees
 WHERE last_name  = 'WINAND'
   AND subsidiary_id = 30
```

The execution plan that was presumably used before the index change did not use an index at all:

```
----------------------------------------------------
| Id | Operation         | Name      | Rows | Cost |
----------------------------------------------------
|  0 | SELECT STATEMENT  |           |    1 |  477 |
|* 1 |  TABLE ACCESS FULL| EMPLOYEES |    1 |  477 |
----------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   1 - filter("LAST_NAME"='WINAND' AND "SUBSIDIARY_ID"=30)
```

Even though the `TABLE ACCESS FULL` must read and process the entire table, it seems to be faster than using the index in this case. That is particularly unusual because the query matches one row only. Using an index to find a single row should be much faster than a full table scan, but in this case it is not. The index seems to be slow.

In such cases it is best to go through each step of the troublesome execution plan. The first step is the `INDEX RANGE SCAN` on the `EMPLOYEES_PK` index. That index does not cover the `LAST_NAME` column—the `INDEX RANGE SCAN` can consider the `SUBSIDIARY_ID` filter only; the Oracle database shows this in the “Predicate Information” area—entry “2” of the execution plan. There you can see the conditions that are applied for each operation.

> **Tip:**
>
> [Appendix A, “*Execution Plans*”](explain-plan.md), explains how to find the “Predicate Information” for other databases.

The `INDEX RANGE SCAN` with operation ID 2 (Example 2.1) applies only the `SUBSIDIARY_ID=30` filter. That means that it traverses the index tree to find the first entry for `SUBSIDIARY_ID` 30. Next it follows the leaf node chain to find all other entries for that subsidiary. The result of the `INDEX RANGE SCAN` is a list of `ROWIDs` that fulfill the `SUBSIDIARY_ID` condition: depending on the subsidiary size, there might be just a few ones or there could be many hundreds.

The next step is the `TABLE ACCESS BY INDEX ROWID` operation. It uses the `ROWIDs` from the previous step to fetch the rows—all columns—from the table. Once the `LAST_NAME` column is available, the database can evaluate the remaining part of the `where` clause. That means the database has to fetch all rows for `SUBSIDIARY_ID=30` before it can apply the `LAST_NAME` filter.

The statement’s response time does not depend on the result set size but on the number of employees in the particular subsidiary. If the subsidiary has just a few members, the `INDEX RANGE SCAN` provides better performance. Nonetheless a `TABLE ACCESS FULL` can be faster for a huge subsidiary because it can read large parts from the table in one shot (see [“*Full Table Scan*”](#concatenated-indexes)).

The query is slow because the index lookup returns many `ROWIDs`—one for each employee of the original company—and the database must fetch them individually. It is the perfect combination of the two ingredients that make an index slow: the database reads a wide index range and has to fetch many rows individually.

Choosing the best execution plan depends on the table’s data distribution as well so the optimizer uses statistics about the contents of the database. In our example, a histogram containing the distribution of employees over subsidiaries is used. This allows the optimizer to estimate the number of rows returned from the index lookup—the result is used for the cost calculation.

> **Sidebar — Statistics**
>
> A cost-based optimizer uses statistics about tables, columns, and indexes. Most statistics are collected on the column level: the number of distinct values, the smallest and largest values (data range), the number of `NULL` occurrences and the column histogram (data distribution). The most important statistical value for a table is its size (in rows and blocks).
>
> The most important index statistics are the tree depth, the number of leaf nodes, the number of distinct keys and the clustering factor (see [Chapter 5, “*Clustering Data: The Second Power of Indexing*”](clustering.md)).
>
> The optimizer uses these values to estimate the selectivity of the `where` clause predicates.

If there are no statistics available—for example because they were deleted—the optimizer uses default values. The default statistics of the Oracle database suggest a small index with medium selectivity. They lead to the estimate that the `INDEX RANGE SCAN` will return 40 rows. The execution plan shows this estimation in the Rows column (again, see Example 2.1). Obviously this is a gross underestimate, as there are 1000 employees working for this subsidiary.

If we provide correct statistics, the optimizer does a better job. The following execution plan shows the new estimation: 1000 rows for the `INDEX RANGE SCAN`. Consequently it calculated a higher cost value for the subsequent table access.

```
---------------------------------------------------------------
|Id |Operation                   | Name         | Rows | Cost |
---------------------------------------------------------------
| 0 |SELECT STATEMENT            |              |    1 |  680 |
|*1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES    |    1 |  680 |
|*2 |  INDEX RANGE SCAN          | EMPLOYEES_PK | 1000 |    4 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
  1 - filter("LAST_NAME"='WINAND')
  2 - access("SUBSIDIARY_ID"=30)
```

The cost value of 680 is even higher than the cost value for the execution plan using the `FULL TABLE SCAN` (477). The optimizer will therefore automatically prefer the `FULL TABLE SCAN`.

This example of a slow index should not hide the fact that proper indexing is the best solution. Of course searching on last name is best supported by an index on `LAST_NAME`:

```
CREATE INDEX emp_name ON employees (last_name)
```

Using the new index, the optimizer calculates a cost value of 3:

### Example 2.2 Execution Plan with Dedicated Index

```
--------------------------------------------------------------
| Id | Operation                   | Name      | Rows | Cost |
--------------------------------------------------------------
|  0 | SELECT STATEMENT            |           |    1 |    3 |
|* 1 |  TABLE ACCESS BY INDEX ROWID| EMPLOYEES |    1 |    3 |
|* 2 |   INDEX RANGE SCAN          | EMP_NAME  |    1 |    1 |
--------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   1 - filter("SUBSIDIARY_ID"=30)
   2 - access("LAST_NAME"='WINAND')
```

The index access delivers—according to the optimizer’s estimation—one row only. The database thus has to fetch only that row from the table: this is definitely faster than a `FULL TABLE SCAN`. A properly defined index is still better than the original full table scan.

The two execution plans from Example 2.1 and Example 2.2 are almost identical. The database performs the same operations and the optimizer calculated similar cost values, nevertheless the second plan performs much better. The efficiency of an `INDEX RANGE SCAN` may vary over a wide range—especially when followed by a table access. Using an index does not automatically mean a statement is executed in the best way possible.
