<!-- Source: https://use-the-index-luke.com/sql/sorting-grouping — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Sorting and Grouping

<sub>Source: https://use-the-index-luke.com/sql/sorting-grouping</sub>

Sorting is a very resource intensive operation. It needs a fair amount of CPU time, but the main problem is that the database must temporarily buffer the results. After all, a sort operation must read the complete input before it can produce the first output. Sort operations cannot be executed in a pipelined manner—this can become a problem for large data sets.

An index provides an ordered representation of the indexed data: this principle was already described in [Chapter 1](anatomy.md). We could also say that an index stores the data in a presorted fashion. The index is, in fact, sorted just like when using the index definition in an `order by` clause. It is therefore no surprise that we can use indexes to avoid the sort operation to satisfy an `order by` clause.

Ironically, an `INDEX RANGE SCAN` also becomes inefficient for large data sets—especially when followed by a table access. This can nullify the savings from avoiding the sort operation. A [`FULL TABLE SCAN`](where-clause-the-equals-operator.md#concatenated-indexes) with an explicit sort operation might be even faster in this case. Again, it is the optimizer’s job to evaluate the different execution plans and select the best one.

An indexed `order by` execution not only saves the sorting effort, however; it is also able to return the first results without processing all input data. The `order by` is thus executed in a *pipelined* manner. [Chapter 7*Partial Results*](partial-results.md), explains how to exploit the pipelined execution to implement efficient pagination queries. This makes the pipelined `order by` so important that I refer to it as the *third power of indexing*.

> **Note:**
>
> The [B-Tree traversal](anatomy.md#the-search-tree-b-tree-makes-the-index-fast) is the first power of indexing.
>
> [Clustering](clustering.md) is the second power of indexing.
>
> Pipelined `order by` is the third power of indexing.

This chapter explains how to use an index for a pipelined `order by` execution. To this end we have to pay special attention to the interactions with the `where` clause and also to `ASC` and `DESC` modifiers. The chapter concludes by applying these techniques to `group by` clauses as well.

## Contents

1. *[Indexed Order By](#indexing-order-by)* — `where` clause interactions
2. *[`ASC`/`DESC` and `NULL FIRST`/`LAST`](#indexing-asc-desc-and-nulls-first-last)* — changing index order
3. *[Indexed Group By](#indexing-group-by)* — Pipelining `group by`


## Indexing Order By

<sub>Source: https://use-the-index-luke.com/sql/sorting-grouping/indexed-order-by</sub>

SQL queries with an `order by` clause do not need to sort the result explicitly if the relevant index already delivers the rows in the required order. That means the same index that is used for the `where` clause must also cover the `order by` clause.

As an example, consider the following query that selects yesterday’s sales ordered by sale date and product ID:

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date = TRUNC(sysdate) - INTERVAL '1' DAY
 ORDER BY sale_date, product_id
```

There is already an index on `SALE_DATE` that can be used for the `where` clause. The database must, however, perform an explicit sort operation to satisfy the `order by` clause:

#### Db2 (LUW)

```
Explain Plan
------------------------------------------------------------
ID | Operation             |                     Rows | Cost
 1 | RETURN                |                          |  682
 2 |  TBSCAN               |     394 of 394 (100.00%) |  682
 3 |   SORT                |     394 of 394 (100.00%) |  682
 4 |    FETCH SALES        |     394 of 394 (100.00%) |  682
 5 |     IXSCAN SALES_DATE | 394 of 1009326 (   .04%) |   19

Predicate Information
 5 - START (Q1.SALE_DATE = (CURRENT DATE - 1 DAYS))
      STOP (Q1.SALE_DATE = (CURRENT DATE - 1 DAYS))
```

The `where`-clause was changed like this to be Db2 compliant: `WHERE sale_date > CURRENT_DATE - 1 DAY`.

#### Oracle

```
---------------------------------------------------------------
|Id | Operation                    | Name       | Rows | Cost |
---------------------------------------------------------------
| 0 | SELECT STATEMENT             |            |  320 |   18 |
| 1 |  SORT ORDER BY               |            |  320 |   18 |
| 2 |   TABLE ACCESS BY INDEX ROWID| SALES      |  320 |   17 |
|*3 |    INDEX RANGE SCAN          | SALES_DATE |  320 |    3 |
---------------------------------------------------------------
```

An `INDEX RANGE SCAN` delivers the result in index order anyway. To take advantage of this fact, we just have to extend the index definition so it corresponds to the `order by` clause:

```
  DROP INDEX sales_date
```

```
CREATE INDEX sales_dt_pr ON sales (sale_date, product_id)
```

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date = TRUNC(sysdate) - INTERVAL '1' DAY
 ORDER BY sale_date, product_id
```

#### Db2 (LUW)

```
Explain Plan
-----------------------------------------------------------
ID | Operation            |                     Rows | Cost
 1 | RETURN               |                          |  688
 2 |  FETCH SALES         |     394 of 394 (100.00%) |  688
 3 |   IXSCAN SALES_DT_PR | 394 of 1009326 (   .04%) |   24

Predicate Information
 3 - START (Q1.SALE_DATE = (CURRENT DATE - 1 DAYS))
      STOP (Q1.SALE_DATE = (CURRENT DATE - 1 DAYS))
```

#### Oracle

```
---------------------------------------------------------------
|Id | Operation                   | Name        | Rows | Cost |
---------------------------------------------------------------
| 0 | SELECT STATEMENT            |             |  320 |  300 |
| 1 |  TABLE ACCESS BY INDEX ROWID| SALES       |  320 |  300 |
|*2 |   INDEX RANGE SCAN          | SALES_DT_PR |  320 |    4 |
---------------------------------------------------------------
```

The sort operation `SORT ORDER BY` disappeared from the execution plan even though the query still has an `order by` clause. The database exploits the index order and skips the explicit sort operation.

> **Important:**
>
> If the index order corresponds to the `order by` clause, the database can omit the explicit sort operation.

Even though the new execution plan has fewer operations, the cost value has increased considerably because the clustering factor of the new index is worse (see “*Automatically Optimized Clustering Factor*”). At this point, it should just be noted that the cost value is not always a good indicator of the execution effort.

> **Sidebar — Automatically Optimized Clustering Factor**
>
> The Oracle database keeps the clustering factor at a minimum by considering the `ROWID` for the index order. Whenever two index entries have the same key values, the `ROWID` decides upon their final order. The index is therefore also ordered according to the table order and thus has the smallest possible clustering factor because the `ROWID` represents the physical address of table row.
>
> By adding another column to an index, you insert a new sort criterion *before* the `ROWID`. The database has less freedom in aligning the index entries according to the table order so the index clustering factor can only get worse.
>
> Regardless, it is still possible that the index order roughly corresponds to the table order. The sales of a day are probably still clustered together in the table as well as in the index—even though their sequence is not exactly the same anymore. The database has to read the table blocks multiple times when using the `SALE_DT_PR` index—but these are just the same table blocks as before. Due to the caching of frequently accessed data, the performance impact could be considerably lower than indicated by the cost values.

For this optimization, it is sufficient that the scanned index range is sorted according to the `order by` clause. Thus the optimization also works for this particular example when sorting by `PRODUCT_ID` only:

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date = TRUNC(sysdate) - INTERVAL '1' DAY
 ORDER BY product_id
```

In Figure 6.1 we can see that the `PRODUCT_ID` is the only relevant sort criterion in the scanned index range. Hence the index order corresponds to the `order by` clause in *this index range* so that the database can omit the sort operation.

*[Figure 6.1 Sort Order in the Relevant Index Range — diagram, see https://use-the-index-luke.com/sql/sorting-grouping/indexed-order-by]*

This optimization can cause unexpected behavior when extending the scanned index range:

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= TRUNC(sysdate) - INTERVAL '1' DAY
 ORDER BY product_id
```

This query does not retrieve *yesterday’s* sales but all sales *since yesterday*. That means it covers several days and scans an index range that is not exclusively sorted by the `PRODUCT_ID`. If we look at Figure 6.1 again and extend the scanned index range to the bottom, we can see that there are again smaller `PRODUCT_ID` values. The database must therefore use an explicit sort operation to satisfy the `order by` clause.

#### Db2 (LUW)

```
Explain Plan
-------------------------------------------------------------
ID | Operation              |                     Rows | Cost
 1 | RETURN                 |                          |  688
 2 |  TBSCAN                |     394 of 394 (100.00%) |  688
 3 |   SORT                 |     394 of 394 (100.00%) |  688
 4 |    FETCH SALES         |     394 of 394 (100.00%) |  688
 5 |     IXSCAN SALES_DT_PR | 394 of 1009326 (   .04%) |   24

Predicate Information
 5 - START ((CURRENT DATE - 1 DAYS) <= Q1.SALE_DATE)
```

#### Oracle

```
---------------------------------------------------------------
|Id |Operation                    | Name        | Rows | Cost |
---------------------------------------------------------------
| 0 |SELECT STATEMENT             |             |  320 |  301 |
| 1 | SORT ORDER BY               |             |  320 |  301 |
| 2 |  TABLE ACCESS BY INDEX ROWID| SALES       |  320 |  300 |
|*3 |   INDEX RANGE SCAN          | SALES_DT_PR |  320 |    4 |
---------------------------------------------------------------
```

If the database uses a sort operation even though you expected a pipelined execution, it can have two reasons: (1) the execution plan with the explicit sort operation has a better cost value; (2) the index order in the scanned index range does not correspond to the `order by` clause.

A simple way to tell the two cases apart is to use the full index definition in the `order by` clause—that means adjusting the query to the index in order to eliminate the second cause. If the database still uses an explicit sort operation, the optimizer prefers this plan due to its cost value; otherwise the database cannot use the index for the original `order by` clause.

> **Tip:**
>
> Use the full index definition in the `order by` clause to find the reason for an explicit sort operation.

In both cases, you might wonder if and how you could possibly reach a pipelined `order by` execution. For this you can execute the query with the full index definition in the `order by` clause and inspect the result. You will often realize that you have a false perception of the index and that the index order is indeed not as required by the original `order by` clause so the database cannot use the index to avoid a sort operation.

If the optimizer prefers an explicit sort operation for its cost value, it is usually because the optimizer takes the best execution plan for the *full execution* of the query. In other words, the optimizer opts for the execution plan which is the fastest to get the last record. If the database detects that the application fetches only the first few rows, it might in turn prefer an indexed `order by`. [Chapter 7*Partial Results*](partial-results.md), explains the corresponding optimization methods.


## Indexing ASC , DESC and NULLS FIRST / LAST

<sub>Source: https://use-the-index-luke.com/sql/sorting-grouping/order-by-asc-desc-nulls-last</sub>

Databases can read indexes in both directions. That means that a pipelined `order by` is also possible if the scanned index range is in the exact opposite order as specified by the `order by` clause. Although `ASC` and `DESC` modifiers in the `order by` clause can prevent a pipelined execution, most databases offer a simple way to change the index order so an index becomes usable for a pipelined `order by`.

The following example uses an index in reverse order. It delivers the sales since yesterday ordered by descending date and descending `PRODUCT_ID.`

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= TRUNC(sysdate) - INTERVAL '1' DAY
 ORDER BY sale_date DESC, product_id DESC
```

The execution plan shows that the database reads the index in a descending direction.

#### Db2 (LUW)

```
Explain Plan
---------------------------------------------------------------------
ID | Operation                      |                     Rows | Cost
 1 | RETURN                         |                          |  688
 2 |  FETCH SALES                   |     394 of 394 (100.00%) |  688
 3 |   IXSCAN (REVERSE) SALES_DT_PR | 394 of 1009326 (   .04%) |   24

Predicate Information
 3 - STOP ((CURRENT DATE - 1 DAYS) <= Q1.SALE_DATE)
```

In Db2 reverse scans can be prevented by using the [`DISALLOW REVERSE SCAN`](https://www.ibm.com/docs/en/db2/11.5.x?topic=statements-create-index) clause during index creation.

#### Oracle

```
---------------------------------------------------------------
|Id |Operation                    | Name        | Rows | Cost |
---------------------------------------------------------------
| 0 |SELECT STATEMENT             |             |  320 |  300 |
| 1 | TABLE ACCESS BY INDEX ROWID | SALES       |  320 |  300 |
|*2 |  INDEX RANGE SCAN DESCENDING| SALES_DT_PR |  320 |    4 |
---------------------------------------------------------------
```

In this case, the database uses the [index tree](anatomy.md#the-search-tree-b-tree-makes-the-index-fast) to find the *last* matching entry. From there on, it follows the leaf node chain “upwards” as shown in Figure 6.2. After all, this is why the database uses a [*doubly* linked list](anatomy.md#the-index-leaf-nodes) to build the leaf node chain.

*[Figure 6.2 Reverse Index Scan — diagram, see https://use-the-index-luke.com/sql/sorting-grouping/order-by-asc-desc-nulls-last]*

Of course it is crucial that the scanned index range is in the exact opposite order as needed for the `order by` clause.

> **Important:**
>
> Databases can read indexes in both directions.

The following example does not fulfill this prerequisite because it mixes `ASC` and `DESC` modifiers in the `order by` clause:

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= TRUNC(sysdate) - INTERVAL '1' DAY
 ORDER BY sale_date ASC, product_id DESC
```

The query must first deliver yesterday’s sales ordered by descending `PRODUCT_ID` and then today’s sales, again by descending `PRODUCT_ID`. Figure 6.3 illustrates this process. To get the sales in the required order, the database would have to “jump” during the index scan.

*[Figure 6.3 Impossible Pipelined order by — diagram, see https://use-the-index-luke.com/sql/sorting-grouping/order-by-asc-desc-nulls-last]*

However, the index has no link from yesterday’s sale with the smallest `PRODUCT_ID` to today’s sale with the greatest. The database can therefore not use this index to avoid an explicit sort operation.

For cases like this, most databases offer a simple method to adjust the index order to the `order by` clause. Concretely, this means that you can use `ASC` and `DESC` modifiers in the index declaration:

```
  DROP INDEX sales_dt_pr
```

```
CREATE INDEX sales_dt_pr
    ON sales (sale_date ASC, product_id DESC)
```

> **Warning:**
>
> Prior to version 8.0, the MySQL database [ignores `ASC` and `DESC` modifiers in the index definition](https://dev.mysql.com/doc/refman/9.7/en/create-index.html). MariaDB honors `DESC` in indexes [only since version 10.8.](https://mariadb.com/docs/release-notes/community-server/old-releases/10.8/what-is-mariadb-108)

Now the index order corresponds to the `order by` clause so the database can omit the sort operation:

#### Db2 (LUW)

```
Explain Plan
-----------------------------------------------------------
ID | Operation            |                     Rows | Cost
 1 | RETURN               |                          |  675
 2 |  FETCH SALES         |     387 of 387 (100.00%) |  675
 3 |   IXSCAN SALES_DT_PR | 387 of 1009326 (   .04%) |   24

Predicate Information
 3 - START ((CURRENT DATE - 1 DAYS) <= Q1.SALE_DATE)
```

#### Oracle

```
---------------------------------------------------------------
|Id | Operation                   | Name        | Rows | Cost |
---------------------------------------------------------------
| 0 | SELECT STATEMENT            |             |  320 |  301 |
| 1 |  TABLE ACCESS BY INDEX ROWID| SALES       |  320 |  301 |
|*2 |   INDEX RANGE SCAN          | SALES_DT_PR |  320 |    4 |
---------------------------------------------------------------
```

Figure 6.4 shows the new index order. The change in the sort direction for the second column in a way swaps the direction of the arrows from the previous figure. That makes the first arrow end where the second arrow starts so that index has the rows in the desired order.

> **Important:**
>
> When using mixed `ASC` and `DESC` modifiers in the `order by` clause, you must define the index likewise in order to use it for a pipelined `order by`.
>
> This does not affect the index’s usability for the `where` clause.

*[Figure 6.4 Mixed-Order Index — diagram, see https://use-the-index-luke.com/sql/sorting-grouping/order-by-asc-desc-nulls-last]*

`ASC`/`DESC` indexing is only needed for sorting individual columns in opposite direction. It is not needed to reverse the order of all columns because the database could still read the index in descending order if needed—[secondary indexes](glossary.md#secondary-index) on [index organized tables](clustering.md#index-organized-tables-and-clustered-indexes) being the only exception. Secondary indexes implicitly add the clustering key to the index without providing any possibility for specifying the sort order. If you need to sort the clustering key in descending order, you have no other option than sorting all other columns in descending order. The database can then read the index in reverse direction to get the desired order.

Besides `ASC` and `DESC`, the SQL standard defines two hardly known modifiers for the `order by` clause: `NULLS FIRST` and `NULLS LAST`. Explicit control over `NULL` sorting was “recently” introduced as an *optional* extension with SQL:2003. As a consequence, database support is sparse. This is particularly worrying because the standard does not exactly define the sort order of `NULL`. It only states that all `NULL`s must appear together after sorting, but it does not specify if they should appear before or after the other entries. Strictly speaking, you would actually need to specify `NULL` sorting for all columns that can be null in the `order by` clause to get a well-defined behavior.

The fact is, however, that the optional extension is neither implemented by SQL Server 2019 nor by MySQL 8.0. The Oracle database, on the contrary, supported `NULLS` sorting even before it was introduced to the standard, but it does not accept it in index definitions as of release 19*c*. The Oracle database can therefore not do a pipelined `order by` when sorting with `NULLS FIRST`. Only the PostgreSQL database (since release 8.3) supports the `NULLS` modifier in both the `order by` clause and the index definition.

The following overview summarizes the features provided by different databases.

*[Figure 6.5 Database/Feature Matrix — diagram, see https://use-the-index-luke.com/sql/sorting-grouping/order-by-asc-desc-nulls-last]*


## Indexing Group By

<sub>Source: https://use-the-index-luke.com/sql/sorting-grouping/indexed-group-by</sub>

SQL databases use two entirely different `group by` algorithms. The first one, the hash algorithm, aggregates the input records in a temporary hash table. Once all input records are processed, the hash table is returned as the result. The second algorithm, the sort/group algorithm, first sorts the input data by the grouping key so that the rows of each group follow each other in immediate succession. Afterwards, the database just needs to aggregate them. In general, both algorithms need to materialize an intermediate state, so they are not executed in a pipelined manner. Nevertheless the sort/group algorithm can use an index to avoid the sort operation, thus enabling a pipelined `group by`.

> **Note:**
>
> MySQL 8.0 doesn’t use the hash algorithm. Nevertheless, the [optimization for the sort/group algorithm](https://dev.mysql.com/doc/refman/8.0/en/group-by-optimization.html) works as described below.

Consider the following query. It delivers yesterday’s revenue grouped by `PRODUCT_ID`:

```
SELECT product_id, sum(eur_value)
  FROM sales
 WHERE sale_date = TRUNC(sysdate) - INTERVAL '1' DAY
 GROUP BY product_id
```

Knowing the index on `SALE_DATE` and `PRODUCT_ID` from the [previous section](#indexing-asc-desc-and-nulls-first-last), the sort/group algorithm is more appropriate because an `INDEX RANGE SCAN` automatically delivers the rows in the required order. That means the database avoids materialization because it does not need an explicit sort operation—the `group by` is executed in a pipelined manner.

#### Db2 (LUW)

```
Explain Plan
------------------------------------------------------------
ID | Operation             |                     Rows | Cost
 1 | RETURN                |                          |  675
 2 |  GRPBY (COMPLETE)     |      25 of 387 (  6.46%) |  675
 3 |   FETCH SALES         |     387 of 387 (100.00%) |  675
 4 |    IXSCAN SALES_DT_PR | 387 of 1009326 (   .04%) |   24

Predicate Information
 4 - START (Q1.SALE_DATE = (CURRENT DATE - 1 DAYS))
      STOP (Q1.SALE_DATE = (CURRENT DATE - 1 DAYS))
```

#### Oracle

```
---------------------------------------------------------------
|Id |Operation                    | Name        | Rows | Cost |
---------------------------------------------------------------
| 0 |SELECT STATEMENT             |             |   17 |  192 |
| 1 | SORT GROUP BY NOSORT        |             |   17 |  192 |
| 2 |  TABLE ACCESS BY INDEX ROWID| SALES       |  321 |  192 |
|*3 |   INDEX RANGE SCAN          | SALES_DT_PR |  321 |    3 |
---------------------------------------------------------------
```

The Oracle database’s execution plan marks a pipelined `SORT GROUP BY` operation with the `NOSORT` addendum. The execution plan of other databases does not mention any sort operation at all.

The pipelined `group by` has the same prerequisites as the pipelined `order by`, except there are no `ASC` and `DESC` modifiers. That means that defining an index with `ASC`/`DESC` modifiers should not affect pipelined `group by` execution. The same is true for `NULLS FIRST`/`LAST`. Nevertheless there are databases that cannot properly use an `ASC`/`DESC` index for a pipelined `group by`.

> **Warning:**
>
> PostgreSQL does not automatically do a pipelined `group by` if the index treats the `NULL` value as the smallest possible value. Adding an `order by` clause with the index order bypasses this problem.
>
> The Oracle database cannot read an index backwards in order to execute a pipelined `group by` that is followed by an `order by`.
>
> More details are available in the respective appendices: [PostgreSQL](example-schema-postgresql.md#postgresql-example-scripts-for-sorting-and-grouping), [Oracle](example-schema-oracle.md#oracle-example-scripts-for-sorting-and-grouping).

If we extend the query to consider all sales *since yesterday*, as we did in the example for the pipelined `order by`, it prevents the pipelined `group by` for the same reason as before: the `INDEX RANGE SCAN` does not deliver the rows ordered by the grouping key (compare [Figure 6.1](#indexing-order-by)).

```
SELECT product_id, sum(eur_value)
  FROM sales
 WHERE sale_date >= TRUNC(sysdate) - INTERVAL '1' DAY
 GROUP BY product_id
```

#### Db2 (LUW)

```
Explain Plan
--------------------------------------------------------------------
ID | Operation                   |                      Rows |  Cost
 1 | RETURN                      |                           | 12527
 2 |  GRPBY (FINAL)              |        25 of 25 (100.00%) | 12527
 3 |   TBSCAN                    |        25 of 25 (100.00%) | 12527
 4 |    SORT (INTERMEDIATE)      |        25 of 25 (100.00%) | 12527
 5 |     GRPBY (HASHED PARTIAL)  |      25 of 8050 (   .31%) | 12527
 6 |      FETCH SALES            |    8050 of 8050 (100.00%) | 12526
 7 |       RIDSCN                |    8050 of 8050 (100.00%) |   375
 8 |        SORT (UNIQUE)        |    8050 of 8050 (100.00%) |   375
 9 |         IXSCAN SALES_DT_PR  | 8050 of 1009326 (   .80%) |   372
```

The following `where` clause was used to obtain this result: `WHERE sale_date >= CURRENT_DATE - 1 MONTH`.

Compared to the Oracle execution plan, this looks overly complex. This is due to two circumstances:

- Db2 explicitly shows a sort operation by physical storage location between the index access and the table access (operations 7 and 8).
- Db2 performs a two-phase aggregation: first it does a partial aggregate to reduce the amount of data to be sorted as early as possible (operation 5) then it does a regular `SORT` + `GRPBY`.

#### Oracle

```
---------------------------------------------------------------
|Id |Operation                    | Name        | Rows | Cost |
---------------------------------------------------------------
| 0 |SELECT STATEMENT             |             |   24 |  356 |
| 1 | HASH GROUP BY               |             |   24 |  356 |
| 2 |  TABLE ACCESS BY INDEX ROWID| SALES       |  596 |  355 |
|*3 |   INDEX RANGE SCAN          | SALES_DT_PR |  596 |    4 |
---------------------------------------------------------------
```

Instead, the Oracle database uses the hash algorithm. The advantage of the hash algorithm is that it only needs to buffer the *aggregated result*, whereas the sort/group algorithm materializes the *complete input set*. In other words: the hash algorithm needs less memory.

As with pipelined `order by`, a fast execution is not the most important aspect of the pipelined `group by` execution. It is more important that the database executes it in a pipelined manner and delivers the first result before reading the entire input. This is the prerequisite for the advanced optimization methods explained in the [next chapter](partial-results.md).

> **Think About It:**
>
> Can you think of any other database operation—besides sorting and grouping—that could possibly use an index to avoid sorting?
