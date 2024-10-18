FROM git.aurora.aur/aurora/cpp

RUN --mount=type=bind,target=. \
    --mount=type=cache,target=/build \
    cmake --workflow --preset=docker
