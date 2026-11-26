#pragma once

#include <windows.h>

namespace QuickPvrShell {

/** Where the handlers are registered. */
enum class Scope {
    /** `HKEY_CURRENT_USER`. Administrator rights are not needed. */
    User,
    /**
     * `HKEY_LOCAL_MACHINE`, for every user. Administrator rights are needed. Windows loads property
     * handlers (for the Details tab and the details pane) only from this scope.
     */
    Machine,
};

/**
 * Register the handlers for `.pvr` files. Both scopes register the thumbnail provider and the
 * preview handler. Scope::Machine also registers the property handler, the `QuickPVR.propdesc`
 * schema next to this DLL, and the properties Explorer shows.
 *
 * @param scope Where to register.
 * @return `S_OK`, or the registry or property schema error.
 */
HRESULT registerServer(Scope scope);

/**
 * Remove what registerServer() added in a scope. Handler entries that another program has since
 * replaced are not removed.
 *
 * @param scope Where to remove the registration from.
 * @return `S_OK`.
 */
HRESULT unregisterServer(Scope scope);

} // namespace QuickPvrShell
