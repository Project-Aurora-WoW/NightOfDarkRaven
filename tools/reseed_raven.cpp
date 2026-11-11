#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "SRP6.h"

// Пересчёт SRP6 salt/verifier для seed-аккаунта raven/raven.
// Печатает SQL UPDATE, который нужно выполнить в ndv_auth.
int main(int argc, char** argv)
{
    try
    {
        std::string username = (argc > 1) ? argv[1] : "raven";
        std::string password = (argc > 2) ? argv[2] : "raven";

        Salt salt = SRP6_GenerateSalt();
        Verifier verifier = SRP6_CalculateVerifier(username, password, salt);

        std::string saltHex = HexEncode(salt.data(), salt.size());
        std::string verHex = HexEncode(verifier.data(), verifier.size());

        std::cout << "UPDATE account SET salt = UNHEX('" << saltHex
                  << "'), verifier = UNHEX('" << verHex
                  << "') WHERE username = '" << Utf8ToUpperOnlyLatin(username) << "';\n";

        bool check = SRP6_VerifyPassword(username, password, salt, verifier);
        std::cerr << "self-check: " << (check ? "OK" : "FAILED") << "\n";
        return check ? 0 : 1;
    }
    catch (std::exception const& e)
    {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
