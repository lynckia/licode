FROM ubuntu:14.04

MAINTAINER Lynckia

WORKDIR /opt

# Download latest version of the code and install dependencies
RUN  apt-get update && apt-get install -y git && \
  git clone https://github.com/ging/licode.git && \
	./licode/scripts/installUbuntuDeps.sh --cleanup

# Install Licode components
RUN ./licode/scripts/installErizo.sh && \
	./licode/scripts/installNuve.sh && \
	./licode/scripts/installBasicExample.sh
COPY initDockerLicode.sh /opt/
CMD ["./initDockerLicode.sh"]

