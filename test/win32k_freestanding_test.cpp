#include <cassert>
#include <cstdint>
#include <cstring>

#include "micant/km/win32k/freestanding.hpp"

namespace {

MicaWin32kRequest makeRequest(uint32_t operation) {
    MicaWin32kRequest request{};
    request.size = sizeof(request);
    request.version = MICANT_WIN32K_REQUEST_VERSION;
    request.operation = operation;
    return request;
}

void setText(uint16_t (&destination)[MICANT_WIN32K_TEXT_CAPACITY],
             const char* text) {
    size_t index = 0;
    while (text[index] && index + 1 < MICANT_WIN32K_TEXT_CAPACITY) {
        destination[index] = static_cast<uint8_t>(text[index]);
        ++index;
    }
}

} // namespace

int main() {
    using micant::NtStatus;
    using micant::win32k::freestanding::dispatchRequest;

    auto registration = makeRequest(MICANT_WIN32K_OP_REGISTER_CLASS);
    setText(registration.className, "MicaNT.Explorer.Desktop");
    assert(dispatchRequest(registration) == NtStatus::Success);
    assert(registration.result != 0);

    auto creation = makeRequest(MICANT_WIN32K_OP_CREATE_WINDOW);
    setText(creation.className, "MicaNT.Explorer.Desktop");
    setText(creation.title, "MicaNT Desktop");
    creation.args[1] = 0x10000000;
    creation.args[2] = 10;
    creation.args[3] = 20;
    creation.args[4] = 640;
    creation.args[5] = 480;
    assert(dispatchRequest(creation) == NtStatus::Success);
    const uint64_t window = creation.result;
    assert(window != 0);

    auto clientRect = makeRequest(MICANT_WIN32K_OP_GET_CLIENT_RECT);
    clientRect.args[0] = window;
    assert(dispatchRequest(clientRect) == NtStatus::Success);
    assert(clientRect.rect.left == 0 && clientRect.rect.top == 0);
    assert(clientRect.rect.right == 640 && clientRect.rect.bottom == 480);

    auto show = makeRequest(MICANT_WIN32K_OP_SHOW_WINDOW);
    show.args[0] = window;
    show.args[1] = 5;
    assert(dispatchRequest(show) == NtStatus::Success);

    auto position = makeRequest(MICANT_WIN32K_OP_SET_WINDOW_POS);
    position.args[0] = window;
    position.args[1] = 30;
    position.args[2] = 40;
    position.args[3] = 800;
    position.args[4] = 600;
    assert(dispatchRequest(position) == NtStatus::Success);

    auto windowRect = makeRequest(MICANT_WIN32K_OP_GET_WINDOW_RECT);
    windowRect.args[0] = window;
    assert(dispatchRequest(windowRect) == NtStatus::Success);
    assert(windowRect.rect.left == 30 && windowRect.rect.top == 40);
    assert(windowRect.rect.right == 830 && windowRect.rect.bottom == 640);

    auto post = makeRequest(MICANT_WIN32K_OP_POST_MESSAGE);
    post.args[0] = window;
    post.args[1] = 0x0100;
    post.args[2] = 'M';
    assert(dispatchRequest(post) == NtStatus::Success);

    auto peek = makeRequest(MICANT_WIN32K_OP_PEEK_MESSAGE);
    peek.args[0] = window;
    peek.args[3] = 1;
    assert(dispatchRequest(peek) == NtStatus::Success);
    assert(peek.result == 1 && peek.messageId == 0x0100);
    assert(peek.messageWParam == 'M');

    auto enumerate = makeRequest(MICANT_WIN32K_OP_ENUM_WINDOWS);
    assert(dispatchRequest(enumerate) == NtStatus::Success);
    assert(enumerate.result == 1 && enumerate.windowHandles[0] == window);

    auto getText = makeRequest(MICANT_WIN32K_OP_GET_WINDOW_TEXT);
    getText.args[0] = window;
    assert(dispatchRequest(getText) == NtStatus::Success);
    assert(getText.result == std::strlen("MicaNT Desktop"));
    assert(getText.text[0] == 'M');

    auto destroy = makeRequest(MICANT_WIN32K_OP_DESTROY_WINDOW);
    destroy.args[0] = window;
    assert(dispatchRequest(destroy) == NtStatus::Success);
    assert(dispatchRequest(clientRect) == NtStatus::InvalidHandle);
}
