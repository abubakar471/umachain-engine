CXX = g++
CXXFLAGS = -std=c++17 -I./include -I./src -D_WIN32_WINNT=0x0A00

SRC = \
    server.cpp \
    src/block/Block.cpp \
    src/blockchain/Blockchain.cpp \
    src/transaction/Transaction.cpp \
    src/crypto/Crypto.cpp \
    src/mempool/Mempool.cpp \
    src/storage/Storage.cpp \

TARGET = server

LDLIBS = -lssl -lcrypto -lws2_32

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

clean:
	rm -f $(TARGET)
