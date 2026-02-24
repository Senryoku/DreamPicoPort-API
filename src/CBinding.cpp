#include <DreamPicoPortApi.hpp>

extern "C" {

void read_complete(const std::string& errStr)
{
    printf("Disconnected%s%s\n", (errStr.empty() ? "" : ": "), errStr.c_str());
    fflush(stdout);
}

std::shared_ptr<dpp_api::DppDevice> dppDevice = nullptr;

uint32_t dpp_send(uint8_t *dest, uint32_t *data, uint32_t len) {
        if (!dppDevice) {
                dpp_api::DppDevice::Filter filter;
                filter.minBcdDevice = 0x0120;
                dppDevice = dpp_api::DppDevice::find(filter);
                if (!dppDevice->connect(read_complete))
                {
                        printf("Failed to connect: %s\n", dppDevice->getLastErrorStr().c_str());
                        return 0;
                }
                if (!dppDevice->isConnected())
                {
                        printf("Not actually connected\n");
                        return 0;
                }
        }
        
        if (dppDevice) {

                std::vector<std::uint32_t> vec(data, data + len);
                // data[0] &= 0xFF3F3FFF;
                printf("Sending: %u\n", vec.size());
                for(const auto p : vec) {
                        printf("  %08X\n", p);
                }
                const auto st = dppDevice->sendSync(dpp_api::msg::tx::Maple32{ .packet = vec, .emu = true }, 500);
                printf("Result: %X\n", st.cmd);
                if(st.cmd == dpp_api::msg::rx::Msg::kCmdSuccess) {
                        uint32_t addr = 0;
                        for(const auto p : st.packet) {
                                printf("  %08X ", p);
                                memcpy(&dest[addr], &p, 4);
                                addr += 4;
                        }
                        printf("\n");
                        return st.packet.size();
                }
        } else {
                printf("not found :(\n");
        }
        return 0;
}

}