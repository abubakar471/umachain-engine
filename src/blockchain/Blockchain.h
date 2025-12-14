#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <vector>
#include <iostream>
#include "../block/Block.h"
#include "../transaction/Transaction.h"
#include "../wallet/WalletManager.h"

class Blockchain
{
private:
    std::vector<Block> chain;
    std::vector<Transaction> mempool; // unconfirmed transactions
    int difficulty;
    double miningReward;

public:
    Blockchain();

    Block createGenesisBlock();

    void addTransaction(const Transaction &tx); // add transaction to mempool for mining

    Block getLatestBlock();

    bool minePendingTransactions(const std::string &minerAddress, WalletManager &walletManager); // mine pending transactions and adds to the blockchain

    double getBalance(const std::string &walletAddress);

    double getEffectiveBalance(const std::string &wallet);

    bool validateTransaction(const Transaction &tx);

    bool isValidChain();

    std::vector<Block> getChain() { return chain; };

    std::vector<Transaction> getMempool() { return mempool; };

    std::vector<Transaction> getTransactionsForWallet(const std::string &walletId);

    Transaction getTransactionById(const std::string &txid);

    std::vector<Transaction> getLatestTransactions(int limit = 20);

    Block getBlockByIndex(int index);

    std::vector<Block> getBlocks(int limit = 100, int offset = 0);
    
    void addConfirmedTransaction(const Transaction &tx);

    void saveToFile();
    void loadFromFile();
    void saveToJSON();
    void loadFromJSON();
};

#endif