-- SQLite RV32I Comprehensive SQL Test Suite
-- 1. Table Creation & Basic Datatypes
CREATE TABLE t1 (id INT PRIMARY KEY, name TEXT, salary REAL, active INT);
INSERT INTO t1 VALUES (1, 'Alice', 105000.50, 1);
INSERT INTO t1 VALUES (2, 'Bob', 85000.25, 0);
INSERT INTO t1 VALUES (3, 'Charlie', 95000.75, 1);
INSERT INTO t1 VALUES (4, 'David', 120000.00, 1);
INSERT INTO t1 VALUES (5, 'Eve', 70000.00, 0);

-- 2. Select & Filtering
SELECT 'SELECT_FILTER', name, salary FROM t1 WHERE active = 1 AND salary > 90000.0 ORDER BY salary DESC;

-- 3. Aggregates & Group By
CREATE TABLE dept (id INT, dept_name TEXT);
INSERT INTO dept VALUES (1, 'Engineering');
INSERT INTO dept VALUES (2, 'Sales');

CREATE TABLE emp_dept (emp_id INT, dept_id INT);
INSERT INTO emp_dept VALUES (1, 1);
INSERT INTO emp_dept VALUES (2, 2);
INSERT INTO emp_dept VALUES (3, 1);
INSERT INTO emp_dept VALUES (4, 1);
INSERT INTO emp_dept VALUES (5, 2);

SELECT 'JOIN_AGGREGATE', d.dept_name, COUNT(e.id), AVG(e.salary), MIN(e.salary), MAX(e.salary)
FROM dept d, t1 e, emp_dept ed
WHERE d.id = ed.dept_id AND e.id = ed.emp_id
GROUP BY d.dept_name
ORDER BY d.dept_name;

-- 4. Updates & Deletes
UPDATE t1 SET salary = salary * 1.10 WHERE name = 'Eve';
SELECT 'AFTER_UPDATE', name, salary FROM t1 WHERE name = 'Eve';

DELETE FROM t1 WHERE name = 'Bob';
SELECT 'AFTER_DELETE', COUNT(*) FROM t1;

-- 5. Views & Triggers
CREATE VIEW v_active AS SELECT name, salary FROM t1 WHERE active = 1;
SELECT 'VIEW_SELECT', name, salary FROM v_active ORDER BY name;

CREATE TABLE log (msg TEXT);
CREATE TRIGGER t1_log AFTER INSERT ON t1
BEGIN
    INSERT INTO log VALUES ('Inserted new employee: ' || new.name);
END;

INSERT INTO t1 VALUES (6, 'Frank', 65000.0, 1);
SELECT 'TRIGGER_LOG', msg FROM log;

-- 6. String functions & Expressions
SELECT 'STRING_MATH', UPPER(name), LENGTH(name), salary / 1000.0 FROM t1 WHERE id = 1;

-- 7. Transactions
BEGIN TRANSACTION;
INSERT INTO t1 VALUES (7, 'Grace', 99000.0, 1);
ROLLBACK;
SELECT 'TRANSACTION_ROLLBACK', COUNT(*) FROM t1 WHERE id = 7;
