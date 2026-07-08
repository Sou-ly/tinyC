#pragma once

#include <stdbool.h>

#define OPTIONAL_OF(T) struct { bool present; T value; }

// Brace initializers with no type — cast at the use site: (OptionalExp)SOME(e).
#define SOME(v) { .present = true, .value = (v) }
#define NONE    { .present = false }
