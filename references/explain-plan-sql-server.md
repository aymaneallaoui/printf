<!-- Source: https://use-the-index-luke.com/sql/explain-plan/sql-server — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# SQL Server

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/sql-server</sub>

The method described in this section applies to SQL Server Management Studio 2005 and later.

## Contents

1. *[Getting](#getting-an-execution-plan)*
2. *[Operations](#operations)*
3. *[Access vs. filter predicates](#distinguishing-access-and-filter-predicates)*


## Getting an Execution Plan

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/sql-server/getting-an-execution-plan</sub>

With SQL Server, there are several ways to fetch an execution plan. The two most important methods are:

Graphically
:   The graphical representation of SQL Server execution plans is easily accessible in the Management Studio but is hard to share because the predicate information is only visible when the mouse is moved over the particular operation (“hover”).

Tabular
:   The tabular execution plan is hard to read but easy to copy because it shows all relevant information at once.

### Graphically

The graphical explain plan is generated with one of the two buttons highlighted below.

[[image: mssql_ssms_explain_button.vqQzQh6C.png]](https://use-the-index-luke.com/static/mssql_ssms_explain_button.vqQzQh6C.png)

The left button explains the highlighted statement directly. The right will capture the plan the next time a SQL statement is executed.

In both cases, the graphical representation of the execution plan appears in the “Execution plan” tab of the “Results” pane.

[[image: mssql_ssms_explain.p0Ulm-iw.png]](https://use-the-index-luke.com/static/mssql_ssms_explain.p0Ulm-iw.png)

The graphical representation is easy to read with a little bit of practice. Nonetheless, it only shows the most fundamental information: the operations and the table or index they act upon.

The Management Studio shows more information when moving the mouse over an operation (mouseover/hover). This makes it hard to share an execution plan with all its details.

### Tabular

The tabular representation of an SQL Server execution plan is fetched by profiling the execution of a statement. The following command enables it:

```
SET STATISTICS PROFILE ON
```

Once enabled, each executed statement produces an extra result set. `select` statements, for example, produce two result sets—the result of the statement first then the execution plan.

The tabular execution plan is hardly usable in SQL Server Management Studio because the `StmtText` is just too wide to fit on a screen.

[[image: mssql_ssms_explain_table.6KapJiT5.png]](https://use-the-index-luke.com/static/mssql_ssms_explain_table.6KapJiT5.png)

The advantage of this representation is that it can be copied without loosing relevant information. This is very handy if you want to post an SQL Server execution plan on a forum or similar platform. In this case, it is often enough to copy the `StmtText` column and reformat it a little bit:

```
select COUNT(*) from employees;
  |--Compute Scalar(DEFINE:([Expr1004]=CONVERT_IMPLICIT(...))
       |--Stream Aggregate(DEFINE:([Expr1005]=Count(*)))
            |--Index Scan(OBJECT:([employees].[employees_pk]))
```

Finally, you can disable the profiling again:

```
SET STATISTICS PROFILE OFF
```


## Operations

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/sql-server/operations</sub>

This section explains the most common execution plan operations of Microsoft’s SQL Server database. You can also have a look at [Microsoft’s documentation](https://learn.microsoft.com/en-us/sql/relational-databases/showplan-logical-and-physical-operators-reference).

### Index and Table Access

SQL Server has a simple terminology: “Scan” operations read the entire index or table while “Seek” operations use the B-tree or a physical address (`RID`, like Oracle `ROWID`) to access a specific part of the index or table.

Index Seek, Clustered Index Seek
:   The `Index Seek` performs a B-tree traversal *and* walks through the leaf nodes to find all matching entries. See also [“*Anatomy of an SQL Index*”](anatomy.md).

Index Scan, Clustered Index Scan
:   Reads the entire index—all the rows—in the index order. Depending on various system statistics, the database might perform this operation if it needs all rows in index order—e.g., because of a corresponding `order by` clause.

Key Lookup (Clustered)
:   Retrieves a single row from a clustered index. This is similar to Oracle `INDEX UNIQUE SCAN` for an Index-Organized-Table (IOT). See also [“*Clustering Data: The Second Power of Indexing*”](clustering.md).

RID Lookup (Heap)
:   Retrieves a single row from a table—like Oracle `TABLE ACCESS BY INDEX ROWID`. See also [“*Anatomy of an SQL Index*”](anatomy.md).

Table Scan
:   This is also known as full table scan. Reads the entire table—all rows and columns—as stored on the disk. Although multi-block read operations can improve the speed of a `Table Scan` considerably, it is still one of the most expensive operations. Besides high IO rates, a `Table Scan` must also inspect all table rows so it can also consume a considerable amount of CPU time. See also [“*Full Table Scan*”](where-clause-the-equals-operator.md#concatenated-indexes).

### Join Operations

Generally join operations process only two tables at a time. In case a query has more joins, they are executed sequentially: first two tables, then the intermediate result with the next table. In the context of joins, the term “table” could therefore also mean “intermediate result”.

Nested Loops
:   Joins two tables by fetching the result from one table and querying the other table for each row from the first. SQL Server also uses the nested loops operation to retrieve table data after an index access. See also [“*Nested Loops*”](join.md#nested-loops).

Hash Match
:   The hash match join loads the candidate records from one side of the join into a hash table which is then probed for each row from the other side of the join. See also [“*Hash Join*”](join.md#hash-join).

Merge Join
:   The merge join combines two sorted lists like a zipper. Both sides of the join must be presorted. See also [“*Sort Merge*”](join.md#sort-merge).

### Sorting and Grouping

Sort
:   Sorts the result according to the `order by` clause. This operation needs large amounts of memory to materialize the intermediate result (not pipelined). See also [“*Indexing Order By*”](sorting-grouping.md#indexing-order-by).

Sort (Top N Sort)
:   Sorts a subset of the result according to the `order by` clause. Used for top-N queries if pipelined execution is not possible. See also [“*Querying Top-N Rows*”](partial-results.md#querying-top-n-rows).

Stream Aggregate
:   Aggregates a presorted set according the `group by` clause. This operation does not buffer the intermediate result—it is executed in a pipelined manner. See also [“*Indexing Group By*”](sorting-grouping.md#indexing-group-by).

Hash Match (Aggregate)
:   Groups the result using a hash table. This operation needs large amounts of memory to materialize the intermediate result (not pipelined). The output is not ordered in any meaningful way. See also [“*Indexing Group By*”](sorting-grouping.md#indexing-group-by).

### Top-N Queries

Top
:   Aborts the underlying operations when the desired number of rows has been fetched. See also [“*Querying Top-N Rows*”](partial-results.md#querying-top-n-rows).

    The efficiency of the top-N query depends on the execution mode of the underlying operations. It is very inefficient when aborting non-pipelined operations such as `Sort`.


## Distinguishing Access and Filter-Predicates

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/sql-server/filter-predicates</sub>

The SQL Server database uses three different methods for applying `where` clauses (predicates):

Access Predicate (“Seek Predicates”)
:   The access predicates express the start and stop conditions of the [leaf node traversal](anatomy.md#the-index-leaf-nodes).

Index Filter Predicate (“Predicates” or “where” for index operations)
:   Index filter predicates are applied during the leaf node traversal only. They do not contribute to the start and stop conditions and do not narrow the scanned range.

Table level filter predicate (“where” for table operations)
:   Predicates on columns which are not part of the index are evaluated on the table level. For that to happen, the database must load the row from the heap table first.

The following section explains how to identify filter predicates in [SQL Server execution plans](#getting-an-execution-plan). It is based on the sample used to [demonstrate the impact of index filter predicates](testing-scalability.md#performance-impacts-of-data-volume) in [Chapter 3](testing-scalability.md). The appendix has the [full scripts](example-schema-sql-server.md#sql-server-scripts-for-testing-and-scalability) to populate the table.

```
CREATE TABLE scale_data (
   section NUMERIC NOT NULL,
   id1     NUMERIC NOT NULL,
   id2     NUMERIC NOT NULL
)
```

```
CREATE INDEX scale_slow ON scale_data(section, id1, id2)
```

The sample statement selects by `SECTION` and `ID2`:

```
SELECT count(*)
  FROM scale_data
 WHERE section = @sec
   AND id2 = @id2
```

### In Graphical Execution Plans

The graphical execution plan hides the predicate information in a tooltip that is only shown when moving the mouse over the `Index Seek` operation. Hover over the `Index Seek` icon to see the predicate information—really, on this web-page.

[[image: mssql_ssms_filter.ZrTov2hZ.png]](https://use-the-index-luke.com/static/mssql_ssms_filter.ZrTov2hZ.png)

The SQL Server’s *Seek Predicates* correspond to Oracle’s access predicates—they narrow the leaf node traversal. Filter predicates are just labeled *Predicates* in SQL Server’s graphical execution plan.

### In Tabular Execution Plans

Tabular execution plans have the predicate information in the same column in which the operations appear. It is therefore very easy to copy and past all the relevant information in one go.

```
DECLARE @sec numeric
```

```
DECLARE @id2 numeric
```

```
SET STATISTICS PROFILE ON
```

```
SELECT count(*)
  FROM scale_data
 WHERE section = @sec
   AND id2 = @id2
```

```
SET STATISTICS PROFILE OFF
```

The execution plan is shown as a second result set in the results pane. The following is the `StmtText` column—with a little reformatting for better reading:

```
|--Compute Scalar(DEFINE:([Expr1004]=CONVERT_IMPLICIT(...))
     |--Stream Aggregate(DEFINE:([Expr1008]=Count(*)))
          |--Index Seek(OBJECT:([scale_data].[scale_slow]),
             SEEK: ([scale_data].[section]=[@sec])
                    ORDERED FORWARD
             WHERE:([scale_data].[id2]=[@id2]))
```

The `SEEK` label introduces access predicates, the `WHERE` label marks filter predicates.

> **Tip:**
>
> - The section [“*Greater, Less and `BETWEEN`*”](where-clause-searching-for-ranges.md#greater-less-and-between) explains the difference between access and index filter predicates by example.
> - [Chapter 3, “*Performance and Scalability*”](testing-scalability.md), demonstrates the performance difference access and index filter predicates make.
