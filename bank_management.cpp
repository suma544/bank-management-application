// Bank Management Application
// C++ console application that simulates core banking operations using
// Object-Oriented Programming and file handling.
//
// OOP concepts used:
//   - Encapsulation : account data is private/protected, accessed via methods
//   - Abstraction   : abstract base class Account with pure virtual functions
//   - Inheritance   : SavingsAccount and CurrentAccount derive from Account
//   - Polymorphism  : withdraw rules / account type resolved at runtime
//
// Security in file storage:
//   - PINs are never stored in plain text; only a salted hash is saved
//   - The data file carries a checksum so tampering is detected on load
//   - Every transaction is appended to a separate log file

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <limits>
#include <iomanip>
#include <ctime>
#include <cstdint>
#include <algorithm>

using namespace std;

const string ACCOUNTS_FILE = "accounts.dat";
const string TRANSACTIONS_FILE = "transactions.txt";
const string SALT = "BMA#2026";

// ---------- Utility functions ----------
string fnvHash(const string &data) {
    uint64_t h = 1469598103934665603ULL;          // FNV-1a 64-bit
    for (unsigned char c : data) { h ^= c; h *= 1099511628211ULL; }
    stringstream ss;
    ss << hex << setw(16) << setfill('0') << h;
    return ss.str();
}

string now() {
    time_t t = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return buf;
}

string money(double v) {
    stringstream ss;
    ss << "Rs. " << fixed << setprecision(2) << v;
    return ss.str();
}

// ---------- Abstract base class ----------
class Account {
protected:
    int accNo;
    string holder;
    string pinHash;
    double balance;

public:
    Account(int no, string name, string hash, double bal)
        : accNo(no), holder(std::move(name)), pinHash(std::move(hash)), balance(bal) {}
    virtual ~Account() = default;

    // Pure virtual functions -> abstraction + polymorphism
    virtual string type() const = 0;
    virtual double minimumBalance() const = 0;

    int number() const { return accNo; }
    const string &name() const { return holder; }
    double getBalance() const { return balance; }

    static string hashPin(int accNo, const string &pin) {
        return fnvHash(SALT + to_string(accNo) + ":" + pin);
    }
    bool verifyPin(const string &pin) const { return hashPin(accNo, pin) == pinHash; }

    bool deposit(double amount) {
        if (amount <= 0) return false;
        balance += amount;
        return true;
    }

    // Common withdraw rule, limit decided by derived class (polymorphism)
    virtual bool withdraw(double amount, string &reason) {
        if (amount <= 0) { reason = "Amount must be positive."; return false; }
        if (balance - amount < minimumBalance()) {
            reason = "Insufficient funds. " + type() + " account must keep minimum balance of "
                     + money(minimumBalance()) + ". Max you can withdraw: "
                     + money(max(0.0, balance - minimumBalance()));
            return false;
        }
        balance -= amount;
        return true;
    }

    string serialize() const {
        stringstream ss;
        ss << accNo << '|' << type() << '|' << holder << '|' << pinHash << '|'
           << fixed << setprecision(2) << balance;
        return ss.str();
    }
};

// ---------- Derived classes ----------
class SavingsAccount : public Account {
public:
    using Account::Account;
    string type() const override { return "Savings"; }
    double minimumBalance() const override { return 500.0; }
};

class CurrentAccount : public Account {
public:
    using Account::Account;
    string type() const override { return "Current"; }
    double minimumBalance() const override { return 0.0; }
};

// ---------- Bank: manages accounts + file handling ----------
class Bank {
    vector<unique_ptr<Account>> accounts;
    int nextAccNo = 1001;

    static unique_ptr<Account> make(const string &type, int no, const string &name,
                                    const string &hash, double bal) {
        if (type == "Savings") return make_unique<SavingsAccount>(no, name, hash, bal);
        return make_unique<CurrentAccount>(no, name, hash, bal);
    }

    void load() {
        ifstream in(ACCOUNTS_FILE);
        if (!in) return;                               // first run: no file yet
        string header, line, body;
        getline(in, header);
        vector<string> lines;
        while (getline(in, line)) if (!line.empty()) { lines.push_back(line); body += line + "\n"; }

        string storedSum = header.size() > 9 ? header.substr(9) : "";
        if (header.rfind("CHECKSUM ", 0) != 0 || storedSum != fnvHash(SALT + body))
            cout << "WARNING: " << ACCOUNTS_FILE << " failed integrity check (file may be tampered).\n";

        for (const string &l : lines) {
            stringstream ss(l);
            string no, type, name, hash, bal;
            getline(ss, no, '|'); getline(ss, type, '|'); getline(ss, name, '|');
            getline(ss, hash, '|'); getline(ss, bal, '|');
            try {
                accounts.push_back(make(type, stoi(no), name, hash, stod(bal)));
                nextAccNo = max(nextAccNo, stoi(no) + 1);
            } catch (...) { /* skip corrupted record */ }
        }
    }

    void save() const {
        string body;
        for (const auto &a : accounts) body += a->serialize() + "\n";
        ofstream out(ACCOUNTS_FILE, ios::trunc);
        out << "CHECKSUM " << fnvHash(SALT + body) << "\n" << body;
    }

    static void logTransaction(int accNo, const string &kind, double amount, double balance) {
        ofstream log(TRANSACTIONS_FILE, ios::app);
        log << now() << '|' << accNo << '|' << kind << '|'
            << fixed << setprecision(2) << amount << '|' << balance << '\n';
    }

    Account *find(int no) {
        for (auto &a : accounts) if (a->number() == no) return a.get();
        return nullptr;
    }

public:
    Bank() { load(); }

    // ----- input helpers -----
    static int readInt(const string &prompt) {
        int v;
        while (true) {
            cout << prompt;
            if (cin >> v) { cin.ignore(numeric_limits<streamsize>::max(), '\n'); return v; }
            if (cin.eof()) exit(0);
            cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "  Invalid number, try again.\n";
        }
    }
    static double readAmount(const string &prompt) {
        double v;
        while (true) {
            cout << prompt;
            if (cin >> v && v > 0) { cin.ignore(numeric_limits<streamsize>::max(), '\n'); return v; }
            if (cin.eof()) exit(0);
            cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "  Enter a positive amount.\n";
        }
    }
    static string readLine(const string &prompt) {
        string s;
        while (true) {
            cout << prompt;
            if (!getline(cin, s)) exit(0);
            s.erase(remove(s.begin(), s.end(), '|'), s.end());
            if (!s.empty()) return s;
            cout << "  Cannot be empty.\n";
        }
    }
    static string readPin(const string &prompt) {
        while (true) {
            string p = readLine(prompt);
            if (p.size() == 4 && all_of(p.begin(), p.end(), ::isdigit)) return p;
            cout << "  PIN must be exactly 4 digits.\n";
        }
    }

    // Asks account number + PIN; returns account only if PIN is correct
    Account *login() {
        int no = readInt("Account number: ");
        string pin = readPin("4-digit PIN: ");
        Account *a = find(no);
        if (!a || !a->verifyPin(pin)) {
            cout << "Access denied: invalid account number or PIN.\n";
            return nullptr;
        }
        return a;
    }

    // ----- operations -----
    void createAccount() {
        cout << "\n--- Create Account ---\n";
        string name = readLine("Account holder name: ");
        int t;
        do { t = readInt("Account type (1 = Savings, 2 = Current): "); } while (t != 1 && t != 2);
        string pin = readPin("Set 4-digit PIN: ");
        double minBal = (t == 1) ? 500.0 : 0.0;
        double initial;
        while ((initial = readAmount("Initial deposit: ")) < minBal)
            cout << "  Savings account needs at least " << money(minBal) << ".\n";

        int no = nextAccNo++;
        accounts.push_back(make(t == 1 ? "Savings" : "Current", no, name,
                                Account::hashPin(no, pin), initial));
        save();
        logTransaction(no, "OPEN", initial, initial);
        cout << "Account created successfully! Your account number is " << no << ".\n";
    }

    void deposit() {
        cout << "\n--- Deposit ---\n";
        Account *a = login();
        if (!a) return;
        double amt = readAmount("Amount to deposit: ");
        a->deposit(amt);
        save();
        logTransaction(a->number(), "DEPOSIT", amt, a->getBalance());
        cout << "Deposited " << money(amt) << ". New balance: " << money(a->getBalance()) << "\n";
    }

    void withdraw() {
        cout << "\n--- Withdraw ---\n";
        Account *a = login();
        if (!a) return;
        double amt = readAmount("Amount to withdraw: ");
        string reason;
        if (!a->withdraw(amt, reason)) { cout << "Withdrawal failed. " << reason << "\n"; return; }
        save();
        logTransaction(a->number(), "WITHDRAW", amt, a->getBalance());
        cout << "Withdrew " << money(amt) << ". New balance: " << money(a->getBalance()) << "\n";
    }

    void balanceEnquiry() {
        cout << "\n--- Balance Enquiry ---\n";
        Account *a = login();
        if (!a) return;
        cout << "Account : " << a->number() << " (" << a->type() << ")\n"
             << "Holder  : " << a->name() << "\n"
             << "Balance : " << money(a->getBalance()) << "\n";
    }

    void transactionHistory() {
        cout << "\n--- Transaction History ---\n";
        Account *a = login();
        if (!a) return;
        ifstream in(TRANSACTIONS_FILE);
        string line;
        cout << left << setw(21) << "Date & Time" << setw(10) << "Type"
             << setw(14) << "Amount" << "Balance\n" << string(58, '-') << '\n';
        while (getline(in, line)) {
            stringstream ss(line);
            string when, no, kind, amt, bal;
            getline(ss, when, '|'); getline(ss, no, '|'); getline(ss, kind, '|');
            getline(ss, amt, '|'); getline(ss, bal, '|');
            if (no == to_string(a->number()))
                cout << left << setw(21) << when << setw(10) << kind
                     << setw(14) << money(stod(amt)) << money(stod(bal)) << '\n';
        }
    }

    void displayAll() const {
        cout << "\n--- All Accounts (" << accounts.size() << ") ---\n";
        if (accounts.empty()) { cout << "No accounts yet.\n"; return; }
        cout << left << setw(10) << "Acc No" << setw(20) << "Holder" << setw(10) << "Type"
             << "Balance\n" << string(55, '-') << '\n';
        for (const auto &a : accounts)
            cout << left << setw(10) << a->number() << setw(20) << a->name()
                 << setw(10) << a->type() << money(a->getBalance()) << '\n';
    }
};

int main() {
    Bank bank;
    while (true) {
        cout << "\n======= BANK MANAGEMENT APPLICATION =======\n"
             << "1. Create Account\n"
             << "2. Deposit\n"
             << "3. Withdraw\n"
             << "4. Balance Enquiry\n"
             << "5. Transaction History\n"
             << "6. Display All Accounts\n"
             << "7. Exit\n";
        switch (Bank::readInt("Enter your choice: ")) {
            case 1: bank.createAccount(); break;
            case 2: bank.deposit(); break;
            case 3: bank.withdraw(); break;
            case 4: bank.balanceEnquiry(); break;
            case 5: bank.transactionHistory(); break;
            case 6: bank.displayAll(); break;
            case 7: cout << "Thank you for banking with us. Data saved securely. Goodbye!\n"; return 0;
            default: cout << "Invalid choice. Please select 1-7.\n";
        }
    }
}
