<!-- Source: https://use-the-index-luke.com/sql/partial-results — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Partial Results

<sub>Source: https://use-the-index-luke.com/sql/partial-results</sub>

Sometimes you do not need the full result of an SQL query but only the first few rows—e.g., to show only the ten most recent messages. In this case, it is also common to allow users to browse through older messages—either using traditional paging navigation or the more modern “infinite scrolling” variant. The related SQL queries used for this function can, however, cause serious performance problems if *all* messages must be sorted in order to find the most recent ones. A [pipelined `order by`](sorting-grouping.md#indexing-order-by) is therefore a very powerful means of optimization for such queries.

Using a pipelined `order by` is not only about saving the effort to sort the result, it is more about delivering the first results without reading and sorting all rows. That means the pipelined `order by` has very low startup costs. It is therefore possible to abort the execution after fetching a few rows without discarding the efforts to prepare the final result.

This chapter demonstrates how to use a pipelined `order by` to efficiently retrieve partial results. Although the syntax of these queries varies from database to database, they still execute the queries in a very similar way. Once again, this illustrates that they all put their pants on one leg at a time.

## Contents

1. *[Selecting Top-N Rows](#querying-top-n-rows)* — if you need the first few rows only
2. *[Fetching The Next Page](#paging-through-results)* — The offset and seek methods compared
3. *[Window-Functions](#using-window-functions-for-efficient-pagination)* — Pagination using analytic queries


## Querying Top-N Rows

<sub>Source: https://use-the-index-luke.com/sql/partial-results/top-n-queries</sub>

Top-N queries are queries that limit the result to a specific number of rows. These are often queries for the most recent or the “best” entries of a result set. For efficient execution, the ranking must be done with a [pipelined `order by`.](sorting-grouping.md#indexing-order-by)

The simplest way to fetch only the first rows of a query is fetching the required rows and then closing the statement. Unfortunately, the optimizer cannot foresee that when preparing the execution plan. To select the best execution plan, the optimizer has to know if the application will ultimately fetch all rows. In that case, a full table scan with explicit sort operation might perform best, although a pipelined `order by` could be better when fetching only ten rows—even if the database has to fetch each row individually. That means that the optimizer has to know if you are going to abort the statement before fetching all rows so it can select the best execution plan.

> **Tip:**
>
> Inform the database whenever you don’t need all rows.

The SQL standard excluded this requirement for a long time. The corresponding extension (`fetch first`) was finally introduced with SQL:2008 and is currently available in IBM Db2, PostgreSQL, SQL Server 2012 and Oracle 12c. On the one hand, this is because the feature is a non-core extension, and on the other hand it’s because each database has been offering its own proprietary solution for many years.

The following examples show the use of these well-known extensions by querying the ten most recent sales. The basis is always the same: fetching *all* sales, beginning with the most recent one. The respective top-N syntax just aborts the execution after fetching ten rows.

#### Db2 (LUW)

Db2 supports the standard’s `fetch first` syntax since version 9 at least (LUW and zOS).

```
SELECT *
  FROM sales
 ORDER BY sale_date DESC
 FETCH FIRST 10 ROWS ONLY
```

The proprietary `limit` keyword is supported since Db2 (LUW) 9.7 (requires `db2set DB2_COMPATIBILITY_VECTOR=MYS`).

#### MySQL

MySQL and PostgreSQL use the `limit` clause to restrict the number of rows to be fetched.

```
SELECT *
  FROM sales
 ORDER BY sale_date DESC
 LIMIT 10
```

#### Oracle

The Oracle database introduced the `fetch first` extension with release 12c. With earlier releases you have to use the pseudo column `ROWNUM` that numbers the rows in the result set automatically. To use this column in a filter, we have to wrap the query:

```
SELECT *
  FROM (
       SELECT *
         FROM sales
        ORDER BY sale_date DESC
       )
 WHERE rownum <= 10
```

#### PostgreSQL

PostgreSQL supports the `fetch first` extension since version 8.4. The previously used `limit` clause still works as shown in the MySQL example.

```
SELECT *
  FROM sales
 ORDER BY sale_date DESC
 FETCH FIRST 10 ROWS ONLY
```

#### SQL Server

SQL Server provides the `top` clause to restrict the number of rows to be fetched.

```
SELECT TOP 10 *
  FROM sales
 ORDER BY sale_date DESC
```

[Starting with release 2012, SQL Server supports the `fetch first` extension as well.](https://learn.microsoft.com/en-us/previous-versions/sql/sql-server-2012/ms188385(v=sql.110))

All of the above shown SQL queries are special because the databases recognize them as top-N queries.

> **Important:**
>
> The database can only optimize a query for a partial result if it knows this from the beginning.

If the optimizer is aware of the fact that we only need ten rows, it will prefer to use a pipelined `order by` if applicable:

#### Db2 (LUW)

```
Explain Plan
-----------------------------------------------------------------
ID | Operation                      |               Rows |   Cost
 1 | RETURN                         |                    |     24
 2 |  FETCH SALES                   |      10 of 1009326 | 458452
 3 |   IXSCAN (REVERSE) SALES_DT_PR | 1009326 of 1009326 |   2624

Predicate Information
```

The top-N behaviour is not directly visible in the Db2 execution plan unless there is a `SORT` operation required (then the [`last_explained` view](explain-plan-db2.md#getting-an-execution-plan) indicates it in brackets: `SORT (TOP-N)`, see next example).

In this particular example, one might suspect that this must be a top-N query because of the sudden drop of the row count estimate that cannot explained by any filtering predicates (Predicate Information section is empty).

#### Oracle

```
-------------------------------------------------------------
| Operation                     | Name        | Rows | Cost |
-------------------------------------------------------------
| SELECT STATEMENT              |             |   10 |    9 |
|  COUNT STOPKEY                |             |      |      |
|   VIEW                        |             |   10 |    9 |
|    TABLE ACCESS BY INDEX ROWID| SALES       | 1004K|    9 |
|     INDEX FULL SCAN DESCENDING| SALES_DT_PR |   10 |    3 |
-------------------------------------------------------------
```

The Oracle execution plan indicates the planned termination with the `COUNT STOPKEY` operation. That means the database recognized the top-N syntax.

> **Tip:**
>
> [Appendix A, “*Execution Plans*”](explain-plan.md), summarizes the corresponding operations for [Db2 (LUW)](explain-plan-db2.md#db2-luw-execution-plan-operations), [MySQL](explain-plan-mysql.md#operations), [Oracle](explain-plan-oracle.md#operations), [PostgreSQL](explain-plan-postgresql.md#operations) and [SQL Server](explain-plan-sql-server.md#operations).

> **Important:**
>
> A pipelined top-N query doesn’t need to read and sort the entire result set.

If there is no suitable index on `SALE_DATE` for a pipelined `order by`, the database must read and sort the entire table. The first row is only delivered after reading the last row from the table.

#### Db2 (LUW)

```
Explain Plan
-----------------------------------------------------------
ID | Operation       |                         Rows |  Cost
 1 | RETURN          |                              | 59835
 2 |  TBSCAN         |           10 of 10 (100.00%) | 59835
 3 |   SORT (TOP-N)  |      10 of 1009326 (   .00%) | 59835
 4 |    TBSCAN SALES | 1009326 of 1009326 (100.00%) | 59739

Predicate Information
```

#### Oracle

```
--------------------------------------------------
| Operation               | Name  | Rows |  Cost |
--------------------------------------------------
| SELECT STATEMENT        |       |   10 | 59558 |
|  COUNT STOPKEY          |       |      |       |
|   VIEW                  |       | 1004K| 59558 |
|    SORT ORDER BY STOPKEY|       | 1004K| 59558 |
|     TABLE ACCESS FULL   | SALES | 1004K|  9246 |
--------------------------------------------------
```

This execution plan has no pipelined `order by` and is almost as slow as aborting the execution from the client side. Using the top-N syntax is still better because the database does not need to materialize the full result but only the ten most recent rows. This requires considerably less memory. The Oracle execution plan indicates this optimization with the `STOPKEY` modifier on the `SORT ORDER BY` operation.

The advantages of a pipelined top-N query include not only immediate performance gains but also improved scalability. Without using pipelined execution, the response time of this top-N query grows with the table size. The response time using a pipelined execution, however, only grows with the number of selected rows. In other words, the response time of a pipelined top-N query is always the same; this is almost independent of the table size. Only when the B-tree depth grows does the query become a little bit slower.

Figure 7.1 shows the scalability for both variants over a growing volume of data. The linear response time growth for an execution without a pipelined `order by` is clearly visible. The response time for the pipelined execution remains constant.

*[Figure 7.1 Scalability of Top-N Queries — diagram, see https://use-the-index-luke.com/sql/partial-results/top-n-queries]*

Although the response time of a pipelined top-N query does not depend on the table size, it still grows with the number of selected rows. The response time will therefore double when selecting twice as many rows. This is particularly significant for “paging” queries that load additional results because these queries often start at the first entry again; they will read the rows already shown on the previous page and discard them before finally reaching the results for the second page. Nevertheless, there is a solution for this problem as well as we will see in the next section.

> **Links:**
>
> Article “[Finding the Best Match With a Top-N Query](https://blog.fatalmind.com/2010/09/29/finding-the-best-match-with-a-top-n-query/)”


## Paging Through Results

<sub>Source: https://use-the-index-luke.com/sql/partial-results/fetch-next-page</sub>

After implementing a [pipelined top-N query](#querying-top-n-rows) to retrieve the first page efficiently, you will often also need another query to fetch the next pages. The resulting challenge is that it has to skip the rows from the previous pages. There are two different methods to meet this challenge: firstly the *offset method*, which numbers the rows from the beginning and uses a filter on this row number to discard the rows before the requested page. The second method, which I call the *seek method*, searches the last entry of the previous page and fetches only the following rows.

The following examples show the more widely used offset method. Its main advantage is that it is very easy to handle—especially with databases that have a dedicated keyword for it (`offset`). This keyword was even taken into the SQL standard as part of the `fetch first` extension.

#### Db2 (LUW)

Db2 supports `offset` since release 11.1. The standard conforming alternative using [`ROW_NUMBER()` window function (see next section)](#using-window-functions-for-efficient-pagination) works in earlier releases. There are two other ways to get offset functionality, none of them recommendable: (1) using `db2set DB2_COMPATIBILITY_VECTOR=MYS` to enable `limit` and `offset` like MySQL supports it. This does, however, not allow to combine `fetch first` with `offset`; (2) using `db2set DB2_COMPATIBILITY_VECTOR=ORA` to get Oracle’s `ROWNUM` pseudo column (see Oracle example).

#### MySQL

MySQL and PostgreSQL offer the `offset` clause for discarding the specified number of rows from the beginning of a top-N query. The `limit` clause is applied afterwards.

```
SELECT *
  FROM sales
 ORDER BY sale_date DESC
 LIMIT 10 OFFSET 10
```

#### Oracle

The Oracle database supports `offset` since release 12c. Earlier releases provide the pseudo column `ROWNUM` that numbers the rows in the result set automatically. It is, however, not possible to apply a greater than or equal to (`>=`) filter on this pseudo-column. To make this work, you need to first “materialize” the row numbers by renaming the column with an alias.

```
SELECT *
  FROM ( SELECT tmp.*, rownum rn
           FROM ( SELECT *
                    FROM sales
                   ORDER BY sale_date DESC
                ) tmp
          WHERE rownum <= 20
       )
 WHERE rn > 10
```

Note the use of the alias `RN` for the lower bound and the `ROWNUM` pseudo column itself for the upper bound (thanks to [Tom Kyte](https://www.reddit.com/r/programming/comments/p7lgl/sql_pagination_in_constant_time_using_the_seek/c3n7s19/)).

#### PostgreSQL

The `fetch first` extension defines an `offset ... rows` clause as well. PostgreSQL, however, only accepts `offset` without the `rows` keyword. The previously used `limit/offset` syntax still works as shown in the MySQL example.

```
SELECT *
  FROM sales
 ORDER BY sale_date DESC
OFFSET 10
 FETCH NEXT 10 ROWS ONLY
```

#### SQL Server

SQL Server does not have an “offset” extension for its proprietary `top` clause but introduced the `fetch first` extension with SQL Server 2012. The `offset` clause is mandatory although the standard defines it as an optional addendum.

```
SELECT *
  FROM sales
 ORDER BY sale_date DESC
OFFSET 10 ROWS
 FETCH NEXT 10 ROWS ONLY
```

Besides the simplicity, another advantage of this method is that you just need the row offset to fetch an arbitrary page. Nevertheless, the database must count all rows from the beginning until it reaches the requested page. Figure 7.2 shows that the scanned index range becomes greater when fetching more pages.

*[Figure 7.2 Access Using the Offset Method — diagram, see https://use-the-index-luke.com/sql/partial-results/fetch-next-page]*

This has two disadvantages: (1) the pages drift when inserting new sales because the numbering is always done from scratch; (2) the response time increases when browsing further back.

The seek method avoids both problems because it uses the *values* of the previous page as a delimiter. That means it searches for the values that must *come behind* the last entry from the previous page. This can be expressed with a simple `where` clause. To put it the other way around: the seek method simply doesn’t select already shown values.

The next example shows the seek method. For the sake of demonstration, we will start with the assumption that there is only one sale per day. This makes the `SALE_DATE` a unique key. To select the sales that must come behind a particular date you must use a less than condition (`<`) because of the descending sort order. For an ascending order, you would have to use a greater than (`>`) condition. The `fetch first` clause is just used to limit the result to ten rows.

```
SELECT *
  FROM sales
 WHERE sale_date < ?
 ORDER BY sale_date DESC
 FETCH FIRST 10 ROWS ONLY
```

Instead of a row number, you use the last value of the previous page to specify the lower bound. This has a huge benefit in terms of performance because the database can use the `SALE_DATE < ?` condition for index access. That means that the database can truly skip the rows from the previous pages. On top of that, you will also get stable results if new rows are inserted.

Nevertheless, this method does not work if there is more than one sale per day—as shown in Figure 7.2—because using the last date from the first page (“yesterday”) skips *all* results from yesterday—not just the ones already shown on the first page. The problem is that the `order by` clause does not establish a deterministic row sequence. That is, however, prerequisite to using a simple range condition for the page breaks.

Without a deterministic `order by` clause, the database by definition does not deliver a deterministic row sequence. The only reason you *usually* get a consistent row sequence is that the database *usually* executes the query in the same way. Nevertheless, the database could in fact shuffle the rows having the same `SALE_DATE` and still fulfill the `order by` clause. In recent releases it might indeed happen that you get the result in a different order every time you run the query, not because the database shuffles the result intentionally but because the database might utilize parallel query execution. That means that the same execution plan can result in a different row sequence because the executing threads finish in a non-deterministic order.

> **Important:**
>
> Paging requires a deterministic sort order.

Even if the functional specifications only require sorting “by date, latest first”, we as the developers must make sure the `order by` clause yields a deterministic row sequence. For this purpose, we might need to extend the `order by` clause with arbitrary columns just to make sure we get a deterministic row sequence. If the index that is used for the pipelined `order by` has additional columns, it is a good start to add them to the `order by` clause so we can continue using this index for the pipelined `order by`. If this still does not yield a deterministic sort order, just add any unique column(s) and extend the index accordingly.

In the following example, we extend the `order by` clause and the index with the primary key `SALE_ID` to get a deterministic row sequence. Furthermore, we must apply the “comes after” logic to both columns *together* to get the desired result:

```
CREATE INDEX sl_dtid ON sales (sale_date, sale_id)
```

```
SELECT *
  FROM sales
 WHERE (sale_date, sale_id) < (?, ?)
 ORDER BY sale_date DESC, sale_id DESC
 FETCH FIRST 10 ROWS ONLY
```

The `where` clause uses the little-known “row values” syntax (see the box entitled “*SQL Row Values*”). It combines multiple values into a logical unit that is applicable to the regular comparison operators. As with scalar values, the less-than condition corresponds to “comes after” when sorting in descending order. That means the query considers only the sales that come after the given `SALE_DATE`, `SALE_ID` pair.

> **Sidebar — SQL Row Values**
>
> Besides regular scalar values, the SQL standard also defines the so-called *row value constructors*. They “Specify an ordered set of values to be constructed into a row or partial row” [[SQL:92](https://www.contrib.andrew.cmu.edu/~shadow/sql/sql1992.txt), §7.1: <row value constructor>]. Syntactically, row values are lists in brackets. This syntax is best known for its use in the `insert` statement.
>
> Using row value constructors in the `where` clause is, however, less well-known but still perfectly valid. The SQL standard actually defines all comparison operators for row value constructors. The definition for the less than operations is, for example, as follows:0
>
> > X < Y is True if and only if Xi = Yi is True for all i < n and Xn < Yn for some n.
>
> Where *i* and *n* reflect positional indexes in the lists. That means a row value X is less than Y if any value Xn is smaller than the corresponding Yn and all preceding value pairs are equal (*Xi = Yi; for i<n*).
>
> This definition makes the expression X < Y synonymous to “X sorts before Y” which is exactly the logic we need for the seek method.

Even though the row values syntax is part of the SQL standard, only a few databases support it. SQL Server 2017 does not support row values at all. The Oracle database supports row values in principle, but cannot apply range operators on them (ORA-01796). MySQL evaluates row value expressions correctly but cannot use them as access predicate during an index access. Db2 (only LUW, since 10.1) and PostgreSQL (since 8.4), however, have a proper support of row value predicates *and* uses them to access the index if there is a corresponding index available.

Nevertheless it is possible to use an approximated variant of the seek method with databases that do not properly support the row values—even though the approximation is not as elegant and efficient as row values in PostgreSQL. For this approximation, we must use “regular” comparisons to express the required logic as shown in this Oracle example:

```
SELECT *
  FROM ( SELECT *
           FROM sales
          WHERE sale_date <= ?
            AND NOT (sale_date = ? AND sale_id >= ?)
          ORDER BY sale_date DESC, sale_id DESC
       )
 WHERE rownum <= 10
```

The `where` clause consists of two parts. The first part considers the `SALE_DATE` only and uses a less than or equal to (`<=`) condition—it selects more rows as needed. This part of the `where` clause is simple enough so that all databases can use it to access the index. The second part of the `where` clause removes the excess rows that were already shown on the previous page. The box entitled “*Indexing Equivalent Logic*” explains why the `where` clause is expressed this way.

> **Sidebar — Indexing Equivalent Logic**
>
> A logical condition can always be expressed in different ways. You could, for example, also implement the above shown skip logic as follows:
>
> ```
> WHERE (
>          (sale_date < ?)
>        OR
>          (sale_date = ? AND sale_id < ?)
>       )
> ```
>
> This variant only uses including conditions and is probably easier to understand—for human beings, at least. Databases have a different point of view. They do not recognize that the `where` clause selects all rows starting with the respective `SALE_DATE`/`SALE_ID` pair—provided that the `SALE_DATE` is the same for both branches. Instead, the database uses the entire `where` clause as filter predicate. We could at least expect the optimizer to “factor the condition `SALE_DATE <= ?` out” of the two or-branches, but none of the databases provides this service.
>
> Nevertheless we can add this redundant condition manually—even though it does not increase readability:
>
> ```
> WHERE sale_date <= ?
>   AND (
>          (sale_date < ?)
>        OR
>          (sale_date = ? AND sale_id < ?)
>       )
> ```
>
> Luckily, all databases are able to use the this part of the `where` clause as access predicate. That clause is, however, even harder to grasp as the approximation logic shown above. Further, the original logic avoids the risk that the “unnecessary” (redundant) part is accidentally removed from the `where` clause later on.

The execution plan shows that the database uses the first part of the `where` clause as access predicate.

```
---------------------------------------------------------------
|Id | Operation                      | Name    |  Rows | Cost |
---------------------------------------------------------------
| 0 | SELECT STATEMENT               |         |    10 |    4 |
|*1 |  COUNT STOPKEY                 |         |       |      |
| 2 |   VIEW                         |         |    10 |    4 |
| 3 |    TABLE ACCESS BY INDEX ROWID | SALES   | 50218 |    4 |
|*4 |     INDEX RANGE SCAN DESCENDING| SL_DTIT |     2 |    3 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   1 - filter(ROWNUM<=10)
   4 - access("SALE_DATE"<=:SALE_DATE)
       filter("SALE_DATE"<>:SALE_DATE
           OR "SALE_ID"<TO_NUMBER(:SALE_ID))
```

The access predicates on `SALE_DATE` enables the database to skip over the days that were fully shown on previous pages. The second part of the `where` clause is a filter predicate only. That means that the database inspects a few entries from the previous page again, but drops them immediately. Figure 7.3 shows the respective access path.

*[Figure 7.3 Access Using the Seek Method — diagram, see https://use-the-index-luke.com/sql/partial-results/fetch-next-page]*

Figure 7.4 compares the performance characteristics of the offset and the seek methods. The accuracy of measurement is insufficient to see the difference on the left hand side of the chart, however the difference is clearly visible from about page 20 onwards.

*[Figure 7.4 Scalability when Fetching the Next Page — diagram, see https://use-the-index-luke.com/sql/partial-results/fetch-next-page]*

Of course the seek method has drawbacks as well, the difficulty in handling it being the most important one. You not only have to phrase the `where` clause very carefully—you also cannot fetch arbitrary pages. Moreover you need to reverse all comparison and sort operations to change the browsing direction. Precisely these two functions—skipping pages and browsing backwards—are not needed when using an infinite scrolling mechanism for the user interface.

*[Figure 7.5 Database/Feature Matrix — diagram, see https://use-the-index-luke.com/sql/partial-results/fetch-next-page]*


## Using Window Functions for Efficient Pagination

<sub>Source: https://use-the-index-luke.com/sql/partial-results/window-functions</sub>

Window functions offer yet another way to implement pagination in SQL. This is a flexible, and above all, standards-compliant method. However, only SQL Server, the Oracle database and PostgreSQL 15+ can use them for a pipelined top-N query. MySQL, MariaDB0and Db2 (LUW) do not abort the index scan after fetching enough rows and therefore execute these queries very inefficiently.

The following example uses the window function `ROW_NUMBER` for a pagination query:

```
SELECT *
  FROM ( SELECT sales.*
              , ROW_NUMBER() OVER (ORDER BY sale_date DESC
                                          , sale_id   DESC) rn
           FROM sales
       ) tmp
 WHERE rn between 11 and 20
 ORDER BY sale_date DESC, sale_id DESC
```

The `ROW_NUMBER` function enumerates the rows according to the sort order defined in the `over` clause. The outer `where` clause uses this enumeration to limit the result to the second page (rows 11 through 20).

The Oracle database recognizes the abort condition and uses the index on `SALE_DATE` and `SALE_ID` to produce a pipelined top-N behavior:

#### Db2 (LUW)

```
Explain Plan
-------------------------------------------------------------
ID | Operation                   |               Rows |  Cost
 1 | RETURN                      |                    | 65658
 2 |  FILTER                     |  100933 of 1009326 | 65658
 3 |   FETCH SALES               | 1009326 of 1009326 | 65295
 4 |    IXSCAN (REVERSE) SL_DTID | 1009326 of 1009326 |  5679

Predicate Information
 2 - RESID (11 <= Q3.$C8)
     RESID (Q3.$C8 <= 20)
```

Please note that Db2 (LUW) 10.5 does not execute this query as top-n query. Although it prevents the sort operation it still reads through the entire index—it does not abort execution after fetching 20 rows.

To get a proper top-n abort after reading 20 rows, you must double-wrap the query to first apply the top-n abort condition and then filtering away the first 10 rows:

```
SELECT *
  FROM (SELECT *
          FROM (SELECT sales.*
                     , ROW_NUMBER() OVER (ORDER BY sale_date DESC
                                                 , sale_id   DESC) rn
                 FROM sales
               ) tmp
         WHERE rn <= 20
       ) tmp2
 WHERE rn > 10
 ORDER BY sale_date DESC, sale_id DESC;
```

```
Explain Plan
-----------------------------------------------------------------
ID | Operation                       |               Rows |  Cost
 1 | RETURN                          |                    |    21
 2 |  FILTER                         |            7 of 20 |    21
 3 |   FETCH SALES                   |      20 of 1009326 | 65352
 4 |    IXSCAN (REVERSE) SALES_DT_ID | 1009326 of 1009326 |  5736

Predicate Information
 2 - RESID (10 < Q3.$C8)
```

Note that the filter `rn <= 20` does not appear in the Predicate Information section, yet the row count estimate reflects it.

#### Oracle

```
---------------------------------------------------------------
|Id | Operation                      | Name    | Rows |  Cost |
---------------------------------------------------------------
| 0 | SELECT STATEMENT               |         | 1004K| 36877 |
|*1 |  VIEW                          |         | 1004K| 36877 |
|*2 |   WINDOW NOSORT STOPKEY        |         | 1004K| 36877 |
| 3 |    TABLE ACCESS BY INDEX ROWID | SALES   | 1004K| 36877 |
| 4 |     INDEX FULL SCAN DESCENDING | SL_DTID | 1004K|  2955 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
1 - filter("RN">=11 AND "RN"<=20)
2 - filter(ROW_NUMBER() OVER (
           ORDER BY "SALE_DATE" DESC, "SALE_ID" DESC )<=20)
```

#### PostgreSQL

Since version 15 the execution plan shows the “Run Condition”, which may abort the downstream execution.

```
                     QUERY PLAN
-------------------------------------------------------
 Subquery Scan on tmp
    (cost=0.42..141724.98 rows=334751 width=249)
    (actual time=0.040..0.052 rows=10 loops=1)
 Filter: (tmp.rn >= 11)
 Rows Removed by Filter: 10
 Buffers: shared hit=5
 -> WindowAgg
       (cost=0.42..129171.80 rows=1004254 width=249)
       (actual time=0.028..0.049 rows=20 loops=1)
    Run Condition: (row_number() OVER (?) <= 20)
    Buffers: shared hit=5
    -> Index Scan Backward using sl_dtid on sales
           (cost=0.42..111597.36 rows=1004254 width=241)
           (actual time=0.018..0.025 rows=22 loops=1)
       Buffers: shared hit=5
```

The `WINDOW NOSORT STOPKEY` operation indicates that there is no sort operation (`NOSORT`) and that the database aborts the execution when reaching the upper threshold (`STOPKEY`). Considering that the aborted operations are executed in a pipelined manner, it means that this query is as efficient as the offset method explained in the [previous section](#paging-through-results).

Support of this optimization is by no means common among SQL products.

1. aOnly with `row_number()`
2. bNot with `partition by` clause (see below)

While this optimization could conceptually work with any monotonic function, there is a clear focus on the `ROW_NUMBER` function among the analyzed implementations.

1. aOnly with `where` clause: `OVER(ORDER BY…)` + `WHERE x=?`

Caution needs to be taken when using `partition by`: Even if the `where` clause limits the rows to a single partition, the pure presence of `partition by` disables this optimization in some products. This might happen if the partitioned window function is part of a view, but the outer query limits the view to a single partition.

1. aNot all functions that work in presence of a `where` clause

The strength of window functions is not pagination, however, but analytical calculations. If you have never used window functions before, you should definitely spend a few hours studying the respective documentation.

> **Links:**
>
> Oracle: [Analytic Functions in 19](https://docs.oracle.com/en/database/oracle/oracle-database/19/sqlrf/Analytic-Functions.html)
>
> PostgreSQL: [Window Functions](https://www.postgresql.org/docs/current/tutorial-window.html)
>
> SQL Server: [OVER Clause in SQL Server](https://learn.microsoft.com/en-us/sql/t-sql/queries/select-over-clause-transact-sql?view=sql-server-ver16)
