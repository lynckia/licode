FROM ubuntu:16.04

MAINTAINER Lynckia

WORKDIR /opt

# Download latest version of the code and install dependencies
RUN  apt-get update && apt-get install -y git wget && \
  git clone https://github.com/fanchuanster/licode.git

WORKDIR /opt/licode

# Install Licode components
RUN  ./scripts/installUbuntuDeps.sh --cleanup && \
  ./scripts/installErizo.sh && \
	./scripts/installNuve.sh && \
	./scripts/installBasicExample.sh

WORKDIR /opt

COPY initDockerLicode.sh /opt/
CMD ["./initDockerLicode.sh"]

