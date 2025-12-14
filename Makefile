CXX = g++
CXXFLAGS = -std=c++17 -O2
LIBS = -lssl -lcrypto -lpthread

TARGET = server
SRC = src/server.cpp src/blockchain/Blockchain.cpp src/wallet/WalletManager.cpp src/crypto/Crypto.cpp

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

run:
	./server
