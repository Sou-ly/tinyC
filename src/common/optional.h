#pragma once

#include <stdbool.h>

// Tagged, like LIST_TYPE: an anonymous optional is a fresh incompatible type at
// every expansion, so it can never be passed or assigned.
// Value payloads only -- for pointers, use NULL.
#define OPTIONAL_TYPE(Name, T) typedef struct Name { bool present; T value; } Name

#define SOME(Name, v) ((Name){ .present = true, .value = (v) })
#define NONE(Name)    ((Name){ .present = false })
