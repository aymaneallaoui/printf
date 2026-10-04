---
name: sql-performance
description: SQL performance tuning and database indexing, based on the complete text of "Use The Index, Luke!" by Markus Winand (use-the-index-luke.com). Use this skill whenever a query is slow, an index is "not being used", an execution plan needs reading (EXPLAIN / EXPLAIN ANALYZE / showplan), someone is designing or reviewing indexes, choosing column order for a multi-column index, writing WHERE clauses, LIKE searches, ORDER BY / GROUP BY, pagination (OFFSET vs keyset), top-N queries, joins (nested loops, hash, sort-merge, N+1), bulk INSERT/UPDATE/DELETE throughput, or any schema or ORM code where database performance matters. Trigger even when the user does not say "performance" but is adding an index, asking why a query is slow in production but fast in test, asking about covering indexes, index-only scans, clustered indexes, partial indexes, function-based indexes, NULL and indexes, bind parameters, or SQL anti-patterns. Covers PostgreSQL, MySQL, Oracle, SQL Server, Db2, SQLite.
---

# SQL Performance (Use The Index, Luke!)

This skill is the full "Use The Index, Luke!" book turned into a working method. The one-sentence
thesis: **SQL performance is an indexing problem, and indexing is the developer's job**, because
only the developer knows which columns the application filters, joins, sorts and pages on. The
database cannot pick an index for a query that was never written with indexes in mind.

Everything below is a distillation. The complete chapters live in `references/` and are the
authority; read the relevant file before giving a non-trivial recommendation, and quote the
execution-plan evidence rather than reasoning from folklore.

## Workflow for a slow query

1. **Get the real execution plan** for the exact statement, on the database that is slow, with
   production-like data volume. Guessing from the SQL text is the most common mistake.
   Per-database instructions: `references/explain-plan-<db>.md` (see table below).
2. **Find the table access operation and its predicates.** In the plan, separate
   *access predicates* (used to navigate the B-tree: start/stop of the leaf-node scan) from
   *filter predicates* (applied to every row after the tree traversal, or after fetching the
   table row). A filter predicate on an index is a hidden full scan of part of the index. This
   single distinction explains most "the index is used but it is still slow" cases.
   `references/explain-plan-<db>.md` → "Distinguishing Access and Filter-Predicates".
3. **Classify the problem** with the symptom table below and read the matching reference.
4. **Fix the index or the query**, preferring: (a) a concatenated index whose column order
   serves the most queries, (b) rewriting the condition so it is index-friendly (no wrapped
   columns, no obfuscated dates, no `OR`-based "smart logic"), (c) only then adding a new index,
   because every index slows down every write (`references/dml.md`).
5. **Verify** by re-reading the plan: the predicate moved from filter to access, row estimates
   are sane, the sort or hash operation disappeared, the table access is gone (index-only), or
   the plan is pipelined for a top-N / pagination query. Then test with realistic data volume
   and concurrency (`references/testing-scalability.md`), because an index that works on 1,000
   rows can be worthless on 10,000,000.

## Rules that resolve most cases

**Index anatomy (`references/anatomy.md`)**
- An index is a B-tree of branch nodes over a doubly-linked list of leaf nodes that point to
  table rows. A lookup costs three steps: tree traversal (fast, logarithmic), leaf-node chain
  walk (can be long), and table access per matching row (random I/O, usually the slow part).
- "Slow index" almost always means a long leaf-node walk plus many table accesses, not a
  "degenerated" tree. Rebuilding indexes rarely helps (`references/myth-directory.md`).
- An `INDEX UNIQUE SCAN` is the ideal; an `INDEX RANGE SCAN` can scan a huge range; a
  `TABLE ACCESS BY INDEX ROWID` for many rows can be slower than a full table scan.

**Equality and concatenated indexes (`references/where-clause-the-equals-operator.md`)**
- A multi-column index is usable only from its leftmost columns, like a phone book sorted by
  last name then first name: `(a, b)` serves `WHERE a = ?` and `WHERE a = ? AND b = ?` but not
  `WHERE b = ?` alone.
- Column order is about **reuse across queries**, not about "most selective first" (that is a
  myth). Put columns that appear alone in some queries first, so one index covers more
  statements. Fewer, wider indexes beat many narrow ones.
- Verify the plan. Unused indexes and index ranges that match thousands of rows are the two
  ingredients of "slow indexes".

**Functions in WHERE (`references/where-clause-functions.md`)**
- Any expression wrapping an indexed column (`UPPER(name)`, `TRUNC(date)`, `col + 1`,
  implicit type casts) disables a plain index on that column. Create a function-based /
  expression index on the exact expression, or move the computation to the literal side.
- Expression indexes need deterministic functions; `SYSDATE`/`NOW()` in the expression cannot
  be indexed. Rewrite the condition as a range on the raw column instead.
- Avoid over-indexing: do not create both `(name)` and `(UPPER(name))` if the app only queries
  one way.

**Bind parameters (`references/where-clause-bind-parameters.md`)**
- Use bind parameters by default: they prevent SQL injection and allow plan caching. The only
  reason to inline a literal is a column with badly skewed data where the optimizer needs the
  value to choose a different plan (histograms). Never concatenate user input.

**Ranges, LIKE and index merges (`references/where-clause-searching-for-ranges.md`)**
- In a concatenated index, put **equality columns first, range column last**. A range condition
  on a leading column turns later columns into filter predicates: `WHERE date BETWEEN ... AND
  status = ?` wants `(status, date)`, not `(date, status)`.
- `LIKE 'TERM%'` is an access predicate; `LIKE '%TERM%'` and `LIKE '%TERM'` scan the whole
  index (filter predicate). Only the part before the first wildcard navigates the tree. For
  real text search use the database's full-text features, not `LIKE`.
- One multi-column index normally beats merging several single-column indexes (bitmap
  index merge / index AND). Index merges are a fallback for truly dynamic ad-hoc filters.

**Partial / filtered indexes (`references/where-clause-partial-and-filtered-indexes.md`)**
- Index only the rows that matter (`WHERE processed = 'N'`) to keep hot indexes tiny. Oracle
  emulates this with a function-based index that returns `NULL` for unwanted rows
  (`references/where-clause-null.md`).

**NULL (`references/where-clause-null.md`)**
- Oracle does not index rows where all indexed columns are `NULL`, so `WHERE col IS NULL`
  cannot use a single-column index unless another indexed column is `NOT NULL` (add a constant
  or a `NOT NULL` column to the index). PostgreSQL, SQL Server, MySQL and Db2 index `NULL`.
- `NOT NULL` constraints matter to the optimizer: without them it may refuse an index-only
  `COUNT(*)` or `IS NULL` plan.

**Obfuscated conditions (`references/where-clause-obfuscation.md`)**
- Dates: never `TRUNC(date_col) = ?` or `TO_CHAR(date_col, ...) = ?`; use a half-open range
  `date_col >= :start AND date_col < :end`. Beware `LIKE` on dates and implicit string→date
  conversions.
- Numeric strings: `WHERE numeric_string_col = 42` casts the column, not the literal; compare
  as strings or fix the type.
- Combined columns (`date || time`, `first || ' ' || last`): index the combination or rewrite as
  row-value / range comparison.
- "Smart logic" (`WHERE (:p1 IS NULL OR col1 = :p1) AND (:p2 IS NULL OR col2 = :p2)`) defeats
  every index because one plan must serve all parameter combinations. Build the SQL
  dynamically with only the predicates that are set, with bind parameters. Dynamic SQL is not
  slow (`references/myth-directory.md`).
- Math (`col * 2 = ?`, `col - 1000 > ?`): isolate the column on one side, or use an expression
  index.

**Joins (`references/join.md`)**
- Nested loops: index the join column of the *inner* (driven) table; this is also the fix for
  the ORM N+1 problem (use eager fetching / `JOIN FETCH`, not one query per row).
- Hash join: index only the independent `WHERE` predicates on each side, not the join
  columns; reduce the hash-table size by selecting fewer columns.
- Sort-merge join: both sides sorted; rare, good for large inputs, indexes on join columns
  can avoid the sort.
- Mind the optimizer's join order; stale statistics give wrong row estimates and wrong plans.

**Clustering and index-only scans (`references/clustering.md`)**
- Table access is the slow part. Add the selected columns to the index (covering index /
  `INCLUDE`) so the query becomes an *index-only scan* and never touches the table. Watch for
  `SELECT *` and functions that force table access.
- A filter predicate on an index column is sometimes **intentional** and good: it avoids table
  access for the rows it filters out.
- Index-organized tables / clustered indexes (InnoDB, SQL Server default) store the table in
  the primary-key B-tree. Secondary indexes then do two B-tree lookups, and a wide or random
  clustering key hurts every secondary index. Keep clustering keys small and sequential.

**Sorting and grouping (`references/sorting-grouping.md`)**
- An index in the right order lets the database return rows already sorted (*pipelined*
  `ORDER BY`) with no sort step. The index must match the `WHERE` equality columns first,
  then the `ORDER BY` columns in order.
- Mixed `ASC`/`DESC` or `NULLS FIRST/LAST` needs an index declared the same way (or the
  mirrored way, because indexes read backwards).
- `GROUP BY` on index order is a pipelined sort-group; otherwise a hash-group that needs the
  whole input first.

**Partial results, top-N, pagination (`references/partial-results.md`)**
- Top-N (`FETCH FIRST n ROWS ONLY`, `LIMIT n`, `TOP n`, `ROWNUM <= n`) is only fast if the
  plan is pipelined via a matching index; then it stops after n rows. Check the plan for a
  `STOPKEY` / `Limit` directly on the index scan with no sort below it.
- `OFFSET` pagination reads and discards all previous pages; page 1,000 is 1,000 times slower
  than page 1 and shifts under concurrent inserts. Use **keyset (seek) pagination**:
  `WHERE (sort_col, id) < (:last_sort_col, :last_id) ORDER BY sort_col DESC, id DESC FETCH
  FIRST n ROWS ONLY`, with an index on `(sort_col, id)`.
- Window functions (`ROW_NUMBER()`) for paging are only efficient if the optimizer
  recognizes the stop condition; keyset is more portable.

**Writes (`references/dml.md`)**
- Every index is maintained on every `INSERT`, on every `DELETE`, and on `UPDATE` of its
  columns. Insert throughput drops roughly in proportion to the number of indexes. Drop
  indexes nobody queries; for bulk loads consider dropping and rebuilding; update only changed
  columns (ORMs that write all columns touch all indexes).

**Testing and scalability (`references/testing-scalability.md`)**
- Response time grows with data volume for filter-predicate plans and stays flat for access-
  predicate plans. Test with realistic volume, realistic distribution, and under load.
- More hardware or horizontal scaling improves throughput, not the response time of a single
  badly indexed query.

## Symptom → where to read

| Symptom in the plan or the complaint | Read |
| --- | --- |
| "Index exists but the plan uses a full table scan" | `where-clause-the-equals-operator.md`, `where-clause-functions.md`, `where-clause-obfuscation.md` |
| Index range scan returning thousands of rows, then table access | `anatomy.md` (Slow Indexes I), `where-clause-the-equals-operator.md` (Slow Indexes II) |
| Predicate shows as *filter* not *access* on the index | `explain-plan-<db>.md`, `where-clause-searching-for-ranges.md`, `clustering.md` |
| Range or `BETWEEN` plus equality on a multi-column index | `where-clause-searching-for-ranges.md` |
| `LIKE '%x%'` or search-as-you-type | `where-clause-searching-for-ranges.md` (Indexing LIKE Filters) |
| Case-insensitive search, `UPPER`/`LOWER`, `TRUNC(date)`, casts | `where-clause-functions.md`, `where-clause-obfuscation.md` |
| Many optional filters with `OR :p IS NULL` | `where-clause-obfuscation.md` (Smart Logic), `myth-directory.md` (Dynamic SQL) |
| `IS NULL` not using an index (Oracle) | `where-clause-null.md` |
| Hot subset of rows (queue, unprocessed flag) | `where-clause-partial-and-filtered-indexes.md` |
| Sort operation in the plan under `ORDER BY` or `GROUP BY` | `sorting-grouping.md` |
| `LIMIT`/`TOP`/`FETCH FIRST` still reads everything | `partial-results.md` (Top-N) |
| Later pages slow, `OFFSET` large | `partial-results.md` (keyset pagination) |
| Join slow, N+1 queries from ORM, hash join memory | `join.md` |
| Still fetching from table although all columns are indexed | `clustering.md` (Index-Only Scan) |
| InnoDB / SQL Server secondary index slower than expected | `clustering.md` (Index-Organized Tables) |
| Inserts or bulk load slow | `dml.md` |
| Fast in dev, slow in production | `testing-scalability.md` |
| "Rebuild the index", "most selective column first", "dynamic SQL is slow" | `myth-directory.md` |
| Unknown term (clustering factor, heap table, covering index) | `glossary.md` |

Replace `<db>` with `postgresql`, `mysql`, `oracle`, `sql-server`, `db2`, `sqlite`, or
`sqlbase`. Example queries in the references use the `EMPLOYEES` / `SALES` tables; the scripts that
create and fill them are in `references/example-schema-<db>.md`.

## Getting an execution plan (quick reference)

Full details, including how to make each database show access vs. filter predicates, are in
`references/explain-plan-<db>.md`.

| Database | Command |
| --- | --- |
| PostgreSQL | `EXPLAIN (ANALYZE, BUFFERS) <query>` — "Index Cond" = access predicate, "Filter" = filter predicate |
| MySQL / MariaDB | `EXPLAIN <query>` or `EXPLAIN FORMAT=JSON`; `type` = `ref`/`range`/`index`/`ALL`, `Extra` shows `Using index`, `Using where`, `Using filesort` |
| Oracle | `EXPLAIN PLAN FOR <query>; SELECT * FROM TABLE(dbms_xplan.display)` — read `access(...)` vs `filter(...)` in Predicate Information |
| SQL Server | `SET STATISTICS PROFILE ON` before the query, or "Include Actual Execution Plan" in SSMS; `Seek Predicates` = access, `Predicate` = filter |
| Db2 (LUW) | `EXPLAIN PLAN FOR <query>` then `SELECT * FROM last_explained` (view defined in the reference); `START`/`STOP` = access, `SARG` = filter |
| SQLite | `EXPLAIN QUERY PLAN <query>`; `SEARCH ... USING INDEX` vs `SCAN` |

## How to answer

- State what the plan shows (operation, access vs. filter predicates, row counts) before
  proposing a change.
- Give the concrete `CREATE INDEX` or rewritten SQL, say which queries it serves and which
  existing index it can replace, and name the write cost.
- Prefer one well-ordered concatenated index over several single-column indexes.
- When reviewing ORM or application code, look for the patterns above in generated SQL: N+1
  loops, `OFFSET` paging, smart-logic filters, functions on columns, wildcard-leading `LIKE`,
  `SELECT *` blocking index-only scans, literals instead of bind parameters.
- Keep database-specific syntax straight (`FETCH FIRST` vs `LIMIT` vs `TOP`, `INCLUDE`
  columns, partial-index support, NULL handling). The per-database sections of each reference
  spell out the differences.

## Reference files

| File | Book chapter |
| --- | --- |
| `references/preface.md` | Preface: Developers Need to Index |
| `references/anatomy.md` | 1. Anatomy of an SQL Index: leaf nodes, B-tree, Slow Indexes Part I |
| `references/where-clause.md` | 2. The Where Clause (overview) |
| `references/where-clause-the-equals-operator.md` | 2.1 Equality: primary keys, concatenated indexes, Slow Indexes Part II |
| `references/where-clause-functions.md` | 2.2 Functions: case-insensitive search, user-defined functions, over-indexing |
| `references/where-clause-bind-parameters.md` | 2.3 Parameterized queries (bind variables), with code in C#, Java, Perl, PHP and Ruby |
| `references/where-clause-searching-for-ranges.md` | 2.4 Ranges: greater/less/BETWEEN, LIKE, index merge |
| `references/where-clause-partial-and-filtered-indexes.md` | 2.5 Partial indexes |
| `references/where-clause-null.md` | 2.6 NULL in the Oracle database: indexing NULL, NOT NULL constraints, emulating partial indexes |
| `references/where-clause-obfuscation.md` | 2.7 Obfuscated conditions: dates, numeric strings, combining columns, smart logic, math |
| `references/testing-scalability.md` | 3. Performance and Scalability: data volume, system load, response time vs. throughput |
| `references/join.md` | 4. The Join Operation: nested loops, hash join, sort-merge |
| `references/clustering.md` | 5. Clustering Data: intentional filter predicates, index-only scan, index-organized tables |
| `references/sorting-grouping.md` | 6. Sorting and Grouping: indexed ORDER BY, ASC/DESC/NULLS FIRST/LAST, indexed GROUP BY |
| `references/partial-results.md` | 7. Partial Results: top-N, next page (keyset), window functions |
| `references/dml.md` | 8. Modifying Data: insert, delete, update |
| `references/explain-plan.md` | Appendix A: Execution Plans (overview) |
| `references/explain-plan-postgresql.md` | A. PostgreSQL: getting plans, operations, access vs. filter predicates |
| `references/explain-plan-mysql.md` | A. MySQL |
| `references/explain-plan-oracle.md` | A. Oracle |
| `references/explain-plan-sql-server.md` | A. SQL Server |
| `references/explain-plan-db2.md` | A. Db2 (LUW) |
| `references/explain-plan-sqlite.md` | A. SQLite |
| `references/explain-plan-sqlbase.md` | A. Gupta SQLBase |
| `references/myth-directory.md` | Myth Directory: indexes degenerate, most selective first, Oracle cannot index NULL, dynamic SQL is slow |
| `references/example-schema.md` | Example schema overview |
| `references/example-schema-<db>.md` | CREATE/INSERT scripts per database (`db2`, `mysql`, `oracle`, `postgresql`, `sql-server`, `sqlite`, `sqlbase`) for each chapter's examples |
| `references/glossary.md` | Glossary of terms |

Each reference file carries `Source:` links to the original pages. Figures are described in
place with a link to the page that shows the diagram.

## Attribution

Content in `references/` is the text of "Use The Index, Luke! A Guide to Database Performance
for Developers" by Markus Winand, https://use-the-index-luke.com, converted to Markdown for
offline use. Copyright remains with the author; the book is also published commercially as
"SQL Performance Explained".
