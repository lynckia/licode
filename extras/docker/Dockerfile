FROM ubuntu:16.04

MAINTAINER Lynckia

WORKDIR /opt

# ffmpeg dependencies for
RUN echo "deb http://us.archive.ubuntu.com/ubuntu trusty main multiverse" | tee -a /etc/apt/sources.list


# Download latest version of the code and install dependencies
RUN  apt-get update && apt-get install -y git wget && \
  git clone https://github.com/fanchuanster/licode.git

WORKDIR /opt/licode

# Install Licode components
RUN  git pull && ./scripts/installUbuntuDeps.sh --cleanup

RUN  git pull && ./scripts/installErizo.sh && \
	./scripts/installNuve.sh && \
	./scripts/installBasicExample.sh

WORKDIR /opt

COPY initDockerLicode.sh /opt/
CMD ["./initDockerLicode.sh"]

