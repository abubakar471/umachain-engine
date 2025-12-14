#ifndef BLOCK_H
#define BLOCK_H

#include <string>
#include <vector>
#include "../transaction/Transaction.h"
#include "../../include/json.hpp"

class Block
{
    public:
        int index;
        std::string timestamp;
        std::vector<Transaction> transactions;
        std::string previousHash;
        std::string hash;
        int nonce;

        Block(int idx, const std::string &time, const std::vector<Transaction> &txs, const std::string &prevHash);

        std::string calculateHash();
        void mineBlock(int difficulty);

        nlohmann::json toJSON() const;
        static Block fromJSON(const nlohmann::json &j);
};

#endif