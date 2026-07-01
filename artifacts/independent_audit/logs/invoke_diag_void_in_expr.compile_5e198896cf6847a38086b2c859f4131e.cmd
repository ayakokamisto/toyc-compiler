@echo off
type "E:\TOYC\artifacts\independent_audit\audit_cases\diag_void_in_expr.tc" | "java" "-jar" "E:\TOYC\toy-c-compiler-master\target\toyc.jar" "-opt" > "E:\TOYC\artifacts\independent_audit\java_output\diag_void_in_expr.compile.stdout" 2> "E:\TOYC\artifacts\independent_audit\java_output\diag_void_in_expr.compile.stderr"
