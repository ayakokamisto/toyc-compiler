@echo off
type "E:\TOYC\artifacts\independent_audit\audit_cases\p2_fact.tc" | "java" "-jar" "E:\TOYC\toy-c-compiler-master\target\toyc.jar" "-opt" > "E:\TOYC\artifacts\independent_audit\java_output\p2_fact.s" 2> "E:\TOYC\artifacts\independent_audit\java_output\p2_fact.compile.stderr"
