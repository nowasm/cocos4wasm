#pragma once

// Source-compat forwarder. The EditBox component was split into
// `EditBox` + `EditBoxImplBase` + `EditBoxImpl` + `TabIndexUtil`,
// mirroring the TS layout at `cocos/ui/editbox/`. Existing call sites
// that include <cocos/ui/components/EditBox.h> keep working.

#include "cocos/ui/editBox/EditBox.h"
