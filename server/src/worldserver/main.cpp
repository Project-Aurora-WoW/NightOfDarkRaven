#include <iostream>
#include <string>
#include "AuthShared.h"
#include "Network.h"
#include "Opcodes.h"

class WorldSession : public Session
{
public:
    using Session::Session;

protected:
    void HandlePacket(uint16_t opcode, ByteBuffer& p) override
    {
        switch (opcode)
        {
            case CMSG_WORLD_AUTH:
            {
                uint32_t token; p >> token;
                uint32_t shared = readSharedToken();
                bool ok = (shared != 0 && token == shared);
                ByteBuffer out; out << uint8_t(ok ? AUTH_OK : AUTH_BAD_TOKEN);
                Send(SMSG_WORLD_AUTH_RESULT, out);
                break;
            }
            case CMSG_PING:
            {
                uint32_t c; p >> c;
                ByteBuffer out; out << c;
                Send(SMSG_PONG, out);
                break;
            }
            case CMSG_PLAYER_LOGIN:
            {
                std::string name; p >> name;
                ByteBuffer out;
                out << uint32_t(0) << 0.f << 0.f << 100.f << 0.f;
                Send(SMSG_LOGIN_VERIFY_WORLD, out);
                break;
            }
            default: break;
        }
    }

private:
    bool _authed = false;
};

int main()
{
    WinsockInitializer winsock;
    Acceptor<WorldSession> acceptor(8085);
    std::cout << "worldserver listening on 8085\n";
    acceptor.Run();
    return 0;
}
