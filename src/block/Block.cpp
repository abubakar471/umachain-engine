#include "Block.h"
#include <iostream>
#include <string>
#include <sstream>
#include <functional> // for std::hash


Block::Block(int idx, const std::string &time, const std::vector<Transaction> &txs, const std::string &prevHashValue)
{
    index = idx;
    timestamp = time;
    transactions = txs;
    previousHash = prevHashValue;
    nonce = 0;
    hash = calculateHash();
}

// hash generator using std::hash
std::string Block::calculateHash()
{
    std::stringstream ss;

    // creating the raw input string with the block's data
    ss << index << timestamp << previousHash << nonce;

    // add all transactions into the hash input
    for(auto &tx : transactions){
        ss << tx.sender << tx.receiver << tx.amount; 
    }

    std::hash<std::string> hasher; // std::string means the input we will pass inside the hasher function, in our case we are passing our canonical string
    /*
        size_t is an unsigned integer type.
        and unsigned, guaranteed to be large enough to hold the size of the largest possible object on the platform. Typical widths: 32-bit on 32-bit systems, 64-bit on 64-bit systems.
    */
    size_t hashValue = hasher(ss.str());

    return std::to_string(hashValue);
}

// our custom proof of work algorithm
// the hash must start with "diffiuclty" number of zeros, that's it
void Block::mineBlock(int difficulty)
{
    std::string target(difficulty, '1'); // e,g if difficulty = 3, then target = "111"
    
    while (hash.substr(0, difficulty) != target)
    {
        // std::cout << "new hash : " << hash << std::endl;
        nonce++;
        hash = calculateHash();
    }
}

nlohmann::json Block::toJSON() const {
    nlohmann::json j;
    j["index"] = index;
    j["timestamp"] = timestamp;
    j["previousHash"] = previousHash;
    j["hash"] = hash;
    j["nonce"] = nonce;
    j["transactions"] = nlohmann::json::array();

    for (const auto &tx : transactions) j["transactions"].push_back(tx.toJSON());
    
    return j;
}

Block Block::fromJSON(const nlohmann::json &j) {
    int idx = j["index"];
    std::string ts = j["timestamp"];
    std::string prev = j["previousHash"];
    std::vector<Transaction> txs;
    for (auto &t : j["transactions"]) txs.push_back(Transaction::fromJSON(t));
    Block b(idx, ts, txs, prev);
    b.hash = j.value("hash", std::string());
    b.nonce = j.value("nonce", 0);
    return b;
}