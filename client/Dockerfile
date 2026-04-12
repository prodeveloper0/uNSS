FROM devkitpro/devkita64:latest

# libcurl 설치
RUN dkp-pacman -Syu --noconfirm --needed switch-curl

ENV DEVKITPRO=/opt/devkitpro
ENV PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/portlibs/switch/bin:/opt/devkitpro/devkitA64/bin:${PATH}

WORKDIR /client
