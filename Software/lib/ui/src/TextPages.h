#ifndef UI_TEXT_PAGES_H
#define UI_TEXT_PAGES_H

#include "DisplayTypes.h"
#include "Strings.h"

namespace ui {
    namespace pages {

        using namespace ui::strings;

        static const TextPage helpPage = {
            .title = helpTitle,
            .body = helpBody,
            .bottomText = helpBottom,
            .qrUrl = helpQr,
        };

        static const TextPage updateCheckingPage = {
            .body = updateChecking,
        };

        static const TextPage noUpdatePage = {
            .body = noUpdateBody,
            .bottomText = noUpdateBottom,
        };

        static const TextPage updatingPage = {
            .title = updatingTitle,
            .body = updatingBody,
        };

        static const TextPage errorPage = {
            .title = error,
        };

        static const TextPage wifiDisconnectedPage = {
            .title = wifiSetup,
            .body = wifiBody,
            .bottomText = wifiBottom,
            .qrUrl = wifiQr,
        };

        static const TextPage wifiConnectedPage = {
            .title = wifiSetup,
            .body = wifiConnected,
            .bottomText = longPressReset,
        };

        static const TextPage pairingPage = {
            .title = pairingTitle,
            .body = pairingBody,
            .bottomText = skip,
        };

        static const TextPage pairingConnectingPage = {
            .title = pairing,
            .body = pairingConnecting,
            .bottomText = skip,

        };

        static const TextPage pairingSuccessPage = {
            .title = pairedTitle,
            .body = pairedBody,
            .bottomText = skip,
        };

    }  // namespace pages
}  // namespace ui

#endif  // UI_TEXT_PAGES_H
