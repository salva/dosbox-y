
#ifndef ZIPSTREAMBUF
#define ZIPSTREAMBUF

#include "zip.h"
#include "unzip.h"
#include "ioapi.h"

#include <assert.h>
#include <algorithm>
#include <climits>

// Transport integrity only: drain a required member before owner installation.
// CRC is checked by minizip only after the complete member has been read.
inline int zip_verify_member(unzFile archive, const char *name) {
	int result = unzLocateFile(archive, name, 1 /* case sensitive */);
	if (result != UNZ_OK) return result;
	result = unzOpenCurrentFile(archive);
	if (result != UNZ_OK) return result;
	char buffer[16384];
	int count;
	do {
		count = unzReadCurrentFile(archive, buffer, sizeof(buffer));
	} while (count > 0);
	const int closed = unzCloseCurrentFile(archive);
	return count < 0 ? count : closed;
}

/* std::streambuf for writing to ZIP archive directly.
 * ZIP archive writer can only write one file at a time, do not
 * use multiple instances of this C++ class at a time! No seeking,
 * only sequential output! */
class zip_ostreambuf : public std::streambuf {
public:
	using Base = std::streambuf;
public:
	zip_ostreambuf(zipFile &n_zf) : basic_streambuf(), zf(n_zf) { }
	virtual ~zip_ostreambuf() { close(); }
	zip_ostreambuf(const zip_ostreambuf &) = delete;
	zip_ostreambuf &operator=(const zip_ostreambuf &) = delete;
public:
	virtual Base::int_type overflow(Base::int_type ch = Base::traits_type::eof()) override {
		if (Base::traits_type::eq_int_type(ch, Base::traits_type::eof()))
			return Base::traits_type::not_eof(ch);
		const auto byte = Base::traits_type::to_char_type(ch);
		return xsputn(&byte, 1) == 1 ? ch : Base::traits_type::eof();
	}
	virtual std::streamsize xsputn(const Base::char_type *s, std::streamsize count) override {
		assert(zf != NULL && !closed);
		std::streamsize done = 0;
		while (done < count) {
			const auto chunk = static_cast<unsigned>(std::min<std::streamsize>(count-done, INT_MAX));
			const int err = zipWriteInFileInZip(zf, s+done, chunk);
			if (err != ZIP_OK) { zf_err = err; break; }
			done += chunk;
		}
		return done;
	}
public:
	int close(void) {
		if (!closed) {
			closed = true;
			const int err = zipCloseFileInZip(zf);
			if (err != ZIP_OK) zf_err = err;
		}
		return zf_err;
	}
private:
	zipFile zf = NULL;
	int zf_err = ZIP_OK;
	bool closed = false;
};

class zip_istreambuf : public std::streambuf {
public:
	using Base = std::streambuf;
public:
	zip_istreambuf(unzFile &n_zf) : basic_streambuf(), zf(n_zf) { }
	virtual ~zip_istreambuf() { close(); }
	zip_istreambuf(const zip_istreambuf &) = delete;
	zip_istreambuf &operator=(const zip_istreambuf &) = delete;
public:
	virtual Base::int_type underflow() override {
		assert(zf != NULL && !closed);
		if (gptr() && gptr() < egptr())
			return Base::traits_type::to_int_type(*gptr());
		const int got = unzReadCurrentFile(zf, &lookahead, 1);
		if (got < 0) zf_err = got;
		if (got != 1) return Base::traits_type::eof();
		setg(&lookahead, &lookahead, &lookahead+1);
		return Base::traits_type::to_int_type(lookahead);
	}
	virtual std::streamsize xsgetn(Base::char_type *s, std::streamsize count) override {
		assert(zf != NULL && !closed);
		std::streamsize done = 0;
		if (count > 0 && gptr() && gptr() < egptr()) {
			*s = *gptr(); gbump(1); ++done;
		}
		while (done < count) {
			const auto chunk = static_cast<unsigned>(std::min<std::streamsize>(count-done, INT_MAX));
			const int got = unzReadCurrentFile(zf, s+done, chunk);
			if (got < 0) { zf_err = got; break; }
			if (!got) break;
			done += got;
		}
		return done;
	}
public:
	int close(void) {
		if (!closed) {
			closed = true;
			const int err = unzCloseCurrentFile(zf);
			if (err != UNZ_OK) zf_err = err;
		}
		return zf_err;
	}
private:
	unzFile zf = NULL;
	char lookahead = 0;
	int zf_err = UNZ_OK;
	bool closed = false;
};


#endif

