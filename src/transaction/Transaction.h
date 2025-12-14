#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include "../../include/json.hpp"

enum TxStatus
{
    PENDING,
    CONFIRMED
};

class Transaction
{
public:
    std::string id;
    std::string sender;
    std::string receiver;
    double amount;
    TxStatus status;
    long long timestamp;

    std::string signatureBase64; // signature of canonical string
    std::string pubKeyPem;       // sender's public key, PEM format

    // constructors
    Transaction();
    Transaction(const std::string &from, const std::string &to, double amt);

    // helpers
    nlohmann::json toJSON() const;
    static Transaction fromJSON(const nlohmann::json &j);
    static std::string generateId(const std::string &sender, const std::string &receiver, double amount, long long timestamp);
};

#endif