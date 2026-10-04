<!-- Source: https://use-the-index-luke.com/sql/explain-plan/db2 — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Working With Execution Plans in Db2 (LUW)

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/db2</sub>

This appendix explains how to work with execution plans in Db2 (LUW). The tools provided by IBM return the execution plan in a pretty awkward format. To get a similar result as for other databases, we introduce our own tool to display execution plans.

## Contents

1. *[Getting](#getting-an-execution-plan)*
2. *[Operations](#db2-luw-execution-plan-operations)*
3. *[Access vs. filter predicates](#distinguishing-access-and-filter-predicates)*


## Getting an Execution Plan

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/db2/getting-an-execution-plan</sub>

Getting an execution plan from Db2 (LUW) is a three step procedure where the first step is a one-time setup.

### Create required tables (once)

The recommended way to create the required explain tables is to call this procedure:

```
CALL SYSPROC.SYSINSTALLOBJECTS('EXPLAIN', 'C',
CAST (NULL AS VARCHAR(128)), CAST (NULL AS VARCHAR(128)))
```

This will install the required tables in the `SYSTOOLS` schema (e.g. `SYSTOOLS.EXPLAIN_STREAM`).

Alternative installation procedures and customizations are explained in the [documentation](https://www.ibm.com/docs/en/db2/11.5.x?topic=sql-explain-tables).

### Explain the statement

Prefix any SQL statement with `explain plan for` to store the execution plan details in the explain tables.

This step does not yet show the execution plan, it just stores it in the database.

```
EXPLAIN PLAN FOR SELECT 1 FROM sysibm.sysdummy1
```

### Display the stored execution plan

IBM provides some [tools to display the data stored in the explain tables](https://www.ibm.com/docs/en/db2/11.5.x?topic=facility-tools-collecting-analyzing-explain-information). However, the output format is not as useful as it could be, so I’m using my own SQL query to display the information I’m usually interested in. The Db2 documentation actually recommends doing this.

Below you find the definition of the view `last_explained` which returns a formatted explain plan for the last statement that was explained for the current user in this database. Please note that it’s scope is not limited to the current session. The view can be used as simple as that:

```
SELECT * FROM last_explained
```

The result might look like this. The [next section](#db2-luw-execution-plan-operations) has the details how to interpret it.

```
Explain Plan
----------------------------------------------
ID | Operation      |             Rows | Cost
 1 | RETURN         |                  |    0
 2 |  TBSCAN GENROW | 1 of 1 (100.00%) |    0

Predicate Information

Explain plan by Markus Winand - NO WARRANTY
http://use-the-index-luke.com/s/last_explained
```

> **Warning:**
>
> This view is highly experimental. It is provided as is without any warranty. It was extended by Ember Crooks to include ActualRows if available. Read [her article](https://datageek.blog/2014/11/18/db2-explain-output-similar-to-other-rdbmses/) to see how to collect them.
>
> The view assumes the explain tables in the `SYSTOOLS` schema. If you are using a different schema, you need to adjust the below view manually.
>
> Minimum requirement: Db2 (LUW) 9.7 FixPack 4

> **Tip:**
>
> The view is available on [GitHub](https://github.com/fatalmind/DB2-last-explained) ([direct download](https://raw.githubusercontent.com/fatalmind/DB2-last-explained/master/last_explained.sql)).

```
-- Copyright (c) 2014-2017, Markus Winand - NO WARRANTY
-- Modifications by Ember Crooks - NO WARRANTY
-- Info & license: http://use-the-index-luke.com/s/last_explained
--
--#SET TERMINATOR ;

CREATE OR REPLACE VIEW last_explained AS
WITH tree(operator_ID, level, path, explain_time, cycle)
AS
(
SELECT 1 operator_id
     , 0 level
     , CAST('001' AS VARCHAR(1000)) path
     , max(explain_time) explain_time
     , 0
  FROM SYSTOOLS.EXPLAIN_OPERATOR O
 WHERE O.EXPLAIN_REQUESTER = SESSION_USER

UNION ALL

SELECT s.source_id
     , level + 1
     , tree.path || '/' || LPAD(CAST(s.source_id AS VARCHAR(3)), 3, '0')  path
     , tree.explain_time
     , POSITION('/' || LPAD(CAST(s.source_id AS VARCHAR(3)), 3, '0')  || '/' IN path USING OCTETS)
  FROM tree
     , SYSTOOLS.EXPLAIN_STREAM S
 WHERE s.target_id    = tree.operator_id
   AND s.explain_time = tree.explain_time
   AND S.Object_Name IS NULL
   AND S.explain_requester = SESSION_USER
   AND tree.cycle = 0
   AND level < 100
)
SELECT *
  FROM (
SELECT "Explain Plan"
  FROM (
SELECT CAST(   LPAD(id,        MAX(LENGTH(id))        OVER(), ' ')
            || ' | '
            || RPAD(operation, MAX(LENGTH(operation)) OVER(), ' ')
            || ' | '
            || LPAD(rows,      MAX(LENGTH(rows))      OVER(), ' ')
            || ' | '
            -- Don't show ActualRows columns if there are no actuals available at all
            || CASE WHEN COUNT(ActualRows) OVER () > 1 -- the heading 'ActualRows' is always present, so "1" means no OTHER values
                    THEN LPAD(ActualRows, MAX(LENGTH(ActualRows)) OVER(), ' ') || ' | '
                    ELSE ''
               END
            || LPAD(cost,      MAX(LENGTH(cost))      OVER(), ' ')
         AS VARCHAR(100)) "Explain Plan"
     , path
  FROM (
SELECT 'ID' ID
     , 'Operation' Operation
     , 'Rows' Rows
     , 'ActualRows' ActualRows
     , 'Cost' Cost
     , '0' Path
  FROM SYSIBM.SYSDUMMY1
-- TODO: UNION ALL yields duplicate. where do they come from?
UNION
SELECT CAST(tree.operator_id as VARCHAR(254)) ID
     , CAST(LPAD(' ', tree.level, ' ')
       || CASE WHEN tree.cycle = 1
               THEN '(cycle) '
               ELSE ''
          END
       || COALESCE (
             TRIM(O.Operator_Type)
          || COALESCE(' (' || argument || ')', '')
          || ' '
          || COALESCE(S.Object_Name,'')
          , ''
          )
       AS VARCHAR(254)) AS OPERATION
     , COALESCE(CAST(rows AS VARCHAR(254)), '') Rows
     , CAST(ActualRows as VARCHAR(254)) ActualRows -- note: no coalesce
     , COALESCE(CAST(CAST(O.Total_Cost AS BIGINT) AS VARCHAR(254)), '') Cost
     , path
  FROM tree
  LEFT JOIN ( SELECT i.source_id
              , i.target_id
              , CAST(CAST(ROUND(o.stream_count) AS BIGINT) AS VARCHAR(12))
                || ' of '
                || CAST (total_rows AS VARCHAR(12))
                || CASE WHEN total_rows > 0
                         AND ROUND(o.stream_count) <= total_rows THEN
                   ' ('
                   || LPAD(CAST (ROUND(ROUND(o.stream_count)/total_rows*100,2)
                          AS NUMERIC(5,2)), 6, ' ')
                   || '%)'
                   ELSE ''
                   END rows
              , CASE WHEN act.actual_value is not null then
                CAST(CAST(ROUND(act.actual_value) AS BIGINT) AS VARCHAR(12))
                || ' of '
                || CAST (total_rows AS VARCHAR(12))
                || CASE WHEN total_rows > 0 THEN
                   ' ('
                   || LPAD(CAST (ROUND(ROUND(act.actual_value)/total_rows*100,2)
                          AS NUMERIC(5,2)), 6, ' ')
                   || '%)'
                   ELSE NULL
                   END END ActualRows
              , i.object_name
              , i.explain_time
         FROM (SELECT MAX(source_id) source_id
                    , target_id
                    , MIN(CAST(ROUND(stream_count,0) AS BIGINT)) total_rows
                    , CAST(LISTAGG(object_name) AS VARCHAR(50)) object_name
                    , explain_time
                 FROM SYSTOOLS.EXPLAIN_STREAM
                WHERE explain_time = (SELECT MAX(explain_time)
                                        FROM SYSTOOLS.EXPLAIN_OPERATOR
                                       WHERE EXPLAIN_REQUESTER = SESSION_USER
                                     )
                GROUP BY target_id, explain_time
              ) I
         LEFT JOIN SYSTOOLS.EXPLAIN_STREAM O
           ON (    I.target_id=o.source_id
               AND I.explain_time = o.explain_time
               AND O.EXPLAIN_REQUESTER = SESSION_USER
              )
         LEFT JOIN SYSTOOLS.EXPLAIN_ACTUALS act
           ON (    act.operator_id  = i.target_id
               AND act.explain_time = i.explain_time
               AND act.explain_requester = SESSION_USER
               AND act.ACTUAL_TYPE  like 'CARDINALITY%'
              )
       ) s
    ON (    s.target_id    = tree.operator_id
        AND s.explain_time = tree.explain_time
       )
  LEFT JOIN SYSTOOLS.EXPLAIN_OPERATOR O
    ON (    o.operator_id  = tree.operator_id
        AND o.explain_time = tree.explain_time
        AND o.explain_requester = SESSION_USER
       )
  LEFT JOIN (SELECT LISTAGG (CASE argument_type
                             WHEN 'UNIQUE' THEN
                                  CASE WHEN argument_value = 'TRUE'
                                       THEN 'UNIQUE'
                                  ELSE NULL
                                  END
                             WHEN 'TRUNCSRT' THEN
                                  CASE WHEN argument_value = 'TRUE'
                                       THEN 'TOP-N'
                                  ELSE NULL
                                  END
                             WHEN 'SCANDIR' THEN
                                  CASE WHEN argument_value != 'FORWARD'
                                       THEN argument_value
                                  ELSE NULL
                                  END
                             ELSE argument_value
                             END
                           , ' ') argument
                  , operator_id
                  , explain_time
               FROM SYSTOOLS.EXPLAIN_ARGUMENT EA
              WHERE argument_type IN ('AGGMODE'   -- GRPBY
                                     , 'UNIQUE', 'TRUNCSRT' -- SORT
                                     , 'SCANDIR' -- IXSCAN, TBSCAN
                                     , 'OUTERJN' -- JOINs
                                     )
                AND explain_requester = SESSION_USER
              GROUP BY explain_time, operator_id

            ) A
    ON (    a.operator_id  = tree.operator_id
        AND a.explain_time = tree.explain_time
       )
     ) O
UNION ALL
VALUES ('Explain plan (c) 2014-2017 by Markus Winand - NO WARRANTY - V20171102','Z0')
    ,  ('Modifications by Ember Crooks - NO WARRANTY','Z1')
    ,  ('http://use-the-index-luke.com/s/last_explained','Z2')
    ,  ('', 'A')
    ,  ('', 'Y')
    ,  ('Predicate Information', 'AA')
UNION ALL
SELECT CAST (LPAD(CASE WHEN operator_id = LAG  (operator_id)
                                          OVER (PARTITION BY operator_id
                                                    ORDER BY pred_order
                                               )
                       THEN ''
                       ELSE operator_id || ' - '
                  END
                , MAX(LENGTH(operator_id )+4) OVER()
                , ' ')
             || how_applied
             || ' '
             || predicate_text
          AS VARCHAR(100)) "Predicate Information"
     , 'P' || LPAD(id_order, 5, '0') || pred_order path
  FROM (SELECT CAST(operator_id AS VARCHAR(254)) operator_id
             , LPAD(trim(how_applied)
                  ,  MAX (LENGTH(TRIM(how_applied)))
                    OVER (PARTITION BY operator_id)
                  , ' '
               ) how_applied
               -- next: capped to length 80 to avoid
               -- SQL0445W  Value "..." has been truncated.  SQLSTATE=01004
               -- error when long literal values may appear (space padded!)
             , CAST(substr(predicate_text, 1, 80) AS VARCHAR(80)) predicate_text
             , CASE how_applied WHEN 'START' THEN '1'
                                WHEN 'STOP'  THEN '2'
                                WHEN 'SARG'  THEN '3'
                                ELSE '9'
               END pred_order
             , operator_id id_order
          FROM systools.explain_predicate p
         WHERE explain_time = (SELECT MAX(explain_time)
                                 FROM systools.explain_operator)
       )
)
ORDER BY path
);
```

TODO:

- Operator ID is currently giving the physical operator ID as stored in the explain tables. It might be more appropriate to display a line number like the Oracle DBMS_XPLAN does so that it is more easy to find a specific ID.
- Indicate the presences of predicates with a asterisk next to the operation ID like Oracle’s DBMS_PLAN does it.

  It might also be good to indicate the presences of filter predicates by using another character (e.g. !).

  Even more sophisticated features would be possible:

  *
  :   START and STOP predicates are present

  >
  :   A START but no STOP predicate is present

  <
  :   A STOP bu no START predicate is present

  !
  :   A SARG predicate is present (additionally, !).
- A heuristic to indicate Top-N clauses: if the number of rows drops for TBSCAN or IXSCAN but there are no predicates, it must be a Top-N limit. This logic is incomplete, of course.


## Db2 (LUW) Execution Plan Operations

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/db2/operations</sub>

A short reference of the most common Db2 (LUW) execution plan operations. Find the full list in the [IBM documentation](https://www.ibm.com/docs/en/db2/11.5.x?topic=tool-operators).

### Index and Table Access

[IXSCAN](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021330.htm)
:   The `IXSCAN` performs the B-tree traversal *and* follows the leaf node chain to find all matching entries. See also [Chapter 1, “*Anatomy of an SQL Index*”](anatomy.md).

    The so-called index filter predicates (“`SARG`” predicates) often cause performance problems for an `IXSCAN`. The [next section](explain-plan-oracle.md#distinguishing-access-and-filter-predicates) explains how to identify them. Similar to Oracle’s family of `INDEX ... SCAN` operations.

    The absence of `START` and `STOP` predicates indicates a full index scan.

    The [`last_explained` view](#getting-an-execution-plan) indicates a reverse scan in brackets (e.g., `IXSCAN (REVERSE)`).

[FETCH](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021323.htm)
:   Retrieves a row from the table using the `RID` retrieved from the preceding index lookup. See also [Chapter 1, “*Anatomy of an SQL Index*”](anatomy.md). Similar to Oracle’s `TABLE ACCESS BY INDEX ROWID`.

[TBSCAN](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021339.htm)
:   This is also known as full table scan. Reads the entire table—all rows and columns—as stored on the disk. Although multi-block read operations improve the speed of a full table scan considerably, it is still one of the most expensive operations. Besides high IO rates, a full table scan must inspect all table rows so it can also consume a considerable amount of CPU time. See also [“*Full Table Scan*”](where-clause-the-equals-operator.md#concatenated-indexes).

[RIDSCN](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021335.htm)
:   This operation is used for [index merge](where-clause-searching-for-ranges.md#index-merge) and, possibly even more often, to prefetch data pages after they have been sorted.

### Joins

Generally join operations process only two tables at a time. In case a query has more joins, they are executed sequentially: first two tables, then the intermediate result with the next table. In the context of joins, the term “table” could therefore also mean “intermediate result”.

[NLJOIN](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021332.htm)
:   Joins two tables by fetching the result from one table and querying the other table for each row from the first. See also [“*Nested Loops*”](join.md#nested-loops).

[HSJOIN](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021327.htm)
:   The hash join loads the candidate records from one side of the join into a hash table that is then probed for each row from the other side of the join. See also [“*Hash Join*”](join.md#hash-join).

[MSJOIN](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021331.htm)
:   The merge join combines two sorted lists like a zipper. Both sides of the join must be presorted. See also [“*Sort Merge*”](join.md#sort-merge).

[ZZJOIN](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0058568.htm)
:   A multi-table join (more than two) specifically for data warehouses using a star schema.

### Sorting and Grouping

[SORT](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021338.htm)
:   Sorts the result according to the `order by` clause. This operation needs large amounts of memory to materialize the intermediate result (not pipelined). This operation is also used to establish a required order for `MSJOIN` or `GRPBY` operations. Additionally, `SORT` might remove duplicate rows for a `distinct` operation. See also [“*Indexing Order By*”](sorting-grouping.md#indexing-order-by).

    The [`last_explained` view](#getting-an-execution-plan) indicates whether a unique is performed in brackets (e.g. `SORT (UNIQUE)`). Top-N sorts are labeled with `TOP-N` (e.g., because of `fetch first ... rows only`).

[UNIQUE](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021338.htm)
:   De-duplicates rows in a pre-sorted set. Used for `distinct` when the required order can be established without a `SORT` operation (e.g., because the `IXSCAN` delivers them in the required order). When a `SORT` operation is necessary, the `SORT` operation itself performs the de-duplication.

[GRPBY](https://www.ibm.com/docs/en/db2/11.5.x?topic=SSEPGG_11.5.0/com.ibm.db2.luw.admin.explain.doc/doc/r0021326.htm)
:   Aggregates a set according the `group by` clause. This operation may be executed using a sort/group-by algorithm or a hash based approach (since v10.1). See also [“*Indexing Group By*”](sorting-grouping.md#indexing-group-by).

    The [`last_explained` view](#getting-an-execution-plan) indicates the aggregation mode in brackets (e.g. `GRPBY (HASH COMPLETE)`).

### Top-N Queries

Db2 (LUW) does not have execution plan operations that directly relate to top-N clauses such as `fetch first ... rows only`. However, if a `SORT` is performed, [`last_explained` view](#getting-an-execution-plan) indicates the top-N optimization in brackets (e.g., `SORT (TOP-N)`).

In case there is no `SORT` operation required, there is no visible mark of the top-n behaviour in the execution plan. However, a sudden drop of the cost value or drop in row count estimates in absence of predicates might give you an idea that there must be a Top-N clause at work.


## Distinguishing Access and Filter-Predicates

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/db2/filter-predicates</sub>

No other database provides better information about the predicate evaluation mode than Db2 because it just says in the execution plan whether a predicate is used as the start and/or stop condition for an [IXSCAN](#db2-luw-execution-plan-operations) or as mere filter predicate. Yet it is confusing because it uses an old definition of the term “*sarg*”.

> In the early days, IBM researchers named these kinds of search conditions “sargable predicates” because SARG is a contraction for Search ARGument. In later days, Microsoft and Sybase redefined “sargable” to mean “can be looked up via the index.”
>
> — [SQL Performance Tuning](https://web.archive.org/web/20241210124531/https://www.informit.com/articles/article.aspx?p=30247)

I find both definitions pretty useless and avoid the term in my books and articles entirely. However, in Db2 execution plans filter predicates are labeled `SARG`—thus we have to make it explicit that IBM Db2 uses the “original” definition as mentioned above. That is, of course, backed by the documentation:

> - *Index sargable* predicates are not used to bracket a search, but are evaluated from the index if one is chosen, because the columns involved in the predicate are part of the index key. […]
> - *Data sargable* predicates […] require the access of individual rows from a base table. If necessary, DMS will retrieve the columns needed to evaluate the predicate, as well as any others to satisfy the columns in the SELECT list that could not be obtained from the index.
>
> — [Predicate processing for queries, Db2 LUW 11.1 documentation](https://www.ibm.com/docs/en/db2/11.5.x?topic=optimization-predicate-processing-queries)

That means, in Db2 predicates labeled `SARG` are generally just filter predicates—either on index level or on table level.

The very nice part about the predicate information shown in Db2 execution plans is that they don’t just label access predicates, but explicitly say which predicates are used as `START` and/or `STOP` conditions.

The following example shows all types of predicates as shown by the [`last_explained` view](#getting-an-execution-plan):

```
Explain Plan
--------------------------------------------------------------
ID | Operation           |                        Rows |  Cost
 1 | RETURN              |                             | 23550
 2 |  GRPBY (COMPLETE)   |        1 of 96480 (   .00%) | 23550
 3 |   IXSCAN SCALE_SLOW | 96480 of 60299800 (   .16%) | 23544

Predicate Information
 3 - START (Q1.SECTION = ?)
      STOP (Q1.SECTION = ?)
      SARG (Q1.ID2 = ?)

Explain plan by Markus Winand - NO WARRANTY
http://use-the-index-luke.com/s/last_explained
```

The scanned index range can be determined quite easily from this output—it’s only determined by the `START` and `STOP` predicates (which happen to be the same in this case). The third predicate on the `ID2` column is labeled `SARG` and thus just a filter predicate.

While `SARG` predicates may appear on other operations too (e.g. `TBSCAN`), `START` and `STOP` are exclusive to `IXSCAN`. The absence of either `START` or `STOP` indicates an search with only upper or lower bound (e.g. `WHERE x > ?`). If neither `START` nor `STOP` appears for an `IXSCAN`, it means that the full index is read.
