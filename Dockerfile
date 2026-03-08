FROM gcc:latest

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN g++ --version | head -1 && cmake --version | head -1

RUN cmake -B build \
    -DCMAKE_CXX_STANDARD=23 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON \
    && cmake --build build -j$(nproc)

CMD ["ctest", "--test-dir", "build", "--output-on-failure"]
