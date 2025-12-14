#include "Blockchain.h"
#include <fstream>
#include <ctime>
#include <limits>
#include <sstream>
#include <iostream>
#include "../../include/json.hpp"

Blockchain::Blockchain()
{
    loadFromFile();
    difficulty = 5;
    miningReward = 2.0;

    if (chain.empty())
    {
        chain.push_back(createGenesisBlock());
        saveToFile();
    }
}

Block Blockchain::createGenesisBlock()
{
    Block newBlock(0, "2025-25-11", {}, "0");
    newBlock.mineBlock(difficulty);

    return newBlock;
}

Block Blockchain::getLatestBlock()
{
    return chain.back();
}

// -----------------------------------
//      Add transaction to mempool
// -----------------------------------

void Blockchain::addTransaction(const Transaction &tx)
{

    mempool.push_back(tx);
}

// -----------------------------------------------------
//      Mine block containing all transaction in mempool
// -----------------------------------------------------

bool Blockchain::minePendingTransactions(const std::string &minerAddress, WalletManager &walletManager)
{
    if (mempool.empty())
    {
        std::cout << "No pending transactions to mine!\n";
        return false;
    }

    // 1. Add block reward (free coins from system)
    Transaction rewardTx("SYSTEM", minerAddress, miningReward);
    rewardTx.status = TxStatus::CONFIRMED;
    mempool.push_back(rewardTx);

    // 2. create block with all mempool transactions
    int newIndex = chain.size();

    // get current time
    time_t now = time(0);

    std::string timestr = ctime(&now);

    // ctime() returns a string that usually ends with '\n' (and on Windows may include '\r').
    // Remove trailing CR/LF so timestamps don't introduce blank lines in saved files.
    while (!timestr.empty() && (timestr.back() == '\n' || timestr.back() == '\r'))
    {
        timestr.pop_back();
    }

    /*
        setting the pending transactions status to confirmed thhose are about to be added to a new block that are going to be added to the blockchain
    */
    for (auto &tx : mempool)
    {
        // deduct from sender
        walletManager.updateBalance(tx.sender, -tx.amount);

        // credit receiver
        walletManager.updateBalance(tx.receiver, tx.amount);

        tx.status = TxStatus::CONFIRMED;
    }

    Block newBlock(newIndex, timestr, mempool, getLatestBlock().hash);

    // 3. Perform PoW, Mine the block
    newBlock.mineBlock(difficulty);

    // 4. Add block to chain
    chain.push_back(newBlock);

    // 5. Clear mempool
    mempool.clear();

    // save updated chain to file
    saveToFile();

    return true; // block mined successfully
}

double Blockchain::getBalance(const std::string &walletAddress)
{
    double balance = 0.0;

    // Go through every block
    for (const auto &block : chain)
    {
        // Go through each transaction
        for (const auto &tx : block.transactions)
        {
            if (tx.sender == walletAddress)
            {
                balance -= tx.amount;
            }
            if (tx.receiver == walletAddress)
            {
                balance += tx.amount;
            }
        }
    }

    return balance;
}

double Blockchain::getEffectiveBalance(const std::string &wallet)
{
    double confirmed = getBalance(wallet);
    double pendingOut = 0;

    for (const Transaction &tx : mempool)
    {
        if (tx.sender == wallet)
        {
            pendingOut += tx.amount;
        }
    }

    std::cout << "confirmed : " << confirmed << std::endl;
    std::cout << "pending out : " << pendingOut << std::endl;
    std::cout << "confirmed - pendingOut = " << confirmed - pendingOut << std::endl;

    return confirmed - pendingOut;
}

bool Blockchain::validateTransaction(const Transaction &tx)
{
    double effective = getEffectiveBalance(tx.sender);

    if (effective < tx.amount)
    {
        std::cerr << "Rejected: double-spend attempt (insufficient effective funds)\n";
        return false;
    }

    return true;
}

// -------------------------
//      Validate the chain
// -------------------------

bool Blockchain::isValidChain()
{
    for (size_t i = 1; i < chain.size(); i++)
    {
        Block current = chain[i];
        Block previous = chain[i - 1];

        if (current.hash != current.calculateHash())
        {
            return false;
        }

        if (current.previousHash != previous.hash)
        {
            return false;
        }
    }

    return true;
}

// -----------------------------
//    Save chain to a .json file
// -----------------------------
void Blockchain::saveToFile()
{
    saveToJSON();
}

void Blockchain::saveToJSON()
{
    nlohmann::json jChain = nlohmann::json::array();

    for (const auto &block : chain)
        jChain.push_back(block.toJSON());

    std::ofstream file("../data/blockchain.json");
    file << jChain.dump(4); // pretty print JSON
}

// -----------------------------
//     Load chain from .json file
// -----------------------------
void Blockchain::loadFromFile()
{
    loadFromJSON();
}

void Blockchain::loadFromJSON()
{
    std::ifstream file("../data/blockchain.json");
    if (!file.good())
        return;

    nlohmann::json jChain;
    file >> jChain;

    chain.clear();
    for (auto &jBlock : jChain)
    {
        int index = jBlock["index"];
        std::string timestamp = jBlock["timestamp"];
        std::string prevHash = jBlock["previousHash"];
        std::string hash = jBlock["hash"];

        std::vector<Transaction> txs;
        for (auto &jTx : jBlock["transactions"])
        {
            Transaction tx(
                jTx["sender"],
                jTx["receiver"],
                jTx["amount"]);
            tx.status = (TxStatus)jTx["status"];
            txs.push_back(tx);
        }

        Block block(index, timestamp, txs, prevHash);
        block.hash = hash;

        chain.push_back(block);
    }
}

// ================================
// Explorer: pagination for blocks
// ================================
std::vector<Block> Blockchain::getBlocks(int limit, int offset)
{
    std::vector<Block> result;

    int total = chain.size();
    if (offset >= total)
        return result;

    int end = std::min(offset + limit, total);

    for (int i = offset; i < end; i++)
    {
        result.push_back(chain[i]);
    }
    return result;
}

// ================================
// Get block by index
// ================================
Block Blockchain::getBlockByIndex(int index)
{
    if (index < 0 || index >= (int)chain.size())
        throw std::runtime_error("Block index out of range");

    return chain[index];
}

// ================================
// Get ALL tx for a given wallet
// ================================
std::vector<Transaction> Blockchain::getTransactionsForWallet(const std::string &walletId)
{
    std::vector<Transaction> out;
    // mempool pending transactions first (optional)
    for (const auto &tx : mempool)
    {
        if (tx.sender == walletId || tx.receiver == walletId)
            out.push_back(tx);
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        for (const auto &tx : it->transactions)
        {
            if (tx.sender == walletId || tx.receiver == walletId)
                out.push_back(tx);
        }
    }
    return out; // newest first
}

// ================================
// Get a single tx by ID
// ================================
Transaction Blockchain::getTransactionById(const std::string &txid)
{
    for (const auto &tx : mempool)
        if (tx.id == txid)
            return tx;
    for (const auto &block : chain)
        for (const auto &tx : block.transactions)
            if (tx.id == txid)
                return tx;
    return Transaction(); // empty
}

// ================================
// Get latest N transactions
// ================================
std::vector<Transaction> Blockchain::getLatestTransactions(int limit)
{
    std::vector<Transaction> out;
    for (auto it = chain.rbegin(); it != chain.rend() && (int)out.size() < limit; ++it)
        for (const auto &tx : it->transactions)
        {
            out.push_back(tx);
            if ((int)out.size() >= limit)
                break;
        }
    // optionally add mempool at front
    for (const auto &tx : mempool)
    {
        if ((int)out.size() >= limit)
            break;
        out.insert(out.begin(), tx); // pending at top
    }
    return out;
}

void Blockchain::addConfirmedTransaction(const Transaction &tx)
{
    // create a copy of the transaction and ensure it's marked confirmed
    Transaction txCopy = tx;
    txCopy.status = TxStatus::CONFIRMED;

    int newIndex = chain.size();
    time_t now = time(0);
    std::string timestr = ctime(&now);
    while (!timestr.empty() && (timestr.back() == '\n' || timestr.back() == '\r'))
        timestr.pop_back();

    // make a block with just this transaction
    std::vector<Transaction> txs;
    txs.push_back(txCopy);

    Block newBlock(newIndex, timestr, txs, getLatestBlock().hash);

    // Mine with zero difficulty so it produces a hash without looping (fast and deterministic)
    // Backup current difficulty and restore after if you want; here we set difficulty 0 for this block
    int savedDifficulty = difficulty;
    newBlock.mineBlock(0); // or use difficulty=0 to skip pow
    // restore difficulty if your code uses global difficulty
    difficulty = savedDifficulty;

    // Add block, save
    chain.push_back(newBlock);
    saveToFile();
}