@echo off
type "E:\TOYC\artifacts\independent_audit\audit_cases\p2c_global_uninit.tc" | "java" "-jar" "E:\TOYC\toy-c-compiler-master\target\toyc.jar" "-opt" > "E:\TOYC\artifacts\independent_audit\java_output\p2c_global_uninit.s" 2> "E:\TOYC\artifacts\independent_audit\java_output\p2c_global_uninit.compile.stderr"
