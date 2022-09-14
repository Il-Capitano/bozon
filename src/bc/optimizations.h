#ifndef BC_OPTIMIZATIONS_H
#define BC_OPTIMIZATIONS_H

#include "core.h"

#include <array>
#include <bz/u8string_view.h>

namespace bc
{

enum class optimization_kind
{
	aa,
	aa_eval,
	adce,
	aggressive_instcombine,
	alignment_from_assumptions,
	always_inline,
	annotation2metadata,
	annotation_remarks,
	assumption_cache_tracker,
	barrier,
	basic_aa,
	basiccg,
	bdce,
	block_freq,
	branch_prob,
	called_value_propagation,
	callsite_splitting,
	constmerge,
	correlated_propagation,
	dce,
	deadargelim,
	demanded_bits,
	div_rem_pairs,
	domtree,
	dse,
	early_cse,
	early_cse_memssa,
	elim_avail_extern,
	float2int,
	forceattrs,
	function_attrs,
	globaldce,
	globalopt,
	globals_aa,
	gvn,
	indvars,
	inferattrs,
	inject_tli_mappings,
	inline_,
	instcombine,
	instsimplify,
	ipsccp,
	jump_threading,
	lazy_block_freq,
	lazy_branch_prob,
	lazy_value_info,
	lcssa,
	lcssa_verification,
	libcalls_shrinkwrap,
	licm,
	loop_accesses,
	loop_deletion,
	loop_distribute,
	loop_idiom,
	loop_load_elim,
	loop_rotate,
	loop_simplify,
	loop_sink,
	loop_unroll,
	loop_vectorize,
	loops,
	lower_constant_intrinsics,
	lower_expect,
	mem2reg,
	memcpyopt,
	memdep,
	memoryssa,
	mldst_motion,
	openmp_opt_cgscc,
	opt_remark_emitter,
	phi_values,
	postdomtree,
	profile_summary_info,
	prune_eh,
	reassociate,
	rpo_function_attrs,
	scalar_evolution,
	sccp,
	scoped_noalias_aa,
	simplifycfg,
	slp_vectorizer,
	speculative_execution,
	sroa,
	strip_dead_prototypes,
	tailcallelim,
	targetlibinfo,
	tbaa,
	transform_warning,
	tti,
	vector_combine,
	verify,

	aggressive_consteval,

	_last,
};

struct optimization_info
{
	optimization_kind kind;
	bz::u8string_view name;
	bz::u8string_view description;
};

constexpr bz::array optimization_infos = []() {
	using result_t = bz::array<optimization_info, static_cast<size_t>(optimization_kind::_last)>;
	using T = optimization_info;
	result_t result = {
		T{ optimization_kind::aa,                         "aa",                         "Function Alias Analysis Results"                                           },
		T{ optimization_kind::aa_eval,                    "aa-eval",                    "Exhaustive Alias Analysis Precision Evaluator"                             },
		T{ optimization_kind::adce,                       "adce",                       "Aggressive Dead Code Elimination"                                          },
		T{ optimization_kind::aggressive_instcombine,     "aggressive-instcombine",     "Combine pattern based expressions"                                         },
		T{ optimization_kind::alignment_from_assumptions, "alignment-from-assumptions", "Alignment from assumptions"                                                },
		T{ optimization_kind::always_inline,              "always-inline",              "Inliner for always_inline functions"                                       },
		T{ optimization_kind::annotation2metadata,        "annotation2metadata",        "Annotation2Metadata"                                                       },
		T{ optimization_kind::annotation_remarks,         "annotation-remarks",         "Annotation Remarks"                                                        },
		T{ optimization_kind::assumption_cache_tracker,   "assumption-cache-tracker",   "Assumption Cache Tracker"                                                  },
		T{ optimization_kind::barrier,                    "barrier",                    "A No-Op Barrier Pass"                                                      },
		T{ optimization_kind::basic_aa,                   "basic-aa",                   "Basic Alias Analysis (stateless AA impl)"                                  },
		T{ optimization_kind::basiccg,                    "basiccg",                    "CallGraph Construction"                                                    },
		T{ optimization_kind::bdce,                       "bdce",                       "Bit-Tracking Dead Code Elimination"                                        },
		T{ optimization_kind::block_freq,                 "block-freq",                 "Block Frequency Analysis"                                                  },
		T{ optimization_kind::branch_prob,                "branch-prob",                "Branch Probability Analysis"                                               },
		T{ optimization_kind::called_value_propagation,   "called-value-propagation",   "Called Value Propagation"                                                  },
		T{ optimization_kind::callsite_splitting,         "callsite-splitting",         "Call-site splitting"                                                       },
		T{ optimization_kind::constmerge,                 "constmerge",                 "Merge Duplicate Global Constants"                                          },
		T{ optimization_kind::correlated_propagation,     "correlated-propagation",     "Value Propagation"                                                         },
		T{ optimization_kind::dce,                        "dce",                        "Dead Code Elimination"                                                     },
		T{ optimization_kind::deadargelim,                "deadargelim",                "Dead Argument Elimination"                                                 },
		T{ optimization_kind::demanded_bits,              "demanded-bits",              "Demanded bits analysis"                                                    },
		T{ optimization_kind::div_rem_pairs,              "div-rem-pairs",              "Hoist/decompose integer division and remainder"                            },
		T{ optimization_kind::domtree,                    "domtree",                    "Dominator Tree Construction"                                               },
		T{ optimization_kind::dse,                        "dse",                        "Dead Store Elimination"                                                    },
		T{ optimization_kind::early_cse,                  "early-cse",                  "Early CSE"                                                                 },
		T{ optimization_kind::early_cse_memssa,           "early-cse-memssa",           "Early CSE w/ MemorySSA"                                                    },
		T{ optimization_kind::elim_avail_extern,          "elim-avail-extern",          "Eliminate Available Externally Globals"                                    },
		T{ optimization_kind::float2int,                  "float2int",                  "Float to int"                                                              },
		T{ optimization_kind::forceattrs,                 "forceattrs",                 "Force set function attributes"                                             },
		T{ optimization_kind::function_attrs,             "function-attrs",             "Deduce function attributes"                                                },
		T{ optimization_kind::globaldce,                  "globaldce",                  "Dead Global Elimination"                                                   },
		T{ optimization_kind::globalopt,                  "globalopt",                  "Global Variable Optimizer"                                                 },
		T{ optimization_kind::globals_aa,                 "globals-aa",                 "Globals Alias Analysis"                                                    },
		T{ optimization_kind::gvn,                        "gvn",                        "Global Value Numbering"                                                    },
		T{ optimization_kind::indvars,                    "indvars",                    "Induction Variable Simplification"                                         },
		T{ optimization_kind::inferattrs,                 "inferattrs",                 "Infer set function attributes"                                             },
		T{ optimization_kind::inject_tli_mappings,        "inject-tli-mappings",        "Inject TLI Mappings"                                                       },
		T{ optimization_kind::inline_,                    "inline",                     "Function Integration/Inlining"                                             },
		T{ optimization_kind::instcombine,                "instcombine",                "Combine redundant instructions"                                            },
		T{ optimization_kind::instsimplify,               "instsimplify",               "Remove redundant instructions"                                             },
		T{ optimization_kind::ipsccp,                     "ipsccp",                     "Interprocedural Sparse Conditional Constant Propagation"                   },
		T{ optimization_kind::jump_threading,             "jump-threading",             "Jump Threading"                                                            },
		T{ optimization_kind::lazy_block_freq,            "lazy-block-freq",            "Lazy Block Frequency Analysis"                                             },
		T{ optimization_kind::lazy_branch_prob,           "lazy-branch-prob",           "Lazy Branch Probability Analysis"                                          },
		T{ optimization_kind::lazy_value_info,            "lazy-value-info",            "Lazy Value Information Analysis"                                           },
		T{ optimization_kind::lcssa,                      "lcssa",                      "Loop-Closed SSA Form Pass"                                                 },
		T{ optimization_kind::lcssa_verification,         "lcssa-verification",         "LCSSA Verifier"                                                            },
		T{ optimization_kind::libcalls_shrinkwrap,        "libcalls-shrinkwrap",        "Conditionally eliminate dead library calls"                                },
		T{ optimization_kind::licm,                       "licm",                       "Loop Invariant Code Motion"                                                },
		T{ optimization_kind::loop_accesses,              "loop-accesses",              "Loop Access Analysis"                                                      },
		T{ optimization_kind::loop_deletion,              "loop-deletion",              "Delete dead loops"                                                         },
		T{ optimization_kind::loop_distribute,            "loop-distribute",            "Loop Distribution"                                                         },
		T{ optimization_kind::loop_idiom,                 "loop-idiom",                 "Recognize loop idioms"                                                     },
		T{ optimization_kind::loop_load_elim,             "loop-load-elim",             "Loop Load Elimination"                                                     },
		T{ optimization_kind::loop_rotate,                "loop-rotate",                "Rotate Loops"                                                              },
		T{ optimization_kind::loop_simplify,              "loop-simplify",              "Canonicalize natural loops"                                                },
		T{ optimization_kind::loop_sink,                  "loop-sink",                  "Loop Sink"                                                                 },
		T{ optimization_kind::loop_unroll,                "loop-unroll",                "Unroll loops"                                                              },
		T{ optimization_kind::loop_vectorize,             "loop-vectorize",             "Loop Vectorization"                                                        },
		T{ optimization_kind::loops,                      "loops",                      "Natural Loop Information"                                                  },
		T{ optimization_kind::lower_constant_intrinsics,  "lower-constant-intrinsics",  "Lower constant intrinsics"                                                 },
		T{ optimization_kind::lower_expect,               "lower-expect",               "Lower 'expect' Intrinsics"                                                 },
		T{ optimization_kind::mem2reg,                    "mem2reg",                    "Promote memory to register"                                                },
		T{ optimization_kind::memcpyopt,                  "memcpyopt",                  "MemCpy Optimization"                                                       },
		T{ optimization_kind::memdep,                     "memdep",                     "Memory Dependence Analysis"                                                },
		T{ optimization_kind::memoryssa,                  "memoryssa",                  "Memory SSA"                                                                },
		T{ optimization_kind::mldst_motion,               "mldst-motion",               "MergedLoadStoreMotion"                                                     },
		T{ optimization_kind::openmp_opt_cgscc,           "openmp-opt-cgscc",           "OpenMP specific optimizations"                                             },
		T{ optimization_kind::opt_remark_emitter,         "opt-remark-emitter",         "Optimization Remark Emitter"                                               },
		T{ optimization_kind::phi_values,                 "phi-values",                 "Phi Values Analysis"                                                       },
		T{ optimization_kind::postdomtree,                "postdomtree",                "Post-Dominator Tree Construction"                                          },
		T{ optimization_kind::profile_summary_info,       "profile-summary-info",       "Profile summary info"                                                      },
		T{ optimization_kind::prune_eh,                   "prune-eh",                   "Remove unused exception handling info"                                     },
		T{ optimization_kind::reassociate,                "reassociate",                "Reassociate expressions"                                                   },
		T{ optimization_kind::rpo_function_attrs,         "rpo-function-attrs",         "Deduce function attributes in RPO"                                         },
		T{ optimization_kind::scalar_evolution,           "scalar-evolution",           "Scalar Evolution Analysis"                                                 },
		T{ optimization_kind::sccp,                       "sccp",                       "Sparse Conditional Constant Propagation"                                   },
		T{ optimization_kind::scoped_noalias_aa,          "scoped-noalias-aa",          "Scoped NoAlias Alias Analysis"                                             },
		T{ optimization_kind::simplifycfg,                "simplifycfg",                "Simplify the CFG"                                                          },
		T{ optimization_kind::slp_vectorizer,             "slp-vectorizer",             "SLP Vectorizer"                                                            },
		T{ optimization_kind::speculative_execution,      "speculative-execution",      "Speculatively execute instructions"                                        },
		T{ optimization_kind::sroa,                       "sroa",                       "Scalar Replacement Of Aggregates"                                          },
		T{ optimization_kind::strip_dead_prototypes,      "strip-dead-prototypes",      "Strip Unused Function Prototypes"                                          },
		T{ optimization_kind::tailcallelim,               "tailcallelim",               "Tail Call Elimination"                                                     },
		T{ optimization_kind::targetlibinfo,              "targetlibinfo",              "Target Library Information"                                                },
		T{ optimization_kind::tbaa,                       "tbaa",                       "Type-Based Alias Analysis"                                                 },
		T{ optimization_kind::transform_warning,          "transform-warning",          "Warn about non-applied transformations"                                    },
		T{ optimization_kind::tti,                        "tti",                        "Target Transform Information"                                              },
		T{ optimization_kind::vector_combine,             "vector-combine",             "Optimize scalar/vector ops"                                                },
		T{ optimization_kind::verify,                     "verify",                     "Module Verifier"                                                           },

		T{ optimization_kind::aggressive_consteval, "aggressive-consteval", "Try to evaluate all expressions at compile time" },
	};

	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const &rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

} // namespace bc

#endif // BC_OPTIMIZATIONS_H
