FROM ubuntu:latest
LABEL org.opencontainers.image.authors="CandiceJoy <candice@candicejoy.com>"
LABEL author="CandiceJoy"
LABEL description="GZDoom compilation image (Designed for GZDoom 4.11pre)"
LABEL verion="4.11pre"

# Update these as needed
ENV GZ_ZMUSIC_URL="https://github.com/coelckers/ZMusic.git"
ENV GZ_ZMUSIC_COMMIT="75d2994b4b1fd6891b20819375075a2976ee34de"
ENV GZ_PACKAGES="build-essential git cmake libsdl2-dev libvpx-dev"

# Update package lists and install package-based build dependencies
RUN apt-get update; apt-get install -y $GZ_PACKAGES

# Install ZMusic
RUN git clone $GZ_ZMUSIC_URL; git reset --hard $GZ_ZMUSIC_COMMIT; cd ZMusic; cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr; make; make install
