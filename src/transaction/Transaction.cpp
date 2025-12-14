#include "Transaction.h"
#include <openssl/sha.h>
#include <iomanip>

Transaction::Transaction() : sender(""), receiver(""), amount(0), status(PENDING), timestamp(0) {}


Transaction::Transaction(const std::string &from, const std::string &to, double amt)
{
    sender = from;
    receiver = to;
    amount = amt;
    status = PENDING;
    timestamp = (long long)(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count());
    id = generateId(sender, receiver, amount, timestamp);
}

/**
 * @brief Generates a unique transaction ID by creating a SHA256 hash of transaction details.
 * 
 * This function concatenates transaction information (sender, receiver, amount, and timestamp)
 * into a single string, computes its SHA256 hash, and converts the hash to a hexadecimal string.
 * 
 * @param sender The address/identifier of the transaction sender
 * @param receiver The address/identifier of the transaction receiver
 * @param amount The transaction amount (formatted to 8 decimal places)
 * @param timestamp The Unix timestamp of the transaction
 * 
 * @return A 64-character hexadecimal string representing the SHA256 hash of the transaction details.
 *         This serves as a unique transaction identifier.
 * 
 * @details
 * - Input string format: "sender|receiver|amount|timestamp"
 * - SHA256 produces a 32-byte (256-bit) binary digest
 * - Each byte is converted to 2 hexadecimal characters (0-255 → 00-FF)
 * - The resulting ID is deterministic: same inputs always produce the same ID
 */
/**
 * @brief dekh ata hocce the real process, inside the hood how we are generating the hash out of the sender|receiver|amount|timestamp canonical string
 * 
 * This function creates a transaction identifier by concatenating transaction details
 * (sender, receiver, amount, and timestamp) and computing their SHA256 hash.
 * The resulting binary hash is then converted to a hexadecimal string representation.
 * 
 * @param sender The sender's identifier/address
 * @param receiver The receiver's identifier/address
 * @param amount The transaction amount (formatted to 8 decimal places)
 * @param timestamp The transaction timestamp in milliseconds/seconds
 * 
 * @return A 64-character hexadecimal string representing the SHA256 hash of the transaction.
 *         Each of the 32 bytes in the SHA256 digest is converted to 2 hexadecimal characters.
 * 
 * @note The formatting stream manipulators work as follows:
 *       - std::hex: Sets the output base to hexadecimal (base 16)
 *       - std::setw(2): Sets the field width to 2 characters for the next value
 *       - std::setfill('0'): Pads the output with '0' characters to reach the field width
 *       - (int)hash[i]: Casts each unsigned char byte to int so it outputs as a number
 *                       instead of a character. Without this cast, the byte would be
 *                       interpreted as an ASCII character rather than a numeric value.
 *       
 *       Example: A byte value of 0x0A (decimal 10) outputs as "0a" instead of "\n"
 */
std::string Transaction::generateId(const std::string &sender, const std::string &receiver, double amount, long long timestamp)
{
    std::ostringstream oss;
    oss << sender << "|" << receiver << "|" << std::fixed << std::setprecision(8) << amount << "|" << timestamp;
    std::string s = oss.str();

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)s.data(), s.size(), hash); // produces a 32-byte or 256 bit binary digest

    std::ostringstream hex;
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        hex << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i]; // each byte getting converted into 2 hexadecimal characters

    return hex.str(); // for each byte 2 hexadecimal characters, that means for 32 byte -> 64 hexadecimal characters, the output string will be a string length of 64 characters
}

nlohmann::json Transaction::toJSON() const
{
    nlohmann::json j;
    j["id"] = id;
    j["sender"] = sender;
    j["receiver"] = receiver;
    j["amount"] = amount;
    j["status"] = status;
    j["timestamp"] = timestamp;
    if (!signatureBase64.empty())
        j["signature"] = signatureBase64;
    if (!pubKeyPem.empty())
        j["pubKeyPem"] = pubKeyPem;
    return j;
}

Transaction Transaction::fromJSON(const nlohmann::json &j)
{
    Transaction tx;
    tx.id = j.value("id", std::string());
    tx.sender = j.value("sender", std::string());
    tx.receiver = j.value("receiver", std::string());
    tx.amount = j.value("amount", 0.0);
    tx.status = j.value("status", PENDING);
    tx.timestamp = j.value("timestamp", 0LL);
    tx.signatureBase64 = j.value("signature", std::string());
    tx.pubKeyPem = j.value("pubKeyPem", std::string());
    return tx;
}