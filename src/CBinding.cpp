#include <bit>

#include <DreamPicoPortApi.hpp>

extern "C" {

void read_complete(const std::string& errStr)
{
    printf("Disconnected%s%s\n", (errStr.empty() ? "" : ": "), errStr.c_str());
    fflush(stdout);
}

std::shared_ptr<dpp_api::DppDevice> dppDevice = nullptr;

struct CommandResult {
        int16_t result;
        uint32_t words_transferred;
};

bool init_ddp() {
        if (!dppDevice) {
                dpp_api::DppDevice::Filter filter;
                filter.minBcdDevice = 0x0120;
                dppDevice = dpp_api::DppDevice::find(filter);
                if (dppDevice) {
                        if (!dppDevice->connect(read_complete))
                        {
                                printf("Failed to connect: %s\n", dppDevice->getLastErrorStr().c_str());
                                return false;
                        }
                        if (!dppDevice->isConnected())
                        {
                                printf("Not actually connected\n");
                                return false;
                        }
                } else {
                        printf("not found :(\n");
                        return false;
                }
        }
        return true;
}

CommandResult dpp_send(uint8_t *dest, uint32_t *data, uint32_t len) {
        if(!init_ddp()) return { .result = -1, .words_transferred = 0 };
        
        if (dppDevice) {
                std::vector<std::uint32_t> vec(data, data + len);
                const auto st = dppDevice->sendSync(dpp_api::msg::tx::Maple32{ .packet = vec, .emu = true }, 500);
                if(st.cmd == dpp_api::msg::rx::Msg::kCmdSuccess) {
                        uint32_t addr = 0;
                        for(const auto p : st.packet) {
                                memcpy(&dest[addr], &p, 4);
                                addr += 4;
                        }
                }
                return { .result = st.cmd, .words_transferred = static_cast<uint32_t>(st.packet.size()) };
        } else {
                printf("not found :(\n");
        }
        return { .result = -1, .words_transferred = 0 };
}

std::mutex async_mutex{};
std::condition_variable async_cv;
std::uint32_t async_inflight_commands = 0;
std::uint32_t async_transferred_words = 0;

void ddp_async_reset() {
        std::lock_guard<std::mutex> lock(async_mutex);
        if(async_inflight_commands != 0) {
                printf("Warning: ddp_async_reset with %i commands in flight\n", async_inflight_commands);
                async_inflight_commands = 0;
        }
        async_transferred_words = 0;
}

void on_async_request()
{
    std::lock_guard<std::mutex> lock(async_mutex);
    ++async_inflight_commands;
}

void on_async_response(std::uint32_t transferred_words)
{
    std::lock_guard<std::mutex> lock(async_mutex);
    --async_inflight_commands;
    async_transferred_words += transferred_words;
    async_cv.notify_all();
}

bool ddp_wait_all_async_complete()
{
    std::unique_lock<std::mutex> lock(async_mutex);
    std::uint32_t* c = &async_inflight_commands;
    async_cv.wait_for(lock, std::chrono::milliseconds(5000), [c](){return *c == 0;});
    return async_inflight_commands == 0;
}

std::uint32_t ddp_async_get_transferred_words()
{
    std::lock_guard<std::mutex> lock(async_mutex);
    return async_transferred_words;
}

void dpp_send_async(uint8_t *dest, uint32_t *data, uint32_t len) {
        if(!init_ddp()) return;
        
        if (dppDevice) {
                std::vector<std::uint32_t> vec(data, data + len);
                on_async_request();
                dppDevice->send(
                        dpp_api::msg::tx::Maple32{ .packet = vec, .emu = true },             
                        [dest](typename dpp_api::msg::tx::Maple32::ResponseType& response)
                        {
                                if(response.cmd == dpp_api::msg::rx::Msg::kCmdSuccess) {
                                        uint32_t addr = 0;
                                        for(const auto p : response.packet) {
                                                // NOTE: In the sync case, the byteswap is done on the Zig side, but in the async case
                                                //       it is more annoying to get the result of each command, so I'm doing it here for now.
                                                *reinterpret_cast<uint32_t*>(&dest[addr]) = __builtin_bswap32(p);
                                                addr += 4;
                                        }
                                } else {
                                        *reinterpret_cast<uint32_t*>(dest) = 0xFFFFFFFF;
                                }
                                on_async_response(static_cast<uint32_t>(response.packet.size()));
                        }, 
                        500
                );
        } else {
                printf("not found :(\n");
        }
}

}