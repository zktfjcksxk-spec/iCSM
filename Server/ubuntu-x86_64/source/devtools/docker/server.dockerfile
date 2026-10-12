# It'd be nice to move to 16.04 LTS, but there is no g++4.6 package for Xenial. 
FROM ubuntu:14.04

# Set up the environment which won't likely change.
ARG HTTP_PROXY=http://proxy.valvesoftware.com:3128
ARG HTTPS_PROXY=http://proxy.valvesoftware.com:3128
ARG FTP_PROXY=http://proxy.valvesoftware.com:3128
ARG NO_PROXY=localhost,valvesoftware.com,valve.org
ARG http_proxy=http://proxy.valvesoftware.com:3128
ARG https_proxy=http://proxy.valvesoftware.com:3128
ARG ftp_proxy=http://proxy.valvesoftware.com:3128
ARG no_proxy=localhost,valvesoftware.com,valve.org

# Just do enough work here to pick up perforce, we want to sync the P4 tree as early as possible.
RUN apt update && apt install -y \
	wget

# Need perforce. 
RUN echo "deb http://package.perforce.com/apt/ubuntu/ trusty release" > /etc/apt/sources.list.d/perforce.list
RUN wget -qO - https://package.perforce.com/perforce.pubkey | apt-key add -
RUN apt update && apt install -y \
	helix-cli

# Need linux build tool chain for now. This toolchain crashes a lot, so it'd be nice to get rid of 
# it as soon as is feasible, but that probably won't be the case until we are using containerization
# for the server run environment as well.
ARG STEAMTOOLS_CHANGELIST=4302742	
RUN echo "Syncing tools from perforce. This will only be done once." && \
	p4 -p perforce.valvesoftware.com:1666 -u script -c docker_build_csgo sync -f //...@${STEAMTOOLS_CHANGELIST} > /dev/null && \
	echo "Tree Sync Complete."


# We already did our upgrade above, so now just need to grab a few other things.
RUN apt install -y \
	binutils \
	libc6-i386 \
	lib32stdc++6 \
	make \
	vim

# We need dumb-init to deal with handling ctrl-c during make correctly.
RUN wget -O /usr/local/bin/dumb-init https://github.com/Yelp/dumb-init/releases/download/v1.2.1/dumb-init_1.2.1_amd64
RUN chmod +x /usr/local/bin/dumb-init

ENV HTTP_PROXY=$HTTP_PROXY
ENV HTTPS_PROXY=$HTTPS_PROXY
ENV FTP_PROXY=$FTP_PROXY
ENV NO_PROXY=$NO_PROXY
ENV http_proxy=$http_proxy
ENV https_proxy=$https_proxy
ENV ftp_proxy=$ftp_proxy
ENV no_proxy=$no_proxy

ENV IN_DOCKER_BUILD=1

ENTRYPOINT [ "/usr/local/bin/dumb-init", "--" ]
CMD [ "/bin/bash" ]
