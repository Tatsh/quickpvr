#pragma once

#include <windows.h>

namespace QuickPvrShell {

/**
 * Register the thumbnail provider and preview handler for `.pvr` files for the current user, under
 * `HKEY_CURRENT_USER\Software\Classes`. Administrator rights are not needed.
 *
 * @return `S_OK`, or the registry error.
 */
HRESULT registerServer();

/**
 * Remove what registerServer() added. Handler entries that another program has since replaced are
 * not removed.
 *
 * @return `S_OK`, or the registry error.
 */
HRESULT unregisterServer();

} // namespace QuickPvrShell
