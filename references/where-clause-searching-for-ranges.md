<!-- Source: https://use-the-index-luke.com/sql/where-clause/searching-for-ranges — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Searching for Ranges

<sub>Source: https://use-the-index-luke.com/sql/where-clause/searching-for-ranges</sub>

Inequality operators such as `<`, `>` and `between` can use indexes just like the equals operator [explained above](where-clause-the-equals-operator.md). Even a `LIKE` filter can—under certain circumstances—use an index just like range conditions do.

Using these operations limits the choice of the column order in multi-column indexes. This limitation can even rule out all optimal indexing options—there are queries where you simply cannot define a “correct” column order at all.

## Contents

1. *[Greater, Less and `BETWEEN`](#greater-less-and-between)* — The column order revisited
2. *[Indexing SQL `LIKE` Filters](#indexing-like-filters)* — `LIKE` is not for full-text search
3. *[Index Combine](#index-merge)* — Why not using one index for every column?


## Greater, Less and BETWEEN

<sub>Source: https://use-the-index-luke.com/sql/where-clause/searching-for-ranges/greater-less-between-tuning-sql-access-filter-predicates</sub>

The biggest performance risk of an `INDEX RANGE SCAN` is the [leaf node traversal](anatomy.md#the-index-leaf-nodes). It is therefore the golden rule of indexing to keep the scanned index range as small as possible. You can check that by asking yourself where an index scan starts and where it ends.

The question is easy to answer if the SQL statement mentions the start and stop conditions explicitly:

```
SELECT first_name, last_name, date_of_birth
  FROM employees
 WHERE date_of_birth >= TO_DATE(?, 'YYYY-MM-DD')
   AND date_of_birth <= TO_DATE(?, 'YYYY-MM-DD')
```

An index on `DATE_OF_BIRTH` is only scanned in the specified range. The scan starts at the first date and ends at the second. We cannot narrow the scanned index range any further.

The start and stop conditions are less obvious if a second column becomes involved:

```
SELECT first_name, last_name, date_of_birth
  FROM employees
 WHERE date_of_birth >= TO_DATE(?, 'YYYY-MM-DD')
   AND date_of_birth <= TO_DATE(?, 'YYYY-MM-DD')
   AND subsidiary_id  = ?
```

Of course an ideal index has to cover both columns, but the question is in which order?

The following figures show the effect of the column order on the scanned index range. For this illustration we search all employees of subsidiary 27 who were born between January 1st and January 9th 1971.

Figure 2.2 visualizes a detail of the index on `DATE_OF_BIRTH` and `SUBSIDIARY_ID`—in that order. Where will the database start to follow the leaf node chain, or to put it another way: where will the [tree traversal](anatomy.md#the-search-tree-b-tree-makes-the-index-fast) end?

*[Figure 2.2 Range Scan in DATE_OF_BIRTH , SUBSIDIARY_ID Index — diagram, see https://use-the-index-luke.com/sql/where-clause/searching-for-ranges/greater-less-between-tuning-sql-access-filter-predicates]*

The index is ordered by birth dates first. Only if two employees were born on the same day is the `SUBSIDIARY_ID` used to sort these records. The query, however, covers a date *range*. The ordering of `SUBSIDIARY_ID` is therefore useless during tree traversal. That becomes obvious if you realize that there is no entry for subsidiary 27 in the branch nodes—although there is one in the leaf nodes. The filter on `DATE_OF_BIRTH` is therefore the only condition that limits the scanned index range. It starts at the first entry matching the date range and ends at the last one—all five leaf nodes shown in Figure 2.2.

The picture looks entirely different when reversing the column order. Figure 2.3 illustrates the scan if the index starts with the `SUBSIDIARY_ID` column.

*[Figure 2.3 Range Scan in SUBSIDIARY_ID , DATE_OF_BIRTH Index — diagram, see https://use-the-index-luke.com/sql/where-clause/searching-for-ranges/greater-less-between-tuning-sql-access-filter-predicates]*

The difference is that the equals operator limits the first index column to a single value. Within the range for this value (`SUBSIDIARY_ID` 27) the index is sorted according to the second column—the date of birth—so there is no need to visit the first leaf node because the branch node already indicates that there is no employee for subsidiary 27 born after June 25th 1969 in the first leaf node.

The tree traversal directly leads to the second leaf node. In this case, all `where` clause conditions limit the scanned index range so that the scan terminates at the very same leaf node.

> **Tip:**
>
> Rule of thumb: index for equality first—then for ranges.

The actual performance difference depends on the data and search criteria. The difference can be negligible if the filter on `DATE_OF_BIRTH` is very selective on its own. The bigger the date range becomes, the bigger the performance difference will be.

With this example, we can also falsify the myth that the most selective column should be at the leftmost index position. If we look at the figures and consider the selectivity of the first column only, we see that both conditions match 13 records. This is the case regardless whether we filter by `DATE_OF_BIRTH` only or by `SUBSIDIARY_ID` only. The selectivity is of no use here, but one column order is still better than the other.

To optimize performance, it is very important to know the scanned index range. With most databases you can even see this in the execution plan—you just have to know what to look for. The following execution plan from the Oracle database unambiguously indicates that the `EMP_TEST` index starts with the `DATE_OF_BIRTH` column.

#### Db2 (LUW)

```
Explain Plan
----------------------------------------------------
ID | Operation         |                 Rows | Cost
 1 | RETURN            |                      |   26
 2 |  FETCH EMPLOYEES  |     3 of 3 (100.00%) |   26
 3 |   IXSCAN EMP_TEST | 3 of 10000 (   .03%) |    6

Predicate Information
 3 - START ( TO_DATE(?, 'YYYY-MM-DD') <= Q1.DATE_OF_BIRTH)
     START (Q1.SUBSIDIARY_ID = ?)
      STOP (Q1.DATE_OF_BIRTH <= TO_DATE(?, 'YYYY-MM-DD'))
      STOP (Q1.SUBSIDIARY_ID = ?)
      SARG (Q1.SUBSIDIARY_ID = ?)
```

In Db2 access predicates are labeled `START` and/or `STOP` while filter predicates are marked shown as `SARG`.

#### Oracle

```
--------------------------------------------------------------
|Id | Operation                    | Name      | Rows | Cost |
--------------------------------------------------------------
| 0 | SELECT STATEMENT             |           |    1 |    4 |
|*1 |  FILTER                      |           |      |      |
| 2 |   TABLE ACCESS BY INDEX ROWID| EMPLOYEES |    1 |    4 |
|*3 |    INDEX RANGE SCAN          | EMP_TEST  |    2 |    2 |
--------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
1 - filter(:END_DT >= :START_DT)
3 - access(DATE_OF_BIRTH >= :START_DT
       AND DATE_OF_BIRTH <= :END_DT)
    filter(SUBSIDIARY_ID  = :SUBS_ID)
```

#### PostgreSQL

```
                            QUERY PLAN
-------------------------------------------------------------------
Index Scan using emp_test on employees
  (cost=0.01..8.59 rows=1 width=16)
  Index Cond: (date_of_birth >= to_date('1971-01-01','YYYY-MM-DD'))
          AND (date_of_birth <= to_date('1971-01-10','YYYY-MM-DD'))
          AND (subsidiary_id = 27::numeric)
```

The PostgreSQL database does not indicate index access and filter predicates in the execution plan. However, the `Index Cond` section lists the columns in order of the index definition. In that case, we see the two `DATE_OF_BIRTH` predicates first, than the `SUBSIDIARY_ID`. Knowing that any predicates following a range condition cannot be an access predicate the `SUBSIDIARY_ID` must be a filter predicate. See [*Distinguishing Access and Filter-Predicates*](explain-plan-postgresql.md#distinguishing-access-and-filter-predicates) for more details.

#### SQL Server

```
|--Nested Loops(Inner Join)
   |--Index Seek(OBJECT:emp_test,
   |               SEEK:       (date_of_birth, subsidiary_id)
   |                        >= ('1971-01-01', 27)
   |                    AND    (date_of_birth, subsidiary_id)
   |                        <= ('1971-01-10', 27),
   |              WHERE:subsidiary_id=27
   |            ORDERED FORWARD)
   |--RID Lookup(OBJECT:employees,
                   SEEK:Bmk1000=Bmk1000
                 LOOKUP ORDERED FORWARD)
```

SQL Server 2012 shows the seek predicates (=access predicates) using the [row-value syntax](partial-results.md#paging-through-results).

The *predicate information* for the `INDEX RANGE SCAN` gives the crucial hint. It identifies the conditions of the `where` clause either as *access* or as *filter* predicates. This is how the database tells us how it uses each condition.

> **Note:**
>
> The execution plan was simplified for clarity. [The appendix](explain-plan-oracle.md#distinguishing-access-and-filter-predicates) explains the details of the “Predicate Information” section in an Oracle execution plan.

The conditions on the `DATE_OF_BIRTH` column are the only ones listed as access predicates; they limit the scanned index range. The `DATE_OF_BIRTH` is therefore the first column in the `EMP_TEST` index. The `SUBSIDIARY_ID` column is used only as a filter.

> **Important:**
>
> The *access predicates* are the start and stop conditions for an index lookup. They define the scanned index range.
>
> *Index filter predicates* are applied during the [leaf node traversal](anatomy.md#the-index-leaf-nodes) only. They do not narrow the scanned index range.
>
> The appendix explains how to recognize access predicates in [MySQL](explain-plan-mysql.md#distinguishing-access-and-filter-predicates), [SQL Server](explain-plan-sql-server.md#distinguishing-access-and-filter-predicates) and [PostgreSQL](explain-plan-postgresql.md#distinguishing-access-and-filter-predicates).

The database can use all conditions as access predicates if we turn the index definition around:

#### Db2 (LUW)

```
-----------------------------------------------------
ID | Operation          |                 Rows | Cost
 1 | RETURN             |                      |   13
 2 |  FETCH EMPLOYEES   |     3 of 3 (100.00%) |   13
 3 |   IXSCAN EMP_TEST2 | 3 of 10000 (   .03%) |    6

Predicate Information
 3 - START (Q1.SUBSIDIARY_ID = ?)
     START ( TO_DATE(?, 'YYYY-MM-DD') <= Q1.DATE_OF_BIRTH)
      STOP (Q1.SUBSIDIARY_ID = ?)
      STOP (Q1.DATE_OF_BIRTH <= TO_DATE(?, 'YYYY-MM-DD'))
```

#### Oracle

```
---------------------------------------------------------------
| Id | Operation                    | Name      | Rows | Cost |
---------------------------------------------------------------
|  0 | SELECT STATEMENT             |           |    1 |    3 |
|* 1 |  FILTER                      |           |      |      |
|  2 |   TABLE ACCESS BY INDEX ROWID| EMPLOYEES |    1 |    3 |
|* 3 |    INDEX RANGE SCAN          | EMP_TEST2 |    1 |    2 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
1 - filter(:END_DT >= :START_DT)
3 - access(SUBSIDIARY_ID  = :SUBS_ID
       AND DATE_OF_BIRTH >= :START_DT
       AND DATE_OF_BIRTH <= :END_T)
```

#### PostgreSQL

```
                            QUERY PLAN
-------------------------------------------------------------------
Index Scan using emp_test on employees
   (cost=0.01..8.29 rows=1 width=17)
   Index Cond: (subsidiary_id = 27::numeric)
           AND (date_of_birth >= to_date('1971-01-01', 'YYYY-MM-DD'))
           AND (date_of_birth <= to_date('1971-01-10', 'YYYY-MM-DD'))
```

The PostgreSQL database does not indicate index access and filter predicates in the execution plan. However, the `Index Cond` section lists the columns in order of the index definition. In that case, we see the `SUBSIDIARY_ID` predicate first, than the two on `DATE_OF_BIRTH`. As there is no further column filtered after the range condition on `DATE_OF_BIRTH` we know that all predicates can be used as access predicate. See [*Distinguishing Access and Filter-Predicates*](explain-plan-postgresql.md#distinguishing-access-and-filter-predicates) for more details.

#### SQL Server

```
|--Nested Loops(Inner Join)
   |--Index Seek(OBJECT:emp_test,
   |               SEEK: subsidiary_id=27
   |                 AND date_of_birth >= '1971-01-01'
   |                 AND date_of_birth <= '1971-01-10'
   |            ORDERED FORWARD)
   |--RID Lookup(OBJECT:employees),
                   SEEK:Bmk1000=Bmk1000
                 LOOKUP ORDERED FORWARD)
```

Finally, there is the `between` operator. It allows you to specify the upper and lower bounds in a single condition:

```
DATE_OF_BIRTH BETWEEN '01-JAN-71'
                  AND '10-JAN-71'
```

Note that `between` always includes the specified values, just like using the less than or equal to (`<=`) and greater than or equal to (`>=`) operators:

```
    DATE_OF_BIRTH >= '01-JAN-71'
AND DATE_OF_BIRTH <= '10-JAN-71'
```


## Indexing LIKE Filters

<sub>Source: https://use-the-index-luke.com/sql/where-clause/searching-for-ranges/like-performance-tuning</sub>

The SQL `LIKE` operator very often causes unexpected performance behavior because some search terms prevent efficient index usage. That means that there are search terms that can be indexed very well, but others can not. It is the position of the wild card characters that makes all the difference.

The following example uses the `%` wild card in the middle of the search term:

```
SELECT first_name, last_name, date_of_birth
  FROM employees
 WHERE UPPER(last_name) LIKE 'WIN%D'
```

#### Db2 (LUW)

```
Explain Plan
----------------------------------------------------
ID | Operation         |                 Rows | Cost
 1 | RETURN            |                      |   13
 2 |  FETCH EMPLOYEES  |     1 of 1 (100.00%) |   13
 3 |   IXSCAN EMP_NAME | 1 of 10000 (   .01%) |    6

Predicate Information
 3 - START ('WIN....................................
      STOP (Q1.LAST_NAME <= 'WIN....................
      SARG (Q1.LAST_NAME LIKE 'WIN%D')
```

For this example, the query was changed to read `WHERE last_name LIKE 'WIN%D'` (no `UPPER`). It seems like Db2 (LUW) 10.5 cannot use an access predicates from `LIKE` on a function-based index (does a full index scan at best).

Otherwise, Db2 shines here: it clearly shows the `START` and `STOP` conditions, which consist of the part before the first wild card, but also shows that the full pattern is applied as filter predicate.

#### MySQL

```
+----+-----------+-------+----------+---------+------+-------------+
| id | table     | type  | key      | key_len | rows | Extra       |
+----+-----------+-------+----------+---------+------+-------------+
|  1 | employees | range | emp_name | 767     |    2 | Using where |
+----+-----------+-------+----------+---------+------+-------------+
```

#### Oracle

```
---------------------------------------------------------------
|Id | Operation                   | Name        | Rows | Cost |
---------------------------------------------------------------
| 0 | SELECT STATEMENT            |             |    1 |    4 |
| 1 |  TABLE ACCESS BY INDEX ROWID| EMPLOYEES   |    1 |    4 |
|*2 |   INDEX RANGE SCAN          | EMP_UP_NAME |    1 |    2 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access(UPPER("LAST_NAME") LIKE 'WIN%D')
       filter(UPPER("LAST_NAME") LIKE 'WIN%D')
```

#### PostgreSQL

```
                       QUERY PLAN
----------------------------------------------------------
Index Scan using emp_up_name on employees
   (cost=0.01..8.29 rows=1 width=17)
   Index Cond: (upper((last_name)::text) ~>=~ 'WIN'::text)
           AND (upper((last_name)::text) ~<~  'WIO'::text)
       Filter: (upper((last_name)::text) ~~ 'WIN%D'::text)
```

`LIKE` filters can only use the characters *before the first wild card* during tree traversal. The remaining characters are just filter predicates that do not narrow the scanned index range. A single `LIKE` expression can therefore contain two predicate types: (1) the part before the first wild card as an access predicate; (2) the other characters as a filter predicate.

> **Caution:**
>
> The `LIKE` operator works on a character-by-character basis while collations can treat multiple characters as a single sorting item. Thus some collations prevent using indexes for `LIKE`. Read [Indexing “LIKE” in PostgreSQL and Oracle](https://www.cybertec-postgresql.com/en/indexing-like-postgresql-oracle/) by Laurenz Albe for further details.

The more selective the prefix before the first wild card is, the smaller the scanned index range becomes. That, in turn, makes the index lookup faster. Figure 2.4 illustrates this relationship using three different `LIKE` expressions. All three select the same row, but the scanned index range—and thus the performance—is very different.

*[Figure 2.4 Various LIKE Searches — diagram, see https://use-the-index-luke.com/sql/where-clause/searching-for-ranges/like-performance-tuning]*

The first expression has two characters before the wild card. They limit the scanned index range to 18 rows. Only one of them matches the entire `LIKE` expression—the other 17 are fetched but discarded. The second expression has a longer prefix that narrows the scanned index range down to two rows. With this expression, the database just reads one extra row that is not relevant for the result. The last expression does not have a filter predicate at all: the database just reads the entry that matches the entire `LIKE` expression.

> **Important:**
>
> Only the part before the first wild card serves as an access predicate.
>
> The remaining characters do not narrow the scanned index range—non-matching entries are just left out of the result.

The opposite case is also possible: a `LIKE` expression that starts with a wild card. Such a `LIKE` expression cannot serve as an access predicate. The database has to scan the entire table if there are no other conditions that provide access predicates.

> **Tip:**
>
> Avoid `LIKE` expressions with leading wildcards (e.g., `'%TERM'`).

The position of the wild card characters affects index usage—at least in theory. In reality the optimizer creates a generic execution plan when the search term is supplied via [bind parameters](where-clause-bind-parameters.md). In that case, the optimizer has to guess whether or not the majority of executions will have a leading wild card.

Most databases just assume that there is no leading wild card when optimizing a `LIKE` condition with bind parameter, but this assumption is wrong if the `LIKE` expression is used for a full-text search. There is, unfortunately, no direct way to tag a `LIKE` condition as full-text search. The box “*Labeling Full-Text `LIKE` Expressions*” shows an attempt that does not work. Specifying the search term without bind parameter is the most obvious solution, but that increases the optimization overhead and opens an SQL injection vulnerability. An effective but still secure and portable solution is to intentionally obfuscate the `LIKE` condition. [“*Combining Columns*”](where-clause-obfuscation.md#combining-columns) explains this in detail.

> **Sidebar — Labeling Full-Text LIKE Expressions**
>
> When using the `LIKE` operator for a full-text search, we could separate the wildcards from the search term:
>
> ```
> WHERE text_column LIKE '%' || ? || '%'
> ```

For the PostgreSQL database, the problem is different because PostgreSQL assumes there *is* a leading wild card when using bind parameters for a `LIKE` expression. PostgreSQL just does not use an index in that case. The only way to get an index access for a `LIKE` expression is to make the actual search term visible to the optimizer. If you do not use a bind parameter but put the search term directly into the SQL statement, you must take other precautions against SQL injection attacks!

Even if the database optimizes the execution plan for a leading wild card, it can still deliver insufficient performance. You can use another part of the `where` clause to access the data efficiently in that case—see also [“*Index Filter Predicates Used Intentionally*”](clustering.md#index-filter-predicates-used-intentionally). If there is no other access path, you might use one of the following proprietary full-text index solutions.

#### Db2 (LUW)

Db2 supports the `contains` keyword. See “[Search functions for Db2 Text Search](https://www.ibm.com/docs/en/db2/11.5.x?topic=indexes-search-functions)“.

#### MySQL

MySQL offers the `match` and `against` keywords for full-text searching. Starting with MySQL 5.6, you can create full-text indexes for InnoDB tables as well—previously, this was only possible with MyISAM tables. See “[Full-Text Search Functions](https://dev.mysql.com/doc/refman/8.0/en/fulltext-search.html)” in the MySQL documentation.

#### Oracle

The Oracle database offers the `contains` keyword. See the “[Oracle Text Application Developer’s Guide](https://docs.oracle.com/en/database/oracle/oracle-database/19/ccapp/#Oracle%C2%AE-Text).”

#### PostgreSQL

PostgreSQL offers the `@@` operator to implement full-text searches. See “[Full Text Search](https://www.postgresql.org/docs/current/textsearch.html)” in the PostgreSQL documentation.

Another option is to use the [WildSpeed](http://www.sai.msu.su/~megera/wiki/wildspeed) extension to optimize `LIKE` expressions directly. The extension stores the text in all possible rotations so that each character is at the beginning once. That means that the indexed text is not only stored once but instead as many times as there are characters in the string—thus it needs a lot of space.

#### SQL Server

SQL Server offers the `contains` keyword. See “[Full-Text Search](https://learn.microsoft.com/en-us/sql/relational-databases/search/full-text-search?view=sql-server-ver16)” in the SQL Server documentation.

> **Think About It:**
>
> How can you index a `LIKE` search that has only one wild card at the beginning of the search term (`'%TERM'`)?


## Index Merge

<sub>Source: https://use-the-index-luke.com/sql/where-clause/searching-for-ranges/index-merge-performance</sub>

It is one of the most common question about indexing: is it better to create one index for each column or a single index for all columns of a `where` clause? The answer is very simple in most cases: one index with multiple columns is better—that is, a concatenated or compound index. [“*Concatenated Indexes*”](where-clause-the-equals-operator.md#concatenated-indexes) explains them in detail.

Nevertheless there are queries where a single index cannot do a perfect job, no matter how you define the index; e.g., queries with two or more independent range conditions as in the following example:

```
SELECT first_name, last_name, date_of_birth
  FROM employees
 WHERE UPPER(last_name) < ?
   AND date_of_birth    < ?
```

It is impossible to define a B-tree index that would support this query without filter predicates. For an explanation, you just need to remember that an [index is a linked list](anatomy.md).

If you define the index as `UPPER(LAST_NAME)`, `DATE_OF_BIRTH` (in that order), the list begins with A and ends with Z. The date of birth is considered only when there are two employees with the same name. If you define the index the other way around, it will start with the eldest employees and end with the youngest. In that case, the names only have a minor impact on the sort order.

No matter how you twist and turn the index definition, the entries are always arranged along a chain. At one end, you have the small entries and at the other end the big ones. An index can therefore only support one range condition as an access predicate. Supporting two independent range conditions requires a second axis, for example like a chessboard. The query above would then match all entries from one corner of the chessboard, but an index is not like a chessboard—it is like a chain. There is no corner.

You can of course accept the filter predicate and use a multi-column index nevertheless. That is the best solution in many cases anyway. The index definition should then mention the more selective column first so it can be used with an access predicate. That might be the origin of the “[most selective first](myth-directory.md#most-selective-first)” myth but this rule only holds true if you cannot avoid a filter predicate.

The other option is to use two separate indexes, one for each column. Then the database must scan both indexes first and then combine the results. The duplicate index lookup alone already involves more effort because the database has to traverse two index trees. Additionally, the database needs a lot of memory and CPU time to combine the intermediate results.

> **Note:**
>
> One index scan is faster than two.

Databases use two methods to combine indexes. Firstly there is the index join. [Chapter 4*The Join Operation*](join.md) explains the related algorithms in detail. The second approach makes use of functionality from the data warehouse world.

The [data warehouse](https://en.wikipedia.org/wiki/Data_warehouse) is the mother of all ad-hoc queries. It just needs a few clicks to combine arbitrary conditions into the query of your choice. It is impossible to predict the column combinations that might appear in the `where` clause and that makes indexing, as explained so far, almost impossible.

Data warehouses use a special purpose index type to solve that problem: the so-called *bitmap index*. The advantage of bitmap indexes is that they can be combined rather easily. That means you get decent performance when indexing each column individually. Conversely if you know the query in advance, so that you can create a tailored multi-column B-tree index, it will still be faster than combining multiple bitmap indexes.

By far the greatest weakness of bitmap indexes is the ridiculous `insert`, `update` and `delete` scalability. Concurrent write operations are virtually impossible. That is no problem in a data warehouse because the load processes are scheduled one after another. In online applications, bitmap indexes are mostly useless.

> **Important:**
>
> Bitmap indexes are almost unusable for online transaction pro­cessing (OLTP).

Many database products offer a hybrid solution between B-tree and bitmap indexes. In the absence of a better access path, they convert the results of several B-tree scans into in-memory bitmap structures. Those can be combined efficiently. The bitmap structures are not stored persistently but discarded after statement execution, thus bypassing the problem of the poor write scalability. The downside is that it needs a lot of memory and CPU time. This method is, after all, an optimizer’s act of desperation.
