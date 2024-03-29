# We chose Alpine to build the image because it has good support for creating
# statically-linked, small programs.
ARG DISTRO_VERSION=edge
FROM frolvlad/alpine-glibc AS base

# Create separate targets for each phase, this allows us to cache intermediate
# stages when using Google Cloud Build, and makes the final deployment stage
# small as it contains only what is needed.
FROM base AS devtools

# Install the typical development tools and some additions:
#   - ninja-build is a backend for CMake that often compiles faster than
#     CMake with GNU Make.
#   - Install the boost libraries.
RUN apk update && \
    apk add \
        boost-dev \
        boost-static \
        build-base \
        cmake \
        git \
        gcc \
        glibc\
        g++ \
        libc-dev \
        nghttp2-static \
        ninja \
        openssl-dev \
        openssl-libs-static \
        tar \
        zlib-static

# Copy the source code to /v/source and compile it.
FROM devtools AS build
COPY . /v/source
WORKDIR /v/source

# Run the CMake configuration step, setting the options to create
# a statically linked C++ program
RUN cmake -S/v/source -B/v/binary -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBoost_USE_STATIC_LIBS=ON \
    -DCMAKE_EXE_LINKER_FLAGS=-static

# Compile the binary and strip it to reduce its size.
RUN cmake --build /v/binary
COPY ./index.html /v/binary
RUN strip /v/binary/TCPServer
RUN ls /v/binary

# Create the final deployment image, using `scratch` (the empty Docker image)
# as the starting point. Effectively we create an image that only contains
# our program.
FROM devtools AS TCPServer
WORKDIR /r



# Copy the program from the previously created stage and make it the entry point.
COPY --from=build /v/binary/TCPServer /r
COPY --from=build /v/binary/index.html /r

RUN apk update && \
    apk add \
    gcc \
    glibc\
    g++ \
    libc-dev 
ENTRYPOINT [ "/r/TCPServer" ]
EXPOSE $PORT
