FROM debian:bookworm

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

RUN sed -i 's/^path-exclude \/usr\/share\/man/#path-exclude \/usr\/share\/man/' \
    /etc/dpkg/dpkg.cfg.d/excludes 2>/dev/null || true

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc g++ make cmake pkg-config libc6-dev \
    gdb valgrind strace ltrace \
    man-db manpages-dev manpages-posix-dev \
    git curl wget vim-tiny less tree file bc locales \
    netcat-openbsd iproute2 iputils-ping dnsutils tcpdump \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash dev

USER dev
WORKDIR /home/dev

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --default-toolchain stable --component rustfmt clippy

ENV PATH="/home/dev/.cargo/bin:${PATH}"

WORKDIR /home/dev/workspace

CMD ["/bin/bash"]
