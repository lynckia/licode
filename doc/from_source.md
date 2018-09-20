# Overview
This guide will guide you through the basics of getting a Licode instance with a basic videoconferencing application up and running.

# Prerequisites
We **only** officially support **Ubuntu 16.04** for production environments.

We do maintain compatibility with **Mac OS X** for **development and testing purposes**.


|Ubuntu 16.04 | Mac OS X > 10.11 |
|-------------|------------------|
|git|Xcode Command Line Tools|
|   |git|


# Clone Licode
Let's start by cloning the Licode repository
```
git clone https://github.com/lynckia/licode.git
cd licode
```
The repository contains scripts for the rest of the steps of this guide.
# Install dependencies
This step installs the dependencies of all the Licode components. This is the only step that depends on your OS
#### Ubuntu
```
./scripts/installUbuntuDeps.sh
```
#### Mac OS X
```
./scripts/installMacDeps.sh
```

# Install Licode
Here we will install all the Licode components in your computer.
```
./scripts/installNuve.sh
./scripts/installErizo.sh
```
# Install basicExample
The basicExample is a really simple node-based web application that relies on Licode to provide a videoconferencing room.
```
./scripts/installBasicExample.sh
```

# Start Licode!
At this points, you have successfully installed all the Licode components in your computer and also a simple application that will let you try it.
Let's use the convenience script to start all Licode components:
```
./scripts/initLicode.sh
```
After that, we just have to start our node application, we also have a script for that:
```
./scripts/initBasicExample.sh
```
** Now you can connect to _http://localhost:3001_ with Chrome or Firefox and test your basic videoconference example! **

# What's next?

Well you now have a taste of what Licode can do. You can start by modifying basicExample. You can find the code in `licode/extras/basic_example`:
* `basicServer.js` is the node.js server that manages the communication between the clients and Nuve. Here you can add your own methods using the server side API (NuveAPI)

Head to [Licode Architecture](index.md) for more information about different Licode components, or start developing your custom service getting into the client or server APIs.
