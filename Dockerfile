FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    zsh \
    gdb \
    vim \
    git \
    net-tools \
    iproute2 \
    procps \
    lsof \
    && rm -rf /var/lib/apt/lists/*


WORKDIR /app

CMD [ "zsh" ]

