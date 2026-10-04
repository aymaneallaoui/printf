<!-- Source: https://use-the-index-luke.com/sql/explain-plan/mysql — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# MySQL

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/mysql</sub>

The method described in this section applies to all versions of MySQL.

http://dev.mysql.com/doc/refman/5.6/en/index-condition-pushdown-optimization.html (aka index filter predicates)

## Contents

1. *[Getting](#getting-an-execution-plan)*
2. *[Operations](#operations)*
3. *[Access vs. filter predicates](#distinguishing-access-and-filter-predicates)*


## Getting an Execution Plan

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/mysql/getting-an-execution-plan</sub>

Put `explain` in front of an SQL statement to retrieve the execution plan.

```
EXPLAIN SELECT 1
```

The plan is shown in tabular form (some less important columns removed):

```
~+-------+------+---------------+------+~+------+------------~
~| table | type | possible_keys | key  |~| rows | Extra
~+-------+------+---------------+------+~+------+------------~
~| NULL  | NULL | NULL          | NULL |~| NULL | No tables...
~+-------+------+---------------+------+~+------+------------~
```

The most important information is in the `TYPE` column. Although the MySQL documentation refers to it as “join type”, I prefer to describe it as “access type” because it actually specifies how the data is accessed. The meaning of the type value is described in the next section.


## Operations

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/mysql/operations</sub>

MySQL reference: [http://dev.mysql.com/doc/refman/5.6/en/explain-output.html](https://dev.mysql.com/doc/refman/8.0/en/explain-output.html)

### Index and Table Access

MySQL’s explain plan tends to give a false sense of safety because it says so much about indexes being used. Although technically correct, it does not mean that it is using the index efficiently. The most important information is in the `TYPE` column of the MySQL’s `explain` output—but even there, the keyword `INDEX` doesn’t indicate proper indexing.

eq_ref, const
:   Performs a B-tree traversal to find *one* row (like `INDEX UNIQUE SCAN`) and fetches additional columns from the table if needed (`TABLE ACCESS BY INDEX ROWID`). The database uses this operation if a primary key or unique constraint ensures that the search criteria will match no more than one entry. See “Using Index” to check whether the table access happens or not.

ref, range
:   Performs a B-tree traversal, walks through the leaf nodes to find all matching index entries (similar to `INDEX RANGE SCAN`) and fetches additional columns from the primary table store if needed (`TABLE ACCESS BY INDEX ROWID`). See “Using Index” to check whether the table access happens or not.

index
:   Reads the entire index—all rows—in the index order (similar to `INDEX FULL SCAN`).

ALL
:   Reads the entire table—all rows and columns—as stored on the disk. Besides high IO rates, a table scan must also inspect all rows from the table so that it can also put a considerable load on the CPU. See also [“*Full Table Scan*”](where-clause-the-equals-operator.md#concatenated-indexes).

Using Index (in the “Extra” column)
:   When the “Extra” column shows “Using Index”, it means that the table is not accessed because the index has all the required data. Think of “using index ONLY”. However, if a clustered index is used (e.g., the `PRIMARY` index when using InnoDB) “Using Index” does not appear in the Extra column although it is technically an Index-Only Scan. See also [“*Clustering Data: The Second Power of Indexing*”](clustering.md).

PRIMARY (in the “key” or “possible_keys” column)
:   `PRIMARY` is the name of the automatically created index for the primary key.

### Sorting and Grouping

using filesort (in the “Extra” column)
:   “using filesort” in the Extra column indicates an explicit sort operation—no matter where the sort takes place (main memory or on disk). “Using filesort” needs large amounts of memory to materialize the intermediate result (not pipelined). See also [“*Indexing Order By*”](sorting-grouping.md#indexing-order-by).

### Top-N Queries

implicit: no “using filesort” in the “Extra” column
:   A MySQL execution plan does not show a top-N query explicitly. If you are using the `limit` syntax and don’t see “using filesort” in the extra column, it is executed in a pipelined manner. See also [“*Querying Top-N Rows*”](partial-results.md#querying-top-n-rows).


## Distinguishing Access and Filter-Predicates

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/mysql/access-filter-predicates</sub>

The MySQL database uses three different ways to evaluate `where` clauses (predicates):

Access predicate (“key_len”, “ref” columns)
:   The access predicates express the start and stop conditions of the [leaf node traversal](anatomy.md#the-index-leaf-nodes).

Index filter predicate (“Using index condition”, since MySQL 5.6)
:   Index filter predicates are applied during the leaf node traversal only. They do not contribute to the start and stop conditions and do not narrow the scanned range.

Table level filter predicate (“Using where” in the “Extra” column)
:   Predicates on columns which are not part of the index are evaluated on the table level. For that to happen, the database must load the row from the table first.

MySQL execution plans do not show which predicate types are used for each condition—they just list the predicate types in use.

In the following example, the entire `where` clause is used as access predicate:

```
CREATE TABLE demo (
   id1 NUMERIC
 , id2 NUMERIC
 , id3 NUMERIC
 , val NUMERIC)
```

```
INSERT INTO demo VALUES (1,1,1,1)
```

```
INSERT INTO demo VALUES (2,2,2,2)
```

```
CREATE INDEX demo_idx
          ON demo
             (id1, id2, id3)
```

```
EXPLAIN
 SELECT *
   FROM demo
  WHERE id1=1
    AND id2=1
```

```
+------+----------+---------+-------------+------+-------+
| type | key      | key_len | ref         | rows | Extra |
+------+----------+---------+-------------+------+-------+
| ref  | demo_idx | 12      | const,const |    1 |       |
+------+----------+---------+-------------+------+-------+
```

There is no “Using where” or “Using index condition” shown in the “Extra” column. The index is, however, used (`type=ref, key=demo_idx`) so you can assume that the entire `where` clause qualifies as access predicate.

Please also note that the `ref` column indicates that two columns are used from the index (both are query constants in this example). Another way to confirm which part of the index is used is the `key_len` value: It shows that the query uses the first 12 bytes of the index definition. To map this to column names, you “just” need to know how much storage space each column needs (see “[Data Type Storage Requirements](https://dev.mysql.com/doc/refman/8.0/en/storage-requirements.html)” in the MySQL documentation). In absence of a `NOT NULL` constraint, MySQL needs an extra byte for each column. After all, each `NUMERIC` column needs 6 bytes in the example. Therefore, the key length of 12 confirms that the first two index columns are used as access predicates.

When filtering with the `ID3` column (instead of the `ID2)` MySQL 5.6 and later use an index filter predicate (“Using index condition”):

```
EXPLAIN
 SELECT *
   FROM demo
  WHERE id1=1
    AND id3=1
```

```
+------+----------+---------+-------+------+-----------------------+
| type | key      | key_len | ref   | rows | Extra                 |
+------+----------+---------+-------+------+-----------------------+
| ref  | demo_idx | 6       | const |    1 | Using index condition |
+------+----------+---------+-------+------+-----------------------+
```

In this case, the `ken_len=6` and only one `const` in the `ref` column means only one index column is used as access predicate.

Previous versions of MySQL used a table level filter predicate for this query—identified by “Using where” in the “Extra” column:

```
+------+----------+---------+-------+------+-------------+
| type | key      | key_len | ref   | rows | Extra       |
+------+----------+---------+-------+------+-------------+
| ref  | demo_idx | 6       | const |    1 | Using where |
+------+----------+---------+-------+------+-------------+
```

> **Tip:**
>
> - The section [“*Greater, Less and `BETWEEN`*”](where-clause-searching-for-ranges.md#greater-less-and-between) explains the difference between access and index filter predicates by example.
> - [Chapter 3, “*Performance and Scalability*”](testing-scalability.md), demonstrates the performance difference access and index filter predicates make.
