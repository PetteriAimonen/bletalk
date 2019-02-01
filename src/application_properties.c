#include <application_properties.h>

__attribute__ ((section(".application_properties")))
const ApplicationProperties_t g_application_properties = {
  .magic = APPLICATION_PROPERTIES_MAGIC,
  .structVersion = APPLICATION_PROPERTIES_VERSION,
  .signatureType = APPLICATION_SIGNATURE_NONE,
  .signatureLocation = 0,

  .app = {
    .type = APPLICATION_TYPE_BLUETOOTH_APP,
    .version = 1,
    .capabilities = 0,
    .productId = { 0xa6, 0x79, 0xc7, 0xdd, 0xab, 0xee, 0xd8, 0xbc, 0xd3, 0x40, 0x9a, 0x53, 0x4e, 0xea, 0xbb, 0xe2 },
  },
};

