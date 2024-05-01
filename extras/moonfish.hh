/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#ifndef MOONFISH_TEXEL_TUNER
#define MOONFISH_TEXEL_TUNER

#define TAPERED 0

#include <string>

#include "texel-tuner/src/base.h"
#include "texel-tuner/src/external/chess.hpp"

namespace moonfish
{
	class MoonfishEval
	{
	public:
		constexpr static bool includes_additional_score = true;
		constexpr static bool supports_external_chess_eval = false;
		constexpr static bool retune_from_zero = true;
		constexpr static tune_t preferred_k = 2.1;
		constexpr static int32_t max_epoch = 5001;
		constexpr static bool enable_qsearch = false;
		constexpr static bool filter_in_check = false;
		constexpr static tune_t initial_learning_rate = 1;
		constexpr static int32_t learning_rate_drop_interval = 10000;
		constexpr static tune_t learning_rate_drop_ratio = 1;
		constexpr static bool print_data_entries = false;
		constexpr static int32_t data_load_print_interval = 10000;
		
		static parameters_t get_initial_parameters(void);
		static EvalResult get_fen_eval_result(std::string fen);
		static EvalResult get_external_eval_result(chess::Board board);
		static void print_parameters(parameters_t parameters);
	};
}

#endif
