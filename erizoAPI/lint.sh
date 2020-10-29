#!/usr/bin/env bash

cpplint --filter=-legal/copyright,-build/include --linelength=120 *.cc *.h
