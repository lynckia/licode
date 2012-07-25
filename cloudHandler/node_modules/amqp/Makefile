NODE=`which node`
SERVER="localhost:5672"
THIS_DIR=$(shell pwd)

default: test

FAIL=echo FAIL
PASS=echo PASS

test:
	@for i in test/test-*.js; do \
	  /bin/echo -n "$$i: "; \
	  $(NODE) $$i $(SERVER) > /dev/null && $(PASS) || $(FAIL); \
	done

.PHONY: test
