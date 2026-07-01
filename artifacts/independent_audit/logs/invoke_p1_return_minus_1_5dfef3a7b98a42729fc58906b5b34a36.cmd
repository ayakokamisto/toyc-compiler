@echo off
type "E:\TOYC\artifacts\independent_audit\audit_cases\p1_return_minus_1.tc" | "java" "-jar" "E:\TOYC\toy-c-compiler-master\target\toyc.jar" "-opt" > "E:\TOYC\artifacts\independent_audit\java_output\p1_return_minus_1.s" 2> "E:\TOYC\artifacts\independent_audit\java_output\p1_return_minus_1.compile.stderr"
