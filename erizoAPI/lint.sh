#!/usr/bin/env bash

 ./../erizo/utils/cpplint.py --filter=-legal/copyright,-build/include --linelength=120 *.cc *.h
