FROM mcr.microsoft.com/devcontainers/cpp:latest

RUN apt-get update && apt-get install -y \
    nlohmann-json3-dev \
    libfmt-dev \
    python3-dev \
    python3-pip \
    pybind11-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip install panel holoviews bokeh --break-system-packages
