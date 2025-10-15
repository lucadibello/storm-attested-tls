#include "oe_enclave_utility.h"

#include <cstdio>
#include <openenclave/bits/module.h>

#define LOG_PREFIX_OE_ENCLAVE_UTILITY "[oe_enclave_utility] "

namespace oe_enclave_utility {
    oe_result_t load_oe_modules()
    {
        oe_result_t result = OE_OK;

        // Host-side: load host resolver and socket interface
        if ((result = oe_load_module_host_resolver()) != OE_OK)
        {
            std::printf(
                "oe_load_module_host_resolver failed with %s\n",
                oe_result_str(result));
            return result;
        }
        if ((result = oe_load_module_host_socket_interface()) != OE_OK)
        {
            printf(
                "oe_load_module_host_socket_interface failed with %s\n",
                oe_result_str(result));
            return result;
        }
        printf(LOG_PREFIX_OE_ENCLAVE_UTILITY "Successfully loaded required Open Enclave modules\n");
        return result;
    }
}
