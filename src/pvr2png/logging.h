#pragma once

namespace Pvr2Png {

/**
 * Send log messages to standard error as `LEVEL: message`, the format of Python's
 * `logging.basicConfig(format='%(levelname)s: %(message)s')`.
 *
 * @param debug Whether debug messages are shown. Information and higher levels are always shown.
 */
void setupLogging(bool debug);

} // namespace Pvr2Png
