#include <iostream>
#include <string>
#include "../include/httplib.h"
#include "../include/json.hpp"
#include "./blockchain/Blockchain.h"
#include "./wallet/WalletManager.h"
#include "./crypto/Crypto.h"
#include <openssl/bio.h>
#include <openssl/evp.h>

WalletManager walletManager;

Blockchain blockchain; // global blockchain instance or object

// exchange rate: 1 USD = UMA_PER_USD UmaCoin
static constexpr double UMA_PER_USD = 0.1; // change as you like

// Simple mock charging function (returns true = success)
static bool mockChargeCard(const std::string &cardNumber, double usdAmount)
{
    // you can implement basic validation if you want
    if (cardNumber.empty() || usdAmount <= 0)
        return false;
    // simulate simple failure for card number "4000-0000-0000-0000" for testing
    if (cardNumber.find("4000") == 0)
        return false;
    return true;
}

// Mock sending money to a bank (selling)
static bool mockSendToBank(const std::string &bankAccount, double usdAmount)
{
    if (bankAccount.empty() || usdAmount <= 0)
        return false;
    // you may simulate failures similarly
    return true;
}

static double usdToUma(double usd)
{
    return usd * UMA_PER_USD;
}
static double umaToUsd(double uma)
{
    return uma / UMA_PER_USD;
}

int main()
{

    httplib::Server server;
    std::string CLIENT_URL = "http://localhost:3000";

    // CORS helper: use CLIENT_URL for more restrictive policy in development
    auto set_cors = [&](httplib::Response &res)
    {
        res.set_header("Access-Control-Allow-Origin", CLIENT_URL.c_str());
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    };

    // Preflight handler for any path
    server.Options(R"(/.*)", [&](const httplib::Request & /*req*/, httplib::Response &res)
                   {
        set_cors(res);
        res.status = 200;
        res.set_content("", "text/plain"); });

    // GET /chain -> returns full chain
    server.Get("/chain", [&](const httplib::Request &, httplib::Response &res)
               {
        nlohmann::json jChain = nlohmann::json::array();

        for (auto &block : blockchain.getChain())
            jChain.push_back(block.toJSON());

        set_cors(res);
        res.set_content(jChain.dump(4), "application/json"); });

    // POST /add-transaction → add tx to mempool
    server.Post("/add-transaction", [&](const httplib::Request &req, httplib::Response &res)
                {
        std::string sender;
        std::string receiver;
        std::string amountStr;
        std::string timestamp;
        std::string sender_user_id;
        std::string signature; // base64
        std::string pubKeyPem; // optional

        // Support both application/x-www-form-urlencoded and multipart/form-data
        if (req.is_multipart_form_data()) {
            if (req.form.has_field("sender")) sender = req.form.get_field("sender");
            if (req.form.has_field("receiver")) receiver = req.form.get_field("receiver");
            if (req.form.has_field("amount")) amountStr = req.form.get_field("amount");
            if (req.form.has_field("timestamp")) timestamp = req.form.get_field("timestamp");
            if (req.form.has_field("sender_user_id")) sender_user_id = req.form.get_field("sender_user_id");
            if (req.form.has_field("signature")) signature = req.form.get_field("signature");
            if (req.form.has_field("pubKeyPem")) pubKeyPem = req.form.get_field("pubKeyPem");
        } else {
            sender = req.get_param_value("sender");
            receiver = req.get_param_value("receiver");
            amountStr = req.get_param_value("amount");
            timestamp = req.get_param_value("timestamp");
            sender_user_id = req.get_param_value("sender_user_id");
            signature = req.get_param_value("signature");
            pubKeyPem = req.get_param_value("pubKeyPem");
        }

        double amount = std::stod(amountStr);

       
        // validate if sender and reciever wallet exists
         if(!walletManager.walletExists(sender) || !walletManager.walletExists(receiver)){
                   nlohmann::json response = {
            {"success", false},
            {"message", "Invalid Wallet Id"},
            };

            set_cors(res);
            return res.set_content(response.dump(), "application/json"); 
        }

        // If pubKeyPem provided, ensure it matches stored pubkey for sender (if any), or bind it
        std::string storedPub = walletManager.getPublicKey(sender);
        
        if (storedPub.empty() && !pubKeyPem.empty()) {
            // bind provided pubkey to sender wallet
            walletManager.bindPublicKeyToWallet(sender, pubKeyPem);
            storedPub = pubKeyPem;
        }

        if (storedPub.empty()){
            nlohmann::json response = {
                {"success", false},
                {"message", "Public key not found for sender"}
            };

            
            set_cors(res);
            return res.set_content(response.dump(), "application/json"); 
        }

        std::ostringstream oss;
        {
            // format amount in a temp stream so std::fixed doesn't affect later output
            std::ostringstream amt_ss;
            // amt_ss << std::fixed << std::setprecision(8) << amount;
            oss << sender << "|" << receiver << "|" << amountStr;
        }

        std::string message = oss.str();

        std::cout << "storedPub : " << storedPub << std::endl;
        std::cout << "message : " << message << std::endl;
        std::cout << "signature : " << signature << std::endl;


        bool ok = Crypto::verifySignaturePEM(storedPub, message, signature);
        
        if (!ok) {
            nlohmann::json response = {
                {"success", false},
                {"message", "Invalid Signature"}
            };

            
            set_cors(res);
            return res.set_content(response.dump(), "application/json"); 
        }

        Transaction tx(sender, receiver, amount);

        if(!blockchain.validateTransaction(tx)){
                   nlohmann::json response = {
            {"success", false},
            {"message", "Insufficient funds"},
            };

            set_cors(res);
            return res.set_content(response.dump(), "application/json"); 
        }


        blockchain.addTransaction(tx);
        double new_balance = blockchain.getEffectiveBalance(sender);

        // return a simple JSON object confirming the operation
        nlohmann::json response = {
            {"success", true},
            {"message", "Transaction added successfully"},
            {"new_balance", new_balance},
        };

        set_cors(res);
        return res.set_content(response.dump(), "application/json"); });

    // GET /mine → mine new block
    server.Get("/mine", [&](const httplib::Request &req, httplib::Response &res)
               {
                   auto miner_address = req.get_param_value("miner_address");
                   bool mined = blockchain.minePendingTransactions(miner_address, walletManager);

                   nlohmann::json response;

                    if (mined){
                       response = {
                           {"success", true},
                           {"message", "Block mined successfully."},
                       };

                    set_cors(res);
                    res.set_content(response.dump(), "application/json");
                   }
                    
                   else
                   {
                       response = {
                           {"success", false},
                           {"message", "No transaction found to mine."},
                       };


                              set_cors(res);
                              res.set_content(response.dump(), "application/json");
                   } });

    // GET /balance/:wallet
    server.Get(R"(/balance/(.*))", [&](const httplib::Request &req, httplib::Response &res)
               {
        std::string wallet = req.matches[1];
        double balance = blockchain.getBalance(wallet);
        
        nlohmann::json response = {
            {"success", true},
            {"wallet_address", wallet},
            {"balance", balance},
        };

        set_cors(res);
        res.set_content(response.dump(), "application/json"); });

    server.Get("/mempool", [&](auto &, auto &res)
               {
        nlohmann::json jChain = nlohmann::json::array();

        for (auto &tx : blockchain.getMempool())
        jChain.push_back(tx.toJSON());

        set_cors(res);
        res.set_content(jChain.dump(4), "application/json"); });

    server.Post("/wallet/init", [&](const httplib::Request &req, httplib::Response &res)
                {
        std::string userId;
        std::string pubKey;

        // Support both application/x-www-form-urlencoded and multipart/form-data
        if (req.is_multipart_form_data()) {
            if (req.form.has_field("user_id")) {
                userId = req.form.get_field("user_id");
            }
            if (req.form.has_field("pubKeyPem")) {
                pubKey = req.form.get_field("pubKeyPem");
            }
        } else {
            userId = req.get_param_value("user_id");
            pubKey = req.has_param("pubKeyPem") ? req.get_param_value("pubKeyPem") : std::string();
        }

        std::string wallet = walletManager.getOrCreateWallet(userId);
        double balance = walletManager.getBalance(wallet);

         if (!pubKey.empty()) {
        walletManager.bindPublicKeyToWallet(wallet, pubKey);
    }
        // std::string json = "{ \"wallet\": \"" + wallet +
        //                "\", \"balance\": " + std::to_string(balance) + " }";
       nlohmann::json response;

     response = { {"success", true}, {"wallet", wallet}, {"balance", balance}, {"basePrice", UMA_PER_USD}, {"pubKeyBound", !pubKey.empty()} };

    set_cors(res);
    res.set_content(response.dump(), "application/json"); });

    server.Get(R"(/wallet/balance/(.*))", [&](const httplib::Request &req, httplib::Response &res)
               {
        auto wallet = req.matches[1];
        double balance = walletManager.getBalance(wallet);

        nlohmann::json response;

        response = {
            {"success", true},
            {"balance", balance}
        };

        set_cors(res);
        res.set_content(response.dump(), "application/json"); });

    server.Post("/transaction/send", [&](const httplib::Request &req, httplib::Response &res)
                {
    std::string sender = req.get_param_value("sender");
    std::string receiver = req.get_param_value("receiver");
    double amount = std::stod(req.get_param_value("amount"));

    Transaction tx(sender, receiver, amount);
    blockchain.addTransaction(tx);

    nlohmann::json response = {
        {"success", true},
        {"message", "Transaction added to pending"},
        {"tx", {
            {"sender", sender},
            {"receiver", receiver},
            {"amount", amount},
            {"status", "PENDING"}
        }}
    };

    set_cors(res);
    res.set_content(response.dump(), "application/json"); });

    // GET /blockchain/blocks?limit=50&offset=0
    server.Get("/blockchain/blocks", [&](const httplib::Request &req, httplib::Response &res)
               {
    int limit = 50;
    int offset = 0;
    if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
    if (req.has_param("offset")) offset = std::stoi(req.get_param_value("offset"));

    auto blocks = blockchain.getBlocks(limit, offset);
    nlohmann::json j = nlohmann::json::array();
    for (auto &b : blocks) j.push_back(b.toJSON());

    set_cors(res);
    res.set_content(j.dump(4), "application/json"); });

    // GET /blockchain/block/:index
    server.Get(R"(/blockchain/block/(\d+))", [&](const httplib::Request &req, httplib::Response &res)
               {
    int idx = std::stoi(req.matches[1]);
    if (idx < 0 || idx >= (int)blockchain.getChain().size()) {
        res.status = 404;
        res.set_content("{\"error\":\"block not found\"}", "application/json");
        return;
    }
    Block b = blockchain.getBlockByIndex(idx);
    set_cors(res);
    res.set_content(b.toJSON().dump(4), "application/json"); });

    // GET /tx/:txid
    server.Get(R"(/tx/(.*))", [&](const httplib::Request &req, httplib::Response &res)
               {
    std::string txid = req.matches[1];
    Transaction tx = blockchain.getTransactionById(txid);
    if (tx.id.empty()) {
        res.status = 404;
        res.set_content("{\"success\":false,\"message\":\"tx not found\"}", "application/json");
        return;
    }
    set_cors(res);
    res.set_content(tx.toJSON().dump(4), "application/json"); });

    // GET /wallet/:wallet/history
    server.Get(R"(/wallet/(.*)/history)", [&](const httplib::Request &req, httplib::Response &res)
               {
    std::string wallet = req.matches[1];
    std::cout << "wallet id : " << wallet << std::endl;
    auto history = blockchain.getTransactionsForWallet(wallet);
    nlohmann::json j = nlohmann::json::array();
    for (auto &tx : history) j.push_back(tx.toJSON());
    
    nlohmann::json final_response = {
        {"success" , true},
        {"transactions" , j},
    };

    set_cors(res);
    res.set_content(final_response.dump(), "application/json"); });

    // GET /transactions/latest?limit=20
    server.Get("/transactions/latest", [&](const httplib::Request &req, httplib::Response &res)
               {
    int limit = 20;
    if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
    auto latest = blockchain.getLatestTransactions(limit);
    nlohmann::json j = nlohmann::json::array();
    for (auto &tx : latest) j.push_back(tx.toJSON());
    set_cors(res);
    res.set_content(j.dump(4), "application/json"); });

    server.Post("/buy", [&](const httplib::Request &req, httplib::Response &res)
                {

    std::string wallet;
    std::string usdStr;
    std::string cardNumber;

    if (req.is_multipart_form_data()) {
        if (req.form.has_field("wallet")) wallet = req.form.get_field("wallet");
        if (req.form.has_field("usd")) usdStr = req.form.get_field("usd");
        if (req.form.has_field("cardNumber")) cardNumber = req.form.get_field("cardNumber");
        } else{
        wallet = req.get_param_value("wallet");
        usdStr = req.get_param_value("usd");
        cardNumber = req.get_param_value("cardNumber");
    }


    if (wallet.empty() || usdStr.empty() || cardNumber.empty()) {
        nlohmann::json response = { {"success", false}, {"message", "Missing parameters"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    double usd = 0.0;
    try { usd = std::stod(usdStr); } catch (...) { usd = 0.0; }

    if (usd <= 0) {
        nlohmann::json response = { {"success", false}, {"message", "Invalid USD amount"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    if (!walletManager.walletExists(wallet)) {
        nlohmann::json response = { {"success", false}, {"message", "Wallet not found"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    // Mock charge card
    if (!mockChargeCard(cardNumber, usd)) {
        nlohmann::json response = { {"success", false}, {"message", "Card charge failed (mock)"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    // Convert and credit wallet
    double umaAmount = usdToUma(usd);

    walletManager.updateBalance(wallet, umaAmount);

    // create a confirmed transaction record: from "FIAT" to wallet
    Transaction tx("FIAT", wallet, umaAmount);
    tx.status = TxStatus::CONFIRMED;
    // optional: attach metadata about fiat amount
    // tx.meta["fiat_amount"] = usd; // if you added a meta field

    blockchain.addConfirmedTransaction(tx);

    double newBalance = walletManager.getBalance(wallet);

    nlohmann::json response = {
        {"success", true},
        {"message", "Purchase successful (mock)"},
        {"wallet", wallet},
        {"credited_uma", umaAmount},
        {"usd_charged", usd},
        {"new_balance", newBalance}
    };

    set_cors(res);
    res.set_content(response.dump(), "application/json"); });

    server.Post("/sell", [&](const httplib::Request &req, httplib::Response &res)
                {
    std::string wallet;
    std::string umaStr;
    std::string bankAccount;

    if (req.is_multipart_form_data()) {
        if (req.form.has_field("wallet")) wallet = req.form.get_field("wallet");
        if (req.form.has_field("uma")) umaStr = req.form.get_field("uma");
        if (req.form.has_field("bankAccount")) bankAccount = req.form.get_field("bankAccount");
        } else{
        wallet = req.get_param_value("wallet");
        umaStr = req.get_param_value("uma");
        bankAccount = req.get_param_value("bankAccount");
    }

    if (wallet.empty() || umaStr.empty() || bankAccount.empty()) {
        nlohmann::json response = { {"success", false}, {"message", "Missing parameters"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    double umaAmount = 0.0;
    try { umaAmount = std::stod(umaStr); } catch (...) { umaAmount = 0.0; }

    if (umaAmount <= 0) {
        nlohmann::json response = { {"success", false}, {"message", "Invalid UMA amount"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    if (!walletManager.walletExists(wallet)) {
        nlohmann::json response = { {"success", false}, {"message", "Wallet not found"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    // Check balance (use walletManager)
    double balance = blockchain.getEffectiveBalance(wallet);
    if (balance < umaAmount) {
        nlohmann::json response = { {"success", false}, {"message", "Insufficient UMA balance"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    // convert UMA to USD (mock)
    double usdAmount = umaToUsd(umaAmount);

    // Mock payout to bank
    if (!mockSendToBank(bankAccount, usdAmount)) {
        nlohmann::json response = { {"success", false}, {"message", "Bank payout failed (mock)"} };
        set_cors(res);
        return res.set_content(response.dump(), "application/json");
    }

    // debit user immediately
    walletManager.updateBalance(wallet, -umaAmount);

    // record a confirmed tx from wallet -> FIAT
    Transaction tx(wallet, "FIAT", umaAmount);
    tx.status = TxStatus::CONFIRMED;
    blockchain.addConfirmedTransaction(tx);

    double newBalance = walletManager.getBalance(wallet);

    nlohmann::json response = {
        {"success", true},
        {"message", "Sell executed (mock)"},
        {"wallet", wallet},
        {"debited_uma", umaAmount},
        {"usd_sent", usdAmount},
        {"new_balance", newBalance}
    };

    set_cors(res);
    res.set_content(response.dump(), "application/json"); });

    std::cout << "Server running on http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);

    return 0;
}