#include <DreamPicoPortApi.hpp>

std::shared_ptr<dpp_api::DppDevice> dppDevice = nullptr;

void read_complete(const std::string& errStr) {
  printf("DreamPicoPort disconnected%s%s\n", (errStr.empty() ? "" : ": "), errStr.c_str());
  fflush(stdout);
}

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
      if (!dppDevice->connect(read_complete)) {
        printf("DDP: Failed to connect: %s\n", dppDevice->getLastErrorStr().c_str());
        dppDevice.reset();
        return false;
      }
      if (!dppDevice->isConnected()) {
        printf("DDP: Not actually connected\n");
        dppDevice.reset();
        return false;
      }
    }
  }
  return dppDevice != nullptr;
}

extern "C" {

CommandResult dpp_send(uint32_t* dest, uint32_t* data, uint32_t len) {
  if (!init_ddp()) {  
    dest[0] = 0xFFFFFFFF;
    return {.result = dpp_api::msg::rx::Maple32::kCmdDisconnect, .words_transferred = 0};
  }

  std::vector<std::uint32_t> vec(data, data + len);
  const auto st = dppDevice->sendSync(dpp_api::msg::tx::Maple32{.packet = vec, .emu = true}, 500);
  // Result < 0 means error with the DPP connection. Positive values are Maple responses.
  if(st.cmd >= 0) {
    uint32_t addr = 0;
    for (const auto p : st.packet) {
      dest[addr++] = p;
    }
  } else {
    dppDevice.reset();
    dest[0] = 0xFFFFFFFF;
  }
  return {
    .result = st.cmd,
    .words_transferred = static_cast<uint32_t>(st.packet.size()),
  };
}

}