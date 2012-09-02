#include "erl_nif.h"
#include "encconv.h"
#include <string>
#include <cstdlib>
#include <memory>
#include <iostream>

#ifdef WIN32
#include <Windows.h>
#endif

using portpp::EncodingConverter;

#ifdef WIN32 && !defined(PORTPP_USE_LIBICONV)
static EncodingConverter* create_converter_object(
    const char* inenc, const char* outenc, EncodingConverter::OPTION opt)
{
    return new EncodingConverter(inenc, outenc, opt);
}

static EncodingConverter* create_converter_noabort(
    const char* inenc, const char* outenc, EncodingConverter::OPTION opt)
{
    // If COM has not been initialized, creating an EncodingConverter instance causes the emulator to abort.
    // We use SEH to avoid it.
    __try {
        return create_converter_object(inenc, outenc, opt);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
#else
#define create_converter_noabort(INENC, OUTENC, OPT) new EncodingConverter(INENC, OUTENC, OPT)
#endif

inline static bool binary_to_string(ErlNifEnv* env, ERL_NIF_TERM term, std::string& out)
{
    ErlNifBinary bin;

    memset(&bin, 0, sizeof(bin));
    if (!enif_inspect_binary(env, term, &bin)) {
        return false;
    }

    out.assign(reinterpret_cast<const char*>(bin.data), bin.size);
    return true;
}

inline static bool string_to_binary(ErlNifEnv* env, const std::string& in, ERL_NIF_TERM& term)
{
    ErlNifBinary bin;

    memset(&bin, 0, sizeof(bin));
    size_t len = in.length();
    if (!enif_alloc_binary(len, &bin)) {
        return false;
    }

    if (len > 0) {
        memcpy(bin.data, &in[0], len);
    }

    term = enif_make_binary(env, &bin);

    return true;
}

inline static bool parse_option_list(ErlNifEnv* env, ERL_NIF_TERM lst, EncodingConverter::OPTION& opt)
{
    char optstr[32];
    ERL_NIF_TERM head;

    if (!enif_is_list(env, lst)) {
        return false;
    }

    opt = EncodingConverter::CONVERT_NONE;

    while (enif_get_list_cell(env, lst, &head, &lst)) {
        if (enif_get_atom(env, head, optstr, sizeof(optstr), ERL_NIF_LATIN1) <= 0) {
            return false;
        }
        if (strcmp("translit", optstr) == 0) {
            opt = (EncodingConverter::OPTION)(opt | EncodingConverter::CONVERT_TRANSLITERATE);
        } else if (strcmp("ignore", optstr) == 0) {
            opt = (EncodingConverter::OPTION)(opt | EncodingConverter::CONVERT_DISCARD_ILSEQ);
        } else {
            return false;
        }
    }

    return true;
}

inline static ERL_NIF_TERM convert_internal(
    ErlNifEnv* env,
    const std::string& in, std::string& out,
    const char* inenc, const char* outenc, EncodingConverter::OPTION opt)
{
    EncodingConverter* conv = 0;
	ERL_NIF_TERM ret = 0;

	do {
		conv = create_converter_noabort(inenc, outenc, opt);
		if (!conv) {
			// Failed to create a converter. Probably initialize() has not been called yet.
			ret = enif_make_tuple2(
				env,
				enif_make_atom(env, "error"),
				enif_make_string(env, "Can't create a converter. Probably you haven't called initialize() yet.", ERL_NIF_LATIN1));
			break;
		}
		if (!conv->valid()) {
			// The converter is not valid. Any of the specified encodings may be wrong/unsupported.
			ret = enif_make_tuple2(
				env,
				enif_make_atom(env, "error"),
				enif_make_string(env,
					(std::string("Unknown encoding or conversion not supported: ") + inenc + " or " + outenc).c_str(), ERL_NIF_LATIN1));
			break;
		}

		// Do conversion
		size_t inlen = in.length();
		out.assign(conv->convert(in.c_str(), inlen));
		out += conv->flush();

		if (inlen > 0 &&
			(opt & EncodingConverter::CONVERT_DISCARD_ILSEQ) == 0)
		{
			// The input was not fully consumed.
			ret = enif_make_tuple2(
				env,
				enif_make_atom(env, "error"),
				enif_make_string(env, "Incomplete/invalid input.", ERL_NIF_LATIN1));
			break;
		}

		if (string_to_binary(env, out, ret)) {
			// Success
			ret = enif_make_tuple3(env, enif_make_atom(env, "ok"), ret, enif_make_uint64(env, inlen));
		} else {
			// Running out of memory?
			ret = enif_make_tuple2(env, enif_make_atom(env, "error"),
				enif_make_string(env, "Unable to make binary.", ERL_NIF_LATIN1));
		}
	} while (false);

	if (conv) delete conv;
	return ret;
}

static ERL_NIF_TERM convert_binary_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    std::string in;
    std::string out;
    char inenc[64];
    char outenc[64];

    if (!binary_to_string(env, argv[0], in) ||
        enif_get_string(env, argv[1], inenc, sizeof(inenc), ERL_NIF_LATIN1) <= 0 ||
        enif_get_string(env, argv[2], outenc, sizeof(outenc), ERL_NIF_LATIN1) <= 0) {
            return enif_make_badarg(env);
    }

    return convert_internal(env, in, out, inenc, outenc, EncodingConverter::CONVERT_DISCARD_ILSEQ);
}

static ERL_NIF_TERM convert_binary_opt_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    std::string in;
    std::string out;
    char inenc[64];
    char outenc[64];
    EncodingConverter::OPTION opt;

    if (!binary_to_string(env, argv[0], in) ||
        enif_get_string(env, argv[1], inenc, sizeof(inenc), ERL_NIF_LATIN1) <= 0 ||
        enif_get_string(env, argv[2], outenc, sizeof(outenc), ERL_NIF_LATIN1) <= 0)
    {
        return enif_make_badarg(env);
    }

    if (!parse_option_list(env, argv[3], opt)) {
        return enif_make_tuple2(
            env,
            enif_make_atom(env, "error"),
            enif_make_string(env, "Unknown option.", ERL_NIF_LATIN1));
    }

    return convert_internal(env, in, out, inenc, outenc, opt);
}

static ERL_NIF_TERM create_converter_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    char inenc[64];
    char outenc[64];
    EncodingConverter::OPTION opt;

    if (enif_get_string(env, argv[0], inenc, sizeof(inenc), ERL_NIF_LATIN1) <= 0 ||
        enif_get_string(env, argv[1], outenc, sizeof(outenc), ERL_NIF_LATIN1) <= 0)
    {
        return enif_make_badarg(env);
    }

    if (!parse_option_list(env, argv[2], opt)) {
        return enif_make_tuple2(
            env,
            enif_make_atom(env, "error"),
            enif_make_string(env, "Unknown option.", ERL_NIF_LATIN1));
    }

    EncodingConverter* conv = create_converter_noabort(inenc, outenc, opt);

    if (!conv) {
        return enif_make_tuple2(
            env,
            enif_make_atom(env, "error"),
            enif_make_string(env, "Can't create a converter. Probably you haven't called initialize() yet.", ERL_NIF_LATIN1));
    }
    if (!conv->valid()) {
        delete conv;
        return enif_make_tuple2(
            env,
            enif_make_atom(env, "error"),
            enif_make_string(env,
                (std::string("Unknown encoding or conversion not supported: ") + inenc + " or " + outenc).c_str(), ERL_NIF_LATIN1));
    }

    return enif_make_tuple2(
        env, enif_make_atom(env, "ok"), enif_make_uint64(env, reinterpret_cast<ErlNifUInt64>(conv)));
}

static ERL_NIF_TERM destroy_converter_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ErlNifUInt64 opaq = 0;
    EncodingConverter* conv = 0;

    if (!enif_get_uint64(env, argv[0], &opaq) || !opaq) {
        return enif_make_badarg(env);
    }

    conv = reinterpret_cast<EncodingConverter*>(opaq);
    delete conv;

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM do_convert_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    std::string in;
    ErlNifUInt64 opaq = 0;
    EncodingConverter* conv = 0;

    if (!binary_to_string(env, argv[0], in) ||
        !enif_get_uint64(env, argv[1], &opaq) || !opaq) {
            return enif_make_badarg(env);
    }
    conv = reinterpret_cast<EncodingConverter*>(opaq);

    size_t inlen = in.length();
    std::string out = conv->convert(in.c_str(), inlen);

    ERL_NIF_TERM ret = 0;
    if (string_to_binary(env, out, ret)) {
        return enif_make_tuple3(env, enif_make_atom(env, "ok"), ret, enif_make_uint64(env, inlen));
    } else {
        return enif_make_tuple2(
            env, enif_make_atom(env, "error"),
            enif_make_string(env, "Unable to make binary.", ERL_NIF_LATIN1));
    }
}

static ERL_NIF_TERM flush_converter_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ErlNifUInt64 opaq = 0;
    EncodingConverter* conv = 0;

    if (!enif_get_uint64(env, argv[0], &opaq) || !opaq) {
        return enif_make_badarg(env);
    }
    conv = reinterpret_cast<EncodingConverter*>(opaq);

    std::string out = conv->flush();
    ERL_NIF_TERM ret = 0;
    if (string_to_binary(env, out, ret)) {
        return enif_make_tuple2(env, enif_make_atom(env, "ok"), ret);
    } else {
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_string(env, "Unable to make binary.", ERL_NIF_LATIN1));
    }
}

static ERL_NIF_TERM reset_converter_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ErlNifUInt64 opaq = 0;
    EncodingConverter* conv = 0;

    if (!enif_get_uint64(env, argv[0], &opaq) || !opaq) {
        return enif_make_badarg(env);
    }
    conv = reinterpret_cast<EncodingConverter*>(opaq);

    conv->reset();

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM initialize_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
#ifdef WIN32
    CoInitialize(NULL);
#endif
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM uninitialize_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
#ifdef WIN32
    CoUninitialize();
#endif
    return enif_make_atom(env, "ok");
}

static ErlNifFunc nif_funcs[] = {
    {"initialize", 0, initialize_nif},
    {"uninitialize", 0, uninitialize_nif},
    {"convert_binary", 3, convert_binary_nif},
    {"convert_binary", 4, convert_binary_opt_nif},
    {"create_converter", 3, create_converter_nif},
    {"destroy_converter", 1, destroy_converter_nif},
    {"do_convert", 2, do_convert_nif},
    {"flush_converter", 1, flush_converter_nif},
    {"reset_converter", 1, reset_converter_nif}
};

ERL_NIF_INIT(encconv, nif_funcs, NULL, NULL, NULL, NULL)
