@echo off
type "E:\TOYC\artifacts\independent_audit\audit_cases\p2c_global_shadow_param.tc" | "java" "-jar" "E:\TOYC\toy-c-compiler-master\target\toyc.jar" "-opt" > "E:\TOYC\artifacts\independent_audit\java_output\p2c_global_shadow_param.s" 2> "E:\TOYC\artifacts\independent_audit\java_output\p2c_global_shadow_param.compile.stderr"
