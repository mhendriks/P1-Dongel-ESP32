#pragma once

#define _VERSION_MAJOR 4
#define _VERSION_MINOR 15
#define _VERSION_PATCH 8
#define _VERSION_ONLY STR(_VERSION_MAJOR) "." STR(_VERSION_MINOR) "." STR(_VERSION_PATCH)

#define STR1(x) #x
#define STR(x) STR1(x)