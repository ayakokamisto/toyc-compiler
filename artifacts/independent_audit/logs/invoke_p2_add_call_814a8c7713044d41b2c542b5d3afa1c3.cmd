@echo off
type "E:\TOYC\artifacts\independent_audit\audit_cases\p2_add_call.tc" | "java" "-jar" "E:\TOYC\toy-c-compiler-master\target\toyc.jar" "-opt" > "E:\TOYC\artifacts\independent_audit\java_output\p2_add_call.s" 2> "E:\TOYC\artifacts\independent_audit\java_output\p2_add_call.compile.stderr"
