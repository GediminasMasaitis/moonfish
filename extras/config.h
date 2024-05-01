/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#ifndef MOONFISH_TEXEL_TUNER_CONFIG
#define MOONFISH_TEXEL_TUNER_CONFIG

#include "moonfish.hh"

using TuneEval = moonfish::MoonfishEval;
constexpr int data_load_thread_count = 4;
constexpr int thread_count = 4;
constexpr static bool print_data_entries = false;
constexpr static int data_load_print_interval = 10000;

#endif
