# sql-performance skill

A Claude Code / Agent Skill for SQL performance tuning and database indexing. It packages the
complete text of [*Use The Index, Luke!*](https://use-the-index-luke.com/) by Markus Winand as
offline Markdown references, with a `SKILL.md` that turns the book into a diagnostic workflow:
get the execution plan, separate access from filter predicates, classify the symptom, fix the
index or the query, verify.

## Layout

```
SKILL.md          workflow, distilled rules, symptom → reference routing
references/       one Markdown file per chapter (or sub-chapter), every section of the site
```

Every reference file keeps a `Source:` link to the original page. Diagrams are described in
place with a link to the page that shows them.

## Install

Copy or symlink this directory into a skills folder, for example:

```
# project-level
mkdir -p .claude/skills && cp -r /path/to/this/repo .claude/skills/sql-performance

# user-level
mkdir -p ~/.claude/skills && cp -r /path/to/this/repo ~/.claude/skills/sql-performance
```

The skill triggers on slow queries, index design, execution plans, pagination, joins, ORDER BY
and GROUP BY, bulk writes, and related questions for PostgreSQL, MySQL, Oracle, SQL Server,
Db2 and SQLite.

## Coverage

Preface · Anatomy of an Index · The Where Clause (equality, concatenated indexes, functions,
bind parameters, ranges, LIKE, index merge, partial indexes, NULL, obfuscated conditions) ·
Testing and Scalability · The Join Operation · Clustering Data · Sorting and Grouping · Partial
Results · Insert, Delete and Update · Execution Plans for Db2, MySQL, Oracle, PostgreSQL, SQL
Server, SQLite and SQLBase · Myth Directory · Example Schema scripts · Glossary.

## Attribution

The text in `references/` is © Markus Winand, from *Use The Index, Luke! A Guide to Database
Performance for Developers* (https://use-the-index-luke.com), also published as *SQL Performance
Explained*. It is reproduced here for offline reference use by the skill; all rights remain with
the author.
