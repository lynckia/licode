FROM ubuntu:20.04

MAINTAINER Lynckia

WORKDIR /opt

#Configure tzdata
ENV TZ=Europe/Madrid
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Download latest version of the code and install dependencies
RUN  apt-get update && apt-get install -y git wget curl

COPY .nvmrc package.json /opt/licode/

COPY scripts/installUbuntuDeps.sh scripts/checkNvm.sh scripts/libnice-014.patch0 /opt/licode/scripts/

WORKDIR /opt/licode/scripts

RUN ./installUbuntuDeps.sh --cleanup --fast

WORKDIR /opt

COPY . /opt/licode

RUN mkdir /opt/licode/.git

# Clone and install licode
WORKDIR /opt/licode/scripts

RUN ./installErizo.sh -idfeacs && \
    ./../nuve/installNuve.sh && \
    ./installBasicExample.sh

RUN ldconfig /opt/licode/build/libdeps/build/lib 

WORKDIR /opt/licode

ARG COMMIT

RUN echo $COMMIT > RELEASE
RUN date --rfc-3339='seconds' >> RELEASE
RUN cat RELEASE

WORKDIR /opt

ENTRYPOINT ["./licode/extras/docker/initDockerLicode.sh"]
