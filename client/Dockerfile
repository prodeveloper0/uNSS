FROM devkitpro/devkita64:latest

# libcurl + SDL2 설치
RUN dkp-pacman -Syu --noconfirm --needed \
    switch-curl \
    switch-sdl2 \
    switch-sdl2_ttf \
    switch-freetype \
    switch-harfbuzz \
    switch-mesa \
    switch-libpng

ENV DEVKITPRO=/opt/devkitpro
ENV PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/portlibs/switch/bin:/opt/devkitpro/devkitA64/bin:${PATH}

WORKDIR /client
