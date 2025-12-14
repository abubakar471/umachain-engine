#include "WalletManager.h"
#include <fstream>
#include <random>
#include <iostream>

// helper: normalize PEM by removing carriage returns (\r)
static std::string normalize_pem_crlf(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\r') continue;
        out.push_back(c);
    }
    return out;
}

using json = nlohmann::json;

// constructor - to load wallet file
WalletManager::WalletManager()
{
    loadFromFile();
}

// generate wallet id
std::string WalletManager::generateWalletId()
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(100000, 999999);

    return "WALLET_" + std::to_string(dist(rng));
}

// get existing or create new wallet
std::string WalletManager::getOrCreateWallet(const std::string &userId)
{
    if (userToWallet.count(userId))
    {
        return userToWallet[userId];
    }

    std::string newWallet = generateWalletId();
    userToWallet[userId] = newWallet;
    walletBalances[newWallet] = 0;

    saveToFile();

    return newWallet;
}

// get existing wallet
std::string WalletManager::getWallet(const std::string &userId)
{
    if (userToWallet.count(userId))
    {
        return userToWallet[userId];
    }

    return "";
}

// get wallet balance
double WalletManager::getBalance(const std::string &walletId)
{
    if (walletBalances.count(walletId))
    {
        return walletBalances[walletId];
    }

    return 0;
}

// update wallet balance
void WalletManager::updateBalance(const std::string &walletId, double amount)
{
    walletBalances[walletId] += amount;
    saveToFile();
}

// check if wallet exists or not
bool WalletManager::walletExists(const std::string &walletId)
{
    if (walletId.rfind("WALLET_", 0) != 0)
        return false;

    return walletBalances.count(walletId) > 0;
}

std::string WalletManager::bindPublicKeyToWallet(const std::string &walletId, const std::string &pubKeyPem)
{
    if (!walletBalances.count(walletId))
    {
        // maybe create it
        walletBalances[walletId] = 0;
    }
    // normalize CRLF to LF so stored keys match what frontend signs/verifies
    walletPublicKey[walletId] = normalize_pem_crlf(pubKeyPem);
    saveToFile();
    return walletId;
}

std::string WalletManager::getPublicKey(const std::string &walletId)
{
    if (walletPublicKey.count(walletId))
        return walletPublicKey[walletId];
    return "";
}

// save to wallets.json
void WalletManager::saveToFile()
{
    json j;

    j["users"] = userToWallet;
    j["balances"] = walletBalances;
    j["pubkeys"] = walletPublicKey;

    std::ofstream file(filename);

    file << j.dump(4);
}

// load from wallets.json
void WalletManager::loadFromFile()
{
    std::ifstream file(filename);

    if (!file.good())
        return;

    json j;

    file >> j;

    if (j.contains("users"))
    {
        userToWallet = j["users"].get<std::unordered_map<std::string, std::string>>();
    }

    if (j.contains("balances"))
    {
        walletBalances = j["balances"].get<std::unordered_map<std::string, double>>();
    }

    if (j.contains("pubkeys"))
    {
        walletPublicKey = j["pubkeys"].get<std::unordered_map<std::string, std::string>>();
        // normalize any CRLF that might be present in stored pubkeys
        for (auto &kv : walletPublicKey) {
            kv.second = normalize_pem_crlf(kv.second);
        }
    }
}