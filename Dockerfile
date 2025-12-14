FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    libssl-dev \
    ca-certificates

WORKDIR /app

COPY . .

RUN g++ -std=c++17 \
src/server.cpp src/blockchain/*.cpp src/crypto/*.cpp src/block/*.cpp src/transaction/*.cpp src/wallet/*.cpp \
-Iinclude -lssl -lcrypto -o server

EXPOSE 8080

CMD ["./server"]
