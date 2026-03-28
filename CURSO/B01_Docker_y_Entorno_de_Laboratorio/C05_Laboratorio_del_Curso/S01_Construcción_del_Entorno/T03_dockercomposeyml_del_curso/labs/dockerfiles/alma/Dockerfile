FROM almalinux:9

ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

RUN dnf install -y epel-release && dnf clean all

RUN dnf install -y \
    gcc gcc-c++ make cmake pkgconf-pkg-config glibc-devel \
    gdb valgrind strace ltrace \
    man-db man-pages \
    git curl wget vim-minimal less tree file bc \
    glibc-langpack-en \
    nmap-ncat iproute iputils bind-utils tcpdump \
    ca-certificates findutils \
    && dnf clean all

RUN useradd -m -s /bin/bash dev

USER dev
WORKDIR /home/dev

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --default-toolchain stable --component rustfmt clippy

ENV PATH="/home/dev/.cargo/bin:${PATH}"

WORKDIR /home/dev/workspace

CMD ["/bin/bash"]
