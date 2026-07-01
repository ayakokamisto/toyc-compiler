@echo off
type "E:\TOYC\artifacts\independent_audit\audit_cases\p1_logic_norm_or.tc" | "java" "-jar" "E:\TOYC\toy-c-compiler-master\target\toyc.jar" "-opt" > "E:\TOYC\artifacts\independent_audit\java_output\p1_logic_norm_or.s" 2> "E:\TOYC\artifacts\independent_audit\java_output\p1_logic_norm_or.compile.stderr"
