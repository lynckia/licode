#!/bin/bash

echo "Creating FirefoxExtension.xpi..."

zip ./FirefoxExtension.xpi icon.png bootstrap.js install.rdf

echo "FirefoxExtension.xpi ready!"
