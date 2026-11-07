#pragma once
// Boot default only. Web UI can change sensor participation until restart.
#ifndef BRYG_POT_ONLY
#define BRYG_POT_ONLY 0
#endif
static_assert(BRYG_POT_ONLY == 0 || BRYG_POT_ONLY == 1, "BRYG_POT_ONLY must be 0 or 1");
constexpr bool POT_ONLY_CONTROL = BRYG_POT_ONLY == 1;
