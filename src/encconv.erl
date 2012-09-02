-module(encconv).
-export([initialize/0, uninitialize/0, convert_binary/3, convert_binary/4,
         create_converter/3, destroy_converter/1, do_convert/2, flush_converter/1, reset_converter/1,
         convert_list/3, convert_list/4]).
-on_load(nifinit/0).

nifinit() ->
  NifFile = case os:type() of
    {win32, _} -> "encconv";
    _ -> "libencconv"
  end,
  LibName = case code:priv_dir(?MODULE) of
    {error, bad_name} ->
      case filelib:is_dir(filename:join(["..", "priv"])) of
        true ->
          filename:join(["..", "priv", NifFile]);
        false ->
          filename:join(["priv", NifFile])
      end;
    Dir ->
      filename:join(Dir, NifFile)
  end,
  ok = erlang:load_nif(LibName, 0).

% Always returns ok.
initialize() ->
	exit(nif_library_not_loaded).

% Always returns ok.
uninitialize() ->
	exit(nif_library_not_loaded).

% Returns {ok, ConvertedBin} when succeeded.
convert_binary(_Data, _InEnc, _OutEnc) ->
	exit(nif_library_not_loaded).

convert_binary(_Data, _InEnc, _OutEnc, _Option) ->
	exit(nif_library_not_loaded).

create_converter(_InEnc, _OutEnc, _Option) ->
	exit(nif_library_not_loaded).

destroy_converter(_Converter) ->
	exit(nif_library_not_loaded).

do_convert(_Data, _Converter) ->
	exit(nif_library_not_loaded).

flush_converter(_Converter) ->
	exit(nif_library_not_loaded).

reset_converter(_Converter) ->
	exit(nif_library_not_loaded).

convert_list(List, InEnc, OutEnc) ->
	case convert_binary(list_to_binary(List), InEnc, OutEnc) of
		{ok, Bin, Rest} -> {ok, binary_to_list(Bin), Rest};
		{error, _}=E -> E;
		_ -> {error, "Unexpected result."}
	end.

convert_list(List, InEnc, OutEnc, Option) ->
	case convert_binary(list_to_binary(List), InEnc, OutEnc, Option) of
		{ok, Bin, Rest} -> {ok, binary_to_list(Bin), Rest};
		{error, _}=E -> E;
		_ -> {error, "Unexpected result."}
	end.
