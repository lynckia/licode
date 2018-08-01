#!/usr/bin/env bash

export NUVE_AUTH_CHECK_TIMESTAMP="false";
mocha nuveAuthenticatorNoCheckTimstamp.js;
export NUVE_AUTH_CHECK_TIMESTAMP="";

mocha nuveAuthenticator.js;
