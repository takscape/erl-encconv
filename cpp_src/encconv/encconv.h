/*
** The author disclaims copyright to this source code.
** In place of a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/
/*
** Any feedback would be appreciated.
** mailto:k-tak@void.in
*/
#ifndef ___PORTPP_ENCCONV_H___
#define ___PORTPP_ENCCONV_H___

#if defined(_WIN32) && !defined(PORTPP_USE_LIBICONV)
#   include <windows.h>
#   include <objbase.h>
#   include <mlang.h>
#else
#   include <iconv.h>
#endif

#include <string>
#include <cstring>
#include <cctype>


namespace portpp {


	class EncodingConverter
	{
	public:
		enum OPTION
		{
			CONVERT_NONE			= 0, // No options.
			CONVERT_TRANSLITERATE	= 1, // Transliterate characters which do not exist in destination charset.
			CONVERT_DISCARD_ILSEQ	= 2, // Discard invalid byte sequences.
		};

	protected:
		std::string		fromEnc_;
		std::string		toEnc_;
		OPTION			opt_;

#if defined(_WIN32) && !defined(PORTPP_USE_LIBICONV)
		DWORD					toCodePage_;
		DWORD					fromCodePage_;
		IMultiLanguage2*		ml_;
		IMLangConvertCharset*	conv_;

		DWORD encNameToCodePage(const char* encName);
#else
		iconv_t					cd_;
#endif

	public:
		/**
		* Constructor.
		* @param fromEnc Source encoding.
		* @param toEnc Destination encoding.
		* @param opt Options. Bit-wise ORed combination of CONVERT_*.
		*/
		EncodingConverter(const char* fromEnc, const char* toEnc, OPTION opt);
		virtual ~EncodingConverter();

		/**
		* Returns the source encoding.
		* @return Source encoding.
		*/
		std::string fromEncoding() const { return fromEnc_; }
		/**
		* Returns the destination encoding.
		* @return Destination encoding.
		*/
		std::string toEncoding() const { return toEnc_; }

		/**
		* Returns true if the converter was successfully initialized.
		* @return true/false.
		*/
		bool valid() const;
		/**
		* Converts input and stores result into output.
		* @param input [in] Input byte sequence.
		* @param inputBytesLeft [in/out] Size of input in bytes.
		*        It will be subtracted by the number of bytes consumed when the method returns.
		* @param output [out] A buffer to be stored with output byte sequence.
		* @param outputBytesLeft [in/out] Size of output in bytes.
		*        It will be subtracted by the number of bytes stored when the method returns.
		* @return true if succeeded.
		*/
		bool convert(const void* input, size_t& inputBytesLeft,
			void* output, size_t& outputBytesLeft);
		/**
		* Flushes any shift string.
		* This method is meaningful only when the destination encoding is a stateful encoding such as ISO-2022-JP.
		* @param output [out] A buffer to be stored with output byte sequence.
		* @param outputBytesLeft [in/out] Size of output in bytes.
		*        It will be subtracted by the number of bytes stored when the method returns.
		* @return true if succeeded.
		*/
		bool flush(void* output, size_t& outputBytesLeft);
		/**
		* Reinitializes the internal state of the converter.
		*/
		void reset();


		///////////////////////////////////////////////////
		// Convenience methods
		/**
		* Converts input and returns result as std::string.
		*/
		std::string convert(const void* input, size_t& inputBytesLeft)
		{
			std::string ret;
			char buf[1024];
			size_t buflen, prevlen;

			do {
				prevlen = inputBytesLeft;
				buflen = sizeof(buf);
				if (!convert(input, inputBytesLeft, buf, buflen)) {
					break;
				}
				ret.append(buf, sizeof(buf)-buflen);
			} while (inputBytesLeft > 0);

			return ret;
		}
		/**
		* Flushes any shift string and returns it as std::string.
		*/
		std::string flush()
		{
			char buf[1024];
			size_t buflen = sizeof(buf);

			flush(buf, buflen);

			return std::string(buf, sizeof(buf)-buflen);
		}
	};



#if defined(_WIN32) && !defined(PORTPP_USE_LIBICONV)

	inline EncodingConverter::EncodingConverter(const char* fromEnc, const char* toEnc, OPTION opt)
	{
		ml_ = 0;
		conv_ = 0;
		opt_ = opt;

		if (FAILED(CoCreateInstance(CLSID_CMultiLanguage, NULL,
			CLSCTX_INPROC_SERVER, IID_IMultiLanguage2, (void**)&ml_)))
		{
			ml_ = 0;
		}

		fromEnc_ = fromEnc;
		toEnc_ = toEnc;
		toCodePage_ = encNameToCodePage(toEnc_.c_str());
		fromCodePage_ = encNameToCodePage(fromEnc_.c_str());

		DWORD prop = MLCONVCHARF_NOBESTFITCHARS;
		if (opt & CONVERT_TRANSLITERATE) {
			prop &= ~(DWORD)MLCONVCHARF_NOBESTFITCHARS;
		}
		if (ml_) {
			HRESULT hr = ml_->CreateConvertCharset(fromCodePage_, toCodePage_, prop, &conv_);
			if (hr != S_OK) {
				// hr can be S_FALSE, which means conversion is not supported.
				if (hr == S_FALSE) {
					conv_->Release();
				}
				conv_ = 0;
				ml_->Release();
				ml_ = 0;
			}
		}

		//////////////////////////////////////
		//   IMultiLanguage* ml1 = 0;
		//   IEnumCodePage* cp = 0;

		//   ml_->QueryInterface(IID_IMultiLanguage, (void**)&ml1);
		//   ml1->EnumCodePages(MIMECONTF_MIME_IE4, &cp);

		//   PMIMECPINFO cpinfo = (PMIMECPINFO)CoTaskMemAlloc(sizeof(MIMECPINFO));
		//   ULONG fetched = 0;
		//   while (S_OK == cp->Next(1, cpinfo, &fetched)) {
		//if (fetched) {
		//    printf("CodePage = %u\n", cpinfo->uiCodePage);
		//    wprintf(L"WebName = %s\n", cpinfo->wszWebCharset);
		//    wprintf(L"HdrName = %s\n", cpinfo->wszHeaderCharset);
		//    wprintf(L"BdyName = %s\n", cpinfo->wszBodyCharset);
		//}
		//cpinfo = (PMIMECPINFO)CoTaskMemRealloc(cpinfo, sizeof(MIMECPINFO));
		//fetched = 0;
		//   }
		//   CoTaskMemFree(cpinfo);

		//   cp->Release();
		//   ml1->Release();
		///////////////////////////////////////
	}

	inline EncodingConverter::~EncodingConverter()
	{
		if (conv_) {
			conv_->Release();
			conv_ = 0;
		}
		if (ml_) {
			ml_->Release();
			ml_ = 0;
		}
	}

	inline bool EncodingConverter::valid() const
	{
		return ((ml_!=0) && (conv_!=0) && (toCodePage_!=0) && (fromCodePage_!=0));
	}

	inline bool EncodingConverter::convert(const void* input, size_t& inputBytesLeft,
		void* output, size_t& outputBytesLeft)
	{
		if (toCodePage_ == fromCodePage_) {
			size_t bytesToCopy = min(inputBytesLeft, outputBytesLeft);
			memcpy(output, input, bytesToCopy);
			inputBytesLeft -= bytesToCopy;
			outputBytesLeft -= bytesToCopy;
		} else {
			BYTE* inbuf = (BYTE*)input;
			BYTE* outbuf = (BYTE*)output;
			UINT srcsize = (inputBytesLeft > (size_t)UINT_MAX) ? UINT_MAX : (UINT)inputBytesLeft;
			UINT dstsize = (outputBytesLeft > (size_t)UINT_MAX) ? UINT_MAX : (UINT)outputBytesLeft;

			HRESULT hr = conv_->DoConversion(inbuf, &srcsize, outbuf, &dstsize);
			if (FAILED(hr)) return false;

			inputBytesLeft -= srcsize;
			outputBytesLeft -= dstsize;
		}

		return true;
	}

	inline bool EncodingConverter::flush(void* output, size_t& outputBytesLeft)
	{
		reset();
		return true;
	}

	inline void EncodingConverter::reset()
	{
		DWORD prop = MLCONVCHARF_NOBESTFITCHARS;
		if (opt_ & CONVERT_TRANSLITERATE) {
			prop &= ~(DWORD)MLCONVCHARF_NOBESTFITCHARS;
		}

		conv_->Initialize(fromCodePage_, toCodePage_, prop);
	}

	DWORD EncodingConverter::encNameToCodePage(const char* encName)
	{
		DWORD codepage = 0;
		MIMECSETINFO charsetInfo;

		BSTR name = NULL;
		WCHAR* wideName = NULL;

		if (!_stricmp(encName, "UTF-16")	||
			!_stricmp(encName, "UTF-16BE")	||
			!_stricmp(encName, "UCS-2")	||
			!_stricmp(encName, "UCS-2BE")	||
			!_stricmp(encName, "UNICODEBIG"))
		{
			encName = "unicodeFFFE";
		}
		else if (!_stricmp(encName, "UTF-16LE")	||
			!_stricmp(encName, "UCS-2LE")	||
			!_stricmp(encName, "UNICODELITTLE"))
		{
			encName = "unicode";
		}
		else if (encName[0] && toupper((unsigned char)encName[0]) == 'C' &&
			encName[1] && toupper((unsigned char)encName[1]) == 'P')
		{
			return (DWORD)atoi(encName+2);
		}

		do {
			int widelen = MultiByteToWideChar(CP_ACP, 0, encName, -1, NULL, 0);
			if (widelen == 0) {
				break;
			}

			wideName = (WCHAR*)malloc((size_t)widelen * sizeof(WCHAR));
			MultiByteToWideChar(CP_ACP, 0, encName, -1, wideName, widelen);
			name = SysAllocString(wideName);

			if (FAILED(ml_->GetCharsetInfo(name, &charsetInfo))) {
				break;
			}
			codepage = charsetInfo.uiInternetEncoding;
		} while (false);

		// cleanup
		if (name) {
			SysFreeString(name);
		}
		if (wideName) {
			free(wideName);
		}

		return codepage;
	}

#else

	inline EncodingConverter::EncodingConverter(const char* fromEnc, const char* toEnc, OPTION opt)
	{
		cd_ = (iconv_t)(-1);

		fromEnc_ = fromEnc;
		toEnc_ = toEnc;

		std::string tocode = toEnc;
		if (opt & CONVERT_TRANSLITERATE) {
			tocode += "//TRANSLIT";
		}
		if (opt & CONVERT_DISCARD_ILSEQ) {
			tocode += "//IGNORE";
		}
		cd_ = iconv_open(tocode.c_str(), fromEnc);
	}

	inline EncodingConverter::~EncodingConverter()
	{
		if (valid()) {
			iconv_close(cd_);
			cd_ = (iconv_t)(-1);
		}
	}

	inline bool EncodingConverter::valid() const
	{
		return (cd_ != (iconv_t)(-1));
	}

	inline bool EncodingConverter::convert(const void* input, size_t& inputBytesLeft,
		void* output, size_t& outputBytesLeft)
	{
		char** inbuf = (char**)(&input);
		char** outbuf = (char**)(&output);

		size_t res = iconv(cd_, inbuf, &inputBytesLeft, outbuf, &outputBytesLeft);

		return (res != (size_t)(-1));
	}

	inline bool EncodingConverter::flush(void* output, size_t& outputBytesLeft)
	{
		char** outbuf = (char**)(&output);
		size_t res = iconv(cd_, NULL, NULL, outbuf, &outputBytesLeft);
		return (res != (size_t)(-1));
	}

	inline void EncodingConverter::reset()
	{
		iconv(cd_, NULL, NULL, NULL, NULL);
	}

#endif


}; // end of namespace portpp

#endif
