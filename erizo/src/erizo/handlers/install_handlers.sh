#!/usr/bin/env bash

CURRENT_DIR=`pwd`

yes | cp -rf $CURRENT_DIR/templates/HandlerImporterHeaders.txt HandlerImporter.h

yes | cp -rf $CURRENT_DIR/templates/HandlerImporterSource.txt HandlerImporter.cpp

node parseHandlers.js
