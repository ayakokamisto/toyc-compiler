JAR := target/toyc.jar

.PHONY: all build clean

all: build

build:
	mvn -q -DskipTests package
	printf '%s\n' '#!/usr/bin/env sh' 'exec java -jar "$$(dirname "$$0")/$(JAR)" "$$@"' > toyc
	chmod +x toyc

clean:
	mvn -q clean
	rm -f toyc
