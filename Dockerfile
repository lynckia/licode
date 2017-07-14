FROM ubuntu:14.04

MAINTAINER Lynckia

WORKDIR /opt

# Download latest version of the code and install dependencies
RUN  apt-get update && apt-get install -y git wget curl

COPY . /opt/licode

# Clone and install licode
WORKDIR /opt/licode/scripts

RUN ./installUbuntuDeps.sh --cleanup --fast && \
    ./installErizo.sh -feacs && \
    ./../nuve/installNuve.sh && \
    ./installBasicExample.sh

WORKDIR /opt

ENTRYPOINT ["./licode/extras/docker/initDockerLicode.sh"]
