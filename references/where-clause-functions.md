<!-- Source: https://use-the-index-luke.com/sql/where-clause/functions — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Functions

<sub>Source: https://use-the-index-luke.com/sql/where-clause/functions</sub>

The index on `LAST_NAME` has improved the performance considerably, but it requires you to search using the same case (upper/lower) as is stored in the database. This section explains how to lift this restriction without a decrease in performance.

### Db2 (LUW)

Db2 supports function based indexes on [zOS](https://www.ibm.com/docs/en/db2-for-zos/13.0.0?topic=statements-create-index) for a while, but only [since version 10.5 on LUW](https://www.ibm.com/docs/en/db2/11.5.x?topic=statements-create-index#sdx-synid_key-expression). The use of user-defined functions in indexes is not allowed.

The backup solution is to create a real column in the table that holds the result of the function or expression. The column must be maintained by a trigger or by the application layer—whatever is more appropriate. The new column can be indexed. The `where` clause must use the new column (without the expression).

### MySQL

MySQL is case-insensitive by default, but that can be [controlled on column level](https://dev.mysql.com/doc/refman/8.0/en/case-sensitivity.html). Starting with version 5.7 MySQL can create indexes on [generated columns](https://dev.mysql.com/doc/refman/8.0/en/generated-column-index-optimizations.html).

The backup solution for older versions is to create a real column in the table that holds the result of the function or expression. The column must be maintained by a trigger or by the application layer—whatever is more appropriate. The new column can be indexed. The `where` clause must use the new column (without the expression).

### Oracle

The Oracle database supports function-based indexes since release 8*i*. Virtual columns were additionally added with 11*g*.

### PostgreSQL

PostgreSQL fully supports [Indexes on Expressions](https://www.postgresql.org/docs/current/indexes-expressional.html) since release 7.4 (partially supported since 7.2)

### SQL Server

SQL Server supports [Computed Columns](https://learn.microsoft.com/en-us/sql/relational-databases/tables/specify-computed-columns-in-a-table?view=sql-server-ver16) that can be indexed since release 2000.

## Contents

1. *[Case-Insensitive Search](#case-insensitive-search-using-upper-or-lower)* — `UPPER` and `LOWER`
2. *[User-Defined Functions](#user-defined-functions)* — Limitations of function-based indexes
3. *[Over-Indexing](#over-indexing)* — Avoid redundancy


## Case-Insensitive Search Using UPPER or LOWER

<sub>Source: https://use-the-index-luke.com/sql/where-clause/functions/case-insensitive-search</sub>

Ignoring the case in a `where` clause is very simple. You can, for example, convert both sides of the comparison to all caps notation:

```
SELECT first_name, last_name, phone_number
  FROM employees
 WHERE UPPER(last_name) = UPPER('winand')
```

Regardless of the capitalization used for the search term or the `LAST_NAME` column, the `UPPER` function makes them match as desired.

> **Note:**
>
> Another way for case-insensitive matching is to use a different “collation”. The default collations used by SQL Server and MySQL do not distinguish between upper and lower case letters—they are case-insensitive by default.

The logic of this query is perfectly reasonable but the execution plan is not:

#### Db2 (LUW)

```
Explain Plan
------------------------------------------------------
ID | Operation         |                   Rows | Cost
 1 | RETURN            |                        |  690
 2 |  TBSCAN EMPLOYEES | 400 of 10000 (  4.00%) |  690

Predicate Information
 2 - SARG ( UPPER(Q1.LAST_NAME) = 'WINAND')
```

#### Oracle

```
----------------------------------------------------
| Id | Operation         | Name      | Rows | Cost |
----------------------------------------------------
|  0 | SELECT STATEMENT  |           |   10 |  477 |
|* 1 |  TABLE ACCESS FULL| EMPLOYEES |   10 |  477 |
----------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   1 - filter(UPPER("LAST_NAME")='WINAND')
```

#### PostgreSQL

```
                     QUERY PLAN
------------------------------------------------------
 Seq Scan on employees
   (cost=0.00..1722.00 rows=50 width=17)
   Filter: (upper((last_name)::text) = 'WINAND'::text)
```

It is a return of our old friend the full table scan. Although there is an index on `LAST_NAME`, it is unusable—because the search is *not* on `LAST_NAME` but on `UPPER(LAST_NAME)`. From the database’s perspective, that’s something *entirely different*.

This is a trap we all might fall into. We recognize the relation between `LAST_NAME` and `UPPER(LAST_NAME)` instantly and expect the database to “see” it as well. In reality the optimizer’s view is more like this:

```
SELECT first_name, last_name, phone_number
  FROM employees
 WHERE BLACKBOX(...) = 'WINAND'
```

The `UPPER` function is just a black box. The parameters to the function are not relevant because there is no general relationship between the function’s parameters and the result.

> **Tip:**
>
> Replace the function name with `BLACKBOX` to understand the optimizer’s point of view.

> **Sidebar — Compile Time Evaluation**
>
> The optimizer can evaluate the expression on the right-hand side during “compile time” because it has all the input parameters. The Oracle execution plan (“Predicate Information” section) therefore only shows the upper case notation of the search term. This behavior is very similar to a compiler that evaluates constant expressions at compile time.

To support that query, we need an index that covers the actual search term. That means we do not need an index on `LAST_NAME` but on `UPPER(LAST_NAME)`:

```
CREATE INDEX emp_up_name
    ON employees (UPPER(last_name))
```

An index whose definition contains functions or expressions is a so-called *function-based index (FBI)*. Instead of copying the column data directly into the index, a function-based index applies the function first and puts the result into the index. As a result, the index stores the names in all caps notation.

The database can use a function-based index if the *exact* expression of the index definition appears in an SQL statement—like in the example above. The execution plan confirms this:

#### Db2 (LUW)

```
Explain Plan
-------------------------------------------------------
ID | Operation            |                 Rows | Cost
 1 | RETURN               |                      |   13
 2 |  FETCH EMPLOYEES     |     1 of 1 (100.00%) |   13
 3 |   IXSCAN EMP_UP_NAME | 1 of 10000 (   .01%) |    6

Predicate Information
 3 - START ( UPPER(Q1.LAST_NAME) = 'WINAND')
      STOP ( UPPER(Q1.LAST_NAME) = 'WINAND')
```

The query was changed to `WHERE UPPER(last_name) = 'WINAND'` (no `UPPER` on the right hand side) to get the expected result. When using `UPPER('winand')`, the optimizer does a gross misestimation and expects 4% of the table rows to be selected. This causes the optimizer to ignore the index and do a `TBSCAN`. See [*Full Table Scan*](where-clause-the-equals-operator.md#concatenated-indexes) to see why that might make sense.

#### Oracle

```
--------------------------------------------------------------
|Id |Operation                   | Name        | Rows | Cost |
--------------------------------------------------------------
| 0 |SELECT STATEMENT            |             |  100 |   41 |
| 1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES   |  100 |   41 |
|*2 |  INDEX RANGE SCAN          | EMP_UP_NAME |   40 |    1 |
--------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
  2 - access(UPPER("LAST_NAME")='WINAND')
```

#### PostgreSQL

```
                       QUERY PLAN
------------------------------------------------------------
Bitmap Heap Scan on employees
  (cost=4.65..178.65 rows=50 width=17)
  Recheck Cond: (upper((last_name)::text) = 'WINAND'::text)
  -> Bitmap Index Scan on emp_up_name
     (cost=0.00..4.64 rows=50 width=0)
     Index Cond: (upper((last_name)::text) = 'WINAND'::text)
```

It is a regular `INDEX RANGE SCAN` as described in [Chapter 1](anatomy.md). The database traverses the B-tree and follows the leaf node chain. There are no dedicated operations or keywords for function-based indexes.

> **Warning:**
>
> Sometimes ORM tools use `UPPER` and `LOWER` without the developer’s knowledge. Hibernate, for example, [injects an implicit `LOWER`](myth-directory.md#dynamic-sql-is-slow) for case-insensitive searches.

The execution plan is not yet the same as it was in the previous section without `UPPER`; the row count estimate is too high. It is particularly strange that the optimizer expects to fetch more rows from the table than the `INDEX RANGE SCAN` delivers in the first place. How can it fetch 100 rows from the table if the preceding index scan returned only 40 rows? The answer is that it can not. Contradicting estimates like this often indicate problems with the statistics. In this particular case it is because the Oracle database does not update the table statistics when creating a new index (see also “*Oracle Statistics for Function-Based Indexes*”).

> **Sidebar — Oracle Statistics for Function-Based Indexes**
>
> The Oracle database maintains the information about the number of distinct column values as part of the table statistics. These figures are reused if a column is part of multiple indexes.
>
> Statistics for a function-based index (FBI) are also kept on table level as *virtual columns*. Although the Oracle database collects the *index statistics* for new indexes automatically ([since release 10*g*](https://docs.oracle.com/cd/B14117_01/server.101/b10763/compat.htm#sthref320)), it does not update the *table statistics*. For this reason, the Oracle documentation recommends updating the table statistics after creating a function-based index:
>
> > After creating a function-based index, collect statistics on both the index and its base table using the `DBMS_STATS` package. Such statistics will enable Oracle Database to correctly decide when to use the index.
> >
> > — [Oracle Database SQL Language Reference](https://docs.oracle.com/en/database/oracle/oracle-database/19/sqlrf/CREATE-INDEX.html#GUID-1F89BBC0-825F-4215-AF71-7588E31D8BFE__I2100962)
>
> My personal recommendation goes even further: after every index change, update the statistics for the base table and all its indexes. That might, however, also lead to unwanted side effects. Coordinate this activity with the database administrators (DBAs) and make a backup of the original statistics.

After updating the statistics, the optimizer calculates more accurate estimates:

#### Oracle

```
--------------------------------------------------------------
|Id |Operation                   | Name        | Rows | Cost |
--------------------------------------------------------------
| 0 |SELECT STATEMENT            |             |    1 |    3 |
| 1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES   |    1 |    3 |
|*2 |  INDEX RANGE SCAN          | EMP_UP_NAME |    1 |    1 |
--------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
  2 - access(UPPER("LAST_NAME")='WINAND')
```

#### PostgreSQL

```
                      QUERY PLAN
----------------------------------------------------------
 Index Scan using emp_up_name on employees
   (cost=0.00..8.28 rows=1 width=17)
   Index Cond: (upper((last_name)::text) = 'WINAND'::text)
```

As the row count estimate has decreased—from 50 in the example above down to 1 in this execution plan—the query planner prefers to use the simpler `Index Scan` operation.

> **Note:**
>
> [The so-called “extended statistics” on expressions and column groups](https://docs.oracle.com/en/database/oracle/oracle-database/19/tgsql/managing-extended-statistics.html#GUID-BD0F0B71-DD8B-44A0-888E-495830FC09A4) were introduced with Oracle release 11*g*.

Although the updated statistics do not improve execution performance in this case—the index was properly used anyway—it is always a good idea to check the optimizer’s estimates. The number of rows processed for each operation (cardinality estimate) is a particularly important figure that is also shown in SQL Server and PostgreSQL execution plans.

> **Tip:**
>
> [Appendix A, “*Execution Plans*”](explain-plan.md), describes the row count estimates in the execution plans of other databases.

SQL Server and MySQL do not support function-based indexes as described but both offer a workaround via computed or generated columns. To make use of this, you have to first add a generated column to the table that can be indexed afterwards:

#### MySQL

Since MySQL 5.7 you [can index a generated columns](https://dev.mysql.com/doc/refman/8.0/en/create-table.html#create-table-secondary-indexes-virtual-columns) as follows:

```
ALTER TABLE employees
  ADD COLUMN last_name_up VARCHAR(255) AS (UPPER(last_name));
```

```
CREATE INDEX emp_up_name ON employees (last_name_up);
```

#### SQL Server

```
ALTER TABLE employees ADD last_name_up AS UPPER(last_name)
```

```
CREATE INDEX emp_up_name ON employees (last_name_up)
```

SQL Server and MySQL are able to use this index whenever the indexed expression appears in the statement. In some simple cases, SQL Server and [MySQL](https://dev.mysql.com/doc/refman/8.0/en/generated-column-index-optimizations.html) can use this index even if the query remains unchanged. Sometimes, however, the query must be changed to refer to the name of the new columns in order to use the index. Always check the execution plan in case of doubt.


## User-Defined Functions

<sub>Source: https://use-the-index-luke.com/sql/where-clause/functions/user-defined-functions</sub>

Function-based indexing is a very generic approach. Besides functions like `UPPER` you can also index expressions like `A + B` and even use user-defined functions in the index definition.

There is one important exception. It is, for example, not possible to refer to the current time in an index definition, neither directly nor indirectly, as in the following example.

```
CREATE FUNCTION get_age(date_of_birth DATE)
RETURN NUMBER
AS
BEGIN
  RETURN
    TRUNC(MONTHS_BETWEEN(SYSDATE, date_of_birth)/12);
END
```

The function `GET_AGE` uses the current date (`SYSDATE`) to calculate the age based on the supplied date of birth. You can use this function in all parts of an SQL query, for example in `select` and the `where` clauses:

```
SELECT first_name, last_name, get_age(date_of_birth)
  FROM employees
 WHERE get_age(date_of_birth) = 42
```

The query lists all 42-year-old employees. Using a function-based index is an obvious idea for optimizing this query, but you cannot use the function `GET_AGE` in an index definition because it is not *deterministic*. That means the result of the function call is not fully determined by its parameters. Only functions that always return the same result for the same parameters—functions that are deterministic—can be indexed.

The reason behind this limitation is simple. When inserting a new row, the database calls the function and stores the result in the index and there it stays, unchanged. There is no periodic process that updates the index. The database updates the indexed age only when the date of birth is changed by an `update` statement. After the next birthday, the age that is stored in the index will be wrong.

Besides *being* deterministic, PostgreSQL and the Oracle database require functions to be *declared* to be deterministic when used in an index so you have to use the keyword `DETERMINISTIC` (Oracle) or `IMMUTABLE` (PostgreSQL).

> **Caution:**
>
> PostgreSQL and the Oracle database trust the `DETERMINISTIC` or `IMMUTABLE` declarations—that means they trust the developer.
>
> You can declare the `GET_AGE` function to be deterministic and use it in an index definition. Regardless of the declaration, it will *not* work as intended because the age stored in the index will not increase as the years pass; the employees will not get older—at least not in the index.

Other examples for functions that cannot be “indexed” are random number generators and functions that depend on environment variables.

> **Note:**
>
> Db2 (LUW) cannot use user-defined functions in indexes (not even if they are deterministic).

> **Think About It:**
>
> How can you still use an index to optimize a query for all 42-year-old employees?


## Over-Indexing

<sub>Source: https://use-the-index-luke.com/sql/where-clause/functions/over-indexing</sub>

If the concept of function-based indexing is new to you, you might be tempted to just index everything, but this is in fact the very last thing you should do. The reason is that every index causes ongoing maintenance. Function-based indexes are particularly troublesome because they make it very easy to create *redundant indexes*.

The [case-insensitive search from above](#case-insensitive-search-using-upper-or-lower) could be implemented with the `LOWER` function as well:

```
SELECT first_name, last_name, phone_number
  FROM employees
 WHERE LOWER(last_name) = LOWER('winand')
```

A single index cannot support both methods of ignoring the case. We could, of course, create a second index on `LOWER(last_name)` for this query, but that would mean the database has to maintain two indexes for each `insert`, `update`, and `delete` statement (see also [Chapter 8, “*Modifying Data*”](dml.md)). To make one index suffice, you should consistently use the same function throughout your application.

> **Tip:**
>
> Unify the access path so that one index can be used by several queries.

> **Warning:**
>
> Sometimes ORM tools use `UPPER` and `LOWER` without the developer’s knowledge. Hibernate, for example, [injects an implicit `LOWER`](myth-directory.md#dynamic-sql-is-slow) for case-insensitive searches.

> **Tip:**
>
> Always aim to index the original data as that is often the most useful information you can put into an index.
