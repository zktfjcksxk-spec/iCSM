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

# For the client build, we also need the chroot. Grab those tools here.
ARG CHROOT_CHANGELIST=4352798
RUN echo "Syncing chroot tools from perforce. This will only be done once." && \
	p4 -p perforce.valvesoftware.com:1666 -u script -c docker_build_csgo_client_trunk sync -f //...@${CHROOT_CHANGELIST} > /dev/null && \
	echo "Tree Sync Complete."


# We already did our upgrade above, so now just need to grab a few other things.
RUN apt update && apt install -y \
	binutils \
	libc6-i386 \
	lib32stdc++6 \
	make \
	vim \
	xz-utils
	

# We need dumb-init to deal with handling ctrl-c during make correctly.
RUN wget -O /usr/local/bin/dumb-init https://github.com/Yelp/dumb-init/releases/download/v1.2.1/dumb-init_1.2.1_amd64
RUN chmod +x /usr/local/bin/dumb-init


# The chroot has an old and busted version of wget that cannot talk to github because of SSL incompatibilities.
# So install the chroot after using wget for the last time.
ARG CHROOT_NAME=_chroot_steamrt_scout_amd64
RUN cd /chroot && tar xaf steamrt_scout_amd64.tar.xz -C / --strip-components=1 > /dev/null 2>&1 || true

ENV HTTP_PROXY=$HTTP_PROXY
ENV HTTPS_PROXY=$HTTPS_PROXY
ENV FTP_PROXY=$FTP_PROXY
ENV NO_PROXY=$NO_PROXY
ENV http_proxy=$http_proxy
ENV https_proxy=$https_proxy
ENV ftp_proxy=$ftp_proxy
ENV no_proxy=$no_proxy
ENV CHROOT_NAME=$CHROOT_NAME


ENV IN_DOCKER_BUILD=1

ENV USE_STEAM_RUNTIME=1

ENTRYPOINT [ "/usr/local/bin/dumb-init", "--" ]
CMD [ "/bin/bash" ]
